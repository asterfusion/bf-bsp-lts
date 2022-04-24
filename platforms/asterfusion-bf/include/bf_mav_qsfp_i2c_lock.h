/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_MAV_QSFP_I2C_LOCK_H
#define _BF_MAV_QSFP_I2C_LOCK_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* take i2c-lock and set the i2c mux to enable misc chn of common pca9548 */
int bf_pltfm_i2c_lock_select_misc_chn (void);
/* disable misc chn of common pca9548 and release i2c-lock */
int bf_pltfm_unselect_i2c_unlock_misc_chn (void);
/* take i2c-lock and disable all channels on common PCA9548 (CP2112_ID_2) */
int bf_pltfm_rtmr_i2c_lock (void);
/* disable misc chn of common pca9548 and release i2c-lock (CP2112_ID_2) */
int bf_pltfm_rtmr_i2c_unlock (void);
/* take global i2c-lock */
void bf_pltfm_i2c_lock (void);
/* release global i2c-lock */
void bf_pltfm_i2c_unlock (void);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_MAV_QSFP_I2C_LOCK_H */
