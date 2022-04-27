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
#include <bfsys/bf_sal/bf_sys_timer.h>
#include <dvm/bf_drv_intf.h>
#include <lld/lld_gpio_if.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <tofino/bf_pal/bf_pal_types.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_led/bf_led.h>
#include <bf_pltfm_bd_cfg.h>
#include <bf_pltfm_led.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <bf_pltfm_sfp.h>
#include <bf_pltfm_syscpld.h>

#include <pltfm_types.h>

//#define HAVE_ASYNC_LED
#define HAVE_TRAFFIC_LED
#define HAVE_LEDCTX_FAST_LKT

#if defined(HAVE_ASYNC_LED)
#include "lq.h"
static bool async_led_debug_on = false;
#endif

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

/* - by tsihang, 18 Oct. 2019 */
#define PINXY BF_IO_PIN_PAIR_2_3    /* Unused */
#define PIN01 BF_IO_PIN_PAIR_0_1    /* PIN_PAIR 0 1 */
#define PIN45 BF_IO_PIN_PAIR_4_5    /* PIN_PAIR 4 5 */

/* - by tsihang, 3 Nov. 2019 */
#define AF_LED_CPLD_I2C_ADDR    0x40
#define AF_SWAP_BYTE(buffer) \
  ((((buffer) >> 8) & 0xff) | (((buffer)&0xff) << 8))

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

/* LED colors can be bit-wise ORed */
#define BF_MAV_PORT_LED_OFF     0x0
#define BF_MAV_PORT_LED_RED     0x4
#define BF_MAV_PORT_LED_GREEN   0x2
#define BF_MAV_PORT_LED_BLUE    0x1
#define BF_MAV_PORT_LED_BLINK   0x8

static volatile int bf_pltfm_led_initialized = 0;

static bf_sys_rmutex_t led_lock;
#define LED_LOCK   \
    bf_sys_rmutex_lock(&led_lock)
#define LED_UNLOCK \
    bf_sys_rmutex_unlock(&led_lock)

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

struct led_ctx_t {
    const char *desc;
    /* Switch port */
    uint32_t         conn_id;
    uint32_t         chnl_id;
    bf_io_pin_pair_t idx;
    uint16_t cpld_offset;
    uint8_t bit_offset;
    uint8_t val;    /* cached led value. */
};
#define NOT_USED            0

static struct led_ctx_t *
connID_chnlID_to_led_ctx_index[BF_PLAT_MAX_QSFP +
                               1][MAX_CHAN_PER_CONNECTOR]
= {{INVALID}};

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

/* Not finished yet.
 * Please help fix it.
 * by tsihang, 2021-07-18. */
static struct led_ctx_t led_ctx_hc[] = {
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

static struct led_ctx_t led_ctx_x312p[] = {
    {"C1", 30, 0, PIN01, (0x71 << 8) | 0x90, 0, 0xff},              //C1
    {"C1", 30, 1, PIN01, (0x71 << 8) | 0x90, 1, 0xff},              //C1
    {"C1", 30, 2, PIN01, (0x71 << 8) | 0x90, 2, 0xff},              //C1
    {"C1", 30, 3, PIN01, (0x71 << 8) | 0x90, 3, 0xff},              //C1

    {"C2", 32, 0, PIN01, (0x71 << 8) | 0x90, 4, 0xff},              //C2
    {"C2", 32, 1, PIN01, (0x71 << 8) | 0x90, 5, 0xff},              //C2
    {"C2", 32, 2, PIN01, (0x71 << 8) | 0x90, 6, 0xff},              //C2
    {"C2", 32, 3, PIN01, (0x71 << 8) | 0x90, 7, 0xff},              //C2

    {"C3",  6, 0, PIN01, (0x81 << 8) | 0xA0, 0, 0xff},              //C3
    {"C3",  6, 1, PIN01, (0x81 << 8) | 0xA0, 1, 0xff},              //C3
    {"C3",  6, 2, PIN01, (0x81 << 8) | 0xA0, 2, 0xff},              //C3
    {"C3",  6, 3, PIN01, (0x81 << 8) | 0xA0, 3, 0xff},              //C3

    {"C4",  2, 0, PIN01, (0x81 << 8) | 0xA0, 4, 0xff},              //C4
    {"C4",  2, 1, PIN01, (0x81 << 8) | 0xA0, 5, 0xff},              //C4
    {"C4",  2, 2, PIN01, (0x81 << 8) | 0xA0, 6, 0xff},              //C4
    {"C4",  2, 3, PIN01, (0x81 << 8) | 0xA0, 7, 0xff},              //C4

