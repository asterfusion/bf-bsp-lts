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

int bf_pltfm_led_de_init ();

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_pltfm_led_ucli_node_create (
    ucli_node_t *m);
#endif

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_MAV_LED_H */
