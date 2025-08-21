/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>

#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_switchd/bf_switchd.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_pltfm.h>
#include <bf_pltfm_ext_phy.h>
#include "bf_pltfm_chss_mgmt_intf.h"
#include "bf_pm_priv.h"

static bf_pm_intf_t intf_obj[MAX_CONNECTORS][MAX_CHAN_PER_CONNECTOR];
static bf_pm_intf_mtx_t intf_mtx[MAX_CONNECTORS][MAX_CHAN_PER_CONNECTOR];

typedef enum {
    PM_INTF_FSM_DISABLED,
    PM_INTF_FSM_INIT,
    PM_INTF_FSM_MEDIA_DETECTED,
    PM_INTF_FSM_WAIT_SERDES_TX_INIT,
    PM_INTF_FSM_WAIT_MEDIA_INIT,
    PM_INTF_FSM_WAIT_LINK_ST,
    PM_INTF_FSM_INCOMPATIBLE_MEDIA,
    PM_INTF_FSM_HA_WAIT_LINK_ST,
    PM_INTF_FSM_LINK_UP,
    PM_INTF_FSM_LINK_DOWN,
    PM_INTF_FSM_HA_LINK_UP,
    PM_INTF_FSM_HA_LINK_DOWN,
    PM_INTF_FSM_WAIT_MEDIA_RX_LOS,
    PM_INTF_FSM_WAIT_MEDIA_RX_LOL,
    PM_INTF_FSM_SET_RX_READY,
} pm_intf_fsm_states_t;

static char *pm_intf_fsm_st_to_str[] = {
    "PM_INTF_FSM_DISABLED           ",
    "PM_INTF_FSM_INIT               ",
    "PM_INTF_FSM_MEDIA_DETECTED     ",
    "PM_INTF_FSM_WAIT_SERDES_TX_INIT",
    "PM_INTF_FSM_WAIT_MEDIA_INIT    ",
    "PM_INTF_FSM_WAIT_LINK_ST       ",
    "PM_INTF_FSM_INCOMPATIBLE_MEDIA ",
    "PM_INTF_FSM_HA_WAIT_FOR_LINK_ST",
    "PM_INTF_FSM_LINK_UP            ",
    "PM_INTF_FSM_LINK_DOWN          ",
    "PM_INTF_FSM_HA_LINK_UP         ",
    "PM_INTF_FSM_HA_LINK_DOWN       ",
    "PM_INTF_FSM_WAIT_MEDIA_RX_LOS  ",
    "PM_INTF_FSM_WAIT_MEDIA_RX_LOL  ",
    "PM_INTF_FSM_SET_RX_READY       ",
};

/* TBD */
#if SDE_VERSION_LT(920)
/**
 * Identifies if a port/lane should use 1/(1+D) precoding or not
 */
typedef enum bf_pm_port_precoding_policy_e {
  /** Applications can specify whether to use precoding for PAM4 links or not.
   * Enable precoding for all PAM4 links with Copper modules.
   *
   * The precoding setting applies to individual lanes and spearately
   * in each direction (Tx or Rx).
   */
  PM_PRECODING_DISABLE = 0,
  PM_PRECODING_ENABLE = 1,

  /** NOTE: For internal use only. Applications should not use this.
   * Forced via ucli to overide values set by applications for debug
   * purposes.
   *
   * To overide application settings, do -
   *    - port-dis <port_str>
   *    - use pc-tx-set or pc-rx-set ucli command
   *    - port-enb <port_str>
   *
   *  To clear overide, do -
   *    - port-dis <port_str>
   *    - use pc-tx-clear or pc-rx-clear ucli command
   *    - port-enb <port_str>
   */
  PM_PRECODING_FORCE_DISABLE = 3,
  PM_PRECODING_FORCE_ENABLE = 4,

  /** Initial state */
  PM_PRECODING_INVALID = 0xFFFF,
} bf_pm_port_precoding_policy_e;
#endif

pm_intf_fsm_states_t pm_intf_fsm_st[MAX_CONNECTORS][MAX_CHAN_PER_CONNECTOR];

#if defined(HAVE_SFP)
static bf_pm_intf_t intf_sfp_obj[BF_PLAT_MAX_SFP + 1];
static bf_pm_intf_mtx_t intf_sfp_mtx[BF_PLAT_MAX_SFP + 1];
pm_intf_fsm_states_t pm_intf_sfp_fsm_st[BF_PLAT_MAX_SFP + 1];
#endif

void bf_pm_intf_obj_reset(void) {
    memset(intf_obj,
           0,
           sizeof(bf_pm_intf_t) * MAX_CONNECTORS * MAX_CHAN_PER_CONNECTOR);
    memset(intf_mtx,
           0,
           sizeof(bf_pm_intf_mtx_t) * MAX_CONNECTORS * MAX_CHAN_PER_CONNECTOR);

#if defined(HAVE_SFP)
    memset(intf_sfp_obj,
           0,
           sizeof(bf_pm_intf_t) * BF_PLAT_MAX_SFP);
    memset(intf_sfp_mtx,
           0,
           sizeof(bf_pm_intf_mtx_t) * BF_PLAT_MAX_SFP);
#endif
}

// assumes caller will hold the lock
static void bf_pm_intf_update_fsm_st(pm_intf_fsm_states_t curr_st,
                                     pm_intf_fsm_states_t next_st,
                                     uint32_t conn_id,
                                     uint32_t chan) {
    if (curr_st != next_st) {
        pm_intf_fsm_st[conn_id][chan] = next_st;
        LOG_DEBUG("PM_INTF_FSM: %2d/%d : %s --> %s",
                  conn_id,
                  chan,
                  pm_intf_fsm_st_to_str[curr_st],
                  pm_intf_fsm_st_to_str[next_st]);
    }
}

static void bf_pm_handle_intf_disable(bf_pm_intf_cfg_t *icfg,
                                      bf_pm_intf_t *intf,
                                      bf_pm_qsfp_info_t *qsfp_info) {
    if (!icfg || !intf || !qsfp_info) return;

    bf_pal_front_port_handle_t port_hdl;
    int module_chan;

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;

    module_chan = bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;

    // Admin-down. Inform the qsfp-fsm to disable the optics
    if (!icfg->admin_up && intf->self_fsm_running && qsfp_info->is_present &&
            qsfp_info->is_optic && (!bf_pltfm_pm_is_ha_mode())) {
        for (uint32_t ch = 0; ch < icfg->intf_nlanes; ch++) {
            qsfp_fsm_ch_disable(icfg->dev_id, port_hdl.conn_id, module_chan + ch);
        }
    }

    // command issued to SDK
    if (intf->self_fsm_running) {
        bf_pm_port_serdes_rx_ready_for_bringup_set(icfg->dev_id, &port_hdl, false);
        bf_pal_pm_front_port_ready_for_bringup(icfg->dev_id, &port_hdl, false);
        intf->self_fsm_running = false;
    }
}

static bool bf_pm_intf_is_qsfpdd_short(bf_pltfm_qsfpdd_type_t qsfpdd_type) {
    switch (qsfpdd_type) {
    case BF_PLTFM_QSFPDD_CU_0_5_M:
    case BF_PLTFM_QSFPDD_CU_1_M:
    case BF_PLTFM_QSFPDD_CU_LOOP:
        return true;
    default:
        break;
    }

    return false;
}