    {"C5",  4, 0, PIN01, (0x91 << 8) | 0xB0, 0, 0xff},              //C5
    {"C5",  4, 1, PIN01, (0x91 << 8) | 0xB0, 1, 0xff},              //C5
    {"C5",  4, 2, PIN01, (0x91 << 8) | 0xB0, 2, 0xff},              //C5
    {"C5",  4, 3, PIN01, (0x91 << 8) | 0xB0, 3, 0xff},              //C5

    {"C6",  5, 0, PIN01, (0x91 << 8) | 0xB0, 4, 0xff},              //C6
    {"C6",  5, 1, PIN01, (0x91 << 8) | 0xB0, 5, 0xff},              //C6
    {"C6",  5, 2, PIN01, (0x91 << 8) | 0xB0, 6, 0xff},              //C6
    {"C6",  5, 3, PIN01, (0x91 << 8) | 0xB0, 7, 0xff},              //C6

    {"C7",  3, 0, PIN01, (0xA1 << 8) | 0xC0, 0, 0xff},              //C7
    {"C7",  3, 1, PIN01, (0xA1 << 8) | 0xC0, 1, 0xff},              //C7
    {"C7",  3, 2, PIN01, (0xA1 << 8) | 0xC0, 2, 0xff},              //C7
    {"C7",  3, 3, PIN01, (0xA1 << 8) | 0xC0, 3, 0xff},              //C7

    {"C8",  1, 0, PIN01, (0xA1 << 8) | 0xC0, 4, 0xff},              //C8
    {"C8",  1, 1, PIN01, (0xA1 << 8) | 0xC0, 5, 0xff},              //C8
    {"C8",  1, 2, PIN01, (0xA1 << 8) | 0xC0, 6, 0xff},              //C8
    {"C8",  1, 3, PIN01, (0xA1 << 8) | 0xC0, 7, 0xff},              //C8

    {"C9", 31, 0, PIN01, (0xB1 << 8) | 0xD0, 0, 0xff},              //C9
    {"C9", 31, 1, PIN01, (0xB1 << 8) | 0xD0, 1, 0xff},              //C9
    {"C9", 31, 2, PIN01, (0xB1 << 8) | 0xD0, 2, 0xff},              //C9
    {"C9", 31, 3, PIN01, (0xB1 << 8) | 0xD0, 3, 0xff},              //C9

    {"C10", 29, 0, PIN01, (0xB1 << 8) | 0xD0, 4, 0xff},             //C10
    {"C10", 29, 1, PIN01, (0xB1 << 8) | 0xD0, 5, 0xff},             //C10
    {"C10", 29, 2, PIN01, (0xB1 << 8) | 0xD0, 6, 0xff},             //C10
    {"C10", 29, 3, PIN01, (0xB1 << 8) | 0xD0, 7, 0xff},             //C10

    {"C11", 27, 0, PIN01, (0xC1 << 8) | 0xE0, 0, 0xff},             //C11
    {"C11", 27, 1, PIN01, (0xC1 << 8) | 0xE0, 1, 0xff},             //C11
    {"C11", 27, 2, PIN01, (0xC1 << 8) | 0xE0, 2, 0xff},             //C11
    {"C11", 27, 3, PIN01, (0xC1 << 8) | 0xE0, 3, 0xff},             //C11

    {"C12", 28, 0, PIN01, (0xC1 << 8) | 0xE0, 4, 0xff},             //C12
    {"C12", 28, 1, PIN01, (0xC1 << 8) | 0xE0, 5, 0xff},             //C12
    {"C12", 28, 2, PIN01, (0xC1 << 8) | 0xE0, 6, 0xff},             //C12
    {"C12", 28, 3, PIN01, (0xC1 << 8) | 0xE0, 7, 0xff},             //C12

