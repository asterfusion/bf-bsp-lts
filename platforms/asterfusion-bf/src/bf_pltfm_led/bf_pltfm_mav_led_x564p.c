/*!
 * @file bf_pltfm_mav_led.c
 * @date 2022/06/10
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

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

#include <pltfm_types.h>

static struct led_ctx_t led_ctx_x564p[] = {
    {"C1",   1, UINT8_MAX, PIN01, 0x48, 0, 0xFF},
    {"C2",   2, UINT8_MAX, PIN01, 0x48, 4, 0xFF},
    {"C3",   3, UINT8_MAX, PIN01, 0x49, 0, 0xFF},
    {"C4",   4, UINT8_MAX, PIN01, 0x49, 4, 0xFF},

    {"C5",   5, UINT8_MAX, PIN01, 0x4C, 0, 0xFF},
    {"C6",   6, UINT8_MAX, PIN01, 0x4C, 4, 0xFF},
    {"C7",   7, UINT8_MAX, PIN01, 0x4D, 0, 0xFF},
    {"C8",   8, UINT8_MAX, PIN01, 0x4D, 4, 0xFF},

    {"C9",   9, UINT8_MAX, PIN01, 0x50, 0, 0xFF},
    {"C10", 10, UINT8_MAX, PIN01, 0x50, 4, 0xFF},
    {"C11", 11, UINT8_MAX, PIN01, 0x51, 0, 0xFF},
    {"C12", 12, UINT8_MAX, PIN01, 0x51, 4, 0xFF},

    {"C13", 13, UINT8_MAX, PIN01, 0x54, 0, 0xFF},
    {"C14", 14, UINT8_MAX, PIN01, 0x54, 4, 0xFF},
    {"C15", 15, UINT8_MAX, PIN01, 0x55, 0, 0xFF},
    {"C16", 16, UINT8_MAX, PIN01, 0x55, 4, 0xFF},

    {"C17", 17, UINT8_MAX, PIN45, 0x48, 0, 0xFF},
    {"C18", 18, UINT8_MAX, PIN45, 0x48, 4, 0xFF},
    {"C19", 19, UINT8_MAX, PIN45, 0x49, 0, 0xFF},
    {"C20", 20, UINT8_MAX, PIN45, 0x49, 4, 0xFF},

    {"C21", 21, UINT8_MAX, PIN45, 0x4C, 0, 0xFF},
    {"C22", 22, UINT8_MAX, PIN45, 0x4C, 4, 0xFF},
    {"C23", 23, UINT8_MAX, PIN45, 0x4D, 0, 0xFF},
    {"C24", 24, UINT8_MAX, PIN45, 0x4D, 4, 0xFF},

    {"C25", 25, UINT8_MAX, PIN45, 0x50, 0, 0xFF},
    {"C26", 26, UINT8_MAX, PIN45, 0x50, 4, 0xFF},
    {"C27", 27, UINT8_MAX, PIN45, 0x51, 0, 0xFF},
    {"C28", 28, UINT8_MAX, PIN45, 0x51, 4, 0xFF},

    {"C29", 29, UINT8_MAX, PIN45, 0x54, 0, 0xFF},
    {"C30", 30, UINT8_MAX, PIN45, 0x54, 4, 0xFF},
    {"C31", 31, UINT8_MAX, PIN45, 0x55, 0, 0xFF},
    {"C32", 32, UINT8_MAX, PIN45, 0x55, 4, 0xFF},

    {"C33", 33, UINT8_MAX, PIN01, 0x4B, 4, 0xFF},
    {"C34", 34, UINT8_MAX, PIN01, 0x4B, 0, 0xFF},
    {"C35", 35, UINT8_MAX, PIN01, 0x4A, 4, 0xFF},
    {"C36", 36, UINT8_MAX, PIN01, 0x4A, 0, 0xFF},

    {"C37", 37, UINT8_MAX, PIN01, 0x4F, 4, 0xFF},
    {"C38", 38, UINT8_MAX, PIN01, 0x4F, 0, 0xFF},
    {"C39", 39, UINT8_MAX, PIN01, 0x4E, 4, 0xFF},
    {"C40", 40, UINT8_MAX, PIN01, 0x4E, 0, 0xFF},

    {"C41", 41, UINT8_MAX, PIN01, 0x53, 4, 0xFF},
    {"C42", 42, UINT8_MAX, PIN01, 0x53, 0, 0xFF},
    {"C43", 43, UINT8_MAX, PIN01, 0x52, 4, 0xFF},
    {"C44", 44, UINT8_MAX, PIN01, 0x52, 0, 0xFF},

    {"C45", 45, UINT8_MAX, PIN01, 0x57, 4, 0xFF},
    {"C46", 46, UINT8_MAX, PIN01, 0x57, 0, 0xFF},
    {"C47", 47, UINT8_MAX, PIN01, 0x56, 4, 0xFF},
    {"C48", 48, UINT8_MAX, PIN01, 0x56, 0, 0xFF},

    {"C49", 49, UINT8_MAX, PIN45, 0x4B, 4, 0xFF},
    {"C50", 50, UINT8_MAX, PIN45, 0x4B, 0, 0xFF},
    {"C51", 51, UINT8_MAX, PIN45, 0x4A, 4, 0xFF},
    {"C52", 52, UINT8_MAX, PIN45, 0x4A, 0, 0xFF},

    {"C53", 53, UINT8_MAX, PIN45, 0x4F, 4, 0xFF},
    {"C54", 54, UINT8_MAX, PIN45, 0x4F, 0, 0xFF},
    {"C55", 55, UINT8_MAX, PIN45, 0x4E, 4, 0xFF},
    {"C56", 56, UINT8_MAX, PIN45, 0x4E, 0, 0xFF},

    {"C57", 57, UINT8_MAX, PIN45, 0x57, 4, 0xFF},
    {"C58", 58, UINT8_MAX, PIN45, 0x57, 0, 0xFF},
    {"C59", 59, UINT8_MAX, PIN45, 0x56, 4, 0xFF},
    {"C60", 60, UINT8_MAX, PIN45, 0x56, 0, 0xFF},

    {"C61", 61, UINT8_MAX, PIN45, 0x53, 4, 0xFF},
    {"C62", 62, UINT8_MAX, PIN45, 0x53, 0, 0xFF},
    {"C63", 63, UINT8_MAX, PIN45, 0x52, 4, 0xFF},
    {"C64", 64, UINT8_MAX, PIN45, 0x52, 0, 0xFF},

    {"Y1",  65,         0, PIN01, 0x58, 0, 0xFF},
    {"Y2",  65,         1, PIN01, 0x58, 4, 0xFF},
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

static void led_cond_convert_to_color_x564p (bf_led_condition_t led_cond,
    uint8_t *led_color)
{
    uint8_t led_col = BF_MAV_PORT_LED_OFF;
    switch (led_cond) {
        case BF_LED_POST_PORT_DEL:
        case BF_LED_POST_PORT_DIS:
            break;

        case BF_LED_PRE_PORT_EN:
        case BF_LED_PORT_LINK_DOWN:
            /* Yellow when port is enabled but link is no up. */
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
bf_pltfm_port_led_by_tofino_sync_set_x564p (
    int chip_id, struct led_ctx_t *p, uint8_t val)
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

void bf_pltfm_mav_led_init_x564p (struct led_ctx_t **led_ctx,
    int *led_siz,
    led_sync_fun_ptr *led_syn,
    led_convert_fun_ptr *led_con) {
    *led_ctx = &led_ctx_x564p[0];
    *led_siz = ARRAY_LENGTH (led_ctx_x564p);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_x564p;
    *led_con = led_cond_convert_to_color_x564p;
}