// Since there is no hardware SI settings for qsfp, map it to qsfpdd.
bf_pltfm_qsfpdd_type_t bf_pm_intf_get_qsfpdd_type(
    bf_pltfm_qsfp_type_t qsfp_type) {
    switch (qsfp_type) {
    case BF_PLTFM_QSFP_CU_0_5_M:
        return BF_PLTFM_QSFPDD_CU_0_5_M;
    case BF_PLTFM_QSFP_CU_1_M:
        return BF_PLTFM_QSFPDD_CU_1_M;
    case BF_PLTFM_QSFP_CU_2_M:
        return BF_PLTFM_QSFPDD_CU_2_M;
    case BF_PLTFM_QSFP_CU_3_M:
        return BF_PLTFM_QSFPDD_CU_2_M;
    // return BF_PLTFM_QSFPDD_CU_3_M;
    case BF_PLTFM_QSFP_CU_LOOP:
        return BF_PLTFM_QSFPDD_CU_LOOP;
    case BF_PLTFM_QSFP_OPT:
        return BF_PLTFM_QSFPDD_OPT;
    case BF_PLTFM_QSFP_UNKNOWN:
        return BF_PLTFM_QSFPDD_UNKNOWN;
    }

    return BF_PLTFM_QSFPDD_UNKNOWN;
}

char *bf_pm_intf_qsfpdd_len_get(bf_pltfm_qsfpdd_type_t qsfp_type) {
    switch (qsfp_type) {
    case BF_PLTFM_QSFPDD_CU_0_5_M:
        return "0.5m";
    case BF_PLTFM_QSFPDD_CU_1_M:
        return "1.0m";
    case BF_PLTFM_QSFPDD_CU_1_5_M:
        return "1.5m";
    case BF_PLTFM_QSFPDD_CU_2_M:
        return "2.0m";
    case BF_PLTFM_QSFPDD_CU_2_5_M:
        return "2.5m";
    case BF_PLTFM_QSFPDD_CU_LOOP:
        return "LPBK";
    case BF_PLTFM_QSFPDD_OPT:
        return "na";
    case BF_PLTFM_QSFPDD_UNKNOWN:
        return "LPBK";
    }

    return "LPBK";
}

extern int pltfm_fir_get(uint32_t conn_id,
                         uint32_t chnl,
                         bool is_osfp,
                         bool is_dac,
                         bool is_kr,
                         char *dac_len,
                         char *vendor,
                         uint32_t serdes_speed_in_gb,
                         uint32_t *pre3,
                         uint32_t *pre2,
                         uint32_t *pre1,
                         uint32_t *main,
                         uint32_t *post);
extern bf_status_t bf_tof3_serdes_port_speed_to_serdes_speed(
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    uint32_t ln,
    bf_port_speed_t speed,
    uint32_t num_lanes,
    uint32_t *serdes_gb,
    bool *is_pam4);
extern bool bf_qsfp_get_vendor_info(int port, qsfp_vendor_info_t *vendor);
extern bool bf_qsfp_get_module_type_str(int port,
                                        char *module_type_str,
                                        bool incl_hex);
extern bf_status_t bf_tof3_serdes_txfir_config_set(bf_dev_id_t dev_id,
        bf_dev_port_t dev_port,
        uint32_t ln,
        uint32_t cm3,
        uint32_t cm2,
        uint32_t cm1,
        uint32_t c0,
        uint32_t c1);
extern bf_status_t bf_pm_serdes_lane_tx_eq_override_get(
    bf_dev_id_t dev_id,
    bf_pal_front_port_handle_t *port_hdl,
    uint32_t ln,
    bool *override);

extern void bf_pm_icfg_speed_and_nlanes_get(uint32_t conn,
        uint32_t chan,
        uint32_t *intf_speed,
        uint32_t *nlanes);
extern void bf_pm_icfg_dev_id_and_dev_port_get(uint32_t conn,
        uint32_t chan,
        bf_dev_id_t *dev_id,
        bf_dev_port_t *dev_port);
extern bf_pltfm_status_t bf_bd_cfg_port_tf3_fir_get(
    bf_pltfm_port_info_t *port_info,
    uint32_t ln,
    uint32_t *pre3,
    uint32_t *pre2,
    uint32_t *pre1,
    uint32_t *main,
    uint32_t *post);
#if 0
static void bf_pm_intf_send_asic_serdes_info_tf3(
    bf_pltfm_port_info_t *port_info) {
    uint32_t pre3;
    uint32_t pre2;
    uint32_t pre1;
    uint32_t main_;
    uint32_t post;
    uint32_t intf_nlanes, conn_id, ch, unused_speed;
    bf_dev_id_t dev_id;
    bf_dev_port_t dev_port;

    conn_id = port_info->conn_id;
    ch = port_info->chnl_id;

    bf_pm_icfg_dev_id_and_dev_port_get(conn_id, ch, &dev_id, &dev_port);
    bf_pm_icfg_speed_and_nlanes_get(conn_id, ch, &unused_speed, &intf_nlanes);

    for (uint32_t ln = 0; ln < intf_nlanes; ln++) {
        // honor the ucli tx-eq override settings
        //
        bf_pal_front_port_handle_t port_hdl;
        bool override_set;
        bf_status_t sts;

        port_hdl.conn_id = conn_id;
        port_hdl.chnl_id = ch;
        sts = bf_pm_serdes_lane_tx_eq_override_get(
                  dev_id, &port_hdl, ln, &override_set);

        // update chnl_id in port_info
        bf_bd_cfg_port_tf3_fir_get(
            port_info, ln, &pre3, &pre2, &pre1, &main_, &post);

        if (!override_set) {
            sts = bf_tof3_serdes_txfir_config_set(
                      dev_id, dev_port, ln, pre3, pre2, pre1, main_, post);
            if (sts) {
                LOG_ERROR(
                    "%2d/%d : Error %d setting TX taps ln=%d", conn_id, ch, sts, ln);
            }
        } else {
            LOG_DEBUG("%2d/%d : tx-eq override set on ln%d. Don't update.",
                      conn_id,
                      ch,
                      ln);
        }
    }
}
#endif
static void bf_pm_intf_send_asic_serdes_info(bf_pm_intf_cfg_t *icfg,
        bf_pm_intf_t *intf,
        bf_pm_qsfp_info_t *qsfp_info) {
    uint32_t conn_id, ch;
    int tx_inv = 0, rx_inv = 0;
    bf_pltfm_serdes_lane_tx_eq_t tx_eq;
    bf_pltfm_port_info_t port_info;
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_id_t dev_id;
    bf_pal_serdes_polarity_t asic_serdes_pol[MAX_CHAN_PER_CONNECTOR] = {{0}};
    bf_pal_serdes_tx_eq_params_t asic_serdes_tx_eq;

    if (!icfg || !intf || !qsfp_info) return;

    dev_id = icfg->dev_id;

    if (bf_pm_intf_is_device_family_tofino(dev_id)) return;

    port_info.conn_id = port_hdl.conn_id = conn_id = icfg->conn_id;
    port_info.chnl_id = port_hdl.chnl_id = icfg->channel;

    //if (bf_pm_intf_is_device_family_tofino3(dev_id)) {
    //    bf_pm_intf_send_asic_serdes_info_tf3(&port_info);
    //    return;
    //}

    // Note: SDK caches it and applies it only when the port-fsm is run
    for (ch = 0; ch < icfg->intf_nlanes; ch++) {
        bf_bd_port_serdes_polarity_get(&port_info, &rx_inv, &tx_inv);
        asic_serdes_pol[ch].rx_inv = (bool)rx_inv;
        asic_serdes_pol[ch].tx_inv = (bool)tx_inv;

        if (bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) {
            bf_bd_port_serdes_tx_params_get(
                &port_info, BF_PLTFM_QSFPDD_CU_0_5_M, &tx_eq, icfg->encoding);
        } else if (bf_qsfp_is_cmis(conn_id)) {
            bf_bd_port_serdes_tx_params_get(
                &port_info, qsfp_info->qsfpdd_type, &tx_eq, icfg->encoding);
        } else {
            bf_bd_port_serdes_tx_params_get(
                &port_info,
                bf_pm_intf_get_qsfpdd_type(qsfp_info->qsfp_type),
                &tx_eq,
                icfg->encoding);
        }

        asic_serdes_tx_eq.tx_eq.tof2[ch].tx_pre1 = tx_eq.tx_pre1;
        asic_serdes_tx_eq.tx_eq.tof2[ch].tx_pre2 = tx_eq.tx_pre2;
        asic_serdes_tx_eq.tx_eq.tof2[ch].tx_main = tx_eq.tx_main;
        asic_serdes_tx_eq.tx_eq.tof2[ch].tx_post1 = tx_eq.tx_post1;
        asic_serdes_tx_eq.tx_eq.tof2[ch].tx_post2 = tx_eq.tx_post2;
        port_info.chnl_id++;
    }

    bf_pm_port_serdes_polarity_set(
        dev_id, &port_hdl, icfg->intf_nlanes, asic_serdes_pol);

    LOG_DEBUG(
        "PM_INTF_FSM: %d %2d/%d : encoding : %s --> Push Serdes TX settings",
        dev_id,
        icfg->conn_id,
        icfg->channel,
        (icfg->encoding == BF_PLTFM_ENCODING_PAM4) ? "PAM4" : "NRZ");

    bf_pm_port_serdes_tx_eq_params_set(
        dev_id, &port_hdl, icfg->intf_nlanes, &asic_serdes_tx_eq);
}