    { "Y1", 21, 3, PIN01, (0xF0 << 8) | 0x20, 0, 0xff},             //Y01
    { "Y2", 21, 2, PIN01, (0xF0 << 8) | 0x20, 1, 0xff},             //Y02
    { "Y3", 20, 3, PIN01, (0xF0 << 8) | 0x20, 2, 0xff},             //Y03
    { "Y4", 21, 1, PIN01, (0xF0 << 8) | 0x20, 3, 0xff},             //Y04
    { "Y5", 21, 0, PIN01, (0xF0 << 8) | 0x20, 4, 0xff},             //Y05
    { "Y6", 20, 2, PIN01, (0xF0 << 8) | 0x20, 5, 0xff},             //Y06
    { "Y7", 19, 3, PIN01, (0xF0 << 8) | 0x20, 6, 0xff},             //Y07
    { "Y8", 19, 2, PIN01, (0xF0 << 8) | 0x20, 7, 0xff},             //Y08
    { "Y9", 20, 1, PIN01, (0x11 << 8) | 0x30, 0, 0xff},             //Y09
    {"Y10", 19, 1, PIN01, (0x11 << 8) | 0x30, 1, 0xff},             //Y10
    {"Y11", 19, 0, PIN01, (0x11 << 8) | 0x30, 2, 0xff},             //Y11
    {"Y12", 20, 0, PIN01, (0x11 << 8) | 0x30, 3, 0xff},             //Y12
    {"Y13", 17, 3, PIN01, (0x11 << 8) | 0x30, 4, 0xff},             //Y13
    {"Y14", 17, 2, PIN01, (0x11 << 8) | 0x30, 5, 0xff},             //Y14
    {"Y15", 15, 3, PIN01, (0x11 << 8) | 0x30, 6, 0xff},             //Y15
    {"Y16", 17, 1, PIN01, (0x11 << 8) | 0x30, 7, 0xff},             //Y16
    {"Y17", 17, 0, PIN01, (0x21 << 8) | 0x40, 0, 0xff},             //Y17
    {"Y18", 15, 2, PIN01, (0x21 << 8) | 0x40, 1, 0xff},             //Y18
    {"Y19", 13, 3, PIN01, (0x21 << 8) | 0x40, 2, 0xff},             //Y19
    {"Y20", 13, 2, PIN01, (0x21 << 8) | 0x40, 3, 0xff},             //Y20
    {"Y21", 15, 1, PIN01, (0x21 << 8) | 0x40, 4, 0xff},             //Y21
    {"Y22", 13, 1, PIN01, (0x21 << 8) | 0x40, 5, 0xff},             //Y22
    {"Y23", 13, 0, PIN01, (0x21 << 8) | 0x40, 6, 0xff},             //Y23
    {"Y24", 15, 0, PIN01, (0x21 << 8) | 0x40, 7, 0xff},             //Y24
    {"Y25", 11, 3, PIN01, (0x31 << 8) | 0x50, 0, 0xff},             //Y25
    {"Y26", 11, 2, PIN01, (0x31 << 8) | 0x50, 1, 0xff},             //Y26
    {"Y27", 12, 3, PIN01, (0x31 << 8) | 0x50, 2, 0xff},             //Y27
    {"Y28", 11, 1, PIN01, (0x31 << 8) | 0x50, 3, 0xff},             //Y28
    {"Y29", 11, 0, PIN01, (0x31 << 8) | 0x50, 4, 0xff},             //Y29
    {"Y30", 12, 2, PIN01, (0x31 << 8) | 0x50, 5, 0xff},             //Y30
    {"Y31", 10, 3, PIN01, (0x31 << 8) | 0x50, 6, 0xff},             //Y31
    {"Y32", 10, 2, PIN01, (0x31 << 8) | 0x50, 7, 0xff},             //Y32
    {"Y33", 12, 1, PIN01, (0x41 << 8) | 0x60, 0, 0xff},             //Y33
    {"Y34", 10, 1, PIN01, (0x41 << 8) | 0x60, 1, 0xff},             //Y34
    {"Y35", 10, 0, PIN01, (0x41 << 8) | 0x60, 2, 0xff},             //Y35
    {"Y36", 12, 0, PIN01, (0x41 << 8) | 0x60, 3, 0xff},             //Y36
    {"Y37",  8, 3, PIN01, (0x41 << 8) | 0x60, 4, 0xff},             //Y37
    {"Y38",  8, 2, PIN01, (0x41 << 8) | 0x60, 5, 0xff},             //Y38
    {"Y39",  9, 3, PIN01, (0x41 << 8) | 0x60, 6, 0xff},             //Y39
    {"Y40",  8, 1, PIN01, (0x41 << 8) | 0x60, 7, 0xff},             //Y40
    {"Y41",  8, 0, PIN01, (0x51 << 8) | 0x70, 0, 0xff},             //Y41
    {"Y42",  9, 2, PIN01, (0x51 << 8) | 0x70, 1, 0xff},             //Y42
    {"Y43",  7, 3, PIN01, (0x51 << 8) | 0x70, 2, 0xff},             //Y43
    {"Y44",  7, 2, PIN01, (0x51 << 8) | 0x70, 3, 0xff},             //Y44
    {"Y45",  9, 1, PIN01, (0x51 << 8) | 0x70, 4, 0xff},             //Y45
    {"Y46",  7, 1, PIN01, (0x51 << 8) | 0x70, 5, 0xff},             //Y46
    {"Y47",  7, 0, PIN01, (0x51 << 8) | 0x70, 6, 0xff},             //Y47
    {"Y48",  9, 0, PIN01, (0x51 << 8) | 0x70, 7, 0xff},             //Y48
    {"Y49", 33, 0, PIN01, (0x61 << 8) | 0x80, 1, 0xff},             //Y49
    {"Y50", 33, 1, PIN01, (0x61 << 8) | 0x80, 0, 0xff},             //Y50
};

static struct led_ctx_t *g_led_ctx = NULL;
static int g_max_led_ctx = 0;

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

static bf_pltfm_status_t bf_pltfm_tf_cpld_write (
    bf_io_pin_pair_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t val)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    static const bf_dev_id_t dev = 0;

