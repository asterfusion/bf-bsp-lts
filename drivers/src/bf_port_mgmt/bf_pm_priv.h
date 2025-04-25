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

#define BF_PM_INTF_MAP_KEY(conn, channel) ((conn) | ((channel) << 16))

#define HAVE_SFP
#if defined(HAVE_SFP)
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#endif
typedef struct bf_pm_qsfp_info_t {
    bool is_present;
    bf_pltfm_qsfp_type_t qsfp_type;
    bf_pltfm_qsfpdd_type_t qsfpdd_type;
    // for Optical module
    bool is_optic;  // 0 for dac;
} bf_pm_qsfp_info_t;

typedef bf_pm_qsfp_info_t bf_pm_sfp_info_t;

// Store all user configuration per port triggered via SDK
typedef struct bf_pm_intf_cfg_ {
    bf_dev_id_t dev_id;
    uint32_t conn_id;
    uint32_t channel;
    bf_port_speed_t intf_speed;
    uint32_t intf_nlanes;
    int intf_chmask;
    bool admin_up;
    bf_dev_port_t dev_port;
    bf_pltfm_encoding_type_t encoding;

    // For DAC:
    // If PM_AN_FORCE_ENABLE or PM_AN_DEFAULT, trigger AN/LT.
    // If PM_AN_FORCE_DISABLE, trigger DFE tunning.
    //
    // For Optic:
    //  ignore an_policy; always do AN-off and DFE tuning
    //
    // Note: If there is a retimer/ext phy, AN/DFE should be done
    // between local and remote ports. Between ASIC and retimers,,
    // DFE or AN/LT kR can be done depending upon platform need.
    //
    // Check if CLI triggers a change to platform or not - TBD
    bf_pm_port_autoneg_policy_e an_policy;

} bf_pm_intf_cfg_t;

// port structure to coordinate between, bf-pm, qsfp and its fsm
// The members of this struct is cleared/initialized after port-add
typedef struct bf_pm_intf_ {
    bf_pm_intf_cfg_t intf_cfg;  // from SDK

    bf_pm_qsfp_info_t *qsfp_info; // pointer to pm_qsfp_info_arr

    // If needed, add ext-phy here

    // Manage
    bool skip_periodic;  // for debugging

    bool self_fsm_running;
    bool intf_added;
    bool link_state;
    int rxlos_debounce_cntr;
    struct timespec next_run_time;
} bf_pm_intf_t;

typedef struct {
    bool mtx_inited;
    bf_sys_mutex_t intf_mtx;
} bf_pm_intf_mtx_t;

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

bf_pm_qsfp_info_t *bf_pltfm_get_pm_qsfp_info_ptr(int connector);
void bf_pm_intf_add(bf_pltfm_port_info_t *port_info,
                    bf_dev_id_t dev_id,
                    bf_dev_port_t dev_port,
                    bf_pal_front_port_cb_cfg_t *port_cfg,
                    bf_pltfm_encoding_type_t encoding);
void bf_pm_intf_del(bf_pltfm_port_info_t *port_info,
                    bf_pal_front_port_cb_cfg_t *port_cfg);
void bf_pm_intf_enable(bf_pltfm_port_info_t *port_info,
                       bf_pal_front_port_cb_cfg_t *port_cfg);
void bf_pm_intf_disable(bf_pltfm_port_info_t *port_info,
                        bf_pal_front_port_cb_cfg_t *port_cfg);
void bf_pm_intf_ha_link_state_notiy(bf_pltfm_port_info_t *port_info,
                                    bool link_up);
bf_status_t bf_pm_intf_ha_wait_port_cfg_done(bf_dev_id_t dev_id);

void bf_pm_intf_init();
void bf_pm_interface_fsm(void);
void bf_pm_interface_sfp_fsm(void);
void bf_pm_qsfp_quick_removal_detected_set (
    uint32_t conn_id, bool flag);
bool bf_pm_qsfp_quick_removal_detected_get (
    uint32_t conn_id);

int qsfp_fsm_update_cfg (int conn_id,
                         int first_ch,
                         bf_port_speed_t intf_speed,
                         int intf_nlanes,
                         bf_pltfm_encoding_type_t encoding);
int qsfp_fsm_deinit_cfg (int conn_id,
                         int first_ch);
int qsfp_fsm_get_enabled_mask(int conn_id, int first_ch);
void bf_pm_intf_set_link_state(bf_pltfm_port_info_t *port_info,
                               bool link_state);

void bf_pm_intf_obj_reset(void);
int qsfp_fsm_get_rx_los(int conn_id, int first_ch, int n_ch, bool *rx_los_flag);
int qsfp_fsm_get_rx_lol(int conn_id, int first_ch, int n_ch, bool *rx_lol_flag);
int qsfp_get_rx_los_support(int port, bool *rx_los_support);
int qsfp_get_rx_lol_support(int port, bool *rx_lol_support);
void qsfp_reset_pres_mask(void);

bf_status_t bf_pltfm_pm_media_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_media_type_t *media_type);
bf_pltfm_status_t bf_pltfm_pm_ha_mode_set();
bf_pltfm_status_t bf_pltfm_pm_ha_mode_clear();
bool bf_pltfm_pm_is_ha_mode();
uint32_t bf_pltfm_pm_dbnc_thres_get();

#if defined(HAVE_SFP)
bf_pm_sfp_info_t *bf_pltfm_get_pm_sfp_info_ptr (
    int module);
int sfp_fsm_get_rx_los(int module, bool *rx_los_flag);
int sfp_fsm_get_rx_lol(int module, bool *rx_lol_flag);
int sfp_get_rx_los_support (int port,
                             bool *rx_los_support);
int sfp_get_rx_lol_support (int port,
                             bool *rx_lol_support);

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