static void bf_pm_intf_send_asic_precoding_info(bf_pm_intf_cfg_t *icfg,
        bf_pm_intf_t *intf,
        bf_pm_qsfp_info_t *qsfp_info) {
    uint32_t ch;
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_id_t dev_id;
    bf_pm_port_precoding_policy_e precode = PM_PRECODING_DISABLE;

    if (!icfg || !intf || !qsfp_info) return;

    dev_id = icfg->dev_id;
    if ((!bf_pm_intf_is_device_family_tofino2(dev_id)) ||
            bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) {
        return;
    }

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;

    if ((!qsfp_info->is_optic) && (icfg->encoding == BF_PLTFM_ENCODING_PAM4)) {
        if (!bf_pm_intf_is_qsfpdd_short(qsfp_info->qsfpdd_type)) {
            precode = PM_PRECODING_ENABLE;
        }
    }

    for (ch = 0; ch < icfg->intf_nlanes; ch++) {
        precode = precode;
        port_hdl = port_hdl;
        bfn_pp_tx_set(dev_id, &port_hdl, ch, precode);
        bfn_pp_rx_set(dev_id, &port_hdl, ch, precode);
    }
}

static void bf_pm_intf_restore_debounce_thresh(bf_pm_intf_cfg_t *icfg,
        bf_pm_intf_t *intf,
        bf_pm_qsfp_info_t *qsfp_info) {
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_id_t dev_id;

    if (!icfg || !intf || !qsfp_info) return;

    dev_id = icfg->dev_id;
    if ((!bf_pm_intf_is_device_family_tofino2(dev_id)) ||
            bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) {
        return;
    }

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;

    port_hdl = port_hdl;
    bfn_pp_dbnc_set(dev_id, &port_hdl, 0);
}

static void bf_pm_intf_set_debounce_thresh(bf_pm_intf_cfg_t *icfg,
        bf_pm_intf_t *intf,
        bf_pm_qsfp_info_t *qsfp_info) {
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_id_t dev_id;
    uint32_t debounce_value = 0;
    bf_pltfm_get_debounce_threshold(&debounce_value);

    if (!icfg || !intf || !qsfp_info) return;

    dev_id = icfg->dev_id;
    if ((!bf_pm_intf_is_device_family_tofino2(dev_id)) ||
            bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) {
        return;
    }

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;

    port_hdl = port_hdl;
    if (icfg->encoding == BF_PLTFM_ENCODING_PAM4) {
            bfn_pp_dbnc_set(dev_id, &port_hdl, debounce_value);
    }
}

static void bf_pm_intf_fsm_next_run_time_set(struct timespec *next_run_time,
        int delay_ms) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    next_run_time->tv_sec = now.tv_sec + (delay_ms / 1000);
    next_run_time->tv_nsec =
        now.tv_nsec + (((delay_ms % 1000) * 1000000) % 1000000000);
    if (next_run_time->tv_nsec >= 1000000000) {
        next_run_time->tv_sec++;
        next_run_time->tv_nsec -= 1000000000;
    }
}

#if defined(HAVE_SFP)
/*****************************************************************
 * TOF2 only. Return the current CH FSM state desc for a transceiver.
 *****************************************************************/
char *bf_pm_intf_sfp_fsm_st_get (int port)
{
    int asize = ARRAY_LENGTH (pm_intf_fsm_st_to_str);
    pm_intf_fsm_states_t csm = pm_intf_sfp_fsm_st[port];

    return pm_intf_fsm_st_to_str[csm % asize] + 12;
}

// assumes caller will hold the lock
static void bf_pm_intf_update_sfp_fsm_st(pm_intf_fsm_states_t curr_st,
                                     pm_intf_fsm_states_t next_st,
                                     int module) {
    uint32_t conn_id = 0, chnl_id = 0;
    bf_sfp_get_conn (module, &conn_id,
                   &chnl_id);
    if (curr_st != next_st) {
        pm_intf_sfp_fsm_st[module] = next_st;
        LOG_DEBUG("PM_INTF_FSM: %2d/%d : %s --> %s",
                  conn_id,
                  chnl_id,
                  pm_intf_fsm_st_to_str[curr_st],
                  pm_intf_fsm_st_to_str[next_st]);
    }
}

