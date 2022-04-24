/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PLTFM_EXT_PHY_H
#define _BF_PLTFM_EXT_PHY_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* platforms specific  retimer/repeater/external phy  APIs */

typedef struct bf_pltfm_ext_phy_cfg_t {
    bf_port_speed_t speed_cfg;
    bool is_an_on;
    bool is_optic;
} bf_pltfm_ext_phy_cfg_t;

/* initialize the retimer/repeater/external phy subsystem
 * called by common port mgmt function in drivers/src
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_init (void);

/* set the retimer/external phy into a specific mode
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_set_mode (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg);

/* reset the mode of a retimer/external phy
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_del_mode (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg);

/* reset a lane of a retimer/external phy
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_lane_reset (
    bf_pltfm_port_info_t *port_info,
    uint32_t num_lanes);

/* configure the retimer/external phy for a specific initial speed
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_init_speed (
    uint8_t port_num,
    bf_port_speed_t port_speed);

/* apply specific equalization settings to a retimer/external phy
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t bf_pltfm_ext_phy_conn_eq_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg);

/* apply a pre-set configuration to the retime/external phyr based on the type i
 * of qsfp module
 *
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t
bf_pltfm_platform_ext_phy_config_set (
    bf_dev_id_t dev_id, uint32_t conn,
    bf_pltfm_qsfp_type_t qsfp_type);

/* configure the prbs pattern in external phy
 *
 * paramxi
 *   port_info
 *   direction:  0:ingress, 1:egress
 *   prbs_mode:  0:PRBS9, 1: PRBS15, 2:PRBS23, 3:PRBS31
 * @return
 *  BF_PLTFM_SUCCESS onsuccess and other values on failure
 */
bf_pltfm_status_t
bf_pltfm_platform_ext_phy_prbs_set (
    bf_pltfm_port_info_t *port_info,
    uint8_t direction, uint16_t prbs_mode);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_EXT_PHY_H */
