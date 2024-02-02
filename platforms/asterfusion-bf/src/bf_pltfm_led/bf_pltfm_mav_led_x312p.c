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

/* Offset of light indicator. */
#define lightoff(val)   ((uint8_t)((val) & 0x00FF))
/* Offset of blink indicator. */
#define blinkoff(val)   ((uint8_t)(((val) >> 8) & 0x00FF))

static struct led_ctx_t led_ctx_x312p[] = {
    {"C1",  1, 0, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 0, 0x00},              //C1
    {"C1",  1, 1, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 1, 0x00},              //C1
    {"C1",  1, 2, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 2, 0x00},              //C1
    {"C1",  1, 3, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 3, 0x00},              //C1

    {"C2",  2, 0, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 4, 0x00},              //C2
    {"C2",  2, 1, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 5, 0x00},              //C2
    {"C2",  2, 2, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 6, 0x00},              //C2
    {"C2",  2, 3, BF_MAV_SYSCPLD1, (0x71 << 8) | 0x90, 7, 0x00},              //C2

    {"C3",  3, 0, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 0, 0x00},              //C3
    {"C3",  3, 1, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 1, 0x00},              //C3
    {"C3",  3, 2, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 2, 0x00},              //C3
    {"C3",  3, 3, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 3, 0x00},              //C3

    {"C4",  4, 0, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 4, 0x00},              //C4
    {"C4",  4, 1, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 5, 0x00},              //C4
    {"C4",  4, 2, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 6, 0x00},              //C4
    {"C4",  4, 3, BF_MAV_SYSCPLD1, (0x81 << 8) | 0xA0, 7, 0x00},              //C4

    {"C5",  5, 0, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 0, 0x00},              //C5
    {"C5",  5, 1, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 1, 0x00},              //C5
    {"C5",  5, 2, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 2, 0x00},              //C5
    {"C5",  5, 3, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 3, 0x00},              //C5

    {"C6",  6, 0, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 4, 0x00},              //C6
    {"C6",  6, 1, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 5, 0x00},              //C6
    {"C6",  6, 2, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 6, 0x00},              //C6
    {"C6",  6, 3, BF_MAV_SYSCPLD1, (0x91 << 8) | 0xB0, 7, 0x00},              //C6

    {"C7",  7, 0, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 0, 0x00},              //C7
    {"C7",  7, 1, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 1, 0x00},              //C7
    {"C7",  7, 2, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 2, 0x00},              //C7
    {"C7",  7, 3, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 3, 0x00},              //C7

    {"C8",  8, 0, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 4, 0x00},              //C8
    {"C8",  8, 1, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 5, 0x00},              //C8
    {"C8",  8, 2, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 6, 0x00},              //C8
    {"C8",  8, 3, BF_MAV_SYSCPLD1, (0xA1 << 8) | 0xC0, 7, 0x00},              //C8

    {"C9",  9, 0, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 0, 0x00},              //C9
    {"C9",  9, 1, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 1, 0x00},              //C9
    {"C9",  9, 2, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 2, 0x00},              //C9
    {"C9",  9, 3, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 3, 0x00},              //C9

    {"C10", 10, 0, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 4, 0x00},             //C10
    {"C10", 10, 1, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 5, 0x00},             //C10
    {"C10", 10, 2, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 6, 0x00},             //C10
    {"C10", 10, 3, BF_MAV_SYSCPLD1, (0xB1 << 8) | 0xD0, 7, 0x00},             //C10

    {"C11", 11, 0, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 0, 0x00},             //C11
    {"C11", 11, 1, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 1, 0x00},             //C11
    {"C11", 11, 2, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 2, 0x00},             //C11
    {"C11", 11, 3, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 3, 0x00},             //C11

    {"C12", 12, 0, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 4, 0x00},             //C12
    {"C12", 12, 1, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 5, 0x00},             //C12
    {"C12", 12, 2, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 6, 0x00},             //C12
    {"C12", 12, 3, BF_MAV_SYSCPLD1, (0xC1 << 8) | 0xE0, 7, 0x00},             //C12

