/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdio.h>
#define __USE_GNU /* See feature_test_macros(7) */
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <dvm/bf_drv_intf.h>
#include <lld/lld_gpio_if.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <tofino/bf_pal/bf_pal_types.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_led/bf_led.h>
#include <bf_pltfm_bd_cfg.h>
#include <bf_mav_led.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <bf_pltfm_sfp.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_uart.h>

#include <pltfm_types.h>

/* Trigger LED by a async queue. */
#define HAVE_ASYNC_LED
/* Keep blink when there's traffic on a port. */
#define HAVE_TRAFFIC_LED
/* LED context lookup table. */
#define HAVE_LEDCTX_FAST_LKT

void bf_pltfm_mav_led_init_x308p (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn);

void bf_pltfm_mav_led_init_x312p (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn);

void bf_pltfm_mav_led_init_x564p (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn);

void bf_pltfm_mav_led_init_x532p (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn);

void bf_pltfm_mav_led_init_hc36y24c (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn);

led_sync_fun_ptr g_mav_led_sync_func;


#if defined(HAVE_ASYNC_LED)
#include "lq.h"
bool async_led_debug_on = false;
#endif
/* For x312p-t, bus Competition is heavy. */
volatile uint64_t g_rt_async_led_q_length = 0;

/**** MAVERICKS LED holding buffer organization ******
 * sysCPLD proxies as the LED driver. Each sysCPLD has got 64  8bit
 * registers. each register serves two Leds (each nibble = 1 Led). Each QSFP
 * has got 4 channels. Each channel has one LED. So, 32 ports require
 * 32*4 = 128 leds ->64 registers in sysCPLD.
 *
 * Tofino's state-out buffer is streamed out to 64 sysCPLD (+64 ) registers to
 * display the LED. so, our design of mapping of LEds contents in state-out
 * buffer is as follows:
 *
 *    byte    port/chn    port/chn
 *   _______________________________
 *
 *    0       2/0           1/0       |
 *    1       2/1           1/1       |
 *    2       2/2           1/2       |
 *    3       2/3           1/3       |
 *    4       4/0           3/0       |--upper 32 ports
 *    5       4/1           3/1       |
 *    6       4/2           3/2       |
 *    7       4/3           3/3       |
 *    8       6/0           5/0       |
 *                                    |
 *                                    |
 *                                    |
 *   63      32/3          31/3       |
 *
 *   64      34/0          33/0       |--lower 32 ports
 *   65      34/1          33/1       |
 *                                    |
 *  127      64/3          63/3       |
 *
 *   128     cpu/1        cpu/0       |--cpu port (irrespective of platform)
 *   129     cpu/3        cpu/2       |
 *
 * Note that  state-out buffer is organized to keep odd or even ports
 * contiguously together. A different organization can also be designed, but
 * that has to be implemented in the APIs accordingly. APIs in this module
 * assume above organization of state-out buffer
 */

/* The code also assumes that all i2c ports used for led belong to a single
 * ethgpio block in tofino.
 */

#define PERIOD_400KHZ_IN_10_NSEC               \
  0x2C /* value acquired from ASIC simulation. Under 400Khz */
/* 400 Khz */
#define PERIOD_100KHZ_IN_10_NSEC               \
  (PERIOD_400KHZ_IN_10_NSEC * 4)
/* 100 Khz */
//#define TOFINO_I2C_PERIOD 0x26
/* 460 Khz */
//#define TOFINO_I2C_PERIOD 0x20
/* 1M */
//#define TOFINO_I2C_PERIOD 0xb

static volatile int bf_pltfm_led_initialized = 0;

static bf_sys_rmutex_t led_lock;
#define LED_LOCK   \
    bf_sys_rmutex_lock(&led_lock)
#define LED_UNLOCK \
    bf_sys_rmutex_unlock(&led_lock)

static struct led_ctx_t *
connID_chnlID_to_led_ctx_index[BF_PLAT_MAX_QSFP +
                               1][MAX_CHAN_PER_CONNECTOR]
= {{INVALID}};

static struct led_ctx_t *g_led_ctx = NULL;
static int g_max_led_ctx = 0;

struct color_t {
    uint8_t v;
    const char *str;
};

static struct color_t colors[] = {
    { BF_MAV_PORT_LED_BLUE, "Blue"},
    { BF_MAV_PORT_LED_GREEN, "Green"},
    { BF_MAV_PORT_LED_GREEN | BF_MAV_PORT_LED_BLUE, "Light Green"},
    { BF_MAV_PORT_LED_RED, "Red"},
    { BF_MAV_PORT_LED_BLUE  | BF_MAV_PORT_LED_RED, "Purple"},
    { BF_MAV_PORT_LED_GREEN | BF_MAV_PORT_LED_RED, "Yellow"},
    { BF_MAV_PORT_LED_GREEN | BF_MAV_PORT_LED_RED | BF_MAV_PORT_LED_BLUE, "White"}
};

/* led ctx fast lookup table init */
void led_ctx_lookup_table_init ()
{
    int i, j;
    struct led_ctx_t *p = NULL;

    for (i = 0; i < g_max_led_ctx; i++) {
        p = &g_led_ctx[i];
        if (p->chnl_id == UINT8_MAX) {
            for (j = 0; j < MAX_CHAN_PER_CONNECTOR; j ++) {
                connID_chnlID_to_led_ctx_index[p->conn_id %
                                           (BF_PLAT_MAX_QSFP + 1)][j %
                                                   MAX_CHAN_PER_CONNECTOR] = p;
            }
        } else {
                connID_chnlID_to_led_ctx_index[p->conn_id %
                                           (BF_PLAT_MAX_QSFP + 1)][p->chnl_id %
                                                   MAX_CHAN_PER_CONNECTOR] = p;
        }
    }

}

