/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef BF_PORT_PLTFM_INTF_H
#define BF_PORT_PLTFM_INTF_H

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <dvm/bf_drv_intf.h>
#include <port_mgr/bf_port_if.h>
#include <port_mgr/bf_serdes_if.h>

bf_pltfm_status_t bf_pltfm_pm_init (
    bf_dev_init_mode_t);

bf_pltfm_status_t bf_pltfm_pm_deinit();

bf_pltfm_status_t bf_pltfm_pm_port_add (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port,
    bf_port_speed_t speed,
    bf_fec_type_t fec_type);

bf_pltfm_status_t bf_pltfm_pm_port_del (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port);

bf_pltfm_status_t bf_pltfm_pm_port_enable (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port);

bf_pltfm_status_t bf_pltfm_pm_port_disable (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port);

bf_pltfm_status_t
bf_pltfm_pm_switchd_port_del_all (bf_dev_id_t
                                  dev_id);

bf_pltfm_status_t bf_pltfm_pm_dev_port_add (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    bf_port_speed_t speed,
    bf_fec_type_t fec_type);

bf_pltfm_status_t bf_pltfm_pm_dev_port_del (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port);

bf_pltfm_status_t bf_pltfm_pm_dev_port_enable (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port);

bf_pltfm_status_t bf_pltfm_pm_dev_port_disable (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port);

bf_pltfm_status_t
bf_pltfm_pm_map_port_info_to_dev_port (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bf_dev_port_t *dev_port);

bf_pltfm_status_t bf_pltfm_pm_port_stats_get (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bf_rmon_counter_array_t *stats);

bf_pltfm_status_t
bf_pltfm_pm_port_oper_status_get (
    bf_pltfm_port_info_t *port_info,
    bool *oper_status);

bf_pltfm_status_t bf_pltfm_pm_port_speed_get (
    bf_pltfm_port_info_t *port_info,
    bf_port_speed_t *speed);

bf_pltfm_status_t bf_pltfm_pm_port_fec_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_fec_type_t *fec_type);

bf_pltfm_status_t
bf_pltfm_pm_map_dev_port_to_port_info (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    uint32_t *conn,
    uint32_t *chnl);

bf_pltfm_status_t
bf_pltfm_pm_oper_status_poll_start();
bf_pltfm_status_t
bf_pltfm_pm_oper_status_poll_stop();
bf_pltfm_status_t
bf_pltfm_pm_qsfp_scan_poll_start();
bf_pltfm_status_t
bf_pltfm_pm_qsfp_scan_poll_stop();

bf_pltfm_status_t
bf_pltfm_pm_port_oper_status_update (
    bf_dev_id_t dev_id);
bf_pltfm_status_t bf_pltfm_pm_port_auto_neg_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    int val);

bf_pltfm_status_t
bf_pltfm_pm_serdes_lane_info_set (bf_dev_id_t
                                  dev_id,
                                  uint32_t mac_id,
                                  uint32_t mac_chnl);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_all_stats_clear (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_this_stat_clear (
    bf_dev_id_t dev_id, bf_dev_port_t dev_port,
    bf_rmon_counter_t ctr_type);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_all_stats_get (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    uint64_t stats[BF_NUM_RMON_COUNTERS]);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_this_stat_get (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    bf_rmon_counter_t ctr_type,
    uint64_t *stat_val);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_all_stats_update (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port);

bf_pltfm_status_t
bf_pltfm_pm_stats_poll_interval_set (
    bf_dev_id_t dev_id,
    uint32_t poll_intvl_ms);

bf_pltfm_status_t
bf_pltfm_pm_fp_idx_to_dev_port_map (
    bf_dev_id_t dev_id,
    uint32_t fp_idx,
    bf_dev_port_t *dev_port);

bf_pltfm_status_t bf_pltfm_pm_max_ports_get (
    bf_dev_id_t dev_id,
    uint32_t *ports);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_auto_neg_set (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    int val);

ucli_node_t *bf_pltfm_pm_ucli_node_create (
    ucli_node_t *m);

bf_pltfm_status_t
bf_pltfm_pm_port_qsfp_pres_ignore_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info, bool val);

bf_pltfm_status_t
bf_pltfm_pm_port_link_debounce_time_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    uint32_t time_ms);

bf_pltfm_status_t
bf_pltfm_pm_dev_port_loopback_mode_set (
    bf_dev_id_t dev_id, bf_dev_port_t dev_port,
    bf_loopback_mode_e mode);

