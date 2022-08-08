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

static struct led_ctx_t led_ctx_x532p[] = {
    {"C1",   1, UINT8_MAX, PIN01, 0x53, 0, 0xFF},
    {"C2",   2, UINT8_MAX, PIN01, 0x53, 4, 0xFF},
    {"C3",   3, UINT8_MAX, PIN01, 0x52, 0, 0xFF},
    {"C4",   4, UINT8_MAX, PIN01, 0x52, 4, 0xFF},

    {"C5",   5, UINT8_MAX, PIN01, 0x51, 0, 0xFF},
    {"C6",   6, UINT8_MAX, PIN01, 0x51, 4, 0xFF},
    {"C7",   7, UINT8_MAX, PIN01, 0x50, 0, 0xFF},
    {"C8",   8, UINT8_MAX, PIN01, 0x50, 4, 0xFF},

    {"C9",   9, UINT8_MAX, PIN01, 0x4F, 0, 0xFF},
    {"C10", 10, UINT8_MAX, PIN01, 0x4F, 4, 0xFF},
    {"C11", 11, UINT8_MAX, PIN01, 0x4E, 0, 0xFF},
    {"C12", 12, UINT8_MAX, PIN01, 0x4E, 4, 0xFF},

    {"C13", 13, UINT8_MAX, PIN01, 0x4D, 0, 0xFF},
    {"C14", 14, UINT8_MAX, PIN01, 0x4D, 4, 0xFF},
    {"C15", 15, UINT8_MAX, PIN01, 0x4C, 0, 0xFF},
    {"C16", 16, UINT8_MAX, PIN01, 0x4C, 4, 0xFF},

    {"C17", 17, UINT8_MAX, PIN01, 0x4B, 0, 0xFF},
    {"C18", 18, UINT8_MAX, PIN01, 0x4B, 4, 0xFF},
    {"C19", 19, UINT8_MAX, PIN01, 0x4A, 0, 0xFF},
    {"C20", 20, UINT8_MAX, PIN01, 0x4A, 4, 0xFF},

    {"C21", 21, UINT8_MAX, PIN01, 0x49, 0, 0xFF},
    {"C22", 22, UINT8_MAX, PIN01, 0x49, 4, 0xFF},
    {"C23", 23, UINT8_MAX, PIN01, 0x48, 0, 0xFF},
    {"C24", 24, UINT8_MAX, PIN01, 0x48, 4, 0xFF},

    {"C25", 25, UINT8_MAX, PIN01, 0x57, 0, 0xFF},
    {"C26", 26, UINT8_MAX, PIN01, 0x57, 4, 0xFF},
    {"C27", 27, UINT8_MAX, PIN01, 0x56, 0, 0xFF},
    {"C28", 28, UINT8_MAX, PIN01, 0x56, 4, 0xFF},

    {"C29", 29, UINT8_MAX, PIN01, 0x55, 0, 0xFF},
    {"C30", 30, UINT8_MAX, PIN01, 0x55, 4, 0xFF},
    {"C31", 31, UINT8_MAX, PIN01, 0x54, 0, 0xFF},
    {"C32", 32, UINT8_MAX, PIN01, 0x54, 4, 0xFF},

    {"Y1",  33,         0, PIN01, 0x58, 4, 0xFF},
    {"Y2",  33,         1, PIN01, 0x58, 0, 0xFF},
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

static int
bf_pltfm_port_led_by_tofino_sync_set_x532p (
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
        fprintf (stdout, "1 tf cpld read error<%d>\n",
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
        fprintf (stdout, "2 tf cpld write error<%d>\n",
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

void bf_pltfm_mav_led_init_x532p (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn) {
    *led_ctx = &led_ctx_x532p[0];
    *led_siz = ARRAY_LENGTH (led_ctx_x532p);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_x532p;
}