#if defined (HAVE_LEDCTX_FAST_LKT)
/* led ctx fast lookup table deinit */
void led_ctx_lookup_table_de_init ()
{
    int i, j;
    struct led_ctx_t *p = NULL;

    for (i = 0; i < BF_PLAT_MAX_QSFP; i++) {
        for (j = 0; j < MAX_CHAN_PER_CONNECTOR; j ++) {
            connID_chnlID_to_led_ctx_index[p->conn_id %
                                       (BF_PLAT_MAX_QSFP + 1)][j %
                                               MAX_CHAN_PER_CONNECTOR] = NULL;
        }
    }
}

/* search in map to rapid lookup speed. */
struct led_ctx_t *led_ctx_lookup_fast (
    IN uint32_t conn, IN uint32_t chnl)
{
    return (connID_chnlID_to_led_ctx_index[conn %
                                           (BF_PLAT_MAX_QSFP + 1)][chnl %
                                                   MAX_CHAN_PER_CONNECTOR]);
}
#endif

struct led_ctx_t *led_ctx_lookup (
    uint32_t conn_id,
    uint32_t chnl_id)
{
    int i;
    struct led_ctx_t *p = NULL;

    for (i = 0; i < g_max_led_ctx; i++) {
        p = &g_led_ctx[i];
        if (p->conn_id == conn_id) {
            if (p->chnl_id == UINT8_MAX) {
                return p;
            } else {
                if (p->chnl_id == chnl_id) {
                    return p;
                }
            }
        }
    }
    return NULL;
}

/* Write CPLD by tofino I2C. */
bf_pltfm_status_t bf_pltfm_tf_cpld_write (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t val)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    static const bf_dev_id_t dev = 0;

    err |= bfn_i2c_wr_reg_blocking (
               dev, sub_devid, (bf_io_pin_pair_t)idx, i2c_addr, offset, 1, &val, 1);

    if (err != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error %s(%d) Tofino i2c write failed for device: 0x%x offset 0x%x ",
            "data: 0x%x\n",
            bf_err_str (err),
            err,
            i2c_addr,
            offset,
            val);
        /* Reset i2c in case i2c is locked on error
         * could block for upto approxomately 100ms.
         * - by tsihang, 4 Nov. 2019 */
        err |= bfn_i2c_reset (dev, sub_devid, (bf_io_pin_pair_t)idx);
        bf_sys_usleep (100000);
    }

    return err;
}

/* Read CPLD by tofino I2C. */
bf_pltfm_status_t bf_pltfm_tf_cpld_read (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t *rd_buf)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    static const bf_dev_id_t dev = 0;

    err |= bfn_i2c_rd_reg_blocking (
               dev, sub_devid, (bf_io_pin_pair_t)idx, i2c_addr, offset, 1, rd_buf, 1);

    if (err != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error %s(%d) Tofino i2c read failed for device: 0x%x offset 0x%x.\n",
            bf_err_str (err),
            err,
            i2c_addr,
            offset);
        /* Reset i2c in case i2c is locked on error
         * could block for upto approxomately 100ms.*/
        err |= bfn_i2c_reset (dev, sub_devid, (bf_io_pin_pair_t)idx);
        bf_sys_usleep (100000);
    }

    return err;
}

/* init tofino i2c to access LED cpld. */
static int bf_pltfm_tf_i2c_init (int chip_id,
                                 bf_io_pin_pair_t idx)
{

    if (chip_id >= BF_MAX_DEV_COUNT ||
        bf_pltfm_led_initialized) {
        return -1;
    }
    /* configure the gpio_pin_pair as i2c pins */
    bfn_io_set_mode_i2c (chip_id, sub_devid, idx);
    /* set the i2c clk to TIMEOUT_100MS Khz (clk  period = 10000 nsec) */
    bfn_i2c_set_clk (chip_id, sub_devid, idx,
                    PERIOD_100KHZ_IN_10_NSEC);
    /* set i2c submode as REG (Different with Reference BSP) */
    bfn_i2c_set_submode (chip_id, sub_devid, idx,
                        BF_I2C_MODE_REG);

    return 0;
}

/** Platform port led set (for Mavericks) using Tofino state-out buffer
 *
 *  @param chip_id
 *   chip_id
 *  @param port
 *   port ( -1 for all the ports)
 *  @param led_col
 *   bit-wise or of BF_MAV_PORT_LED_RED, BF_MAV_PORT_LED_GREEN and
 *BF_MAV_PORT_LED_BLUE
 *  @param led_blink
 *   LED_BLINK or LED_NO_BLINK
 *  @return
 *   0 on success and -1 on error
 */
static int bf_pltfm_port_led_by_tofino_sync_set (
    int chip_id,
    bf_pltfm_port_info_t *port_info,
    uint8_t led_col,
    bf_led_blink_t led_blink)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    uint8_t val = 0;
    struct led_ctx_t *p;

    if (bf_pltfm_led_initialized == 0 || !port_info ||
        (port_info->conn_id <= 0 ||
         port_info->conn_id > (uint32_t)
         platform_num_ports_get() ||
         port_info->chnl_id > 3)) {
        return -1;
    }

    val = led_col &
          (BF_MAV_PORT_LED_RED | BF_MAV_PORT_LED_GREEN |
           BF_MAV_PORT_LED_BLUE);
    if (led_blink & LED_BLINK) {
        val |= BF_MAV_PORT_LED_BLINK;
    }

#if defined (HAVE_LEDCTX_FAST_LKT)
    p = led_ctx_lookup_fast (port_info->conn_id,
                        port_info->chnl_id);
