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

static struct led_ctx_t led_ctx_x308p[] = {
    { "C1",  1, UINT8_MAX, PIN45, 0x48, NOT_USED, 0xff},               //C1
    { "C2",  2, UINT8_MAX, PIN45, 0x4A, NOT_USED, 0xff},               //C2
    { "C3",  3, UINT8_MAX, PIN45, 0x4C, NOT_USED, 0xff},               //C3
    { "C4",  4, UINT8_MAX, PIN45, 0x4E, NOT_USED, 0xff},               //C4
    { "C5",  5, UINT8_MAX, PIN45, 0x50, NOT_USED, 0xff},               //C5
    { "C6",  6, UINT8_MAX, PIN45, 0x52, NOT_USED, 0xff},               //C6
    { "C7",  7, UINT8_MAX, PIN45, 0x54, NOT_USED, 0xff},               //C7
    { "C8",  8, UINT8_MAX, PIN45, 0x56, NOT_USED, 0xff},               //C8

    { "Y1",  9, 0, PIN01, 0x48, 0, 0xff},               //Y01
    { "Y2",  9, 1, PIN01, 0x48, 2, 0xff},               //Y02
    { "Y3",  9, 2, PIN01, 0x48, 4, 0xff},               //Y03
    { "Y4",  9, 3, PIN01, 0x48, 6, 0xff},               //Y04
    { "Y5", 10, 0, PIN01, 0x49, 0, 0xff},               //Y05
    { "Y6", 10, 1, PIN01, 0x49, 2, 0xff},               //Y06
    { "Y7", 10, 2, PIN01, 0x49, 6, 0xff},               //Y07
    { "Y8", 10, 3, PIN01, 0x49, 4, 0xff},               //Y08
    { "Y9", 11, 0, PIN01, 0x4A, 0, 0xff},               //Y09
    {"Y10", 11, 1, PIN01, 0x4A, 2, 0xff},               //Y10
    {"Y11", 11, 2, PIN01, 0x4A, 4, 0xff},               //Y11
    {"Y12", 11, 3, PIN01, 0x4A, 6, 0xff},               //Y12
    {"Y13", 12, 0, PIN01, 0x4B, 4, 0xff},               //Y13
    {"Y14", 12, 1, PIN01, 0x4B, 2, 0xff},               //Y14
    {"Y15", 12, 2, PIN01, 0x4B, 6, 0xff},               //Y15
    {"Y16", 12, 3, PIN01, 0x4B, 0, 0xff},               //Y16
    {"Y17", 13, 0, PIN01, 0x4C, 0, 0xff},               //Y17
    {"Y18", 13, 1, PIN01, 0x4C, 2, 0xff},               //Y18
    {"Y19", 13, 2, PIN01, 0x4C, 4, 0xff},               //Y19
    {"Y20", 13, 3, PIN01, 0x4C, 6, 0xff},               //Y20
    {"Y21", 14, 0, PIN01, 0x4D, 0, 0xff},               //Y21
    {"Y22", 14, 1, PIN01, 0x4D, 2, 0xff},               //Y22
    {"Y23", 14, 2, PIN01, 0x4D, 6, 0xff},               //Y23
    {"Y24", 14, 3, PIN01, 0x4D, 4, 0xff},               //Y24