    { "Y1", 13, 0, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 0, 0x00},             //Y01
    { "Y2", 13, 1, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 1, 0x00},             //Y02
    { "Y3", 15, 0, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 2, 0x00},             //Y03
    { "Y4", 13, 2, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 3, 0x00},             //Y04
    { "Y5", 13, 3, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 4, 0x00},             //Y05
    { "Y6", 15, 1, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 5, 0x00},             //Y06
    { "Y7", 14, 0, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 6, 0x00},             //Y07
    { "Y8", 14, 1, BF_MAV_SYSCPLD1, (0xF0 << 8) | 0x20, 7, 0x00},             //Y08
    { "Y9", 15, 2, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 0, 0x00},             //Y09
    {"Y10", 14, 2, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 1, 0x00},             //Y10
    {"Y11", 14, 3, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 2, 0x00},             //Y11
    {"Y12", 15, 3, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 3, 0x00},             //Y12
    {"Y13", 16, 0, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 4, 0x00},             //Y13
    {"Y14", 16, 1, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 5, 0x00},             //Y14
    {"Y15", 18, 0, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 6, 0x00},             //Y15
    {"Y16", 16, 2, BF_MAV_SYSCPLD1, (0x11 << 8) | 0x30, 7, 0x00},             //Y16
    {"Y17", 16, 3, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 0, 0x00},             //Y17
    {"Y18", 18, 1, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 1, 0x00},             //Y18
    {"Y19", 17, 0, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 2, 0x00},             //Y19
    {"Y20", 17, 1, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 3, 0x00},             //Y20
    {"Y21", 18, 2, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 4, 0x00},             //Y21
    {"Y22", 17, 2, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 5, 0x00},             //Y22
    {"Y23", 17, 3, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 6, 0x00},             //Y23
    {"Y24", 18, 3, BF_MAV_SYSCPLD1, (0x21 << 8) | 0x40, 7, 0x00},             //Y24
    {"Y25", 19, 0, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 0, 0x00},             //Y25
    {"Y26", 19, 1, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 1, 0x00},             //Y26
    {"Y27", 21, 0, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 2, 0x00},             //Y27
    {"Y28", 19, 2, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 3, 0x00},             //Y28
    {"Y29", 19, 3, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 4, 0x00},             //Y29
    {"Y30", 21, 1, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 5, 0x00},             //Y30
    {"Y31", 20, 0, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 6, 0x00},             //Y31
    {"Y32", 20, 1, BF_MAV_SYSCPLD1, (0x31 << 8) | 0x50, 7, 0x00},             //Y32
    {"Y33", 21, 2, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 0, 0x00},             //Y33
    {"Y34", 20, 2, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 1, 0x00},             //Y34
    {"Y35", 20, 3, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 2, 0x00},             //Y35
    {"Y36", 21, 3, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 3, 0x00},             //Y36
    {"Y37", 22, 0, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 4, 0x00},             //Y37
    {"Y38", 22, 1, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 5, 0x00},             //Y38
    {"Y39", 24, 0, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 6, 0x00},             //Y39
    {"Y40", 22, 2, BF_MAV_SYSCPLD1, (0x41 << 8) | 0x60, 7, 0x00},             //Y40
    {"Y41", 22, 3, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 0, 0x00},             //Y41
    {"Y42", 24, 1, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 1, 0x00},             //Y42
    {"Y43", 23, 0, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 2, 0x00},             //Y43
    {"Y44", 23, 1, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 3, 0x00},             //Y44
    {"Y45", 24, 2, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 4, 0x00},             //Y45
    {"Y46", 23, 2, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 5, 0x00},             //Y46
    {"Y47", 23, 3, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 6, 0x00},             //Y47
    {"Y48", 24, 3, BF_MAV_SYSCPLD1, (0x51 << 8) | 0x70, 7, 0x00},             //Y48
    {"Y49", 33, 0, BF_MAV_SYSCPLD1, (0x61 << 8) | 0x80, 1, 0x00},             //Y49
    {"Y50", 33, 1, BF_MAV_SYSCPLD1, (0x61 << 8) | 0x80, 0, 0x00},             //Y50
};

static struct led_init_t led_init_x312p[] = {
    {BF_MAV_SYSCPLD1, 0x20, 0xff},
    {BF_MAV_SYSCPLD1, 0x30, 0xff},
    {BF_MAV_SYSCPLD1, 0x40, 0xff},
    {BF_MAV_SYSCPLD1, 0x50, 0xff},
    {BF_MAV_SYSCPLD1, 0x60, 0xff},
    {BF_MAV_SYSCPLD1, 0x70, 0xff},
    {BF_MAV_SYSCPLD1, 0x80, 0xff},
    {BF_MAV_SYSCPLD1, 0x90, 0xff},
    {BF_MAV_SYSCPLD1, 0xA0, 0xff},
    {BF_MAV_SYSCPLD1, 0xB0, 0xff},
    {BF_MAV_SYSCPLD1, 0xC0, 0xff},
    {BF_MAV_SYSCPLD1, 0xD0, 0xff},
    {BF_MAV_SYSCPLD1, 0xE0, 0xff},
    {BF_MAV_SYSCPLD1, 0xF0, 0xff},
    {BF_MAV_SYSCPLD1, 0x11, 0xff},
    {BF_MAV_SYSCPLD1, 0x21, 0xff},
    {BF_MAV_SYSCPLD1, 0x31, 0xff},
    {BF_MAV_SYSCPLD1, 0x41, 0xff},
    {BF_MAV_SYSCPLD1, 0x51, 0xff},
    {BF_MAV_SYSCPLD1, 0x61, 0xff},
    {BF_MAV_SYSCPLD1, 0x71, 0xff},
    {BF_MAV_SYSCPLD1, 0x81, 0xff},
    {BF_MAV_SYSCPLD1, 0x91, 0xff},
    {BF_MAV_SYSCPLD1, 0xA1, 0xff},
    {BF_MAV_SYSCPLD1, 0xB1, 0xff},
    {BF_MAV_SYSCPLD1, 0xC1, 0xff},
};