#else
    p = led_ctx_lookup (port_info->conn_id,
                    port_info->chnl_id);
#endif
    if (!p) {
        return 0;
    }

    LED_LOCK;
    g_mav_led_sync_func (chip_id, p, val);
    LED_UNLOCK;

    return err;
}

#if defined(HAVE_ASYNC_LED)
static struct oryx_lq_ctx_t *async_led_q;
static pthread_attr_t async_led_q_attr;
static pthread_t async_led_q_thread;

struct lq_element_t {
    struct lq_prefix_t lq;
    uint32_t module;
    uint32_t chnl_id;
    uint8_t led_col;
} __attribute__ ((aligned (64)));

#define lq_element_size (sizeof(struct lq_element_t))

/** Platform port led set (for Mavericks) using Tofino state-out buffer
 *
 *  @param chip_id
 *   chip_id
 *  @param port
 *   port ( -1 for all the ports)
 *  @param led_col
 *   bit-wise or of BF_MAV_PORT_LED_RED, BF_MAV_PORT_LED_GREEN and
 *BF_MAV_PORT_LED_BLUE
 *  @param led_blink
 *   LED_BLINK or LED_NO_BLINK
 *  @return
 *   0 on success and -1 on error
 */
static int bf_pltfm_port_led_by_tofino_async_set (
    int chip_id,
    bf_pltfm_port_info_t *port_info,
    uint8_t led_col,
    bf_led_blink_t led_blink)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    uint8_t val = 0;
    struct lq_element_t *lqe;

    if (bf_pltfm_led_initialized == 0 || !port_info ||
        (port_info->conn_id <= 0 ||
         port_info->conn_id > (uint32_t)
         platform_num_ports_get() ||
         port_info->chnl_id > 3)) {
        return -1;
    }

    val = led_col &
          (BF_MAV_PORT_LED_RED | BF_MAV_PORT_LED_GREEN |
           BF_MAV_PORT_LED_BLUE);
    if (led_blink & LED_BLINK) {
        val |= BF_MAV_PORT_LED_BLINK;
    }

    lqe = malloc (lq_element_size);
    BUG_ON (lqe == NULL);
    memset ((void *)lqe, 0, lq_element_size);
    lqe->module = port_info->conn_id;
    lqe->chnl_id = port_info->chnl_id;
    lqe->led_col = val;
    /* equeue */
    oryx_lq_enqueue (async_led_q, lqe);

    if (async_led_debug_on) {
        fprintf (stdout, "%25s   : %2d/%2d : %2x\n",
                 "async eq", port_info->conn_id,
                 port_info->chnl_id,
                 lqe->led_col);
    }

    return err;
}

void *led_async_func (void *arg)
{
    (void)arg;
    /* led async set */
    struct lq_element_t *lqe;
    bf_pltfm_port_info_t port_info;
    uint8_t led_col = 0;
    int times = 0, max_usleep,
        max_try_times = 3;
    bf_led_blink_t blink = LED_NO_BLINK;

    bf_pltfm_led_initialized = 1;

    FOREVER {
        while ((lqe = oryx_lq_dequeue (async_led_q)) != NULL) {
            if (platform_type_equal (X312P)) {
                g_rt_async_led_q_length = oryx_lq_length (async_led_q);
            }
            port_info.conn_id = lqe->module;
            port_info.chnl_id = lqe->chnl_id;
            led_col           = lqe->led_col;
            blink             = LED_NO_BLINK;

            if (led_col & BF_MAV_PORT_LED_BLINK) {
                blink = LED_BLINK;
            }

            if (async_led_debug_on) {
                fprintf (stdout, "%25s   : %2d/%2d : %2x\n",
                         "async dq", port_info.conn_id, port_info.chnl_id,
                         led_col);
            }

            /* Retry @max_try_times when failed. */
            times = 0;
            max_usleep = 500;
            while ((times ++ < max_try_times) &&
                   bf_pltfm_port_led_by_tofino_sync_set (0,
                           &port_info,
                           led_col, blink)) {
                /* Give more time to avoid i2c busy after every time of retry. */
                max_usleep += 5000;
                bf_sys_usleep (max_usleep);
#if defined(BUILD_DEBUG)
                oryx_lq_dump (async_led_q);
#endif
                fprintf (stdout,
                         "%25s   : %2d/%2d : %2x : retry %d times\n",
                         "async dq", port_info.conn_id, port_info.chnl_id,
                         led_col,
                         times);
                continue;
            }
            /* No matter pass or fail, free lqe element at last. */
            free (lqe);
            lqe = NULL;
        }

#if !defined(LQ_ENABLE_PASSIVE)
        /* Make sure the thread is NOT dysphoric when the PASSIVE mode of the LQ enabled. */
        bf_sys_usleep (100000);
#endif
    }
    return NULL;
}

static ucli_status_t
bf_pltfm_ucli_ucli__led_async_debug_
(ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "led-async-debug", 0,
        "enable/disable led async debug.");

    async_led_debug_on = !async_led_debug_on;

    oryx_lq_dump (async_led_q);

    aim_printf (&uc->pvs,
                "Async LED debug %s \n",
                async_led_debug_on ? "enabled" : "disabled");
    return 0;
}

#endif  /* end of HAVE_ASYNC_LED */

static int bf_pltfm_port_led_by_tofino_set (
    int chip_id,
    bf_pltfm_port_info_t *port_info,
    uint8_t led_col,
    bf_led_blink_t led_blink)
{
#if defined(HAVE_ASYNC_LED)
    return bf_pltfm_port_led_by_tofino_async_set (
               chip_id, port_info, led_col, led_blink);
#else
    return bf_pltfm_port_led_by_tofino_sync_set (
               chip_id, port_info, led_col, led_blink);
#endif
}