/**
 * @brief Set the Serdes Tx Equalization Main Parameter (Especially exposed for
 *Diag
 *module)
 *
 * @param dev_id Device Id
 * @param port_info Structre containing the conn id and the chnl id
 * @param allow_unassigned_port Indicates if the serdes settings are to allowed
 *on an unadded port
 * @param tx_attn Serdes TX Attenuation param
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t
bf_pltfm_pm_port_serdes_tx_eq_main_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bool allow_unassigned_port,
    int tx_attn);

/**
 * @brief Set the Serdes Tx Equalization Post Parameter (Especially exposed for
 *Diag
 *module)
 *
 * @param dev_id Device Id
 * @param port_info Structre containing the conn id and the chnl id
 * @param allow_unassigned_port Indicates if the serdes settings are to allowed
 *on an unadded port
 * @param tx_post Serdes TX Post Emphasis param
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t
bf_pltfm_pm_port_serdes_tx_eq_post_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bool allow_unassigned_port,
    int tx_post);

/**
 * @brief Set the Serdes Tx Equalization Pre Parameter (Especially exposed for
 *Diag
 *module)
 *
 * @param dev_id Device Id
 * @param port_info Structre containing the conn id and the chnl id
 * @param allow_unassigned_port Indicates if the serdes settings are to allowed
 *on an unadded port
 * @param tx_pre Serdes TX Pre Emphasis param
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t
bf_pltfm_pm_port_serdes_tx_eq_pre_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bool allow_unassigned_port,
    int tx_pre);

/**
 * @brief Change the speed of an already existing port (Especially exposed for
 *Diag module)
 *
 * @param dev_id Device Id
 * @param port_info Structure containing the conn id and the chnl id
 * @param new_port_speed New speed to be set for the port
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t bf_pltfm_pm_port_speed_change (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bf_port_speed_t new_port_speed);

/**
 * @brief Enable/Disable FEC for an already existing port (Especially exposed
 *for Diag module)
 *
 * @param dev_id Device Id
 * @param port_info Structure containing the conn id and the chnl id
 * @param fec_enable Boolean to indicate if FEC is to be set for a port or not
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t bf_pltfm_pm_port_fec_enable (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    bool fec_enable);

/**
 * @brief Setup PRBS test for a port
 *
 * @param dev_id Device Id
 * @param conn Front panel connector id (Mavericks:1-65, Montara:1-33)
 * @param prbs_speed Speed in which PRBS test is to be set
 * @param prbs_mode Mode of the PRBS test
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_port_prbs_set (
    bf_dev_id_t dev_id,
    int conn,
    bf_port_prbs_speed_t prbs_speed,
    bf_port_prbs_mode_t prbs_mode);

/**
 * @brief Setup PRBS test for multiple ports at once
 *
 * @param dev_id Device Id
 * @param conn List of Front panel connector id (Mavericks:1-65, Montara:1-33)
 * @param num_conn Number of connector ids in the list (User has to ensure
 *        right number is passed here)
 * @param prbs_speed Speed in which PRBS test is to be set
 * @param prbs_mode Mode of the PRBS test
 *
 * @return Status of the API call
 */
bf_pltfm_status_t
bf_pltfm_pm_port_prbs_set_unsafe (
    bf_dev_id_t dev_id,
    int *conn,
    int num_conn,
    bf_port_prbs_speed_t prbs_speed,
    bf_port_prbs_mode_t prbs_mode);

/**
 * @brief Cleanup PRBS test for a port
 *
 * @param dev_id Device Id
 * @param conn Front panel connector id (Mavericks:1-65, Montara:1-33)
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_port_prbs_cleanup (
    bf_dev_id_t dev_id, int conn);

/**
 * @brief Cleanup PRBS test for multiple ports at once
 *
 * @param dev_id Device Id
 * @param conn List of Front panel connector id (Mavericks:1-65, Montara:1-33)
 * @param num_conn Number of connector ids in the list (User has to ensure
 *        right number is passed here)
 *
 * @return Status of the API call
 */
bf_pltfm_status_t
bf_pltfm_pm_port_prbs_cleanup_unsafe (
    bf_dev_id_t dev_id,
    int *conn,
    int num_conn);

/** @brief Set the FEC type for an already added port (Please note that if the
 * being set is different than the one with which the port was added, then the
 * port will be deleted and re-added)
 *
 * @param dev_id Device Id
 * @param dev_port Device port number
 * @param fec_type FEC type to be set
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_dev_port_fec_set (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    bf_fec_type_t fec_type);

/**
 * @brief Get the detected media type for a port (If no QSFP is inserted,
 * media type reported is UNKNOWN
 *
 * @param dev_id Device Id
 * @param dev_port Device port number
 * @param media_type Media type detected for the port
 *
 * @return Status of the API call
 */
