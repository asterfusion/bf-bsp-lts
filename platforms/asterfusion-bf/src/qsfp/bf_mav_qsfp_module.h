/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_MAV_QSFP_MODULE_H
#define _BF_MAV_QSFP_MODULE_H

#define BF_MAV_SUB_PORT_CNT                                                 \
  64 /* number of ports under one cp2112 subsystem TBD : get it from bd_cfg \
        lib */

#define MAV_PCA9535_REG_INP 0
#define MAV_PCA9535_REG_OUT 2
#define MAV_PCA9535_REG_POL 4
#define MAV_PCA9535_REG_CFG 6

#define MAV_PCA9535_LP0 0
#define MAV_PCA9535_LP1 1
#define MAV_PCA9535_PR0 2
#define MAV_PCA9535_PR1 3
#define MAV_PCA9535_INT0 4
#define MAV_PCA9535_INT1 5
#define MAV_PCA9535_MISC 6
#define MAV_PCA9548_CPU_PORT 7

#define MAV_PCA9535_MAX 7 /* this must be equal to the last PCA 9535 */

#define MAV_DEF_PCA9548_COMM_EN 0x3F /* all 9435 enabled */

int bf_pltfm_init_cp2112_qsfp_bus (
    bf_pltfm_cp2112_device_ctx_t *hndl1,
    bf_pltfm_cp2112_device_ctx_t *hndl2);
bf_pltfm_status_t bf_mav_qsfp_sub_module_read (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    uint8_t i2c_addr,
    unsigned int offset,
    int len,
    uint8_t *buf);
bf_pltfm_status_t bf_mav_qsfp_sub_module_write (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    uint8_t i2c_addr,
    unsigned int offset,
    int len,
    uint8_t *buf);
int bf_pltfm_get_sub_module_pres (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
int bf_pltfm_get_sub_module_int (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *intr);
int bf_pltfm_get_cpu_module_pres (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres);
int bf_pltfm_get_cpu_module_int (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *intr);
int bf_pltfm_get_cpu_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *lpmode);
int bf_pltfm_get_sub_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *lpmod);
int bf_pltfm_set_sub_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    bool lpmod);
int bf_pltfm_set_cpu_module_reset (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    bool reset);
int bf_pltfm_sub_module_reset (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module, bool reset);
int bf_pltfm_set_cpu_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    bool lpmode);
int bf_mav_unselect_unlock_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl);
int bf_mav_lock_select_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl);
int bf_mav_lock_unselect_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl);
#endif
