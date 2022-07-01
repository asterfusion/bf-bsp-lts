/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_LED_H
#define _BF_PLTFM_LED_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* platforms specific led APIs */

/* initialize the LED sub system
 * this function must be invoked by platforms initiazation functions,
 * like, bf_pltfm_platform_port_init()
 * @return
 *   0 on success and -1 on error
 */
int bf_pltfm_led_init (int chip_id);

int bf_pltfm_led_de_init ();

/* set the LED
 * led_col can be suitably interpreted by the platforms specifics.
 * But, the bf-platform/drivers layer would call this function
 * with specific led_col and led_blink parameters depending on the
 * state of the underlying port
 * @return
 *   0 on success and -1 on error
 */
int bf_pltfm_port_led_set (int chip_id,
                           bf_pltfm_port_info_t *port_info,
                           bf_led_condition_t led_cond);
#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_LED_H */