bf_pltfm_status_t
bf_pltfm_pm_dev_port_media_type_get (
    bf_dev_id_t dev_id, bf_dev_port_t dev_port,
    bf_media_type_t *media_type);

/**
 * @brief Display Serdes stats for a port (This function doesn't actually get
 *any stats from the SDE port mgr. It just calls the dump function in SDE port
 *mgr. Exposed especially for internal debugging purposes)
 *
 * @param dev_id Device Id
 * @param conn Front panel connector id (Mavericks:1-65, Montara:1-33)
 * @param arg ucli context (typecasted as (void *)) in which to print
 *
 * @return Status of the API call
 */
bf_pltfm_status_t
bf_pltfm_pm_port_sd_stats_display (
    bf_dev_id_t dev_id,
    int conn,
    void *arg);
/**
 * @brief Get Serdes stats for a port (This function actually queries the serdes
 *stats from the SDE port mgr)
 *
 * @param dev_id Device Id
 * @param conn Front panel connector id (Mavericks:1-65, Montara:1-33)
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_port_sd_stats_get (
    bf_dev_id_t dev_id, int conn,
    bf_sds_debug_stats_per_quad_t *q_stats);

/**
 * @brief Given an integer, return the corresponding prbs mode
 *
 * @param mode integer to be converted into the prbs mode
 * @param p_mode Corresponding PRBS mode
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_prbs_mode_get (
    int mode,
    bf_port_prbs_mode_t *p_mode);
/**
 * @brief Set the serdes lane map info for the device
 * @param dev_id Device Id
 *
 * @return Status of the API call
 */
bf_status_t bf_pltfm_pm_serdes_lane_map_set (
    bf_dev_id_t dev_id);

/**
 * @brief Init the serdes for the device
 * @param dev_id Device Id
 *
 * @return Status of the API call
 */
bf_status_t bf_pltfm_pm_serdes_init (
    bf_dev_id_t dev_id);

/**
 * @brief Set the Serdes Tx Equalization Parameters
 *
 * @param dev_id Device Id
 * @param port_info Structre containing the conn id and the chnl id
 * @param tx_attn Serdes TX Attenuation param
 * @param tx_post Serdes TX Post Emphasis param
 * @param tx_pre Serdes TX Pre Emphasis param
 *
 * @return Status of the API call
 **/
bf_pltfm_status_t
bf_pltfm_pm_port_serdes_tx_param_set (
    bf_dev_id_t dev_id,
    bf_pltfm_port_info_t *port_info,
    int tx_attn,
    int tx_post,
    int tx_pre);

/**
 * @brief Return the type of the QSFP connected to a port
 *
 * @param port_info Structre containing the conn id and the chnl id
 * @param qsfp_type Type of the QSFP connected
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_port_qsfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type);

/* qsfp soft state maintenance functions (maintained by bf_pltfm_pm layer) */
void qsfp_lpmode_sw_set (bf_dev_id_t dev_id,
                         int conn_id, bool lpmode);

/* Maybe ucli use this API later. */
void qsfp_fsm_update_cdr (bf_dev_id_t dev_id,
                         int conn_id);

void qsfp_fsm_ch_disable (bf_dev_id_t dev_id,
                          int conn_id, int ch);

void qsfp_fsm_ch_enable (bf_dev_id_t dev_id,
                         int conn_id, int ch);

void qsfp_state_ha_enable_config_delete (
    int conn_id, int ch);

void qsfp_state_ha_enable_config_set (int conn_id,
                                      int ch);

void qsfp_fsm_notify_bf_pltfm (bf_dev_id_t dev_id,
                               int conn_id);

void qsfp_oper_info_get (int conn_id,
                         bool *present,
                         uint8_t *pg0_lower,
                         uint8_t *pg0_upper);
void qsfp_oper_info_get_pg1 (int conn_id,
                             bool *present, uint8_t *pg3);
void qsfp_oper_info_get_pg3 (int conn_id,
                             bool *present, uint8_t *pg3);
void qsfp_oper_info_get_pg16 (int conn_id,
                             bool *present, uint8_t *pg16);
void qsfp_oper_info_get_pg17 (int conn_id,
                             bool *present, uint8_t *pg17);

void bf_pltfm_pm_qsfp_simulate_all_removed (void);

void qsfp_luxtera_lpbk (int conn_id,
                        bool near_lpbk);
void qsfp_luxtera_hipwr (int conn_id,
                         bool is_cmis);

