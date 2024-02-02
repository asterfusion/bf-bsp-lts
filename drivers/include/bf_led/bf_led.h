/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_LED_H
#define _BF_LED_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* platforms implementation to display LED appropriately */
    BF_LED_POST_PORT_DEL = 0,
    BF_LED_POST_PORT_DIS = 1,
    BF_LED_PRE_PORT_EN = 2,
    BF_LED_PORT_LINK_UP = 3,
    BF_LED_PORT_LINK_DOWN = 4,

    /* combinable hard coded LED colors conditions for convenient use */
    BF_LED_PORT_RED = 5,
    BF_LED_PORT_GREEN = 6,
    BF_LED_PORT_BLUE = 7,

    /* by tsihang, 21 Nov. 2019 */
    BF_LED_PORT_LINKUP_1G_10G = 8,
    BF_LED_PORT_LINKUP_25G = 9,
    BF_LED_PORT_LINKUP_40G = 10,
    BF_LED_PORT_LINKUP_50G = 11,
    BF_LED_PORT_LINKUP_100G = 12,
    /* by tsihang, 25 Dec. 2023. */
    BF_LED_PORT_LINKUP_200G = 13,
    BF_LED_PORT_LINKUP_400G = 14,
} bf_led_condition_t;

typedef enum { LED_NO_BLINK = 0, LED_BLINK } bf_led_blink_t;

/* display port led  by directly writing into sysCPLD */
int bf_port_led_set (int chip_id,
                     bf_pltfm_port_info_t *port_info,
                     bf_led_condition_t led_cond);
#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_LED_H */
