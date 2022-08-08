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

/* Not finished yet.
 * Please help fix it.
 * by tsihang, 2021-07-18. */
struct led_ctx_t led_ctx_hc[] = {
    {"C1",  22, UINT8_MAX, PIN45, 0x4f, 4, 0xFF},   /* QSFP  22 */
    {"C2",  21, UINT8_MAX, PIN45, 0x53, 4, 0xFF},   /* QSFP  21 */
    {"C3",  24, UINT8_MAX, PIN45, 0x4f, 0, 0xFF},   /* QSFP  24 */
    {"C4",  20, UINT8_MAX, PIN45, 0x4e, 4, 0xFF},   /* QSFP  20 */
    {"C5",  19, UINT8_MAX, PIN45, 0x53, 0, 0xFF},   /* QSFP  19 */
    {"C6",  23, UINT8_MAX, PIN45, 0x4e, 0, 0xFF},   /* QSFP  23 */
    {"C7",  16, UINT8_MAX, PIN45, 0x52, 4, 0xFF},   /* QSFP  16 */
    {"C8",  15, UINT8_MAX, PIN45, 0x50, 4, 0xFF},   /* QSFP  15 */
    {"C9",  18, UINT8_MAX, PIN45, 0x52, 0, 0xFF},   /* QSFP  18 */
    {"C10", 14, UINT8_MAX, PIN45, 0x51, 4, 0xFF},   /* QSFP  14 */
    {"C11", 13, UINT8_MAX, PIN45, 0x50, 0, 0xFF},   /* QSFP  13 */
    {"C12", 17, UINT8_MAX, PIN45, 0x51, 0, 0xFF},   /* QSFP  17 */
    {"C13", 10, UINT8_MAX, PIN45, 0x4c, 4, 0xFF},   /* QSFP  10 */
    {"C14",  9, UINT8_MAX, PIN45, 0x4d, 4, 0xFF},   /* QSFP   9 */
    {"C15", 12, UINT8_MAX, PIN45, 0x4c, 0, 0xFF},   /* QSFP  12 */
    {"C16",  8, UINT8_MAX, PIN45, 0x4b, 4, 0xFF},   /* QSFP   8 */
    {"C17",  7, UINT8_MAX, PIN45, 0x4d, 0, 0xFF},   /* QSFP   7 */
    {"C18", 11, UINT8_MAX, PIN45, 0x4b, 0, 0xFF},   /* QSFP  11 */
    {"C19",  4, UINT8_MAX, PIN45, 0x49, 4, 0xFF},   /* QSFP   4 */
    {"C20",  3, UINT8_MAX, PIN45, 0x4a, 4, 0xFF},   /* QSFP   3 */
    {"C21",  6, UINT8_MAX, PIN45, 0x49, 0, 0xFF},   /* QSFP   6 */
    {"C22",  2, UINT8_MAX, PIN45, 0x48, 4, 0xFF},   /* QSFP   2 */
    {"C23",  1, UINT8_MAX, PIN45, 0x4a, 0, 0xFF},   /* QSFP   1 */
    {"C24",  5, UINT8_MAX, PIN45, 0x48, 0, 0xFF},   /* QSFP   5 */