    err |= bf_i2c_wr_reg_blocking (
               dev, idx, i2c_addr, offset, 1, &val, 1);

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
        err |= bf_i2c_reset (dev, idx);
        bf_sys_usleep (100000);
    }

    return err;
}

static bf_pltfm_status_t bf_pltfm_tf_cpld_read (
    bf_io_pin_pair_t idx,
    uint8_t i2c_addr,
    uint8_t offset,
    uint8_t *rd_buf)
{
    bf_pltfm_status_t err = BF_PLTFM_SUCCESS;
    static const bf_dev_id_t dev = 0;

    err |= bf_i2c_rd_reg_blocking (
               dev, idx, i2c_addr, offset, 1, rd_buf, 1);

    if (err != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error %s(%d) Tofino i2c read failed for device: 0x%x offset 0x%x.\n",
            bf_err_str (err),
            err,
            i2c_addr,
            offset);
        /* Reset i2c in case i2c is locked on error
         * could block for upto approxomately 100ms.*/
        err |= bf_i2c_reset (dev, idx);
        bf_sys_usleep (100000);
    }

    return err;
}

static int bf_pltfm_tf_i2c_init (int chip_id,
                                 bf_io_pin_pair_t idx)
{

    if (chip_id >= BF_MAX_DEV_COUNT ||
        bf_pltfm_led_initialized) {
        return -1;
    }
    /* configure the gpio_pin_pair as i2c pins */
    bf_io_set_mode_i2c (chip_id, idx);
    /* set the i2c clk to TIMEOUT_100MS Khz (clk  period = 10000 nsec) */
    bf_i2c_set_clk (chip_id, idx,
                    PERIOD_100KHZ_IN_10_NSEC);
    /* set i2c submode as REG (Different with Reference BSP) */
    bf_i2c_set_submode (chip_id, idx,
                        BF_I2C_MODE_REG);

    return 0;
}

static int
bf_pltfm_port_led_by_tofino_sync_set_x5 (
    int chip_id,
    struct led_ctx_t *p,
    uint8_t val)
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
                                 AF_LED_CPLD_I2C_ADDR, p->cpld_offset, &val0);
    if (err) {
        fprintf (stdout, "1 tf cpld read error<%d>\n",
                 err);
        return -1;
    }
    // set val
    if (p->bit_offset) {
        val0 = (0x0F & val0) | (val << 4);
    } else {
        val0 = (0xF0 & val0) | val;
    }
    // write
    err = bf_pltfm_tf_cpld_write (p->idx,
                                  AF_LED_CPLD_I2C_ADDR, p->cpld_offset, val0);
    if (err) {
        fprintf (stdout, "2 tf cpld write error<%d>\n",
                 err);
        return -2;
    }
    // record
    p->val = val;
    bf_sys_usleep (500);

#if defined(HAVE_ASYNC_LED)
    // log
    if (async_led_debug_on) {
        fprintf (stdout,
                 "%25s   : %2d/%2d : %2x : %2x : %2x\n",
                 "led to cpld", p->conn_id,
                 p->chnl_id,
                 val0, val, p->val);
    }
