/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_led/bf_led.h>
#include <bf_types/bf_types.h>

#define HAVE_SFP
#if defined(HAVE_SFP)
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#endif
typedef struct bf_pm_qsfp_info_t {
    bool is_present;
    Ethernet_compliance eth_comp;
    Ethernet_extended_compliance eth_ext_comp;
    bf_pltfm_qsfp_type_t qsfp_type;
    // for Optical module
    bool is_optic;  // 0 for dac;
} bf_pm_qsfp_info_t;

uint32_t num_lanes_consumed_get (bf_port_speed_t
                                 port_speed);

// bf_pltfm_pm_qsfp_info_t *bf_pltfm_pm_qsfp_info_get(uint32_t chnl);

bf_pltfm_status_t pltfm_pm_port_qsfp_is_present (
    bf_pltfm_port_info_t *port_info,
    bool *is_present);

bf_pltfm_status_t qsfp_scan (bf_dev_id_t dev_id);

bf_pltfm_status_t qsfp_fsm (bf_dev_id_t dev_id);

void qsfp_deassert_all_reset_pins (void);

void qsfp_fsm_inserted (int conn_id);

void qsfp_fsm_removed (int conn_id);

void qsfp_state_ha_config_set (bf_dev_id_t dev_id,
                               int conn_id);

void qsfp_state_ha_config_delete (int conn_id);
void qsfp_fsm_ch_notify_not_ready (
    bf_dev_id_t dev_id, int conn_id, int ch);

bf_status_t bf_pltfm_pm_media_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_media_type_t *media_type);
bf_pltfm_status_t bf_pltfm_pm_ha_mode_set();
bf_pltfm_status_t bf_pltfm_pm_ha_mode_clear();
bool bf_pltfm_pm_is_ha_mode();

#if defined(HAVE_SFP)
bf_pltfm_status_t sfp_fsm (bf_dev_id_t dev_id);
extern void sfp_fsm_inserted (int module);
extern void sfp_fsm_removed (int module);
extern void sfp_fsm_st_disable (bf_dev_id_t
                                dev_id, int module);
extern void sfp_fsm_st_enable (bf_dev_id_t
                               dev_id, int module);
extern void sfp_fsm_notify_not_ready (
    bf_dev_id_t dev_id, int module);
void sfp_fsm_enable (bf_dev_id_t dev_id,
                     int module);
extern int bf_pm_num_sfp_get (void);
void sfp_deassert_all_reset_pins (void);

void bf_pm_sfp_quick_removal_detected_set (
    uint32_t module, bool flag);
bool bf_pm_sfp_quick_removal_detected_get (
    uint32_t module);
bf_pltfm_status_t pltfm_pm_port_sfp_is_present (
    bf_pltfm_port_info_t *port_info,
    bool *is_present);

#endif