static void bf_pm_interface_sfp_fsm_run(bf_pm_intf_cfg_t *icfg,
                                    bf_pm_intf_t *intf,
                                    bf_pm_qsfp_info_t *sfp_info) {
    pm_intf_fsm_states_t st;
    pm_intf_fsm_states_t next_st;
    bf_pal_front_port_handle_t port_hdl;
    bool asic_serdes_tx_ready;
    int module;
    bool rx_los_flag, rx_lol_flag, rx_los_sup;
    int delay_ms = 0;

    if ((!icfg) || (!intf) || (!sfp_info)) return;

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;
    bf_sfp_get_port (port_hdl.conn_id,
                     port_hdl.chnl_id, &module);
    next_st = st = pm_intf_sfp_fsm_st[module];

    // handle qsfp-removal in any state
    if ((!bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) &&
            (!sfp_info->is_present) && !bf_pltfm_pm_is_ha_mode()) {
        bf_pm_handle_intf_disable(icfg, intf, sfp_info);
        // since fsm-runs only on admin-enabled, move state to init
        if (icfg->admin_up) {
            next_st = PM_INTF_FSM_INIT;
        }
    }

    switch (st) {
    case PM_INTF_FSM_DISABLED:
        break;
    case PM_INTF_FSM_INIT:
        // check for media presence, if so advance states (internal ports are
        // assumed to always be present)
        if ((bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) ||
                (sfp_info->is_present)) {
            next_st = PM_INTF_FSM_MEDIA_DETECTED;
        } else if (bf_pltfm_pm_is_ha_mode()) {
            next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
        }
        break;
    case PM_INTF_FSM_MEDIA_DETECTED:
        // set up switch tx eq settings
        bf_pm_intf_send_asic_serdes_info(icfg, intf, sfp_info);

        bf_pm_intf_send_asic_precoding_info(icfg, intf, sfp_info);

        // enable SERDES TX
        if (sfp_info->is_optic) {
            bf_pm_pltfm_front_port_eligible_for_autoneg(
                icfg->dev_id, &port_hdl, false);
            bf_pal_pm_front_port_ready_for_bringup(icfg->dev_id, &port_hdl, true);
            if (!bf_pltfm_pm_is_ha_mode()) {
                next_st = PM_INTF_FSM_WAIT_SERDES_TX_INIT;
            } else {
                // Wait for link-state notification from SDK for next action
                next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
                intf->self_fsm_running = true;
            }
            break;
        } else {  // copper or internal
            switch (icfg->an_policy) {
            case PM_AN_FORCE_DISABLE:
                bf_pm_pltfm_front_port_eligible_for_autoneg(
                    icfg->dev_id, &port_hdl, false);
                // since we are not interested in asic-serdes-tx, indicate RX is
                // ready for DFE
                bf_pm_port_serdes_rx_ready_for_bringup_set(
                    icfg->dev_id, &port_hdl, true);
                bf_pal_pm_front_port_ready_for_bringup(
                    icfg->dev_id, &port_hdl, true);
                break;
            case PM_AN_DEFAULT:
            case PM_AN_FORCE_ENABLE:
            default:
                bf_pm_pltfm_front_port_eligible_for_autoneg(
                    icfg->dev_id, &port_hdl, true);
                bf_pal_pm_front_port_ready_for_bringup(
                    icfg->dev_id, &port_hdl, true);
                break;
            }
            if (!bf_pltfm_pm_is_ha_mode()) {
                // skip media init states for passive copper
                next_st = PM_INTF_FSM_WAIT_LINK_ST;
            } else {
                // Wait for link-state notification from SDK for next action
                next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
            }
            intf->self_fsm_running = true;
            break;
        }

        // we should never get here
        break;
    case PM_INTF_FSM_WAIT_SERDES_TX_INIT:
        // Wait for the SERDES TX init to complete
        // This state is only used for optical cables
        bf_pm_port_serdes_tx_ready_get(
            icfg->dev_id, &port_hdl, &asic_serdes_tx_ready);

        if (!asic_serdes_tx_ready) {  // keep waiting
            break;
        }

        sfp_fsm_st_enable(icfg->dev_id, module);

        // Now that we've advanced the sfp_ch_fsm, advance the pm_intf_fsm
        // to wait for sfp init to complete
        intf->self_fsm_running = true;
        next_st = PM_INTF_FSM_WAIT_MEDIA_INIT;
        break;
    case PM_INTF_FSM_WAIT_MEDIA_INIT:
        intf->self_fsm_running = true;
        intf->rxlos_debounce_cntr = bf_sfp_rxlos_debounce_get(module);
        next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOS;

        // keep waiting
        break;
    case PM_INTF_FSM_WAIT_LINK_ST:
        if (intf->link_state) {
            next_st = PM_INTF_FSM_LINK_UP;
        } else {
            next_st = PM_INTF_FSM_LINK_DOWN;
        }
        break;
    case PM_INTF_FSM_LINK_UP:
        // link is up
        // wait for media to be removed or port deletion or reconfig or los
        break;
    case PM_INTF_FSM_INCOMPATIBLE_MEDIA:
        // Media is incompatible with switch configuration, either because media
        // power requirements are too high or the media supported Applications
        // do not match the current switch configuration. Wait for media
        // to be replaced or configuration to be changed
        break;
    case PM_INTF_FSM_HA_WAIT_LINK_ST:
        // After warm-init end link could be up or down. Wait for nofitication
        // from SDK
        break;
    case PM_INTF_FSM_LINK_DOWN:
        if ((!bf_pltfm_pm_is_ha_mode()) && sfp_info->is_present &&
                icfg->admin_up && sfp_info->is_optic) {
            rx_los_flag = rx_lol_flag = true;
            sfp_fsm_get_rx_los(module, &rx_los_flag);
            sfp_fsm_get_rx_lol(module, &rx_lol_flag);
            if (rx_los_flag || rx_lol_flag) {
                bf_pm_port_serdes_rx_ready_for_bringup_set(
                    icfg->dev_id, &port_hdl, false);
                intf->self_fsm_running = true;
                intf->rxlos_debounce_cntr =
                    bf_qsfp_rxlos_debounce_get(module);
                next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOS;
            }
        }
        break;
    case PM_INTF_FSM_HA_LINK_DOWN:
        next_st = PM_INTF_FSM_INIT;
        break;
    case PM_INTF_FSM_HA_LINK_UP:
        next_st = PM_INTF_FSM_LINK_UP;
        break;
    case PM_INTF_FSM_WAIT_MEDIA_RX_LOS:
        if (!bf_pltfm_pm_is_ha_mode()) {
            rx_los_flag = rx_los_flag;
            rx_lol_flag = rx_lol_flag;
            rx_los_sup = rx_los_sup;
            rx_los_flag = true;
            bf_sfp_get_rx_los (
                module, &rx_los_flag);
            rx_los_flag = false;
            if (rx_los_flag) {
                /* We have 3 times here, maybe not really required. */
                intf->rxlos_debounce_cntr = bf_sfp_rxlos_debounce_get(module);
            } else if (intf->rxlos_debounce_cntr > 0) {
                intf->rxlos_debounce_cntr--;
            } else {
                next_st = PM_INTF_FSM_SET_RX_READY;
            }
            delay_ms = 100;
            intf->self_fsm_running = true;
        }
        break;

    case PM_INTF_FSM_SET_RX_READY:
        if (!bf_pltfm_pm_is_ha_mode()) {
            bf_pm_port_serdes_rx_ready_for_bringup_set(
                icfg->dev_id, &port_hdl, true);
            intf->self_fsm_running = true;
            next_st = PM_INTF_FSM_WAIT_LINK_ST;
        }
        break;
    default:
        break;
    }
    if (st != next_st) {
        bf_pm_intf_update_sfp_fsm_st(st, next_st, module);
    }
    bf_pm_intf_fsm_next_run_time_set(&intf->next_run_time, delay_ms);
}
#endif

/*****************************************************************
 * TOF2 only. Return the current CH FSM state desc for a transceiver.
 *****************************************************************/
