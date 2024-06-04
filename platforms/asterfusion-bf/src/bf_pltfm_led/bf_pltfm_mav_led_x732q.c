/*!
 * @file bf_pltfm_mav_led.c
 * @date 2023/08/21
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

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

#include <pltfm_types.h>

static struct led_ctx_t led_ctx_x732q[] = {
    {"QC1",   1, UINT8_MAX, PIN01, 0x00, 0, 0xFF},
    {"QC2",   2, UINT8_MAX, PIN01, 0x00, 4, 0xFF},
    {"QC3",   3, UINT8_MAX, PIN01, 0x01, 0, 0xFF},
    {"QC4",   4, UINT8_MAX, PIN01, 0x01, 4, 0xFF},

    {"QC5",   5, UINT8_MAX, PIN01, 0x02, 0, 0xFF},
    {"QC6",   6, UINT8_MAX, PIN01, 0x02, 4, 0xFF},
    {"QC7",   7, UINT8_MAX, PIN01, 0x03, 0, 0xFF},
    {"QC8",   8, UINT8_MAX, PIN01, 0x03, 4, 0xFF},

    {"QC9",   9, UINT8_MAX, PIN01, 0x04, 0, 0xFF},
    {"QC10", 10, UINT8_MAX, PIN01, 0x04, 4, 0xFF},
    {"QC11", 11, UINT8_MAX, PIN01, 0x05, 0, 0xFF},
    {"QC12", 12, UINT8_MAX, PIN01, 0x05, 4, 0xFF},

    {"QC13", 13, UINT8_MAX, PIN01, 0x06, 0, 0xFF},
    {"QC14", 14, UINT8_MAX, PIN01, 0x06, 4, 0xFF},
    {"QC15", 15, UINT8_MAX, PIN01, 0x07, 0, 0xFF},
    {"QC16", 16, UINT8_MAX, PIN01, 0x07, 4, 0xFF},

    {"QC17", 17, UINT8_MAX, PIN01, 0x08, 0, 0xFF},
    {"QC18", 18, UINT8_MAX, PIN01, 0x08, 4, 0xFF},
    {"QC19", 19, UINT8_MAX, PIN01, 0x09, 0, 0xFF},
    {"QC20", 20, UINT8_MAX, PIN01, 0x09, 4, 0xFF},

    {"QC21", 21, UINT8_MAX, PIN01, 0x0A, 0, 0xFF},
    {"QC22", 22, UINT8_MAX, PIN01, 0x0A, 4, 0xFF},
    {"QC23", 23, UINT8_MAX, PIN01, 0x0B, 0, 0xFF},
    {"QC24", 24, UINT8_MAX, PIN01, 0x0B, 4, 0xFF},

    {"QC25", 25, UINT8_MAX, PIN01, 0x0C, 0, 0xFF},
    {"QC26", 26, UINT8_MAX, PIN01, 0x0C, 4, 0xFF},
    {"QC27", 27, UINT8_MAX, PIN01, 0x0D, 0, 0xFF},
    {"QC28", 28, UINT8_MAX, PIN01, 0x0D, 4, 0xFF},

    {"QC29", 29, UINT8_MAX, PIN01, 0x0E, 0, 0xFF},
    {"QC30", 30, UINT8_MAX, PIN01, 0x0E, 4, 0xFF},
    {"QC31", 31, UINT8_MAX, PIN01, 0x0F, 0, 0xFF},
    {"QC32", 32, UINT8_MAX, PIN01, 0x0F, 4, 0xFF},

    {"Y1",  33,         0, PIN01, 0x10, 0, 0xFF},
    {"Y2",  33,         1, PIN01, 0x10, 4, 0xFF},
};

/* Write CPLD by tofino I2C. */
bf_pltfm_status_t bf_pltfm_tf_cpld_write (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t val);
/* Read CPLD by tofino I2C. */
bf_pltfm_status_t bf_pltfm_tf_cpld_read (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t *rd_buf);