    {"Y25", 15, 0, PIN01, 0x4E, 0, 0xff},               //Y25
    {"Y26", 15, 1, PIN01, 0x4E, 2, 0xff},               //Y26
    {"Y27", 15, 2, PIN01, 0x4E, 4, 0xff},               //Y27
    {"Y28", 15, 3, PIN01, 0x4E, 6, 0xff},               //Y28
    {"Y29", 16, 0, PIN01, 0x4F, 4, 0xff},               //Y29
    {"Y30", 16, 1, PIN01, 0x4F, 2, 0xff},               //Y30
    {"Y31", 16, 2, PIN01, 0x4F, 6, 0xff},               //Y31
    {"Y32", 16, 3, PIN01, 0x4F, 0, 0xff},               //Y32
    {"Y33", 17, 0, PIN01, 0x50, 0, 0xff},               //Y33
    {"Y34", 17, 1, PIN01, 0x50, 2, 0xff},               //Y34
    {"Y35", 17, 2, PIN01, 0x50, 4, 0xff},               //Y35
    {"Y36", 17, 3, PIN01, 0x50, 6, 0xff},               //Y36
    {"Y37", 18, 0, PIN01, 0x51, 0, 0xff},               //Y37
    {"Y38", 18, 1, PIN01, 0x51, 2, 0xff},               //Y38
    {"Y39", 18, 2, PIN01, 0x51, 6, 0xff},               //Y39
    {"Y40", 18, 3, PIN01, 0x51, 4, 0xff},               //Y40
    {"Y41", 19, 0, PIN01, 0x52, 0, 0xff},               //Y41
    {"Y42", 19, 1, PIN01, 0x52, 2, 0xff},               //Y42
    {"Y43", 19, 2, PIN01, 0x52, 4, 0xff},               //Y43
    {"Y44", 19, 3, PIN01, 0x52, 6, 0xff},               //Y44
    {"Y45", 20, 0, PIN01, 0x53, 4, 0xff},               //Y45
    {"Y46", 20, 1, PIN01, 0x53, 2, 0xff},               //Y46
    {"Y47", 20, 2, PIN01, 0x53, 6, 0xff},               //Y47
    {"Y48", 20, 3, PIN01, 0x53, 0, 0xff},               //Y48
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

static void led_cond_convert_to_color_x308p (bf_led_condition_t led_cond,
    uint8_t *led_color)
{
    uint8_t led_col = BF_MAV_PORT_LED_OFF;
    switch (led_cond) {
        case BF_LED_POST_PORT_DEL:
        case BF_LED_POST_PORT_DIS:
        case BF_LED_PRE_PORT_EN:
        case BF_LED_PORT_LINK_DOWN:
            break;

        /* Green when link is up. The color will be reset to the one which indicates current speed. */
        case BF_LED_PORT_LINK_UP:
        /* Green at 25/10/1G, 22 Dec, 2023. */
        case BF_LED_PORT_LINKUP_1G_10G:
        case BF_LED_PORT_LINKUP_25G:
            led_col = BF_MAV_PORT_LED_GREEN;
            break;

        /* Red at 40G */
        case BF_LED_PORT_LINKUP_40G:
            led_col = BF_MAV_PORT_LED_RED;
            break;

        /* Yellow at 50G/100G */
        case BF_LED_PORT_LINKUP_50G:
        case BF_LED_PORT_LINKUP_100G:
            led_col = BF_MAV_PORT_LED_RED |
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

int
bf_pltfm_port_led_by_tofino_sync_set_x308p (
    int chip_id, struct led_ctx_t *p, uint8_t val)
{
    // define
    int err = BF_PLTFM_SUCCESS;
    uint8_t val0;

    // read cached led color, if same as the value in cpld, return as early as possible.
    if (p->val == val) {
        return 0;
    }

    // translate to the data format for CPLD
    uint8_t led_active = (val & BF_MAV_PORT_LED_BLINK) ? 0x01 : 0x00;
    uint8_t led_link, led_speed;

    switch (val & (BF_MAV_PORT_LED_RED + BF_MAV_PORT_LED_GREEN + BF_MAV_PORT_LED_BLUE)) {
        case BF_MAV_PORT_LED_GREEN:
            led_link  = 0x01;
            led_speed = 0x00;           // 10G
            break;
        case (BF_MAV_PORT_LED_GREEN + BF_MAV_PORT_LED_BLUE):
            led_link  = 0x01;
            led_speed = 0x01;           // 25G
            break;
        case BF_MAV_PORT_LED_BLUE:
            led_link  = 0x01;
            led_speed = 0x02;           // 40G
            break;
        case (BF_MAV_PORT_LED_BLUE + BF_MAV_PORT_LED_RED):
            led_link  = 0x01;
            led_speed = 0x03;           // 50G
            break;
        case (BF_MAV_PORT_LED_RED + BF_MAV_PORT_LED_GREEN + BF_MAV_PORT_LED_BLUE):
            led_link  = 0x01;
            led_speed = 0x03;           // 100G
            break;
        case (BF_MAV_PORT_LED_GREEN + BF_MAV_PORT_LED_RED):
            led_link  = 0x00;
            led_speed = 0x00;           // link down
            break;
        default:
            led_link  = 0x00;
            led_speed = 0x00;           // link down
            break;
    }

    // read
    err = bf_pltfm_tf_cpld_read (p->idx,
                AF_LED_CPLD_I2C_ADDR, p->off, &val0);
    if (err) {
        LOG_WARNING ("tf cpld read error<%d>\n",
                 err);
        return -1;
    }

    if (p->desc[0] == 'C') {
        // set val
        val0 = (led_speed << 2) + (led_link << 1) + led_active;

        // write
        err  = bf_pltfm_tf_cpld_write (p->idx,
                AF_LED_CPLD_I2C_ADDR, p->off,   val0);
        err |= bf_pltfm_tf_cpld_write (p->idx,
                AF_LED_CPLD_I2C_ADDR, p->off+1, 0);
        if (err) {
            LOG_WARNING ("tf cpld write error<%d>\n",
                     err);
            return -2;
        }
    } else {
        // set val
        val0 = (val0 & ~(0x03 << p->off_b)) | (((led_link << 1) + led_active) << p->off_b);

        // write
        err = bf_pltfm_tf_cpld_write (p->idx,
               AF_LED_CPLD_I2C_ADDR, p->off, val0);
        if (err) {
            LOG_WARNING ("tf cpld write error<%d>\n",
                     err);
            return -2;
        }
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

void bf_pltfm_mav_led_init_x308p (struct led_ctx_t **led_ctx,
    int *led_siz,
    led_sync_fun_ptr *led_syn,
    led_convert_fun_ptr *led_con) {
    *led_ctx = &led_ctx_x308p[0];
    *led_siz = ARRAY_LENGTH (led_ctx_x308p);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_x308p;
    *led_con = led_cond_convert_to_color_x308p;
}