char *bf_pm_intf_fsm_st_get (int conn_id,
                               int ch)
{
    int asize = ARRAY_LENGTH (pm_intf_fsm_st_to_str);
    pm_intf_fsm_states_t csm = pm_intf_fsm_st[conn_id % MAX_CONNECTORS][ch % MAX_CHAN_PER_CONNECTOR];

    //if (ch == qsfp_state[conn_id].dev_cfg[ch].host_head_ch) {
        return pm_intf_fsm_st_to_str[csm % asize] + 12;
    //} else {
        // Skip the lanes which is not in fsm.
        //return " ";
    //}
}

static void bf_pm_interface_fsm_run(bf_pm_intf_cfg_t *icfg,
                                    bf_pm_intf_t *intf,
                                    bf_pm_qsfp_info_t *qsfp_info) {
    pm_intf_fsm_states_t st;
    pm_intf_fsm_states_t next_st;
    bf_pal_front_port_handle_t port_hdl;
    bool asic_serdes_tx_ready;
    uint32_t ch = 0;
    int module_chan, rc;
    bool rx_los_flag, rx_lol_flag, rx_lol_sup, rx_los_sup;
    int delay_ms = 0;
    bf_pm_port_dir_e dir_mode = PM_PORT_DIR_DEFAULT;

    if ((!icfg) || (!intf) || (!qsfp_info)) return;

    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;
    next_st = st = pm_intf_fsm_st[port_hdl.conn_id][port_hdl.chnl_id];

    bf_pm_port_direction_get (icfg->dev_id, &port_hdl, &dir_mode);
    // handle qsfp-removal in any state
    if ((!bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) &&
            (!qsfp_info->is_present) && !bf_pltfm_pm_is_ha_mode() && (dir_mode != PM_PORT_DIR_TX_ONLY)) {
        bf_pm_handle_intf_disable(icfg, intf, qsfp_info);
        // since fsm-runs only on admin-enabled, move state to init
        if (icfg->admin_up) {
            next_st = PM_INTF_FSM_INIT;
        }
    }

    switch (st) {
    case PM_INTF_FSM_DISABLED:
        break;
    case PM_INTF_FSM_INIT:
        // check for media presence, if so advance states (internal ports are
        // assumed to always be present)
        if ((bf_bd_is_this_port_internal(icfg->conn_id, icfg->channel)) ||
                (qsfp_info->is_present)) {
            next_st = PM_INTF_FSM_MEDIA_DETECTED;
        } else if (bf_pltfm_pm_is_ha_mode()) {
            next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
        } else if((!qsfp_info->is_present) && (dir_mode == PM_PORT_DIR_TX_ONLY)){
            bf_pal_pm_front_port_ready_for_bringup(icfg->dev_id, &port_hdl, true);
            next_st = PM_INTF_FSM_WAIT_LINK_ST;
        }
        break;
    case PM_INTF_FSM_MEDIA_DETECTED:
        // media is present but the data path is not yet being initalized. check
        // to make sure media is supported, set up serdes tx

        // check compatibility of module supported Applications with switch
        // config.
        //
        // For Copper, just allow any speed config or channelized mode
        // Note: We are not catching mismatch like module is 100G DAC but
        // configuration is 400G. Worst case, link will not comeup.
        //
        // For optics, find-match-app is common for both sff8636 and CMIS
        if (qsfp_info->is_optic) {
            module_chan =
                bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;
            if ((rc = bf_cmis_find_matching_Application(
                        icfg->conn_id, icfg->intf_speed, icfg->intf_nlanes, module_chan
                        /* icfg->encoding */)) < 0) {
                LOG_ERROR(
                    "pm intf fsm: qsfp %d ch %d Matching Application not found. "
                    "Speed mismtach between optical-module and configuration "
                    "<intf_nlanes %d : module_chan %d : rc %d> \n",
                    icfg->conn_id,
                    icfg->channel,
                    icfg->intf_nlanes,
                    module_chan,
                    rc);
                if (bf_qsfp_is_cmis(port_hdl.conn_id) ||
                        (icfg->intf_nlanes > bf_qsfp_get_ch_cnt(port_hdl.conn_id))) {
                    next_st = PM_INTF_FSM_INCOMPATIBLE_MEDIA;
                    break;
                }
            }
        }

        // set up switch tx eq settings
        bf_pm_intf_send_asic_serdes_info(icfg, intf, qsfp_info);

        bf_pm_intf_send_asic_precoding_info(icfg, intf, qsfp_info);

        // restore debounce threshold
        bf_pm_intf_restore_debounce_thresh(icfg, intf, qsfp_info);

        // Set debounce threshold for PAM4
        bf_pm_intf_set_debounce_thresh(icfg, intf, qsfp_info);

        // enable SERDES TX
        if (qsfp_info->is_optic) {
            bf_pm_pltfm_front_port_eligible_for_autoneg(
                icfg->dev_id, &port_hdl, false);
            bf_pal_pm_front_port_ready_for_bringup(icfg->dev_id, &port_hdl, true);
            bf_pm_port_direction_get (icfg->dev_id, &port_hdl, &dir_mode);
            if (!bf_pltfm_pm_is_ha_mode()) {
                if (dir_mode == PM_PORT_DIR_TX_ONLY) {
                    module_chan =
                        bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;
                    // Tell the qsfp_ch_fsm that its ok to enable the lanes in this link.
                    // Loop through all lanes since we only run the pm_intf_fsm
                    // for head channels
                    for (ch = 0; ch < icfg->intf_nlanes; ch++) {
                        qsfp_fsm_ch_enable(icfg->dev_id, port_hdl.conn_id, module_chan + ch);
                    }
                    next_st = PM_INTF_FSM_WAIT_LINK_ST;
                } else {
                    next_st = PM_INTF_FSM_WAIT_SERDES_TX_INIT;
                }
            } else {
                // Wait for link-state notification from SDK for next action
                next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
                intf->self_fsm_running = true;
            }
            break;
        } else {  // copper or internal
            switch (icfg->an_policy) {
            case PM_AN_FORCE_DISABLE:
                bf_pm_pltfm_front_port_eligible_for_autoneg(
                    icfg->dev_id, &port_hdl, false);
                // since we are not interested in asic-serdes-tx, indicate RX is
                // ready for DFE
                bf_pm_port_serdes_rx_ready_for_bringup_set(
                    icfg->dev_id, &port_hdl, true);
                bf_pal_pm_front_port_ready_for_bringup(
                    icfg->dev_id, &port_hdl, true);
                break;
            case PM_AN_DEFAULT:
            case PM_AN_FORCE_ENABLE:
            default:
                bf_pm_pltfm_front_port_eligible_for_autoneg(
                    icfg->dev_id, &port_hdl, true);
                bf_pal_pm_front_port_ready_for_bringup(
                    icfg->dev_id, &port_hdl, true);
                break;
            }
            if (!bf_pltfm_pm_is_ha_mode()) {
                // skip media init states for passive copper
                next_st = PM_INTF_FSM_WAIT_LINK_ST;
            } else {
                // Wait for link-state notification from SDK for next action
                next_st = PM_INTF_FSM_HA_WAIT_LINK_ST;
            }
            intf->self_fsm_running = true;
            break;
        }

        // we should never get here
        break;
    case PM_INTF_FSM_WAIT_SERDES_TX_INIT:
        // Wait for the SERDES TX init to complete
        // This state is only used for optical cables
        bf_pm_port_direction_get (icfg->dev_id, &port_hdl, &dir_mode);

        if (dir_mode == PM_PORT_DIR_RX_ONLY) {
            asic_serdes_tx_ready = true;
        } else {
            bf_pm_port_serdes_tx_ready_get(
                icfg->dev_id, &port_hdl, &asic_serdes_tx_ready);
        }
        if (!asic_serdes_tx_ready) {  // keep waiting
            break;
        }

        module_chan =
            bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;

        // Tell the qsfp_ch_fsm that its ok to enable the lanes in this link.
        // Loop through all lanes since we only run the pm_intf_fsm
        // for head channels
        for (ch = 0; ch < icfg->intf_nlanes; ch++) {
            qsfp_fsm_ch_enable(icfg->dev_id, port_hdl.conn_id, module_chan + ch);
        }

        // Now that we've advanced the qsfp_ch_fsm, advance the pm_intf_fsm
        // to wait for qsfp init to complete
        intf->self_fsm_running = true;
        next_st = PM_INTF_FSM_WAIT_MEDIA_INIT;
        break;
    case PM_INTF_FSM_WAIT_MEDIA_INIT:
        // Media is present, in high power mode, and is initializing the data
        // path.
        // This state is only used for optical cables

        // poll the qsfp_ch_fsm to see when all lanes in the data path are
        // finished
        module_chan =
            bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;
        if (qsfp_fsm_get_enabled_mask(port_hdl.conn_id, module_chan) ==
                icfg->intf_chmask) {
            intf->self_fsm_running = true;
            intf->rxlos_debounce_cntr =
                bf_qsfp_rxlos_debounce_get(port_hdl.conn_id);
            next_st = PM_INTF_FSM_SET_RX_READY;
            next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOS; // by tsihang, 2025/07/28.
            break;
        }

        // keep waiting
        break;
    case PM_INTF_FSM_WAIT_LINK_ST:
        if (intf->link_state) {
            next_st = PM_INTF_FSM_LINK_UP;
        } else {
            next_st = PM_INTF_FSM_LINK_DOWN;
        }
        break;
    case PM_INTF_FSM_LINK_UP:
        // link is up
        // wait for media to be removed or port deletion or reconfig or los
        break;
    case PM_INTF_FSM_INCOMPATIBLE_MEDIA:
        // Media is incompatible with switch configuration, either because media
        // power requirements are too high or the media supported Applications
        // do not match the current switch configuration. Wait for media
        // to be replaced or configuration to be changed
        break;
    case PM_INTF_FSM_HA_WAIT_LINK_ST:
        // After warm-init end link could be up or down. Wait for nofitication
        // from SDK
        break;
    case PM_INTF_FSM_LINK_DOWN:
        if ((!bf_pltfm_pm_is_ha_mode()) && qsfp_info->is_present &&
                icfg->admin_up && qsfp_info->is_optic) {
            rx_los_flag = rx_lol_flag = true;
            module_chan =
                bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;
            qsfp_fsm_get_rx_los(
                port_hdl.conn_id, module_chan, icfg->intf_nlanes, &rx_los_flag);
            qsfp_fsm_get_rx_lol(
                port_hdl.conn_id, module_chan, icfg->intf_nlanes, &rx_lol_flag);
            if (rx_los_flag || rx_lol_flag) {
                bf_pm_port_serdes_rx_ready_for_bringup_set(
                    icfg->dev_id, &port_hdl, false);
                intf->self_fsm_running = true;
                intf->rxlos_debounce_cntr =
                    bf_qsfp_rxlos_debounce_get(port_hdl.conn_id);
                next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOS;
            }
        }
        break;
    case PM_INTF_FSM_HA_LINK_DOWN:
        next_st = PM_INTF_FSM_INIT;
        break;
    case PM_INTF_FSM_HA_LINK_UP:
        next_st = PM_INTF_FSM_LINK_UP;
        break;
    case PM_INTF_FSM_WAIT_MEDIA_RX_LOS:
        if ((!bf_pltfm_pm_is_ha_mode()) && qsfp_info->is_present) {
            rx_los_sup = false;
            rx_los_flag = true;
            module_chan =
                bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;

            qsfp_get_rx_los_support(port_hdl.conn_id, &rx_los_sup);
            if (!rx_los_sup) {
                intf->self_fsm_running = true;
                next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOL;
                break;
            }
            qsfp_fsm_get_rx_los(
                port_hdl.conn_id, module_chan, icfg->intf_nlanes, &rx_los_flag);
            if (rx_los_flag) {
                intf->rxlos_debounce_cntr =
                    bf_qsfp_rxlos_debounce_get(port_hdl.conn_id);
            } else if (intf->rxlos_debounce_cntr > 0) {
                intf->rxlos_debounce_cntr--;
            } else {
                next_st = PM_INTF_FSM_WAIT_MEDIA_RX_LOL;
            }
            delay_ms = 100;
            intf->self_fsm_running = true;
        }
        break;
    case PM_INTF_FSM_WAIT_MEDIA_RX_LOL:
        if ((!bf_pltfm_pm_is_ha_mode()) && qsfp_info->is_present) {
            rx_lol_sup = false;
            rx_lol_flag = true;
            module_chan =
                bf_bd_first_conn_ch_get(port_hdl.conn_id) + port_hdl.chnl_id;

            qsfp_get_rx_lol_support(port_hdl.conn_id, &rx_lol_sup);
            if (!rx_lol_sup) {
                intf->self_fsm_running = true;
                next_st = PM_INTF_FSM_SET_RX_READY;
                break;
            }
            qsfp_fsm_get_rx_lol(
                port_hdl.conn_id, module_chan, icfg->intf_nlanes, &rx_lol_flag);
            if (!rx_lol_flag) {
                delay_ms = 100;
                intf->self_fsm_running = true;
                next_st = PM_INTF_FSM_SET_RX_READY;
                break;
            }
        }
        break;
    case PM_INTF_FSM_SET_RX_READY:
        if ((!bf_pltfm_pm_is_ha_mode()) && qsfp_info->is_present) {
            /* If Rx not ready, bf_pm_fsm_run will keep in BF_PM_FSM_ST_WAIT_SIGNAL_OK state. */
            bf_pm_port_serdes_rx_ready_for_bringup_set(
                icfg->dev_id, &port_hdl, true);
            intf->self_fsm_running = true;
            next_st = PM_INTF_FSM_WAIT_LINK_ST;
        }
        break;
    default:
        break;
    }
    if (st != next_st) {
        bf_pm_intf_update_fsm_st(st, next_st, port_hdl.conn_id, port_hdl.chnl_id);
    }
    bf_pm_intf_fsm_next_run_time_set(&intf->next_run_time, delay_ms);
}

