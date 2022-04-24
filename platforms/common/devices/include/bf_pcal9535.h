/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PCAL9535_H
#define _BF_PCAL9535_H -

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// PCAL9535 is backward compatible with PCA953x.
typedef enum pca_device_id_ { PCAL9535 = 0, PCA9535 } pca_device_id;

// Platform IO support
typedef struct _pcal9535_info {
    bf_pltfm_status_t (*read) (uint8_t device,
                               uint8_t device_offset,
                               uint8_t len,
                               uint16_t *buf,
                               void *user_data);

    bf_pltfm_status_t (*write) (uint8_t device,
                                uint8_t device_offset,
                                uint8_t len,
                                uint16_t *buf,
                                void *user_data);
    void (*device_lock) (uint8_t device);
    void (*device_unlock) (uint8_t device);

    pca_device_id pca_dev_id;

    bool debug;
} pcal9535_info_t;

// Do platform-init before any other API access
int pcal9535_platform_init (pcal9535_info_t
                            *pca_info);

// If grp=0, register pairs are accessed in REG_0 followed by REG_1.
// If grp=1, *_REG1 is accessed.
bf_pltfm_status_t pcal9535_bf_enable_input_latch (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool enable,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_get_config_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_get_input_latch (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_get_input_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data);

bf_pltfm_status_t pcal9535_bf_get_output_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_get_pol_inv_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_set_config_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool input,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_set_output_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool enable,
    void *user_data);
bf_pltfm_status_t pcal9535_bf_set_pol_inv (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool invert,
    void *user_data);
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_BF_PCAL9535_H */