#endif
    return err;
}

static int
bf_pltfm_port_led_by_tofino_sync_set_x308p (
    int chip_id,
    struct led_ctx_t *p,
    uint8_t val)
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
                                 AF_LED_CPLD_I2C_ADDR, p->cpld_offset, &val0);
    if (err) {
        fprintf (stdout, "1 tf cpld read error<%d>\n",
                 err);
        return -1;
    }

    // set val
    if (p->desc[0] == 'C') {
        if (led_speed == 0x03) {
            val0 = (led_speed << 2) + (led_link << 1) + led_active;
        } else {
            val = (led_speed << 2) + (led_link << 1) + led_active;
            val0 = (val0 & ~(0x0F << p->bit_offset)) | (val << p->bit_offset);
        }
    } else {
        val = (led_link << 1) + led_active;
        val0 = (val0 & ~(0x03 << p->bit_offset)) | (val << p->bit_offset);
    }

    // write
    err = bf_pltfm_tf_cpld_write (p->idx,
                                  AF_LED_CPLD_I2C_ADDR, p->cpld_offset, val0);
    if (err) {
        fprintf (stdout, "2 tf cpld write error<%d>\n",
                 err);
        return -2;
    }

    // if speed == 50G/100G, write 0 at offset + 1
    if (led_speed == 0x03) {
        err = bf_pltfm_tf_cpld_write (p->idx,
                                  AF_LED_CPLD_I2C_ADDR, p->cpld_offset+1, 0);
        if (err) {
            fprintf (stdout, "2 tf cpld write error<%d>\n",
                 err);
            return -2; 
        }
    }

    // record
    p->val = val;
    bf_sys_usleep (500);

#if defined(HAVE_ASYNC_LED)
    // log
    if (async_led_debug_on) {
        fprintf (stdout,
                 "%25s   : %2d/%2d : %2x : %2x : %2x\n",
                 "led to cpld", p->conn_id,
                 p->chnl_id,
                 val0, val, p->val);
    }
#endif
    return err;
}

static int
bf_pltfm_port_led_by_tofino_sync_set_hc (
    int chip_id,
    struct led_ctx_t *p,
    uint8_t val)
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
                                 AF_LED_CPLD_I2C_ADDR, p->cpld_offset, &val0);
    if (err) {
        fprintf (stdout, "1 tf cpld read error<%d>\n",
                 err);
        return -1;
    }
    // set val
    if (p->bit_offset) {
        val0 = (0x0F & val0) | (val << 4);
    } else {
        val0 = (0xF0 & val0) | val;
    }
    // write
    err = bf_pltfm_tf_cpld_write (p->idx,
                                  AF_LED_CPLD_I2C_ADDR, p->cpld_offset, val0);
    if (err) {
        fprintf (stdout, "2 tf cpld write error<%d>\n",
                 err);
        return -2;
    }
    // record
    p->val = val;
    bf_sys_usleep (500);

#if defined(HAVE_ASYNC_LED)
    // log
    if (async_led_debug_on) {
        fprintf (stdout,
                 "%25s   : %2d/%2d : %2x : %2x : %2x\n",
                 "led to cpld", p->conn_id,
                 p->chnl_id,
                 val0, val, p->val);
    }
#endif
    return err;
}