void bf_pm_icfg_dev_id_and_dev_port_get(uint32_t conn,
                                        uint32_t chan,
                                        bf_dev_id_t *dev_id,
                                        bf_dev_port_t *dev_port) {
    *dev_id = intf_obj[conn][chan].intf_cfg.dev_id;
    *dev_port = intf_obj[conn][chan].intf_cfg.dev_port;
}

void bf_pm_icfg_speed_and_nlanes_get(uint32_t conn,
                                     uint32_t chan,
                                     uint32_t *intf_speed,
                                     uint32_t *intf_nlanes) {
    *intf_speed = intf_obj[conn][chan].intf_cfg.intf_speed;
    *intf_nlanes = intf_obj[conn][chan].intf_cfg.intf_nlanes;
}

void bf_pm_qsfp_dd_is_dac_get(uint32_t conn, uint32_t chan, bool *is_dac) {
    *is_dac = (intf_obj[conn][chan].qsfp_info->is_optic == false);
}

void bf_pm_qsfp_dd_type_get(uint32_t conn, uint32_t chan, char *qsfp_dd_type) {
    *qsfp_dd_type = intf_obj[conn][chan].qsfp_info->qsfpdd_type;
}

// Manages the port bringup
// Runs every 100msec
void bf_pm_interface_fsm(void) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    unsigned long intf_key = 0;
    bf_pm_qsfp_info_t *qsfp_info;
    bf_pm_intf_mtx_t *mtx = NULL;
    struct timespec now;

    // This fsm is common to both qsfp and internal-ports.
    // Run through all the ports; ignores link-status
    for (int conn = 1; conn <= bf_pm_num_qsfp_get(); conn++) {
        for (int chan = 0; chan < MAX_CHAN_PER_CONNECTOR; chan++) {
            intf = &intf_obj[conn][chan];
            mtx = &intf_mtx[conn][chan];

            icfg = &intf->intf_cfg;
            if ((!icfg->admin_up) || (!intf->intf_added)) continue;
            if (!mtx->mtx_inited) continue;

            bf_sys_mutex_lock(&mtx->intf_mtx);
            qsfp_info = intf->qsfp_info;

            clock_gettime(CLOCK_MONOTONIC, &now);
            if ((!intf->skip_periodic) && (icfg->admin_up) && (intf->intf_added) &&
                    ((intf->next_run_time.tv_sec < now.tv_sec) ||
                     ((intf->next_run_time.tv_sec == now.tv_sec) &&
                      (intf->next_run_time.tv_nsec <= now.tv_nsec)))) {
                bf_pm_interface_fsm_run(icfg, intf, qsfp_info);
            }

            bf_sys_mutex_unlock(&mtx->intf_mtx);
        }
    }

    (void)intf_key;
}