/* Write LED CPLD by master I2C. */
static bf_pltfm_status_t bf_pltfm_led_cpld_write (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t val)
{
    i2c_addr = i2c_addr;
    return bf_pltfm_cpld_write_byte (
                  idx, offset, val);
}

/* Read LED CPLD by master I2C. */
static bf_pltfm_status_t bf_pltfm_led_cpld_read (
    uint8_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t *rd_buf)
{
    i2c_addr = i2c_addr;
    return bf_pltfm_cpld_read_byte (
                  idx, offset, rd_buf);
}

static void led_cond_convert_to_color_x312p (bf_led_condition_t led_cond,
    uint8_t *led_color)
{
    uint8_t led_col = BF_MAV_PORT_LED_OFF;
    switch (led_cond) {
        case BF_LED_POST_PORT_DEL:
        case BF_LED_POST_PORT_DIS:
        case BF_LED_PRE_PORT_EN:
        case BF_LED_PORT_LINK_DOWN:
            break;
        case BF_LED_PORT_LINK_UP:
        case BF_LED_PORT_LINKUP_1G_10G:
        case BF_LED_PORT_LINKUP_25G:
        case BF_LED_PORT_LINKUP_40G:
        case BF_LED_PORT_LINKUP_50G:
        case BF_LED_PORT_LINKUP_100G:
            led_col = BF_MAV_PORT_LED_GREEN;
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
bf_pltfm_port_led_by_tofino_sync_set_x312p (
    int chip_id, struct led_ctx_t *p, uint8_t val)
{
    // define
    int err = BF_PLTFM_SUCCESS;
    bool need_light = (val & (~BF_MAV_PORT_LED_BLINK))
                      ? true : false;
    bool need_blink = (val & BF_MAV_PORT_LED_BLINK) ?
                      true : false;
    bool cached_light = (p->val &
                         (~BF_MAV_PORT_LED_BLINK)) ? true : false;
    bool cached_blink = (p->val &
                         BF_MAV_PORT_LED_BLINK) ? true : false;
    uint8_t val0, val1;

    // read cached led color, if same as the value in cpld, return as early as possible.
    if ((need_blink == cached_blink) &&
        (need_light == cached_light)) {
        return 0;
    }

    //fprintf(stdout, "%d/%d(Y%d) val is 0x%x, %s %s\n", p->conn_id, p->chnl_id, module, val, need_light?"need_light":"", need_blink?"need_blink":"");

    if (need_light != cached_light) {
        // read light
        err = bf_pltfm_led_cpld_read (
                  p->idx, INVALID, lightoff(p->off), &val0);
        if (err) {
            LOG_WARNING ("%d/%d read light error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -1;
        }

        // change val
        val1 = val0;
        if (need_light) {
            val1 &= ~ (1 << p->off_b);
        } else {
            val1 |= (1 << p->off_b);
        }

        // write light
        err = bf_pltfm_led_cpld_write (
                  p->idx, INVALID, lightoff(p->off), val1);
        if (err) {
            LOG_WARNING ("%d/%d write light error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -2;
        }
        bf_sys_usleep (500);
    }

    if (need_blink != cached_blink) {
        // read blink
        err = bf_pltfm_led_cpld_read (
                  p->idx, INVALID, blinkoff(p->off), &val0);
        if (err) {
            LOG_WARNING ("%d/%d read blink error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -3;
        }

        // change val
        val1 = val0;
        if (need_blink) {
            val1 &= ~ (1 << p->off_b);
        } else {
            val1 |= (1 << p->off_b);
        }

        // write blink
        err = bf_pltfm_led_cpld_write (
                  p->idx, INVALID, blinkoff(p->off), val1);
        if (err) {
            LOG_WARNING ("%d/%d write blink error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -4;
        }
        bf_sys_usleep (500);
    }

    // record
    p->val = val;

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

void bf_pltfm_mav_led_init_x312p (struct led_ctx_t **led_ctx,
    int *led_siz,
    led_sync_fun_ptr *led_syn,
    led_convert_fun_ptr *led_con) {
    *led_ctx = &led_ctx_x312p[0];
    *led_siz = ARRAY_LENGTH (led_ctx_x312p);
    *led_syn = bf_pltfm_port_led_by_tofino_sync_set_x312p;
    *led_con = led_cond_convert_to_color_x312p;

    /* Let all LED be off quickly. */
    foreach_element (0, ARRAY_LENGTH (led_init_x312p)) {
        bf_pltfm_cpld_write_byte(led_init_x312p[each_element].idx,
                                 led_init_x312p[each_element].off,
                                 led_init_x312p[each_element].val);
    }
}