/** Platform port led set (for Mavericks) using sysCPLD access
*
*  @param chip_id
*   chip_id
*  @param port_info
*    bf_pltfm_port_info_t *, NULL for all ports
*  @param led_cond
*   bf_led_condition_t
*  @return
*   0 on success and -1 on error
*/
int bf_pltfm_port_led_set (int chip_id,
                           bf_pltfm_port_info_t *port_info,
                           bf_led_condition_t led_cond)
{
    uint8_t led_col = BF_MAV_PORT_LED_OFF;

    switch (led_cond) {
        case BF_LED_POST_PORT_DIS:
            //if (platform_type_equal(X532P) ||
            //    platform_type_equal(X564P)) {
            //    led_col = BF_MAV_PORT_LED_RED |
            //              BF_MAV_PORT_LED_GREEN;
            //}
            break;
        case BF_LED_PRE_PORT_EN:
            if (platform_type_equal(X532P) ||
                platform_type_equal(X564P)) {
                led_col = BF_MAV_PORT_LED_RED |
                          BF_MAV_PORT_LED_GREEN;
            }
            break;
        case BF_LED_PORT_LINK_DOWN:
            if (platform_type_equal(X532P) ||
                platform_type_equal(X564P)) {
                led_col = BF_MAV_PORT_LED_RED |
                          BF_MAV_PORT_LED_GREEN;
            }
            break;
        case BF_LED_PORT_LINK_UP:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;
        case BF_LED_PORT_RED:
            led_col = BF_MAV_PORT_LED_RED;
            break;
        case BF_LED_PORT_GREEN:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;
        case BF_LED_PORT_BLUE:
            led_col = BF_MAV_PORT_LED_BLUE;
            break;

        /* by tsihang, 21 Nov. 2019 */
        case BF_LED_PORT_LINKUP_1G_10G:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;
        case BF_LED_PORT_LINKUP_25G:
            led_col = BF_MAV_PORT_LED_GREEN |
                      BF_MAV_PORT_LED_BLUE;
            break;
        case BF_LED_PORT_LINKUP_40G:
            led_col = BF_MAV_PORT_LED_BLUE;
            break;
        case BF_LED_PORT_LINKUP_50G:
            led_col = BF_MAV_PORT_LED_BLUE |
                      BF_MAV_PORT_LED_RED;
            break;
        case BF_LED_PORT_LINKUP_100G:
            led_col = BF_MAV_PORT_LED_BLUE |
                      BF_MAV_PORT_LED_RED |
                      BF_MAV_PORT_LED_GREEN;
            break;

        case BF_LED_POST_PORT_DEL:
        default:
            led_col = BF_MAV_PORT_LED_OFF;
            break;
    }

    return (bf_pltfm_port_led_by_tofino_set (
                chip_id, port_info, led_col, LED_NO_BLINK));
}

#if defined(HAVE_TRAFFIC_LED)
static pthread_attr_t traffic_led_attr;
static pthread_t traffic_led_thread;
static void bf_pltfm_port_has_traffic (
    bf_pltfm_port_info_t *port_info,
    bool *has_traffic)
{
    bf_pal_front_port_handle_t port_hdl;

    port_hdl.conn_id = port_info->conn_id;
    port_hdl.chnl_id = port_info->chnl_id;
    bf_pm_port_traffic_status_get (0, &port_hdl,
                                   has_traffic);
}

void *led_traffic_func (void *arg)
{
    (void)arg;
    bf_pltfm_port_info_t port_info;
    struct led_ctx_t *p;
    int lane = 0;
    bool has_traffic = false;
    bf_led_blink_t blink = LED_NO_BLINK;
    static uint64_t alive = 0;
    uint8_t led_col = 0;

    FOREVER {
        sleep (5);
        alive ++;
        //fprintf(stdout, "alive : %lu\n", alive);
#if defined(HAVE_ASYNC_LED)
        //if (alive % 3 == 0) {
        //    oryx_lq_dump (async_led_q);
        //}
#endif
        foreach_element (0, g_max_led_ctx)
        {
            has_traffic = false;
            p = &g_led_ctx[each_element];
            port_info.conn_id = p->conn_id;
            port_info.chnl_id = p->chnl_id;
            if (p->chnl_id == UINT8_MAX) {
                /* this means this port is QSFP and only has one led light */
                // has traffic ?
                for (lane = 0; lane < QSFP_NUM_CHN; lane ++) {
                    port_info.chnl_id = lane;
                    bf_pltfm_port_has_traffic (&port_info,
                                               &has_traffic);
#if defined(HAVE_ASYNC_LED)
                    if (async_led_debug_on && has_traffic) {
                        fprintf (stdout, "Traffic : %2d/%d %s\n",
                                 port_info.conn_id, port_info.chnl_id,
                                 has_traffic ? "has traffic" : "no traffic");
                    }
#endif
                    break;
                }
            } else {
                /* this means this port is SFP or this port is QSFP with 4 led lights */
                // has traffic ?
                bf_pltfm_port_has_traffic (&port_info,
                                           &has_traffic);
#if defined(HAVE_ASYNC_LED)
                if (async_led_debug_on && has_traffic) {
                    fprintf (stdout, "Traffic : %2d/%d %s\n",
                             port_info.conn_id, port_info.chnl_id,
                             has_traffic ? "has traffic" : "no traffic");
                }
#endif
            }

            /* Keep blinking if there's traffic while the LED has already been blinking. */
            if (has_traffic &&
                (p->val & BF_MAV_PORT_LED_BLINK)) {
                continue;
            }

            /* Keep no blinking if there's no traffic. */
            if (!has_traffic &&
                ! (p->val & BF_MAV_PORT_LED_BLINK)) {
                continue;
            }

            /* Start to blink if there's traffic while the LED does not blink at this moment. */
            if (has_traffic &&
                ! (p->val & BF_MAV_PORT_LED_BLINK)) {
                blink = LED_BLINK;
                led_col = (p->val & ~BF_MAV_PORT_LED_BLINK);
            }

            /* Stop blinking if there's no traffic while the LED is blinking. */
            if (!has_traffic &&
                (p->val & BF_MAV_PORT_LED_BLINK)) {
                blink = LED_NO_BLINK;
                led_col = (p->val & ~BF_MAV_PORT_LED_BLINK);
            }

            bf_pltfm_port_led_by_tofino_set (0, &port_info,
                                             led_col, blink);
        }
    }
    return NULL;
}
#endif  /* end of HAVE_TRAFFIC_LED */