static int
bf_pltfm_port_led_by_tofino_sync_set_x312p (
    int chip_id,
    struct led_ctx_t *p,
    uint8_t val)
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
    uint8_t light_offset = (uint8_t) (p->cpld_offset &
                                      0x00FF);
    uint8_t blink_offset = (uint8_t) ((p->cpld_offset >> 8) & 0x00FF);
    uint8_t val0, val1;

    // read cached led color, if same as the value in cpld, return as early as possible.
    if ((need_blink == cached_blink) &&
        (need_light == cached_light)) {
        return 0;
    }

    //fprintf(stdout, "%d/%d(Y%d) val is 0x%x, %s %s\n", p->conn_id, p->chnl_id, module, val, need_light?"need_light":"", need_blink?"need_blink":"");

    if (need_light != cached_light) {
        // read light
        err = bf_pltfm_cpld_read_byte (
                  BF_MON_SYSCPLD1_I2C_ADDR, light_offset, &val0);
        if (err) {
            fprintf (stdout, "%d/%d read light error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -1;
        }

        // chage val
        val1 = val0;
        if (need_light) {
            val1 &= ~ (1 << p->bit_offset);
        } else {
            val1 |= (1 << p->bit_offset);
        }

        // write light
        err = bf_pltfm_cpld_write_byte (
                  BF_MON_SYSCPLD1_I2C_ADDR, light_offset, val1);
        if (err) {
            fprintf (stdout, "%d/%d write light error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -1;
        }
        bf_sys_usleep (500);
    }

    if (need_blink != cached_blink) {
        // read blink
        err = bf_pltfm_cpld_read_byte (
                  BF_MON_SYSCPLD1_I2C_ADDR, blink_offset, &val0);
        if (err) {
            fprintf (stdout, "%d/%d read blink error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -1;
        }

        // chage val
        val1 = val0;
        if (need_blink) {
            val1 &= ~ (1 << p->bit_offset);
        } else {
            val1 |= (1 << p->bit_offset);
        }

        // write blink
        err = bf_pltfm_cpld_write_byte (
                  BF_MON_SYSCPLD1_I2C_ADDR, blink_offset, val1);
        if (err) {
            fprintf (stdout, "%d/%d write blink error<%d>\n",
                     p->conn_id,
                     p->chnl_id,
                     err);
            return -1;
        }
        bf_sys_usleep (500);
    }

    // record
    p->val = val;

#if defined(HAVE_ASYNC_LED)
    // log
    if (async_led_debug_on) {
        fprintf (stdout,
                 "%25s   : %2d/%2d : %2x : %2x : %2x\n",
                 "led to cpld", p->conn_id,
                 p->chnl_id,
                 val0, val, p->val);
    }
#endif

    return err;
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
    if (platform_type_equal (X564P) ||
        platform_type_equal (X532P)) {
        err = bf_pltfm_port_led_by_tofino_sync_set_x5 (
                  chip_id, p, val);
    } else if (platform_type_equal (X308P)) {
        err = bf_pltfm_port_led_by_tofino_sync_set_x308p (
                  chip_id, p, val);
    } else if (platform_type_equal (X312P)) {
        err = bf_pltfm_port_led_by_tofino_sync_set_x312p (
                  chip_id, p, val);
    } else if (platform_type_equal (HC)) {
        err = bf_pltfm_port_led_by_tofino_sync_set_hc (
                  chip_id, p, val);
    }
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
        while ((lqe = oryx_lq_dequeue (async_led_q)) != NULL)
        {
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
    uint8_t led_col = 0;

    switch (led_cond) {
        case BF_LED_POST_PORT_DIS:
            //led_col = BF_MAV_PORT_LED_RED | BF_MAV_PORT_LED_GREEN;
            led_col = 0;
            break;
        case BF_LED_PRE_PORT_EN:
            if (platform_type_equal (X312P)) {
                return 0;
            }
            led_col = BF_MAV_PORT_LED_RED |
                      BF_MAV_PORT_LED_GREEN;
            break;
        case BF_LED_PORT_LINK_DOWN:
            if (platform_type_equal (X312P)) {
                led_col = BF_MAV_PORT_LED_OFF;
                break;
            }
            led_col = BF_MAV_PORT_LED_RED |
                      BF_MAV_PORT_LED_GREEN;
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
        g_led_ctx = &led_ctx_x532p[0];
        g_max_led_ctx = ARRAY_LENGTH (led_ctx_x532p);
    } else if (platform_type_equal (X564P)) {
        g_led_ctx = &led_ctx_x564p[0];
        g_max_led_ctx = ARRAY_LENGTH (led_ctx_x564p);
    } else if (platform_type_equal (X308P)) {
        g_led_ctx = &led_ctx_x308p[0];
        g_max_led_ctx = ARRAY_LENGTH (led_ctx_x308p);
    } else if (platform_type_equal (X312P)) {
        g_led_ctx = &led_ctx_x312p[0];
        g_max_led_ctx = ARRAY_LENGTH (led_ctx_x312p);
    } else if (platform_type_equal (HC)) {
        g_led_ctx = &led_ctx_hc[0];
        g_max_led_ctx = ARRAY_LENGTH (led_ctx_hc);
    }
    BUG_ON (g_led_ctx == NULL);

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
        err = bf_pltfm_port_led_set (chip_id, &port_info,
                                     BF_LED_POST_PORT_DEL);
        if (err) {
            LOG_ERROR ("Unable to reset LED on port %d/%d",
                       port_info.conn_id,
                       port_info.chnl_id);
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