    {"Y1",  32,         0, PIN01, 0x4c, 0, 0xFF},   /* QSFP 32 */
    {"Y2",  32,         1, PIN01, 0x50, 4, 0xFF},   /* QSFP 32 */
    {"Y3",  33,         3, PIN01, 0x5a, 4, 0xFF},   /* QSFP 33 */
    {"Y4",  32,         2, PIN01, 0x55, 0, 0xFF},   /* QSFP 32 */
    {"Y5",  32,         3, PIN01, 0x59, 4, 0xFF},   /* QSFP 32 */
    {"Y6",  33,         2, PIN01, 0x56, 0, 0xFF},   /* QSFP 33 */
    {"Y7",  31,         0, PIN01, 0x4c, 4, 0xFF},   /* QSFP 31 */
    {"Y8",  31,         1, PIN01, 0x51, 0, 0xFF},   /* QSFP 31 */
    {"Y9",  33,         1, PIN01, 0x51, 4, 0xFF},   /* QSFP 33 */
    {"Y10", 31,         2, PIN01, 0x55, 4, 0xFF},   /* QSFP 31 */
    {"Y11", 31,         3, PIN01, 0x5a, 0, 0xFF},   /* QSFP 31 */
    {"Y12", 33,         0, PIN01, 0x4d, 0, 0xFF},   /* QSFP 33 */
    {"Y13", 29,         0, PIN01, 0x4a, 4, 0xFF},   /* QSFP 29 */
    {"Y14", 29,         1, PIN01, 0x4f, 0, 0xFF},   /* QSFP 29 */
    {"Y15", 30,         3, PIN01, 0x59, 0, 0xFF},   /* QSFP 30 */
    {"Y16", 29,         2, PIN01, 0x53, 4, 0xFF},   /* QSFP 29 */
    {"Y17", 29,         3, PIN01, 0x58, 0, 0xFF},   /* QSFP 29 */
    {"Y18", 30,         2, PIN01, 0x54, 4, 0xFF},   /* QSFP 30 */
    {"Y19", 28,         0, PIN01, 0x4b, 0, 0xFF},   /* QSFP 28 */
    {"Y20", 28,         1, PIN01, 0x4f, 4, 0xFF},   /* QSFP 28 */
    {"Y21", 30,         1, PIN01, 0x50, 0, 0xFF},   /* QSFP 30 */
    {"Y22", 28,         2, PIN01, 0x54, 0, 0xFF},   /* QSFP 28 */
    {"Y23", 28,         3, PIN01, 0x58, 4, 0xFF},   /* QSFP 28 */
    {"Y24", 30,         0, PIN01, 0x4b, 4, 0xFF},   /* QSFP 30 */
    {"Y25", 26,         0, PIN01, 0x49, 0, 0xFF},   /* QSFP 26 */
    {"Y26", 26,         1, PIN01, 0x4d, 4, 0xFF},   /* QSFP 26 */
    {"Y27", 27,         3, PIN01, 0x57, 4, 0xFF},   /* QSFP 27 */
    {"Y28", 26,         2, PIN01, 0x52, 0, 0xFF},   /* QSFP 26 */
    {"Y29", 26,         3, PIN01, 0x56, 4, 0xFF},   /* QSFP 26 */
    {"Y30", 27,         2, PIN01, 0x4a, 0, 0xFF},   /* QSFP 27 */
    {"Y31", 25,         0, PIN01, 0x49, 4, 0xFF},   /* QSFP 25 */
    {"Y32", 25,         1, PIN01, 0x4e, 0, 0xFF},   /* QSFP 25 */
    {"Y33", 27,         1, PIN01, 0x4e, 4, 0xFF},   /* QSFP 27 */
    {"Y34", 25,         2, PIN01, 0x52, 4, 0xFF},   /* QSFP 25 */
    {"Y35", 25,         3, PIN01, 0x57, 0, 0xFF},   /* QSFP 25 */
    {"Y36", 27,         0, PIN01, 0x53, 0, 0xFF},   /* QSFP 27 */

    {"Y37", 65,         1, PIN01, 0x48, 4, 0xFF},    /* QSFP 65 */
    {"Y38", 65,         0, PIN01, 0x48, 0, 0xFF},    /* QSFP 65 */
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

int
bf_pltfm_port_led_by_tofino_sync_set_hc (
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

void bf_pltfm_mav_led_init_hc36y24c (struct led_ctx_t **led_ctx,
    int *led_siz, led_sync_fun_ptr *led_syn) {
    *led_ctx = &led_ctx_hc[0];
    *led_siz = ARRAY_LENGTH (led_ctx_hc);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_hc;
}