static inline bool bf_pltfm_has_loc_led ()
{
    bool has_loc_led = true;

    if (platform_type_equal (X312P) || platform_type_equal (X308P)) {
        has_loc_led = false;
    } else if (platform_type_equal (X532P) &&
              (platform_subtype_equal (v1dot0) ||
               platform_subtype_equal (v1dot1))) {
        has_loc_led = false;
    } else if (platform_type_equal (X564P) &&
              (platform_subtype_equal (v1dot1) ||
               platform_subtype_equal (v1dot2))) {
        has_loc_led = false;
    }
    return has_loc_led;
}

/* Initialized by BMC and OFF by default.
 * Return Value:
 *         0: Success
 *  Negative: Failed
 *    0xdead: no location LED.
 */
int bf_pltfm_location_led_set (int on_off)
{
    uint8_t buf[2] = {0x00, 0xaa};

    if (!bf_pltfm_has_loc_led()) {
        return 0xdead;
    } else {
        buf[0] = (on_off == 1) ? 1 : 0;
        return bf_pltfm_bmc_uart_write_read (0x03, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
    }
}

static ucli_status_t
bf_pltfm_ucli_ucli__loc_led__ (ucli_context_t
                               *uc)
{
    bool loc_led_on = false;
//    uint8_t buf[2] = {0x00, 0xaa};

    /* Keep the name consistent with the document X-T Programmable Bare Metal. */
    UCLI_COMMAND_INFO (uc, "loc-led", 1, "loc-led <on/off>");

    if (memcmp (uc->pargs->args[0], "on", 2) == 0) {
        loc_led_on = true;
    } else if (memcmp (uc->pargs->args[0], "off", 3) == 0){
        loc_led_on = false;
    } else {
        aim_printf (&uc->pvs, "Usage: loc-led <on/off>\n");
        return 0;
    }

    if (!bf_pltfm_has_loc_led()) {
        aim_printf (&uc->pvs, "No location LED on this device.\n");
        return 0;
    }

    aim_printf (&uc->pvs, "Location LED: %s\n",
        loc_led_on ? "ON" : "OFF" );
    LOG_WARNING ("Location LED: %s\n",
        loc_led_on ? "ON" : "OFF" );

    bf_pltfm_location_led_set(loc_led_on ? 1 : 0);

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__led_cpld_rd_tofino_
(ucli_context_t *uc)
{
    int reg, reg0, reg1, chn;
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    bf_io_pin_pair_t idx;
    uint8_t val = 0;

    UCLI_COMMAND_INFO (
        uc, "tf-cpld-rd", 3,
        "<chn, 0-PIN0/1 1-PIN4/5> 0x<start reg> 0x<end reg>");

    chn  = atoi (uc->pargs->args[0]);
    reg0 = strtol (uc->pargs->args[1], NULL, 16);
    reg1 = strtol (uc->pargs->args[2], NULL, 16);

    aim_printf (&uc->pvs,
                "tf-cpld-rd <chn=%d> <addr=0x%02x> <start reg=0x%x> <end reg=0x%x>\n",
                chn, AF_LED_CPLD_I2C_ADDR, reg0, reg1);
    if (chn == 0) {
        idx = PIN01;
    } else if (chn == 1) {
        idx = PIN45;
    } else if (chn == 2) {
        idx = PINXY;
    } else {
        aim_printf (&uc->pvs,
                    "chn error\n");
        return 0;
    }

    if (reg0 == reg1) {
        reg = reg0;
        err = bf_pltfm_tf_cpld_read (
                  idx,
                  AF_LED_CPLD_I2C_ADDR,
                  reg,
                  &val);
        if (!err) {
            aim_printf (&uc->pvs, "Success "
                        "0x%02x: 0x%02x\n", reg, val);
        } else {
            aim_printf (&uc->pvs, "* err %d: "
                        "reg 0x%02x\n", reg, err);
        }
    } else {
        for (reg = reg0; reg <= reg1; reg ++) {
            val = 0;
            err = BF_PLTFM_SUCCESS;
            err = bf_pltfm_tf_cpld_read (
                      idx,
                      AF_LED_CPLD_I2C_ADDR,
                      reg,
                      &val);
            if (!err) {
                aim_printf (&uc->pvs, "Success "
                            "0x%02x: 0x%02x\n", reg, val);
            } else {
                aim_printf (&uc->pvs, "* err %d: "
                            "reg 0x%02x\n", reg, err);
            }
        }
    }
    return 0;
}

/* - by tsihang, 3 Nov. 2019 */
static ucli_status_t
bf_pltfm_ucli_ucli__led_cpld_wr_tofino_
(ucli_context_t *uc)
{
    int reg, reg0, reg1, chn;
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    bf_io_pin_pair_t idx;
    uint8_t val, rd_val;

    UCLI_COMMAND_INFO (
        uc, "tf-cpld-wr", 4,
        "<chn, 0-PIN0/1 1-PIN4/5> 0x<start reg> 0x<end reg> 0x<val>");

    chn = atoi (uc->pargs->args[0]);
    reg0 = strtol (uc->pargs->args[1], NULL, 16);
    reg1 = strtol (uc->pargs->args[2], NULL, 16);
    val = strtol (uc->pargs->args[3], NULL, 16);

    aim_printf (&uc->pvs,
                "tf-cpld-wr <chn=%d> <addr=0x%02x> <start reg=0x%x> <end reg=0x%x> <val=0x%x>\n",
                chn, AF_LED_CPLD_I2C_ADDR, reg0, reg1, val);

    if (chn == 0) {
        idx = PIN01;
    } else if (chn == 1) {
        idx = PIN45;
    } else if (chn == 2) {
        idx = PINXY;
    } else {
        aim_printf (&uc->pvs,
                    "chn error\n");
        return 0;
    }

    if (reg0 == reg1) {
        reg = reg0;
        err = BF_PLTFM_SUCCESS;
        err |= bf_pltfm_tf_cpld_write (
                   idx,
                   AF_LED_CPLD_I2C_ADDR,
                   reg,
                   val
               );

        bf_sys_usleep (500);
        err |= bf_pltfm_tf_cpld_read (
                   idx,
                   AF_LED_CPLD_I2C_ADDR,
                   reg,
                   &rd_val
               );
        if (!err && (val == rd_val)) {
            aim_printf (&uc->pvs,
                        "Success\n");
        } else {
            aim_printf (&uc->pvs, "* err %d: "
                        "wr 0x%02x to 0x%02x\n", val, reg, err);
        }
    } else {
        for (reg = reg0; reg <= reg1; reg ++) {
            err = BF_PLTFM_SUCCESS;
            err |= bf_pltfm_tf_cpld_write (
                       idx,
                       AF_LED_CPLD_I2C_ADDR,
                       reg,
                       val
                   );
            bf_sys_usleep (500);
            err |= bf_pltfm_tf_cpld_read (
                       idx,
                       AF_LED_CPLD_I2C_ADDR,
                       reg,
                       &rd_val
                   );
            if (err || (val != rd_val)) {
                aim_printf (&uc->pvs, "* err %d: "
                            "rd 0x%02x from 0x%02x\n", val, reg, err);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__led_loop_tofino_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    int i;
    int secs = 1, usecs = 100000;

    struct color_t *col;

    UCLI_COMMAND_INFO (
        uc, "led-loop", 0, " Test LED");

    int err;
    bf_pltfm_port_info_t port_info;

    // kill all light
    aim_printf (&uc->pvs, "All off\n");
    foreach_element (0, g_max_led_ctx) {
        err = 0;
        port_info.conn_id =
            g_led_ctx[each_element].conn_id;
        port_info.chnl_id =
            g_led_ctx[each_element].chnl_id;
        if (port_info.chnl_id == UINT8_MAX) {
            port_info.chnl_id = 0;
        }
        err = bf_pltfm_port_led_set (dev, &port_info,
                                     BF_LED_POST_PORT_DEL);
        if (err) {
            LOG_ERROR ("Unable to reset LED on port %d/%d",
                       port_info.conn_id,
                       port_info.chnl_id);
        }
    }

    // light one by one with each color(no blink)
    for (i = 0; i < (int)ARRAY_LENGTH (colors);
         i ++) {
        col = &colors[i];
        aim_printf (&uc->pvs,
                    "light one by one with each color(no blink) %s\n",
                    col->str);
        foreach_element (0, g_max_led_ctx) {
            err = 0;
            port_info.conn_id =
                g_led_ctx[each_element].conn_id;
            port_info.chnl_id =
                g_led_ctx[each_element].chnl_id;
            if (port_info.chnl_id == UINT8_MAX) {
                port_info.chnl_id = 0;
            }
            err = bf_pltfm_port_led_by_tofino_set (dev,
                                                   &port_info, col->v, LED_NO_BLINK);
            if (err) {
                LOG_ERROR ("Unable to set LED on port %d/%d",
                           port_info.conn_id,
                           port_info.chnl_id);
            }
            bf_sys_usleep (usecs);
        }
        bf_sys_usleep (secs * 1000000);
        if (platform_type_equal (X312P)) {
            break;
        }
    }

    // light one by one with each color(blink)
    for (i = 0; i < (int)ARRAY_LENGTH (colors);
         i ++) {
        col = &colors[i];
        aim_printf (&uc->pvs,
                    "light one by one with each color(blink) %s\n",
                    col->str);
        foreach_element (0, g_max_led_ctx) {
            err = 0;
            port_info.conn_id =
                g_led_ctx[each_element].conn_id;
            port_info.chnl_id =
                g_led_ctx[each_element].chnl_id;
            if (port_info.chnl_id == UINT8_MAX) {
                port_info.chnl_id = 0;
            }
            err = bf_pltfm_port_led_by_tofino_set (dev,
                                                   &port_info, col->v, LED_BLINK);
            if (err) {
                LOG_ERROR ("Unable to set LED on port %d/%d",
                           port_info.conn_id,
                           port_info.chnl_id);
            }
            bf_sys_usleep (usecs);
        }
        bf_sys_usleep (secs * 1000000);
        if (platform_type_equal (X312P)) {
            break;
        }
    }

    // kill all light
    aim_printf (&uc->pvs, "All off\n");
    foreach_element (0, g_max_led_ctx) {
        err = 0;
        port_info.conn_id =
            g_led_ctx[each_element].conn_id;
        port_info.chnl_id =
            g_led_ctx[each_element].chnl_id;
        if (port_info.chnl_id == UINT8_MAX) {
            port_info.chnl_id = 0;
        }
        err = bf_pltfm_port_led_set (dev, &port_info,
                                     BF_LED_POST_PORT_DEL);
        if (err) {
            LOG_ERROR ("Unable to reset LED on port %d/%d",
                       port_info.conn_id,
                       port_info.chnl_id);
        }
    }

    aim_printf (&uc->pvs, "LED loops done\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__led_set_tofino_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    int port, chn;
    int col;
    int blink;
    int err = -1;
    bf_led_blink_t led_blink;
    bf_pltfm_port_info_t port_info;

    UCLI_COMMAND_INFO (
        uc, "led-set", 5,
        "led-set <dev> <port> <chn> 0x<col> <blink>");

    dev = atoi (uc->pargs->args[0]);
    port = atoi (uc->pargs->args[1]);
    chn = atoi (uc->pargs->args[2]);
    col = strtol (uc->pargs->args[3], NULL, 16);
    blink = atoi (uc->pargs->args[4]);

    aim_printf (&uc->pvs,
                "bf_pltfm_led: led-set <dev=%d> <port=%d> <chn=%d> <col=0x%x> "
                "<blink=%d>\n",
                dev,
                port,
                chn,
                col,
                blink);

    led_blink = blink ? LED_BLINK : LED_NO_BLINK;

    if (port == -1) {
        foreach_element (0, g_max_led_ctx) {
            err = 0;
            port_info.conn_id =
                g_led_ctx[each_element].conn_id;
            port_info.chnl_id =
                g_led_ctx[each_element].chnl_id;
            if (port_info.chnl_id == UINT8_MAX) {
                port_info.chnl_id = 0;
            }
            err = bf_pltfm_port_led_by_tofino_set (dev,
                                                   &port_info, col, led_blink);
        }
    } else {
        port_info.conn_id = port;
        port_info.chnl_id = chn;
        err = bf_pltfm_port_led_by_tofino_set (dev,
                                               &port_info, col, led_blink);
    }

    if (err) {
        aim_printf (&uc->pvs,
                    "error setting LED(%02d/%2d : %d)\n",
                    err);
    } else {
        aim_printf (&uc->pvs, "LED set OK\n");
    }
    return 0;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_led_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__led_set_tofino_,
    bf_pltfm_ucli_ucli__led_cpld_rd_tofino_,
    bf_pltfm_ucli_ucli__led_cpld_wr_tofino_,
    bf_pltfm_ucli_ucli__led_loop_tofino_,
    bf_pltfm_ucli_ucli__loc_led__,
#if defined(HAVE_ASYNC_LED)
    bf_pltfm_ucli_ucli__led_async_debug_,
#endif
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_led_ucli_module__
= {
    "led_ucli", NULL, bf_pltfm_led_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_led_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_led_ucli_module__);
    n = ucli_node_create ("led", m,
                          &bf_pltfm_led_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("led"));
    return n;
}

/* Init function  */
int bf_pltfm_led_init (int chip_id)
{
    int err = 0;

    /* No matter in SYNC led mode or ASYNC led mode,
     * a lock should be used to protect TF i2c.
     * by tsihang, 2021-07-07. */
    if (bf_sys_rmutex_init (&led_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: LED lock init failed\n");
        return -1;
    }

    /* PIN pair 0:1 go to upper CPLD */
    if (platform_type_equal (X564P) ||
        platform_type_equal (X532P) ||
        platform_type_equal (X308P) ||
        platform_type_equal (HC)) {
        /* PIN pair 0:1 go to upper CPLD */
        if (bf_pltfm_tf_i2c_init (chip_id, PIN01)) {
            LOG_ERROR ("error upper tof_i2c_led_init\n");
            return -1;
        }
    }

    if (platform_type_equal (X564P) ||
        platform_type_equal (X308P) ||
        platform_type_equal (HC)) {
        /* PIN pair 4:5 go to lower CPLD */
        if (bf_pltfm_tf_i2c_init (chip_id, PIN45)) {
            LOG_ERROR ("error lower tof_i2c_led_init\n");
            return -1;
        }
    }

    if (platform_type_equal (X532P)) {
        bf_pltfm_mav_led_init_x532p ((struct led_ctx_t **)&g_led_ctx, &g_max_led_ctx, &g_mav_led_sync_func);
    } else if (platform_type_equal (X564P)) {
        bf_pltfm_mav_led_init_x564p ((struct led_ctx_t **)&g_led_ctx, &g_max_led_ctx, &g_mav_led_sync_func);
    } else if (platform_type_equal (X308P)) {
        bf_pltfm_mav_led_init_x308p ((struct led_ctx_t **)&g_led_ctx, &g_max_led_ctx, &g_mav_led_sync_func);
    } else if (platform_type_equal (X312P)) {
        bf_pltfm_mav_led_init_x312p ((struct led_ctx_t **)&g_led_ctx, &g_max_led_ctx, &g_mav_led_sync_func);
    } else if (platform_type_equal (HC)) {
        bf_pltfm_mav_led_init_hc36y24c ((struct led_ctx_t **)&g_led_ctx, &g_max_led_ctx, &g_mav_led_sync_func);
    }
    BUG_ON (g_led_ctx == NULL);
    BUG_ON (g_mav_led_sync_func == NULL);

#if defined (HAVE_LEDCTX_FAST_LKT)
    /* init led ctx fast lookup table. */
    led_ctx_lookup_table_init();
#endif

    /* If the platform cann't be detected, will quit
     * at the beginning of platform init. So, here
     * we don't need to check g_led_ctx.
     * by tsihang, 2021-07-13. */

#if !defined(HAVE_ASYNC_LED)
    bf_pltfm_led_initialized = 1;
#endif

#if defined(HAVE_ASYNC_LED)
    uint32_t    lq_cfg = LQ_TYPE_PASSIVE;

    oryx_lq_new ("LED ASYNC QUEUE", lq_cfg,
                 (void **)&async_led_q);
    oryx_lq_dump (async_led_q);

    /* Start async led first. */
    err |= pthread_attr_init (&async_led_q_attr);
    err |= pthread_create (&async_led_q_thread,
                           &async_led_q_attr,
                           led_async_func, NULL);
    err |= pthread_setname_np (async_led_q_thread,
                               "led_async");
    err |= pthread_detach (async_led_q_thread);
    if (!err) {
        LOG_DEBUG (
            "LED Async thread started(%s)"
            "\n",
            "led_async");
    } else {
        LOG_ERROR (
            "%s[%d], "
            "pthread_detach(%s)"
            "\n",
            __FILE__, __LINE__, oryx_safe_strerror (errno));
    }
    /* Wait for led_async_func ready. */
    while (!bf_pltfm_led_initialized);
#endif

    if (platform_type_equal (X312P)) {
        ;
    } else {
        bf_pltfm_port_info_t port_info;
        foreach_element (0, g_max_led_ctx) {
            err = 0;
            port_info.conn_id =
                g_led_ctx[each_element].conn_id;
            port_info.chnl_id =
                g_led_ctx[each_element].chnl_id;
            if (port_info.chnl_id == UINT8_MAX) {
                port_info.chnl_id = 0;
            }
            err = bf_pltfm_port_led_by_tofino_sync_set(chip_id, &port_info,
                                        BF_MAV_PORT_LED_OFF, LED_NO_BLINK);

            if (err) {
                LOG_ERROR ("Unable to reset LED on port %d/%d",
                        port_info.conn_id,
                        port_info.chnl_id);
            }
        }
    }

#if defined(HAVE_ASYNC_LED)
    /* Actually, we do NOT know the time when the LED initialized finish in ASYNC mode.
     * So, here we wait few seconds for that procedure done at startup stage.
     * by tsihang, 2021-07-07. */
    sleep (3);
#endif

#if defined(HAVE_TRAFFIC_LED)
    /* LED traffic should startup after all led init.
     * By tsihang, 2021-07-07. */
    err |= pthread_attr_init (&traffic_led_attr);
    err |= pthread_create (&traffic_led_thread,
                           &traffic_led_attr,
                           led_traffic_func, NULL);
    err |= pthread_setname_np (traffic_led_thread,
                               "led_traffic");
    err |= pthread_detach (traffic_led_thread);
    if (!err) {
        LOG_DEBUG (
            "LED Traffic thread started(%s)"
            "\n",
            "led_traffic");
    } else {
        LOG_ERROR (
            "%s[%d], "
            "LED Traffic thread started(%s)"
            "\n",
            __FILE__, __LINE__, oryx_safe_strerror (errno));
    }
#endif

    return 0;
}

int bf_pltfm_led_de_init ()
{
    int ret, err;
    pthread_attr_t attr;
    int chip_id = 0;
    /* Make sure all LED off. */
    bf_pltfm_port_info_t port_info;

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);
    if (!bf_pltfm_led_initialized) {
        return BF_PLTFM_SUCCESS;
    }

    if (platform_type_equal (X312P)) {
        ;
    } else {
        foreach_element (0, g_max_led_ctx) {
            err = 0;
            port_info.conn_id =
                g_led_ctx[each_element].conn_id;
            port_info.chnl_id =
                g_led_ctx[each_element].chnl_id;
            if (port_info.chnl_id == UINT8_MAX) {
                port_info.chnl_id = 0;
            }
            err = bf_pltfm_port_led_set (chip_id, &port_info,
                                         BF_LED_POST_PORT_DEL);
            if (err) {
                LOG_ERROR ("Unable to reset LED on port %d/%d",
                           port_info.conn_id,
                           port_info.chnl_id);
            }
        }
    }

    sleep (3);

#if defined(HAVE_ASYNC_LED)
    if (async_led_q_thread) {
        err = pthread_getattr_np (
                  async_led_q_thread,
                  &attr);
        ret = pthread_cancel (
                  async_led_q_thread);
        if (ret != 0) {
            LOG_ERROR (
                "pltfm_mgr: ERROR: thread cancelation failed for ASYNC LED, "
                "ret=%d\n", ret);
        } else {
            pthread_join (
                async_led_q_thread, NULL);
        }
        if (!err) {
            pthread_attr_destroy (&attr);
        }
        async_led_q_thread = 0;
    }
#endif

#if defined(HAVE_TRAFFIC_LED)
    if (traffic_led_thread) {
        err = pthread_getattr_np (
                  traffic_led_thread,
                  &attr);
        ret = pthread_cancel (
                  traffic_led_thread);
        if (ret != 0) {
            LOG_ERROR (
                "pltfm_mgr: ERROR: thread cancelation failed for Traffic LED, "
                "ret=%d\n", ret);
        } else {
            pthread_join (
                traffic_led_thread, NULL);
        }
        if (!err) {
            pthread_attr_destroy (&attr);
        }
        traffic_led_thread = 0;
    }
#endif

#if defined (HAVE_LEDCTX_FAST_LKT)
    /* De init led ctx fast lookup table.
     * There's a bug when switchd lauched by stratum.
     * by tsihang, 2022-04-21. */
    //led_ctx_lookup_table_de_init ();
#endif

    bf_sys_rmutex_del (&led_lock);
    /* Make warm init happy. Added by tsihang, 2021-11-01. */
    bf_pltfm_led_initialized = 0;

    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);
    return 0;
}

