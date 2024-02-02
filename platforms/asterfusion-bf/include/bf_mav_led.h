/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_MAV_LED_H
#define _BF_MAV_LED_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* LED colors can be bit-wise */
#define BF_MAV_PORT_LED_OFF     0x0
#define BF_MAV_PORT_LED_RED     0x4
#define BF_MAV_PORT_LED_GREEN   0x2
#define BF_MAV_PORT_LED_BLUE    0x1
#define BF_MAV_PORT_LED_BLINK   0x8

struct led_ctx_t {
    const char *desc;
    /* Switch port */
    uint32_t         conn_id;
    uint32_t         chnl_id;
    /* cpld or tf pin pair. */
    uint8_t idx;
    /* offset of <idx> */
    uint16_t off;
    /* bit offset of <off> */
    uint8_t off_b;
    /* value to <idx.off.off_b> */
    uint8_t val;    /* cached led value. */
};
#define NOT_USED            0

struct led_init_t {
    /* cpld or tf pin pair. */
    uint8_t idx;
    /* offset of <idx> */
    uint8_t off;
    /* value to <idx.off> */
    uint8_t val;
};

/* - by tsihang, 18 Oct. 2019 */
#define PINXY BF_IO_PIN_PAIR_2_3    /* Unused */
#define PIN01 BF_IO_PIN_PAIR_0_1    /* PIN_PAIR 0 1 */
#define PIN45 BF_IO_PIN_PAIR_4_5    /* PIN_PAIR 4 5 */

/* - by tsihang, 3 Nov. 2019 */
#define AF_LED_CPLD_I2C_ADDR    0x40
#define AF_SWAP_BYTE(buffer) \
  ((((buffer) >> 8) & 0xff) | (((buffer)&0xff) << 8))


extern bool async_led_debug_on;

typedef int (*led_sync_fun_ptr) (int, struct led_ctx_t *, uint8_t);
/* By tsihang, 2023/12/26. */
typedef void (*led_convert_fun_ptr) (bf_led_condition_t,
    uint8_t *);

/* Light on/off LOC LED */
int bf_pltfm_location_led_set (int on_off);

/* Light on/off LOC LED */
int bf_pltfm_location_led_set (int on_off);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_pltfm_led_ucli_node_create (
    ucli_node_t *m);
#endif

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_MAV_LED_H */