// Manages the port bringup
// Runs every 100msec
void bf_pm_interface_sfp_fsm(void) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    unsigned long intf_key = 0;
    bf_pm_qsfp_info_t *qsfp_info;
    bf_pm_intf_mtx_t *mtx = NULL;
    struct timespec now;

    // This fsm is common to both qsfp and internal-ports.
    // Run through all the ports; ignores link-status
    for (int module = 1;
         module <= bf_pm_num_sfp_get(); module++) {
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];

        icfg = &intf->intf_cfg;
        if ((!icfg->admin_up) || (!intf->intf_added)) continue;
        if (!mtx->mtx_inited) continue;

        bf_sys_mutex_lock(&mtx->intf_mtx);
        qsfp_info = intf->qsfp_info;

        clock_gettime(CLOCK_MONOTONIC, &now);
        if ((!intf->skip_periodic) && (icfg->admin_up) && (intf->intf_added) &&
                ((intf->next_run_time.tv_sec < now.tv_sec) ||
                 ((intf->next_run_time.tv_sec == now.tv_sec) &&
                  (intf->next_run_time.tv_nsec <= now.tv_nsec)))) {
            bf_pm_interface_sfp_fsm_run(icfg, intf, qsfp_info);
        }

        bf_sys_mutex_unlock(&mtx->intf_mtx);
    }

    (void)intf_key;
}

void bf_pm_intf_init(void) {
    int port, ch;
    for (port = 0; port < bf_pm_num_qsfp_get(); port++) {
        for (ch = 0; ch < MAX_CHAN_PER_CONNECTOR; ch++) {
            pm_intf_fsm_st[port][ch] = PM_INTF_FSM_DISABLED;
        }
    }
#if defined(HAVE_SFP)
    for (port = 0; port < bf_pm_num_sfp_get(); port++) {
        pm_intf_sfp_fsm_st[port] = PM_INTF_FSM_DISABLED;
    }
#endif
}

void bf_pm_intf_add(bf_pltfm_port_info_t *port_info,
                    bf_dev_id_t dev_id,
                    bf_dev_port_t dev_port,
                    bf_pal_front_port_cb_cfg_t *port_cfg,
                    bf_pltfm_encoding_type_t encoding) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    bf_pm_intf_mtx_t *mtx = NULL;
    uint32_t conn, chan, curchan;
    int module;

    if ((!port_info) || (!port_cfg)) {
        return;
    }

    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
        memset(intf, 0, sizeof(bf_pm_intf_t));
        intf->qsfp_info = bf_pltfm_get_pm_sfp_info_ptr(module);
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
        memset(intf, 0, sizeof(bf_pm_intf_t));
        intf->qsfp_info = bf_pltfm_get_pm_qsfp_info_ptr(port_info->conn_id);
    }
    if (!mtx->mtx_inited) {
        bf_sys_mutex_init(&mtx->intf_mtx);
        mtx->mtx_inited = true;
    }
    bf_sys_mutex_lock(&mtx->intf_mtx);
    if (intf->intf_added) {
        // Catch duplicate
        LOG_ERROR(
            "Add failed. Port-already exist on conn %d chan %d \n", conn, chan);
        bf_sys_mutex_unlock(&mtx->intf_mtx);
        return;
    }

    icfg = &intf->intf_cfg;
    icfg->dev_id = dev_id;
    icfg->conn_id = port_info->conn_id;
    icfg->channel = port_info->chnl_id;
    icfg->intf_speed = port_cfg->speed_cfg;
    icfg->intf_nlanes = port_cfg->num_lanes;
    icfg->dev_port = dev_port;
    icfg->encoding = encoding;

    chan += bf_bd_first_conn_ch_get(port_info->conn_id);
    icfg->intf_chmask = 0;
    for (curchan = chan; curchan < (uint32_t)(chan + icfg->intf_nlanes);
            curchan++) {
        icfg->intf_chmask |= 1 << curchan;
    }

    qsfp_fsm_update_cfg(icfg->conn_id,
                        icfg->channel,
                        icfg->intf_speed,
                        icfg->intf_nlanes,
                        icfg->encoding);

    intf->intf_added = true;
    bf_sys_mutex_unlock(&mtx->intf_mtx);
}

void bf_pm_intf_del(bf_pltfm_port_info_t *port_info,
                    bf_pal_front_port_cb_cfg_t *port_cfg) {
    bf_pm_intf_t *intf = NULL;
    bf_pm_intf_mtx_t *mtx = NULL;
    bf_pm_intf_cfg_t *icfg = NULL;
    uint32_t conn, chan;
    int module;

    if (!port_info) {
        return;
    }
    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
    }

    if (!intf->intf_added) {
        // Catch duplicate
        LOG_ERROR(
            "Del failed. Port does not exist on conn %d chan %d \n", conn, chan);
        return;
    }

    bf_sys_mutex_lock(&mtx->intf_mtx);
    icfg = &intf->intf_cfg;
    icfg->admin_up = false;
    intf->intf_added = false;
    bf_sys_mutex_unlock(&mtx->intf_mtx);
    (void)port_cfg;
}