void bf_pm_qsfp_luxtera_state_capture (
    int conn_id, uint8_t arr_0x3k[0x3000]);
int bf_pm_num_qsfp_get (void);

char *qsfp_channel_fsm_st_get (int conn_id,
                               int ch);
void qsfp_channel_fsm_st_set (int conn_id, int ch, uint8_t state, bool stop);
char *qsfp_module_fsm_st_get (int conn_id);
void qsfp_module_fsm_st_set (int conn_id, uint8_t state);

char *sfp_channel_fsm_st_get (int port);
char *sfp_module_fsm_st_get (int port);

/**
 * @brief Enable the temperature monitoring for optical modules.
 *        a) Default disabled and Platform Specific.
 *            Result will be cached.
 *            Two actions are taken as noted in b) and c) below
 *
 *        b) Common-qsfp management code :
 *            If module see high alarm temperature, it will be put into low
 *            power mode. Recovery requires unplug and plug of the module.
 *            Since this will require some analysis of hardware or module.
 *
 *        c) Fan-speed control :
 *           Platform specific health monitor thread reads
 *           from cached data and sends the highest temperature among the
 *           modules to bmc for controlling the speed.
 *           Temperature Profiling and hysteri is platform/hardware specific
 *           and in handled by fan-daemon in bmc*
 *
 * @param enable true or false
 *
 * @return Void
 */
void bf_port_qsfp_mgmnt_temper_monitor_set (
    bool enable);

/**
 * @brief Returns true if temperature monitoring enabled for optical modules.
 *
 * @return True if enabled. False otherwise
 */
bool bf_port_qsfp_mgmnt_temper_monitor_get (void);

/**
 * @brief Set the temperature monitoring interval. Default 5 seconds
 *
 * @param poll_intv_sec in 5 sec
 *
 * @return Void
 */
void bf_port_qsfp_mgmnt_temper_monitor_period_set (
    uint32_t poll_intv_sec);

/**
 * @brief Get the temperature monitoring interval in secdons.
 *
 * @return Void
 */
uint32_t bf_port_qsfp_mgmnt_temper_monitor_period_get (
    void);

// only for cli
bool bf_port_qsfp_mgmnt_temper_high_alarm_flag_get (
    int port);

// return recorded temperature during module high-alarm
// only for cli
double bf_port_qsfp_mgmnt_temper_high_record_get (
    int port);

/**
 * @brief Enable the temperature monitoring for optical modules.
 *        a) Default disabled and Platform Specific.
 *            Result will be cached.
 *            Two actions are taken as noted in b) and c) below
 *
 *        b) Common-qsfp management code :
 *            If module see high alarm temperature, it will be put into low
 *            power mode. Recovery requires unplug and plug of the module.
 *            Since this will require some analysis of hardware or module.
 *
 *        c) Fan-speed control :
 *           Platform specific health monitor thread reads
 *           from cached data and sends the highest temperature among the
 *           modules to bmc for controlling the speed.
 *           Temperature Profiling and hysteri is platform/hardware specific
 *           and in handled by fan-daemon in bmc*
 *
 * @param enable true or false
 *
 * @return Void
 */
void bf_port_sfp_mgmnt_temper_monitor_set (
    bool enable);

/**
 * @brief Returns true if temperature monitoring enabled for optical modules.
 *
 * @return True if enabled. False otherwise
 */
bool bf_port_sfp_mgmnt_temper_monitor_get (void);

/**
 * @brief Set the temperature monitoring interval. Default 5 seconds
 *
 * @param poll_intv_sec in 5 sec
 *
 * @return Void
 */
void bf_port_sfp_mgmnt_temper_monitor_period_set (
    uint32_t poll_intv_sec);

/**
 * @brief Get the temperature monitoring interval in secdons.
 *
 * @return Void
 */
uint32_t bf_port_sfp_mgmnt_temper_monitor_period_get (
    void);

// only for cli
bool bf_port_sfp_mgmnt_temper_high_alarm_flag_get (
    int port);

// return recorded temperature during module high-alarm
// only for cli
double bf_port_sfp_mgmnt_temper_high_record_get (
    int port);

//#if defined(HAVE_SFP)
/**
 * @brief Return the type of the SFP connected to a port
 *
 * @param port_info Structre containing the conn id and the chnl id
 * @param qsfp_type Type of the QSFP connected
 *
 * @return Status of the API call
 */
bf_pltfm_status_t bf_pltfm_pm_port_sfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type);
void sfp_fsm_notify_bf_pltfm (bf_dev_id_t dev_id,
                              int module);

int bf_pm_num_sfp_get (void);

//#endif

#endif