static void led_cond_convert_to_color_x732q (bf_led_condition_t led_cond,
    uint8_t *led_color)
{
    uint8_t led_col = BF_MAV_PORT_LED_OFF;
    switch (led_cond) {
        case BF_LED_POST_PORT_DEL:
        case BF_LED_POST_PORT_DIS:
            break;
        case BF_LED_PRE_PORT_EN:
        case BF_LED_PORT_LINK_DOWN:
            /* Yellow when port is enabled but link is not up */
            led_col = BF_MAV_PORT_LED_RED |
                      BF_MAV_PORT_LED_GREEN;
            break;

        /* Green when link is up. The color will be reset to the one which indicates current speed. */
        case BF_LED_PORT_LINK_UP:
        /* Green at 25/10/1G, 22 Dec, 2023. */
        case BF_LED_PORT_LINKUP_1G_10G:
        case BF_LED_PORT_LINKUP_25G:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;

        case BF_LED_PORT_LINKUP_40G:
            /* Blue at 50G */
            led_col = BF_MAV_PORT_LED_BLUE;
            break;

        case BF_LED_PORT_LINKUP_50G:
            /* Purple at 50G */
            led_col = BF_MAV_PORT_LED_BLUE |
                      BF_MAV_PORT_LED_RED;
            break;

        case BF_LED_PORT_LINKUP_100G:
            /* White at 100G */
            led_col = BF_MAV_PORT_LED_BLUE |
                      BF_MAV_PORT_LED_RED |
                      BF_MAV_PORT_LED_GREEN;
            break;

        case BF_LED_PORT_LINKUP_200G:
            /* Light Green at 200G */
            led_col = BF_MAV_PORT_LED_BLUE |
                      BF_MAV_PORT_LED_GREEN;
            break;

        case BF_LED_PORT_LINKUP_400G:
            /* Red at 400G */
            led_col = BF_MAV_PORT_LED_RED;
            break;

        /* Test only */
        case BF_LED_PORT_RED:
            led_col = BF_MAV_PORT_LED_RED;
            break;
        case BF_LED_PORT_GREEN:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;
        case BF_LED_PORT_BLUE:
            led_col = BF_MAV_PORT_LED_BLUE;
            break;

        default:
            led_col = BF_MAV_PORT_LED_OFF;
            break;
    }

    *led_color |= led_col;
}

static int
bf_pltfm_port_led_by_tofino_sync_set_x732q (
    int chip_id, struct led_ctx_t *p, uint8_t val
)
{
    // define
    int err = BF_PLTFM_SUCCESS;
    uint8_t val0;

    // read cached led color, if same as the value in cpld, return as early as possible.
    if (p->val == val) {
        return 0;
    }

    // read
    err = bf_pltfm_tf_cpld_read (p->idx,
                AF_LED_CPLD_I2C_ADDR, p->off, &val0);
    if (err) {
        LOG_WARNING ("1 tf cpld read error<%d>\n",
                 err);
        return -1;
    }
    // set val
    if (p->off_b) {
        val0 = (0x0F & val0) | (val << 4);
    } else {
        val0 = (0xF0 & val0) | val;
    }
    // write
    err = bf_pltfm_tf_cpld_write (p->idx,
                AF_LED_CPLD_I2C_ADDR, p->off, val0);
    if (err) {
        LOG_WARNING ("2 tf cpld write error<%d>\n",
                 err);
        return -2;
    }
    // record
    p->val = val;
    bf_sys_usleep (500);

    // log
    if (async_led_debug_on) {
        fprintf (stdout,
                 "%25s   : %2d/%2d : %2x : %2x : %2x\n",
                 "led to cpld", p->conn_id,
                 p->chnl_id,
                 val0, val, p->val);
    }
    return err;
}

void bf_pltfm_mav_led_init_x732q (struct led_ctx_t **led_ctx,
    int *led_siz,
    led_sync_fun_ptr *led_syn,
    led_convert_fun_ptr *led_con) {
    *led_ctx = &led_ctx_x732q[0];
    *led_siz = ARRAY_LENGTH (led_ctx_x732q);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_x732q;
    *led_con = led_cond_convert_to_color_x732q;
}