void bf_pm_intf_enable(bf_pltfm_port_info_t *port_info,
                       bf_pal_front_port_cb_cfg_t *port_cfg) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    bf_pal_front_port_handle_t port_hdl;
    bf_pm_intf_mtx_t *mtx = NULL;
    uint32_t conn, chan;
    int module;
    pm_intf_fsm_states_t fsm_state;

    if ((!port_info) || (!port_cfg)) {
        return;
    }

    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
        fsm_state = pm_intf_sfp_fsm_st[module];
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
        fsm_state = pm_intf_fsm_st[conn][chan];
    }
    if (!intf->intf_added) return;

    bf_sys_mutex_lock(&mtx->intf_mtx);
    icfg = &intf->intf_cfg;
    // Not part of port_cfg :(
    port_hdl.conn_id = icfg->conn_id;
    port_hdl.chnl_id = icfg->channel;
    bf_pm_port_autoneg_get(icfg->dev_id, &port_hdl, &icfg->an_policy);
    if (is_panel_sfp (conn, chan)) {
        bf_pm_intf_update_sfp_fsm_st(
            fsm_state, PM_INTF_FSM_INIT, module);
    } else {
        bf_pm_intf_update_fsm_st(
            fsm_state, PM_INTF_FSM_INIT, conn, chan);
    }
    bf_pm_intf_fsm_next_run_time_set(&intf->next_run_time, 0);
    icfg->admin_up = true;
    bf_sys_mutex_unlock(&mtx->intf_mtx);

    if (is_panel_sfp(conn, chan)) {

    } else {
        module = conn;
    }

    LOG_DEBUG("PM_INTF_FSM: %2d/%d : %cSFP %2d : admin_up %d : is_present %d : is_optic %d : qsfp_type %d : qsfpdd_type %d\n",
         icfg->conn_id, icfg->channel,
         is_panel_sfp(conn, chan) ? ' ' : 'Q',
         module,
         icfg->admin_up,
         intf->qsfp_info->is_present,
         intf->qsfp_info->is_optic,
         intf->qsfp_info->qsfp_type,
         intf->qsfp_info->qsfpdd_type);
}

void bf_pm_intf_disable(bf_pltfm_port_info_t *port_info,
                        bf_pal_front_port_cb_cfg_t *port_cfg) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    bf_pm_intf_mtx_t *mtx = NULL;
    // uint32_t conn, chan, curchan;
    uint32_t conn, chan;
    int module;
    bf_pm_qsfp_info_t *qsfp_info;
    pm_intf_fsm_states_t fsm_state;

    if ((!port_info) || (!port_cfg)) {
        return;
    }
    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
        fsm_state = pm_intf_sfp_fsm_st[module];
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
        fsm_state = pm_intf_fsm_st[conn][chan];
    }

    if (!intf->intf_added) return;
    bf_sys_mutex_lock(&mtx->intf_mtx);
    icfg = &intf->intf_cfg;
    icfg->admin_up = false;
    icfg->an_policy = 0;
    qsfp_info = intf->qsfp_info;
    bf_pm_handle_intf_disable(icfg, intf, qsfp_info);
    bf_pm_intf_update_fsm_st(
        fsm_state, PM_INTF_FSM_DISABLED, conn, chan);
    bf_sys_mutex_unlock(&mtx->intf_mtx);
}

// At the end of warm-init/fast-reconfig, this will be called to move
// the state.
void bf_pm_intf_ha_link_state_notiy(bf_pltfm_port_info_t *port_info,
                                    bool link_up) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;
    bf_pm_intf_mtx_t *mtx = NULL;
    uint32_t conn, chan;
    int module;
    pm_intf_fsm_states_t fsm_state;

    if (!port_info) {
        return;
    }

    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
        fsm_state = pm_intf_sfp_fsm_st[module];
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
        fsm_state = pm_intf_fsm_st[conn][chan];
    }
    if (!intf->intf_added) return;

    bf_sys_mutex_lock(&mtx->intf_mtx);
    icfg = &intf->intf_cfg;
    if (!icfg->admin_up) {
        LOG_DEBUG(
            "PM_INTF_FSM: %2d/%d : warm_init_end link-state notification on "
            "disabled "
            "port, ignore link %s state\n",
            conn,
            chan,
            link_up ? "UP" : "DOWN");
        return;
    }
    intf->link_state = link_up;
    LOG_DEBUG(
        "PM_INTF_FSM: %2d/%d : warm_init_end link-state notification : link is "
        "%s\n",
        conn,
        chan,
        link_up ? "UP" : "DOWN");

    if (link_up) {
        bf_pm_intf_update_fsm_st(
            fsm_state, PM_INTF_FSM_HA_LINK_UP, conn, chan);
    } else {
        bf_pm_intf_update_fsm_st(
            fsm_state, PM_INTF_FSM_HA_LINK_DOWN, conn, chan);
    }
    bf_sys_mutex_unlock(&mtx->intf_mtx);
}

void bf_pm_intf_set_link_state(bf_pltfm_port_info_t *port_info,
                               bool link_state) {
    bf_pm_intf_t *intf = NULL;
    bf_pm_intf_mtx_t *mtx = NULL;
    uint32_t conn, chan;
    int module;
    pm_intf_fsm_states_t fsm_state;

    if (!port_info) {
        return;
    }

    conn = port_info->conn_id;
    chan = port_info->chnl_id;

    if (is_panel_sfp (conn, chan)) {
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        intf = &intf_sfp_obj[module];
        mtx = &intf_sfp_mtx[module];
        fsm_state = pm_intf_sfp_fsm_st[module];
    } else {
        intf = &intf_obj[conn][chan];
        mtx = &intf_mtx[conn][chan];
        fsm_state = pm_intf_fsm_st[conn][chan];
    }

    bf_sys_mutex_lock(&mtx->intf_mtx);
    if (fsm_state != PM_INTF_FSM_HA_WAIT_LINK_ST) {
        intf->link_state = link_state;
        if (link_state) {
            bf_pm_intf_update_fsm_st(
                fsm_state, PM_INTF_FSM_LINK_UP, conn, chan);
        } else {
            bf_pm_intf_update_fsm_st(
                fsm_state, PM_INTF_FSM_LINK_DOWN, conn, chan);
        }
    }
    bf_sys_mutex_unlock(&mtx->intf_mtx);
}

bf_status_t bf_pm_intf_ha_wait_port_cfg_done(bf_dev_id_t dev_id) {
    bf_pm_intf_cfg_t *icfg = NULL;
    bf_pm_intf_t *intf = NULL;

    for (int conn = 1; conn <= bf_pm_num_qsfp_get(); conn++) {
        for (uint chan = 0; chan < MAX_CHAN_PER_CONNECTOR; chan++) {
            intf = &intf_obj[conn][chan];
            icfg = &intf->intf_cfg;
            if ((!icfg->admin_up) || (!intf->intf_added)) continue;
            // Wait till the port reaches PM_INTF_FSM_HA_WAIT_LINK_ST
            while (pm_intf_fsm_st[conn][chan] != PM_INTF_FSM_HA_WAIT_LINK_ST) {
                bf_sys_usleep(100);
            }
        }
    }

    for (int module = 0; module < bf_pm_num_sfp_get(); module++) {
        intf = &intf_sfp_obj[module];
        icfg = &intf->intf_cfg;
        if ((!icfg->admin_up) || (!intf->intf_added)) continue;
        // Wait till the port reaches PM_INTF_FSM_HA_WAIT_LINK_ST
        while (pm_intf_sfp_fsm_st[module] != PM_INTF_FSM_HA_WAIT_LINK_ST) {
            bf_sys_usleep(100);
        }
    }

    return BF_SUCCESS;
}
