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
#include <unistd.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_switchd/bf_switchd.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_pltfm.h>
#include <bf_pltfm_ext_phy.h>
#include <bf_pltfm_bd_cfg.h>

#include "bf_pm_priv.h"

bool devport_state_tx_mode_chk (bf_dev_id_t dev_id,
                    bf_pal_front_port_handle_t *port_hdl);
bool devport_state_loopback_mode_chk (bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl);

void devport_state_enabled_set(bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl, bool set);
bool devport_state_enabled_chk (bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl);
int devport_speed_to_led_color (bf_port_speed_t speed,
    bf_led_condition_t *led_cond);

#define PM_BIT_SET(val, bit_pos) ((val) |= (1 << (bit_pos)))
#define PM_BIT_CLR(val, bit_pos) ((val) &= ~(1 << (bit_pos)))
#define PM_BIT_GET(val, bit_pos) (((val) >> (bit_pos)) & 1)

#if defined(HAVE_SFP)
/* SFP FSM. by tsihang, 2021-07-16. */
#define MAX_SFP_PRES_VECTOR    2

#define SFP_SCAN_TMR_PERIOD_MS 2000
bf_sys_timer_t sfp_scan_timer;

#define SFP_FSM_TMR_PERIOD_MS 100
bf_sys_timer_t sfp_fsm_timer;

static int bf_pm_num_sfp = 0;

static bf_pm_sfp_info_t
pm_sfp_info_arr[BF_PLAT_MAX_QSFP + 1];

static uint32_t
sfp_pres_mask[MAX_SFP_PRES_VECTOR];

static uint32_t
sfp_quick_rmv_pres_mask[MAX_SFP_PRES_VECTOR];

static bool pm_sfp_quick_removed[BF_PLAT_MAX_SFP
                                 + 1];
#endif

#define QSFP_SCAN_TMR_PERIOD_MS 1500
bf_sys_timer_t qsfp_scan_timer;

#define QSFP_FSM_TMR_PERIOD_MS 100
bf_sys_timer_t qsfp_fsm_timer;

static bool pm_qsfp_quick_removed[BF_PLAT_MAX_QSFP
                                  + 1];

static bf_pm_qsfp_info_t
pm_qsfp_info_arr[BF_PLAT_MAX_QSFP + 1];

static uint32_t
qsfp_pres_mask[3];  // 0->lower mask, 1->upper mask 2->cpu port mask

static uint32_t
qsfp_quick_rmv_pres_mask[3];  // 0->lower mask, 1->upper mask
// 2->cpu port mask

static int bf_pm_num_qsfp = 0;
static int bf_pm_num_mac  = 0;

// bf_bd_cfg_bd_port_num_nlanes_get
int bf_pm_num_lanes_get(
    uint32_t conn_id)
{
    int nlanes = 4;

    if (platform_type_equal(AFN_X732QT)) {
        if (conn_id != 33)
            nlanes = 8;
    } else {
        // do nothing
    }
    return nlanes;
}

int bf_pm_num_qsfp_get (void)
{
    return bf_pm_num_qsfp;
}

static bool bf_pltfm_pm_ha_mode = false;

void qsfp_reset_pres_mask (void)
{
    qsfp_pres_mask[0] = 0xffffffff;
    qsfp_pres_mask[1] = 0xffffffff;
    qsfp_pres_mask[2] = 0xffffffff;
}

void bf_pm_qsfp_quick_removal_detected_set (
    uint32_t conn_id, bool flag)
{
    int mask_id = 2;  // cpu port
    if (conn_id <= 32) {
        mask_id = 1;
    } else if (conn_id <= 64) {
        mask_id = 0;
    }

    if (flag) {
        ((conn_id % 32) != 0)
            ? (PM_BIT_SET(qsfp_quick_rmv_pres_mask[mask_id], (conn_id % 32) - 1))
            : (PM_BIT_SET(qsfp_quick_rmv_pres_mask[mask_id], 31));
    } else {
        ((conn_id % 32) != 0)
            ? (PM_BIT_CLR(qsfp_quick_rmv_pres_mask[mask_id], (conn_id % 32) - 1))
            : (PM_BIT_CLR(qsfp_quick_rmv_pres_mask[mask_id], 31));
    }
    pm_qsfp_quick_removed[conn_id] = flag;
}

bool bf_pm_qsfp_quick_removal_detected_get (
    uint32_t conn_id)
{
    return pm_qsfp_quick_removed[conn_id];
}

static void qsfp_info_clear (uint32_t conn_id)
{
    pm_qsfp_info_arr[conn_id].is_present = false;
    pm_qsfp_info_arr[conn_id].qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    pm_qsfp_info_arr[conn_id].qsfpdd_type =
        BF_PLTFM_QSFPDD_UNKNOWN;
}

// populate qsfp_info_arr for CMIS modules
static void cmis_populate_qsfp_info_arr (
    int conn_id)
{
    if (bf_cmis_type_get (conn_id,
                      &pm_qsfp_info_arr[conn_id].qsfpdd_type) != 0) {
        LOG_ERROR ("PM_INTF QSFP    %2d : error getting QSFP type\n",
                 conn_id);
    }

    // Hw-init method. Now handled in the module fsm as host-init
    // bf_qsfp_set_transceiver_lpmode(conn_id, true);
    // bf_qsfp_reset(conn_id, true);
    // bf_qsfp_set_transceiver_lpmode(conn_id, false);
    // bf_qsfp_reset(conn_id, false);
}

// populate qsfp_info_arr for SFF-8636 modules
static void sff8636_populate_qsfp_info_arr (
    int conn_id)
{
    if (bf_qsfp_type_get (conn_id,
                          &pm_qsfp_info_arr[conn_id].qsfp_type) != 0) {
        LOG_ERROR ("PM_INTF QSFP    %2d : error getting QSFP type\n",
                   conn_id);
    }
}

static void qsfp_present_actions (int conn_id)
{
    uint8_t module_type = 0;

    if (conn_id > bf_pm_num_qsfp_get()) {
        LOG_ERROR ("PM_INTF QSFP    %2d : Invalid. Max supported = %2d",
                   conn_id,
                   bf_pm_num_qsfp_get());
        return;
    }

    bf_qsfp_get_module_type (conn_id, &module_type);
    if (module_type == 0) {
        LOG_ERROR ("PM_INTF QSFP    %2d : Unknown module detected (id=%d)\n",
                   conn_id,
                   module_type);
        return;
    }

    if (bf_qsfp_is_cmis (conn_id)) {
        cmis_populate_qsfp_info_arr (conn_id);
    } else {
        sff8636_populate_qsfp_info_arr (conn_id);
    }
    pm_qsfp_info_arr[conn_id].is_optic =
        bf_qsfp_is_optical (conn_id);

    if (bf_qsfp_is_fsm_logging(conn_id)) {
        LOG_DEBUG ("PM_INTF QSFP    %2d : %s module : qsfp_type %d : qsfpdd_type %d",
               conn_id,
               pm_qsfp_info_arr[conn_id].is_optic ? "Optical" :
               "Copper",
               pm_qsfp_info_arr[conn_id].qsfp_type,
               pm_qsfp_info_arr[conn_id].qsfpdd_type);
    }

    // mark present
    pm_qsfp_info_arr[conn_id].is_present = true;
}

static void qsfp_all_info_read (int conn_id)
{
    // Get the type of the QSFP connector
    if (bf_qsfp_type_get (conn_id,
                          &pm_qsfp_info_arr[conn_id].qsfp_type) != 0) {
        LOG_ERROR ("Port   %2d : error getting QSFP type\n",
                   conn_id);
    }
}

static bool autoneg_to_apply (uint32_t conn,
                              uint32_t chnl)
{
    if ((pm_qsfp_info_arr[conn].qsfp_type ==
         BF_PLTFM_QSFP_CU_0_5_M) ||
        (pm_qsfp_info_arr[conn].qsfp_type ==
         BF_PLTFM_QSFP_CU_1_M) ||
        (pm_qsfp_info_arr[conn].qsfp_type ==
         BF_PLTFM_QSFP_CU_2_M) ||
        (pm_qsfp_info_arr[conn].qsfp_type ==
         BF_PLTFM_QSFP_CU_3_M)) {
        return true;
    }
    return false;
}

static void qsfp_detection_actions (
    bf_dev_id_t dev_id, int conn_id)
{
    int chnl_id;
    bf_status_t sts;
    bf_pal_front_port_handle_t port_hdl;
    bool an_elig = false;
    bool is_subport_cfg_lp_mode = false, is_subport_cfg_force_tx_mode = false;

    if (!bf_pm_intf_is_device_family_tofino (
            dev_id)) {
        return;
    }

    qsfp_all_info_read (conn_id);

    bf_pltfm_platform_ext_phy_config_set (
        dev_id, conn_id,
        pm_qsfp_info_arr[conn_id].qsfp_type);
    port_hdl.conn_id = conn_id;
    for (chnl_id = 0; chnl_id < bf_pm_num_lanes_get(conn_id);
         chnl_id++) {
        port_hdl.chnl_id = chnl_id;
        an_elig = autoneg_to_apply (conn_id, chnl_id);
        sts = bf_pal_pm_front_port_eligible_for_autoneg (
                  dev_id, &port_hdl, an_elig);
        if (sts != BF_SUCCESS) {
            LOG_ERROR (
                "Unable to mark port eligible for AN for dev : %d : front port : "
                "%d/%d : %s (%d)",
                dev_id,
                port_hdl.conn_id,
                port_hdl.chnl_id,
                bf_err_str (sts),
                sts);
        } else {
            if (bf_qsfp_is_fsm_logging(conn_id)) {
                LOG_DEBUG ("PM_INTF QSFP    %2d/%d : mark port eligible_%s for AN",
                        port_hdl.conn_id,
                        port_hdl.chnl_id,
                        an_elig ? "true" : "false");
            }
        }

        sts = bf_pal_pm_front_port_ready_for_bringup (
                  dev_id, &port_hdl, true);
        if (sts != BF_SUCCESS) {
            LOG_ERROR (
                "Unable to mark port ready for bringup for dev : %d : front port : "
                "%d/%d : %s (%d)",
                dev_id,
                port_hdl.conn_id,
                port_hdl.chnl_id,
                bf_err_str (sts),
                sts);
        }
    }

    bool kick_off_ch_fsm = false;
    port_hdl.conn_id = conn_id;
    for (chnl_id = 0; chnl_id < bf_pm_num_lanes_get(conn_id);
         chnl_id++) {
        port_hdl.chnl_id = chnl_id;
        is_subport_cfg_lp_mode = devport_state_loopback_mode_chk (dev_id, &port_hdl);
        is_subport_cfg_force_tx_mode = devport_state_tx_mode_chk (dev_id, &port_hdl);
        if ((is_subport_cfg_lp_mode || is_subport_cfg_force_tx_mode) &&
            devport_state_enabled_chk (dev_id, &port_hdl)) {
            kick_off_ch_fsm = true;
        }
    }

    if (kick_off_ch_fsm) {
        port_hdl.conn_id = conn_id;
        for (chnl_id = 0; chnl_id < bf_pm_num_lanes_get(conn_id);
             chnl_id++) {
            port_hdl.chnl_id = chnl_id;
            /* Kick off channel FSM  to enable opt Tx.
             * by Hang Tsi, 2024/02/04. */
            qsfp_fsm_ch_enable (dev_id, port_hdl.conn_id, port_hdl.chnl_id);
        }
    }
}

static void qsfp_removal_actions (bf_dev_id_t
                                  dev_id, int conn_id)
{
    int chnl_id;
    bf_status_t sts;
    bf_pal_front_port_handle_t port_hdl;
    bf_port_speed_t speed = BF_SPEED_NONE, max_speed = BF_SPEED_NONE;
    bool is_subport_cfg_lp_mode = false, is_subport_cfg_force_tx_mode = false;
    bf_led_condition_t led_cond = BF_LED_POST_PORT_DEL;

    qsfp_info_clear (conn_id);

    // if (!bf_pm_intf_is_device_family_tofino(dev_id)) {
    //     return;
    // }

    port_hdl.conn_id = conn_id;
    for (chnl_id = 0; chnl_id < bf_pm_num_lanes_get(conn_id);
         chnl_id++) {
        port_hdl.chnl_id = chnl_id;
        /* At least one of subports. */
        if (devport_state_loopback_mode_chk (dev_id, &port_hdl)) {
            is_subport_cfg_lp_mode = true;
            /* Get Max speeed to choose a suitable led color. */
            if (!bf_pm_port_speed_get (dev_id, &port_hdl, &max_speed)) {
                speed = (speed > max_speed) ? speed : max_speed;
            }
            goto  skip_bring_port_dwn;
        }
        if (devport_state_tx_mode_chk (dev_id, &port_hdl)) {
            is_subport_cfg_force_tx_mode = true;
            goto  skip_bring_port_dwn;
        }
        sts = bf_pal_pm_front_port_eligible_for_autoneg (
                  dev_id, &port_hdl, false);
        if (sts != BF_SUCCESS) {
            LOG_ERROR (
                "Unable to mark port eligible for AN for dev : %d : front port : "
                "%d/%d : %s (%d)",
                dev_id,
                port_hdl.conn_id,
                port_hdl.chnl_id,
                bf_err_str (sts),
                sts);
        }
        qsfp_fsm_ch_notify_not_ready (dev_id,
                                      port_hdl.conn_id, port_hdl.chnl_id);

        sts = bf_pal_pm_front_port_ready_for_bringup (
                  dev_id, &port_hdl, false);
        if (sts != BF_SUCCESS) {
            LOG_ERROR (
                "Unable to mark port ready for bringup for dev : %d : front port : "
                "%d/%d : %s (%d)",
                dev_id,
                port_hdl.conn_id,
                port_hdl.chnl_id,
                bf_err_str (sts),
                sts);
        }
    }

skip_bring_port_dwn:
    fprintf (stdout, "QSFP    %2d : removed\n",
             port_hdl.conn_id);

    /* Loopback/ForceTx with removal action. */
    if (is_subport_cfg_lp_mode || is_subport_cfg_force_tx_mode) {
        return;
    }

    /* No breakout LED, so treat port_hdl.chnl_id = 0. */
    bf_pltfm_port_info_t port_info;
    port_info.conn_id = port_hdl.conn_id;
    port_info.chnl_id = 0; /* this is QSFP. */
    sts = bf_port_led_set (dev_id, &port_info,
                           led_cond);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_DEBUG (
            "Unable to set led on port removal event for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_info.conn_id,
            port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
    }

    (void)dev_id;
}

void qsfp_scan_removed (bf_dev_id_t dev_id,
                        int conn_id)
{
    pm_qsfp_info_arr[conn_id].is_present = false;
    qsfp_removal_actions (dev_id, conn_id);
    qsfp_fsm_removed (conn_id);
}

/* edit by tsihang, 2019.8.1 */
static bf_pltfm_status_t qsfp_scan_helper (
    bf_dev_id_t dev_id,
    int conn_id,
    uint32_t mask,
    int mask_id)
{
    uint8_t res_bit;
    uint32_t res_mask;
    bool is_present = false;

    res_mask = qsfp_pres_mask[mask_id] ^ mask;

    if ((res_mask != 0) && ((res_mask & mask) != res_mask)) {
        /* If any transceiver pluged, wait for the transceiver, 100ms is theoretical. */
        bf_sys_usleep (200000);
    }

    while (res_mask != 0) {
        // Indicates that there is change in the number of the plugged in qsfps
        // Probe each bit to find the exact id of the qsfp
        res_bit = res_mask & 0x01;
        while (res_bit == 0) {
            res_mask = res_mask >> 1;
            res_bit = res_mask & 0x01;

            // mask = mask >> 1;
            conn_id++;
        }
        res_mask = res_mask >> 1;

#if 0
        if (!bf_pltfm_pm_is_ha_mode()) {
            // Handle any previous removal found via fsm, if any.
            if (bf_pm_qsfp_quick_removal_detected_get (
                    conn_id)) {
                goto handle_removal;
            }

            // Actual reset sequence etc per spec are handled via qsfp-fsm.
            // Just toggle reset so that we can read the module
            if (bf_qsfp_get_reset (conn_id)) {
                int rc;

                LOG_DEBUG ("PM_INTF QSFP    %2d : RESETL = true",
                           conn_id);
                // assert resetL
                rc = bf_qsfp_reset (conn_id, true);
                if (rc != 0) {
                    LOG_ERROR (
                        "PM_INTF QSFP    %2d : Error <%d> asserting resetL",
                        conn_id, rc);
                }

                bf_sys_usleep (3); // really 2 micro-seconds

                LOG_DEBUG ("PM_INTF QSFP    %2d : RESETL = false",
                           conn_id);
                // de-assert resetL
                rc = bf_qsfp_reset (conn_id, false);
                if (rc != 0) {
                    LOG_ERROR ("PM_INTF QSFP    %2d : Error <%d> de-asserting resetL",
                               conn_id,
                               rc);
                }
                // We need 2-seconds for module to be ready, hence we continue.
                // In case, module is not ready, bf_qsfp_detect_transceiver will
                // retry.

                conn_id++;
                continue;  // back to outer while loop
            }
        }
#endif
        bool qsfp_curr_st_abs = ((conn_id % 32) != 0)
                                ? (PM_BIT_GET(mask, (conn_id % 32) - 1))
                                : (PM_BIT_GET(mask, 31));
        bool qsfp_prev_st_abs =
            ((conn_id % 32) != 0)
            ? (PM_BIT_GET(qsfp_pres_mask[mask_id], (conn_id % 32) - 1))
            : (PM_BIT_GET(qsfp_pres_mask[mask_id], 31));
        if (bf_qsfp_is_fsm_logging(conn_id)) {
            LOG_DEBUG ("PM_INTF QSFP    %2d : curr-pres-st : %d prev-pres-st : %d",
                   conn_id,
                   qsfp_curr_st_abs,
                   qsfp_prev_st_abs);
        }
        if (qsfp_curr_st_abs) {
            if (!qsfp_prev_st_abs) {
                LOG_DEBUG ("PM_INTF QSFP    %2d : unplugged (from plug st)",
                           conn_id);
                is_present = false;
                // hack to clear the states.
                bf_qsfp_set_present (conn_id, is_present);
                goto handle_removal;
            }
            // we should never land here. But fall through and handle as done
            // previously
            LOG_DEBUG ("PM_INTF QSFP    %2d : unplugged (from unplugged st)",
                       conn_id);
        }

        int detect_st = bf_qsfp_detect_transceiver (
                            conn_id, &is_present);
        if (bf_qsfp_is_fsm_logging(conn_id)) {
            LOG_DEBUG ("PM_INTF QSFP    %2d : detect-st : %d is-present : %d",
                   conn_id,
                   detect_st,
                   is_present);
        }

        // Find if the said qsfp module was removed or added
        if (detect_st) {
            // hopefully, detect it in the next iteration
            conn_id++;
            continue;  // back to outer while loop
        } else {
handle_removal:
            if (bf_pm_qsfp_quick_removal_detected_get (
                    conn_id)) {
                // over-ride present bit so that we go through clean state in next cycle
                if (is_present) {
                    LOG_DEBUG (
                        "PM_INTF QSFP    %2d : Latched removal conditon detected. Doing "
                        "removal "
                        "actions.",
                        conn_id);
                    is_present = false;
                }
                bf_pm_qsfp_quick_removal_detected_set (conn_id,
                                                       false);
            }
            // update qsfp_pres_mask after successfully detecting the module
            if (is_present) {
                ((conn_id % 32) != 0)
                            ? (PM_BIT_CLR(qsfp_pres_mask[mask_id], (conn_id % 32) - 1))
                            : (PM_BIT_CLR(qsfp_pres_mask[mask_id], 31));
            } else {
                ((conn_id % 32) != 0)
                    ? (PM_BIT_SET(qsfp_pres_mask[mask_id], (conn_id % 32) - 1))
                    : (PM_BIT_SET(qsfp_pres_mask[mask_id], 31));
            }
        }
        if (is_present == true) {
            bf_qsfp_set_detected (conn_id, false);
            if (!bf_pm_intf_is_device_family_tofino (
                    dev_id)) {
                // determines qsfp type only
                qsfp_present_actions (conn_id);
            }
            // kick off the module FSM
            if (!bf_pltfm_pm_is_ha_mode()) {
                qsfp_fsm_inserted (conn_id);
            } else {
                qsfp_state_ha_config_set (dev_id, conn_id);
            }
        } else {
            bf_qsfp_set_detected (conn_id, false);
            // Update the cached status of the qsfp
            if (!bf_pltfm_pm_is_ha_mode()) {
                qsfp_scan_removed (dev_id, conn_id);
            } else {
                qsfp_state_ha_config_delete (conn_id);
            }
        }

        conn_id++;
    }  // Outside while
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t qsfp_scan (bf_dev_id_t dev_id)
{
    uint32_t lower_mask, upper_mask, cpu_mask;
    uint32_t lower_mask_prev, upper_mask_prev,
             cpu_mask_prev;
    bf_pltfm_status_t sts;
    upper_mask_prev = upper_mask = qsfp_pres_mask[0];
    lower_mask_prev = lower_mask = qsfp_pres_mask[1];
    cpu_mask_prev = cpu_mask = qsfp_pres_mask[2];

    // Query the QSFP library for the qsfp presence mask
    if (bf_qsfp_get_transceiver_pres (&lower_mask,
                                      &upper_mask, &cpu_mask) != 0) {
        return BF_PLTFM_COMM_FAILED;
    }

    // Handle quick unplug and plug
    // Cross check with latched state and over-ride the mask
    if (lower_mask_prev !=
        qsfp_quick_rmv_pres_mask[1]) {
        LOG_DEBUG ("pres-lower-mask changed: 0x%0x -> 0x%0x",
                   lower_mask_prev,
                   qsfp_quick_rmv_pres_mask[1]);
        lower_mask = qsfp_quick_rmv_pres_mask[1];
    }

    if (upper_mask_prev !=
        qsfp_quick_rmv_pres_mask[0]) {
        LOG_DEBUG ("pres-upper-mask changed: 0x%0x -> 0x%0x",
                   upper_mask_prev,
                   qsfp_quick_rmv_pres_mask[0]);
        upper_mask = qsfp_quick_rmv_pres_mask[0];
    }

    if (cpu_mask_prev !=
        qsfp_quick_rmv_pres_mask[2]) {
        LOG_DEBUG ("pres-cpu-mask changed: 0x%0x -> 0x%0x",
                   cpu_mask_prev,
                   qsfp_quick_rmv_pres_mask[2]);
        cpu_mask = qsfp_quick_rmv_pres_mask[2];
    }

    if (platform_type_equal (AFN_X564PT)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 33, upper_mask,
                                0);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (33-64)  at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 65, cpu_mask, 2);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFP (65) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 33, cpu_mask, 2);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFP (33) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    }  else if (platform_type_equal (AFN_X732QT)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 33, cpu_mask, 2);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFP (33) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X312PT)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 33, cpu_mask, 2);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFP (33) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        sts = qsfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = qsfp_scan_helper (dev_id, 33, cpu_mask, 2);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the QSFP (33) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    }

    qsfp_quick_rmv_pres_mask[0] = qsfp_pres_mask[0];
    qsfp_quick_rmv_pres_mask[1] = qsfp_pres_mask[1];
    qsfp_quick_rmv_pres_mask[2] = qsfp_pres_mask[2];

    return BF_PLTFM_SUCCESS;
}

void qsfp_scan_timer_cb (struct bf_sys_timer_s
                         *timer, void *data)
{
    bf_pltfm_status_t sts;
    bf_dev_id_t dev_id = (bf_dev_id_t) (intptr_t)data;

    static int dumped = 0;
    if (!dumped) {
        LOG_DEBUG ("QSFP SCAN timer started..");
        dumped = 1;
    }
    // printf("QSFP SCAN timer off\n");
    // Scan the insertion and detection of QSFPs
    sts = qsfp_scan (dev_id);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: in scanning qsfps at %s:%d\n",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
    }
    (void)timer;
}

void qsfp_fsm_timer_cb (struct bf_sys_timer_s
                        *timer, void *data)
{
    bf_pltfm_status_t sts;
    bf_dev_id_t dev_id = (bf_dev_id_t) (intptr_t)data;

    static int dumped = 0;
    if (!dumped) {
        LOG_DEBUG ("QSFP FSM timer started..");
        dumped = 1;
    }
    // printf("QSFP FSM timer off\n");
    // Process QSFP start-up actions (if necessary)
    sts = qsfp_fsm (dev_id);
    //bf_pm_interface_fsm();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: in qsfp fsm at %s:%d\n",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
    }

    /* For x732q_t. */
    if (!bf_pm_intf_is_device_family_tofino(dev_id)) {
        bf_pm_interface_fsm();
    }

    (void)timer;
}

/* A port which has 3 type: internal, sfp, qsfp.
 * by tsihang, 2021-07-18.
 */
bf_status_t bf_pltfm_pm_media_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_media_type_t *media_type)
{
    /* bf_pm_num_mac is equle the number of MAC. No matter SFP or QSFP,
     * this branch can be used for two of them. So I keep it here.
     * by tsihang, 2021-07-18.
     */
    if ((int)port_info->conn_id > bf_pm_num_mac) {
        *media_type = BF_MEDIA_TYPE_UNKNOWN;
        return BF_PLTFM_INVALID_ARG;
    }

    /* The flag of an internal port is hold by the struct of bd_map_ent_t.
     * So I keep it here.
     * by tsihang, 2021-07-18.
     */
    if (bf_bd_is_this_port_internal (
            port_info->conn_id, port_info->chnl_id)) {
        // Indicates that it is an internal port and hence media type is unknown
        *media_type = BF_MEDIA_TYPE_UNKNOWN;
        return BF_PLTFM_SUCCESS;
    }

#if defined(HAVE_SFP)
    /* Check the (SFP) its own media type.
     * by tsihang, 2021-07-18. */
    if (is_panel_sfp (port_info->conn_id,
                      port_info->chnl_id)) {
        int module = 0;
        bf_sfp_get_port (port_info->conn_id,
                         port_info->chnl_id, &module);
        if (!pm_sfp_info_arr[module].is_present) {
            /*Indicates that no SFP is inserted in the port. Hence the media type is
              unknown*/
            *media_type = BF_MEDIA_TYPE_UNKNOWN;
            return BF_PLTFM_SUCCESS;
        }
        if (bf_sfp_is_optical (module)) {
            *media_type = BF_MEDIA_TYPE_OPTICAL;
        } else {
            *media_type = BF_MEDIA_TYPE_COPPER;
        }
        return BF_PLTFM_SUCCESS;
    } else
#endif
    {
        if (!pm_qsfp_info_arr[port_info->conn_id].is_present) {
            /*Indicates that no QSFP is inserted in the port. Hence the media type is
              unknown*/
            *media_type = BF_MEDIA_TYPE_UNKNOWN;
            return BF_PLTFM_SUCCESS;
        }

        if (bf_qsfp_is_optical (port_info->conn_id)) {
            *media_type = BF_MEDIA_TYPE_OPTICAL;
        } else {
            *media_type = BF_MEDIA_TYPE_COPPER;
        }
    }
    return BF_PLTFM_SUCCESS;
}

#if defined(HAVE_SFP)

void sfp_reset_pres_mask (void)
{
    sfp_pres_mask[0] = 0xffffffff;
    sfp_pres_mask[1] = 0xffffffff;
}

static void sfp_all_info_read (int module)
{
    // Get the type of the QSFP connector
    if (bf_sfp_type_get (module,
                         &pm_sfp_info_arr[module].qsfp_type) != 0) {
        LOG_ERROR ("Port   %2d : error getting SFP type\n",
                   module);
    }
}

static bool sfp_autoneg_to_apply (int module)
{
    if ((pm_sfp_info_arr[module].qsfp_type ==
         BF_PLTFM_QSFP_CU_0_5_M) ||
        (pm_sfp_info_arr[module].qsfp_type ==
         BF_PLTFM_QSFP_CU_1_M) ||
        (pm_sfp_info_arr[module].qsfp_type ==
         BF_PLTFM_QSFP_CU_2_M) ||
        (pm_sfp_info_arr[module].qsfp_type ==
         BF_PLTFM_QSFP_CU_3_M)) {
        return true;
    }
    return false;
}

int bf_pm_num_sfp_get (void)
{
    return bf_pm_num_sfp;
}

static void sfp_info_clear (uint32_t module)
{
    pm_sfp_info_arr[module].is_present = false;
    pm_sfp_info_arr[module].qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    //pm_sfp_info_arr[module].qsfpdd_type =
    //    BF_PLTFM_QSFPDD_UNKNOWN;
}
static void sff8472_populate_sfp_info_arr (
    int module)
{
    if (bf_sfp_type_get (module,
                         &pm_sfp_info_arr[module].qsfp_type) != 0) {
        LOG_ERROR ("Port   %2d : error getting SFP type\n",
                   module);
    }
}

static void sfp_removal_actions (bf_dev_id_t
                                 dev_id, int module)
{
    bf_status_t sts;
    bf_pal_front_port_handle_t port_hdl;
    bf_port_speed_t speed = BF_SPEED_NONE, max_speed = BF_SPEED_NONE;
    bool is_subport_cfg_lp_mode = false, is_subport_cfg_force_tx_mode = false;
    bf_led_condition_t led_cond = BF_LED_POST_PORT_DEL;

    sfp_info_clear (module);

    /* get conn_id and chnl_id by module. */
    bf_sfp_get_conn (module, &port_hdl.conn_id,
                     &port_hdl.chnl_id);
    is_subport_cfg_lp_mode = devport_state_loopback_mode_chk (dev_id, &port_hdl);
    if (is_subport_cfg_lp_mode) {
        /* Get Max speeed to choose a suitable led color. */
        if (!bf_pm_port_speed_get (dev_id, &port_hdl, &max_speed)) {
            speed = (speed > max_speed) ? speed : max_speed;
        }
        goto  skip_bring_port_dwn;
    }
    is_subport_cfg_force_tx_mode = devport_state_tx_mode_chk (dev_id, &port_hdl);
    if (is_subport_cfg_force_tx_mode) {
       goto  skip_bring_port_dwn;
    }

    sts = bf_pal_pm_front_port_eligible_for_autoneg (
              dev_id, &port_hdl, false);
    if (sts != BF_SUCCESS) {
        LOG_ERROR (
            "Unable to mark port eligible for AN for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_hdl.conn_id,
            port_hdl.chnl_id,
            bf_err_str (sts),
            sts);
    }
    sfp_fsm_notify_not_ready (dev_id, module);
    sts = bf_pal_pm_front_port_ready_for_bringup (
              dev_id, &port_hdl, false);
    if (sts != BF_SUCCESS) {
        LOG_ERROR (
            "Unable to mark port ready for bringup for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_hdl.conn_id,
            port_hdl.chnl_id,
            bf_err_str (sts),
            sts);
    }

skip_bring_port_dwn:
    fprintf (stdout,
             " SFP    %2d : %2d/%d : removed\n",
             module,
             port_hdl.conn_id,
             port_hdl.chnl_id);

    if (is_subport_cfg_lp_mode || is_subport_cfg_force_tx_mode) {
        return;
    }

    bf_pltfm_port_info_t port_info;
    port_info.conn_id = port_hdl.conn_id;
    port_info.chnl_id = port_hdl.chnl_id;
    bf_port_led_set (dev_id, &port_info,
                     led_cond);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_DEBUG (
            "Unable to set led on port removal event for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_info.conn_id,
            port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
    }

    (void)dev_id;
}


void sfp_scan_removed (bf_dev_id_t dev_id,
                       int module)
{
    pm_sfp_info_arr[module].is_present = false;
    sfp_removal_actions (dev_id, module);
    sfp_fsm_removed (module);
}

static void sfp_present_actions (int module)
{
    if (module > bf_pm_num_sfp_get()) {
        LOG_ERROR ("PM_INTF  SFP    %2d : Invalid. Max supported = %2d",
                   module,
                   bf_pm_num_sfp_get());
        return;
    }

    sff8472_populate_sfp_info_arr (module);

    pm_sfp_info_arr[module].is_optic =
        bf_sfp_is_optical (module);

    if (bf_sfp_is_fsm_logging (module)) {
        LOG_DEBUG ("PM_INTF  SFP    %2d : contains %s module",
               module,
               pm_qsfp_info_arr[module].is_optic ? "Optical" :
               "Copper");
    }
    // mark present
    pm_sfp_info_arr[module].is_present = true;
}

static void sfp_detection_actions (
    bf_dev_id_t dev_id, int module)
{
    bf_status_t sts;
    bf_pal_front_port_handle_t port_hdl;
    bool an_elig = false;
    bool is_subport_cfg_lp_mode = false, is_subport_cfg_force_tx_mode = false;

    sfp_all_info_read (module);

    bf_sfp_get_conn (module, &port_hdl.conn_id,
                     &port_hdl.chnl_id);

    an_elig = sfp_autoneg_to_apply (module);
    sts = bf_pal_pm_front_port_eligible_for_autoneg (
              dev_id, &port_hdl, an_elig);
    if (sts != BF_SUCCESS) {
        LOG_ERROR (
            "Unable to mark port eligible for AN for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_hdl.conn_id,
            port_hdl.chnl_id,
            bf_err_str (sts),
            sts);
    }

    sts = bf_pal_pm_front_port_ready_for_bringup (
              dev_id, &port_hdl, true);
    if (sts != BF_SUCCESS) {
        LOG_ERROR (
            "Unable to mark port ready for bringup for dev : %d : front port : "
            "%d/%d : %s (%d)",
            dev_id,
            port_hdl.conn_id,
            port_hdl.chnl_id,
            bf_err_str (sts),
            sts);
    }

    is_subport_cfg_lp_mode = devport_state_loopback_mode_chk (dev_id, &port_hdl);
    is_subport_cfg_force_tx_mode = devport_state_tx_mode_chk (dev_id, &port_hdl);
    if ((is_subport_cfg_lp_mode || is_subport_cfg_force_tx_mode) &&
        devport_state_enabled_chk (dev_id, &port_hdl)) {
        /* Kick off channel FSM  to enable opt Tx.
         * by Hang Tsi, 2024/02/04. */
        sfp_fsm_st_enable (dev_id, module);
    }
}

void bf_pm_sfp_quick_removal_detected_set (
    uint32_t module, bool flag)
{
    int mask_id = 0;
    if (module <= 32) {
        mask_id = 1;
    }

    if (flag) {
        ((module % 32) != 0)
            ? (PM_BIT_SET(sfp_quick_rmv_pres_mask[mask_id], (module % 32) - 1))
            : (PM_BIT_SET(sfp_quick_rmv_pres_mask[mask_id], 31));
    } else {
        ((module % 32) != 0)
            ? (PM_BIT_CLR(sfp_quick_rmv_pres_mask[mask_id], (module % 32) - 1))
            : (PM_BIT_CLR(sfp_quick_rmv_pres_mask[mask_id], 31));
    }
    pm_sfp_quick_removed[module] = flag;
}

bool bf_pm_sfp_quick_removal_detected_get (
    uint32_t module)
{
    return pm_sfp_quick_removed[module];
}

static bf_pltfm_status_t sfp_scan_helper (
    bf_dev_id_t dev_id,
    int module,
    uint32_t mask,
    int mask_id)
{
    uint8_t res_bit;
    uint32_t res_mask;
    bool is_present = false;

    res_mask = sfp_pres_mask[mask_id] ^ mask;

    if ((res_mask != 0) && ((res_mask & mask) != res_mask)) {
        /* If any transceiver pluged, wait for the transceiver, 100ms is theoretical. */
        bf_sys_usleep (200000);
    }

    while (res_mask != 0) {
        // Indicates that there is change in the number of the plugged in qsfps
        // Probe each bit to find the exact id of the qsfp
        res_bit = res_mask & 0x01;
        while (res_bit == 0) {
            res_mask = res_mask >> 1;
            res_bit = res_mask & 0x01;

            // mask = mask >> 1;
            module++;
        }
        res_mask = res_mask >> 1;

        if (!bf_pltfm_pm_is_ha_mode()) {
            // Handle any previous removal found via fsm, if any.
            if (bf_pm_sfp_quick_removal_detected_get (
                    module)) {
                goto handle_removal;
            }

            // Actual reset sequence etc per spec are handled via qsfp-fsm.
            // Just toggle reset so that we can read the module
            if (bf_sfp_get_reset (module)) {
                int rc;

                LOG_DEBUG ("PM_INTF  SFP    %2d : RESETL = true",
                           module);
                // assert resetL
                rc = bf_sfp_reset (module, true);
                if (rc != 0) {
                    LOG_ERROR ("PM_INTF  SFP    %2d : Error <%d> asserting resetL",
                               module, rc);
                }

                bf_sys_usleep (3); // really 2 micro-seconds

                LOG_DEBUG ("PM_INTF  SFP    %2d : RESETL = false",
                           module);
                // de-assert resetL
                rc = bf_sfp_reset (module, false);
                if (rc != 0) {
                    LOG_ERROR (
                        "PM_INTF  SFP    %2d : Error <%d> de-asserting resetL",
                        module, rc);
                }
                // We need 2-seconds for module to be ready, hence we continue.
                // In case, module is not ready, bf_qsfp_detect_transceiver will
                // retry.

                module++;
                continue;  // back to outer while loop
            }
        }

        bool sfp_curr_st_abs = ((module % 32) != 0)
                                ? (PM_BIT_GET(mask, (module % 32) - 1))
                                : (PM_BIT_GET(mask, 31));
        bool sfp_prev_st_abs =
            ((module % 32) != 0)
            ? (PM_BIT_GET(sfp_pres_mask[mask_id], (module % 32) - 1))
            : (PM_BIT_GET(sfp_pres_mask[mask_id], 31));
        if (bf_sfp_is_fsm_logging(module)) {
            LOG_DEBUG ("PM_INTF  SFP    %2d : curr-pres-st : %d prev-pres-st : %d",
                   module,
                   sfp_curr_st_abs,
                   sfp_prev_st_abs);
        }
        if (sfp_curr_st_abs) {
            if (!sfp_prev_st_abs) {
                LOG_DEBUG ("PM_INTF  SFP    %2d : unplugged (from plug st)",
                           module);
                is_present = false;
                // hack to clear the states.
                bf_sfp_set_present (module, is_present);
                goto handle_removal;
            }
            // we should never land here. But fall through and handle as done
            // previously
            LOG_DEBUG ("PM_INTF  SFP    %2d : unplugged (from unplugged st)",
                       module);
        }

        int detect_st = bf_sfp_detect_transceiver (
                            module, &is_present);

        /* get conn_id and chnl_id by module. */
        uint32_t conn_id = 0, chnl_id = 0;
        bf_sfp_get_conn (module, &conn_id,
                         &chnl_id);
        if (bf_sfp_is_fsm_logging(module)) {
            LOG_DEBUG ("PM_INTF  SFP    %2d : %2d/%d : detect-st : %d is-present : %d",
                   module,
                   conn_id,
                   chnl_id,
                   detect_st,
                   is_present);
        }

        // Find if so-called sfp module was removed or added
        if (detect_st) {
            // hopefully, detect it in the next iteration
            LOG_ERROR ("PM_INTF  SFP    %2d : error detecting SFP",
                       module);
            module++;
            continue;  // back to outer while loop
        } else {
handle_removal:
            if (bf_pm_sfp_quick_removal_detected_get (
                    module)) {
                // over-ride present bit so that we go through clean state in next cycle
                if (is_present) {
                    LOG_DEBUG (
                        "PM_INTF  SFP    %2d : Latched removal conditon detected. Doing removal "
                        "actions.\n",
                        module);
                    is_present = false;
                }
                bf_pm_sfp_quick_removal_detected_set (module,
                                                      false);
            }
            // update qsfp_pres_mask after successfully detecting the module
            if (is_present) {
                ((module % 32) != 0)
                        ? (PM_BIT_CLR(sfp_pres_mask[mask_id], (module % 32) - 1))
                        : (PM_BIT_CLR(sfp_pres_mask[mask_id], 31));
            } else {
                ((module % 32) != 0)
                        ? (PM_BIT_SET(sfp_pres_mask[mask_id], (module % 32) - 1))
                        : (PM_BIT_SET(sfp_pres_mask[mask_id], 31));
            }
        }
        if (is_present == true) {
            bf_sfp_set_detected (module, false);
            if (!bf_pm_intf_is_device_family_tofino (
                    dev_id)) {
                // determines sfp type only
                sfp_present_actions (module);
            }
            // kick off the module FSM
            if (!bf_pltfm_pm_is_ha_mode()) {
                sfp_fsm_inserted (module);
            } else {
                //sfp_state_ha_config_set (dev_id, conn_id);
                sfp_fsm_inserted (module);
            }
        } else {
            bf_sfp_set_detected (module, false);
            // Update the cached status of the qsfp
            if (!bf_pltfm_pm_is_ha_mode()) {
                sfp_scan_removed (dev_id, module);
            } else {
                //sfp_state_ha_config_delete (module);
                sfp_scan_removed (dev_id, module);
            }
        }

        module++;
    }  // Outside while

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t sfp_scan (bf_dev_id_t dev_id)
{
    uint32_t lower_mask, upper_mask;
    uint32_t lower_mask_prev, upper_mask_prev;
    bf_pltfm_status_t sts;
    upper_mask_prev = upper_mask = sfp_pres_mask[0];
    lower_mask_prev = lower_mask = sfp_pres_mask[1];

    // Query the SFP library for the qsfp presence mask
    if (bf_sfp_get_transceiver_pres (&lower_mask,
                                     &upper_mask) != 0) {
        return BF_PLTFM_COMM_FAILED;
    }

    // Handle quick unplug and plug
    // Cross check with latched state and over-ride the mask
    if (lower_mask_prev !=
        sfp_quick_rmv_pres_mask[1]) {
        LOG_DEBUG ("pres-lower-mask changed: 0x%0x -> 0x%0x",
                   lower_mask_prev,
                   sfp_quick_rmv_pres_mask[1]);
        lower_mask = sfp_quick_rmv_pres_mask[1];
    }

    if (upper_mask_prev !=
        sfp_quick_rmv_pres_mask[0]) {
        LOG_DEBUG ("pres-upper-mask changed: 0x%0x -> 0x%0x",
                   upper_mask_prev,
                   sfp_quick_rmv_pres_mask[0]);
        upper_mask = sfp_quick_rmv_pres_mask[0];
    }

    if (platform_type_equal (AFN_X564PT)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = sfp_scan_helper (dev_id, 33, upper_mask,
                               0);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (33-64)  at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_X312PT)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = sfp_scan_helper (dev_id, 33, upper_mask,
                               0);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (33-64)  at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        sts = sfp_scan_helper (dev_id, 1, lower_mask, 1);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (1-32) at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
        sts = sfp_scan_helper (dev_id, 33, upper_mask,
                               0);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error:%s in scanning the SFPs (33-64)  at %s:%d\n",
                       bf_pltfm_err_str (sts),
                       __func__,
                       __LINE__);
        }
    }

    sfp_quick_rmv_pres_mask[0] = sfp_pres_mask[0];
    sfp_quick_rmv_pres_mask[1] = sfp_pres_mask[1];

    return BF_PLTFM_SUCCESS;
}

void sfp_fsm_timer_cb (struct bf_sys_timer_s
                       *timer, void *data)
{
    bf_pltfm_status_t sts;
    bf_dev_id_t dev_id = (bf_dev_id_t) (intptr_t)data;

    static int dumped = 0;
    if (!dumped) {
        LOG_DEBUG (" SFP FSM timer started..");
        dumped = 1;
    }
    // printf(" SFP FSM timer off\n");
    /* Process SFP start-up actions (if necessart) */
    sts = sfp_fsm (dev_id);
    //bf_pm_interface_fsm();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: in sfp FSM at %s:%d\n",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
    }

    /* For x732q_t. */
    if (!bf_pm_intf_is_device_family_tofino(dev_id)) {
        bf_pm_interface_sfp_fsm();
    }

    (void)timer;
}

void sfp_scan_timer_cb (struct bf_sys_timer_s
                        *timer, void *data)
{
    bf_pltfm_status_t sts;
    bf_dev_id_t dev_id = (bf_dev_id_t) (intptr_t)data;

    static int dumped = 0;
    if (!dumped) {
        LOG_DEBUG (" SFP SCAN timer started..");
        dumped = 1;
    }
    // printf("SFP Scan timer off\n");
    // Scan the insertion and detection of SFPs
    sts = sfp_scan (dev_id);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: in scanning sfps at %s:%d\n",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
    }
    (void)timer;
}

bf_pltfm_status_t bf_pltfm_pm_port_sfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type)
{
    int module = 0;

    if (!port_info || !qsfp_type) {
        return BF_PLTFM_INVALID_ARG;
    }
    /* bf_pm_num_mac is equle the number of MAC. No matter SFP or QSFP,
     * this branch can be used for two of them. So I keep it here.
     * by tsihang, 2021-07-18.
     *
     * bf_pm_num_lanes_get is quite different as there are only 4 lanes
     * for AUX MAC. While it's lucky that we can handle this case
     * wisely with is_panel_sfp to dobule check.
     * by Hang Tsi, 2024-04-10.
     */
    if ((port_info->conn_id > (uint32_t)bf_pm_num_mac) ||
        (port_info->chnl_id >= (uint32_t)bf_pm_num_lanes_get(port_info->conn_id)) ||
        !is_panel_sfp (port_info->conn_id,
                       port_info->chnl_id)) {
        return BF_PLTFM_INVALID_ARG;
    }

    bf_sfp_get_port (port_info->conn_id,
                     port_info->chnl_id, &module);
    *qsfp_type =
        pm_sfp_info_arr[module].qsfp_type;

    return BF_PLTFM_SUCCESS;
}

/* This API is used for bf_pm_porting.c */
bf_pltfm_status_t pltfm_pm_port_sfp_is_present (
    bf_pltfm_port_info_t *port_info,
    bool *is_present)
{
    int module = 0;

    if (!port_info) {
        return BF_PLTFM_INVALID_ARG;
    }
    /* bf_pm_num_mac is equle the number of MAC. No matter SFP or QSFP,
     * this branch can be used for two of them. So I keep it here.
     * by tsihang, 2021-07-18.
     *
     * bf_pm_num_lanes_get is quite different as there are only 4 lanes
     * for AUX MAC. While it's lucky that we can handle this case
     * wisely with is_panel_sfp to dobule check.
     * by Hang Tsi, 2024-04-10.
     */
    if ((port_info->conn_id > (uint32_t)bf_pm_num_mac) ||
        (port_info->chnl_id >= (uint32_t)bf_pm_num_lanes_get(port_info->conn_id)) ||
        !is_panel_sfp (port_info->conn_id,
                       port_info->chnl_id)) {
        return BF_PLTFM_INVALID_ARG;
    }

    bf_sfp_get_port (port_info->conn_id,
                     port_info->chnl_id, &module);

    // use bf_sfp_is_present for consistent view - TBD
    *is_present =
        pm_sfp_info_arr[module].is_present;

    return BF_PLTFM_SUCCESS;
}

void sfp_fsm_notify_bf_pltfm (bf_dev_id_t dev_id,
                              int module)
{
    bf_dev_port_t dev_port = (bf_dev_port_t)0L;
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_id_t dev_id_of_port = 0;

    // notify bf_pltfm of new SFP
    pm_sfp_info_arr[module].is_present = true;

    /* get conn_id and chnl_id by module. */
    bf_sfp_get_conn (module, &port_hdl.conn_id,
                     &port_hdl.chnl_id);
    bfn_fp2_dp (dev_id_of_port, &port_hdl, &dev_port);
    dev_id = dev_id_of_port;

    bf_serdes_tx_loop_bandwidth_set (
        dev_id, dev_port, 0, BF_SDS_TOF_TX_LOOP_BANDWIDTH_DEFAULT);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) Notify platform",
        module, port_hdl.conn_id, port_hdl.chnl_id);

    sfp_detection_actions (dev_id, module);

}

bf_pm_sfp_info_t *bf_pltfm_get_pm_sfp_info_ptr (
    int module)
{
    return &pm_sfp_info_arr[module];
}
#endif

bf_pltfm_status_t bf_pltfm_pm_port_qsfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type)
{
    if (!port_info || !qsfp_type) {
        return BF_PLTFM_INVALID_ARG;
    }
    /* bf_pm_num_mac is equle the number of MAC. No matter SFP or QSFP,
     * this branch can be used for two of them. So I keep it here.
     * by tsihang, 2021-07-18.
     *
     * bf_pm_num_lanes_get is quite different as there are only 4 lanes
     * for AUX MAC. While it's lucky that we can handle this case
     * wisely with is_panel_sfp to dobule check.
     * by Hang Tsi, 2024-04-10.
     */
    if ((port_info->conn_id > (uint32_t)bf_pm_num_mac) ||
        (port_info->chnl_id >= (uint32_t)bf_pm_num_lanes_get(port_info->conn_id))) {
        return BF_PLTFM_INVALID_ARG;
    }

    if (is_panel_sfp (port_info->conn_id,
                      port_info->chnl_id)) {
        uint32_t module;
        bf_pltfm_sfp_lookup_by_conn (port_info->conn_id,
                                     port_info->chnl_id, &module);
        if (bf_sfp_is_passive_cu (module)) {
            *qsfp_type = BF_PLTFM_QSFP_CU_0_5_M;
        } else {
            *qsfp_type = BF_PLTFM_QSFP_OPT;
        }
    } else {
        *qsfp_type =
            pm_qsfp_info_arr[port_info->conn_id].qsfp_type;
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t pltfm_pm_port_qsfp_is_present (
    bf_pltfm_port_info_t *port_info,
    bool *is_present)
{
    if (!port_info) {
        return BF_PLTFM_INVALID_ARG;
    }
    /* bf_pm_num_mac is equle the number of MAC. No matter SFP or QSFP,
     * this branch can be used for two of them. So I keep it here.
     * by tsihang, 2021-07-18.
     *
     * bf_pm_num_lanes_get is quite different as there are only 4 lanes
     * for AUX MAC. While it's lucky that we can handle this case
     * wisely with is_panel_sfp to dobule check.
     * by Hang Tsi, 2024-04-10.
     */
    if ((port_info->conn_id > (uint32_t)bf_pm_num_mac) ||
        (port_info->chnl_id >= (uint32_t)bf_pm_num_lanes_get(port_info->conn_id))) {
        return BF_PLTFM_INVALID_ARG;
    }

    // use bf_qsfp_is_present for consistent view - TBD
    *is_present =
        pm_qsfp_info_arr[port_info->conn_id].is_present;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_pm_qsfp_scan_poll_start()
{
    bf_sys_timer_status_t rc;
    // Start the QSFP scan only if the board type is not Emulator/Model
    if (platform_is_hw()) {
        rc = bf_sys_timer_start (&qsfp_scan_timer);
        if (rc) {
            LOG_ERROR ("Error %d: in starting qsfp-scan timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
        rc = bf_sys_timer_start (&qsfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("Error %d: in starting qsfp-fsm timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
#if defined(HAVE_SFP)
        rc = bf_sys_timer_start (&sfp_scan_timer);
        if (rc) {
            LOG_ERROR ("Error %d: in starting sfp-scan timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
        rc = bf_sys_timer_start (&sfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("Error %d: in starting sfp-fsm timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
#endif
    }
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_pm_qsfp_scan_poll_stop()
{
    bf_sys_timer_status_t rc;
    rc = bf_sys_timer_stop (&qsfp_scan_timer);
    if (rc) {
        LOG_ERROR ("PM_INTF QSFP    %2d: in stopping qsfp-scan timer\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }
    rc = bf_sys_timer_stop (&qsfp_fsm_timer);
    if (rc) {
        LOG_ERROR ("PM_INTF QSFP    %2d: in stopping qsfp-fsm timer\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }

#if defined(HAVE_SFP)
    rc = bf_sys_timer_stop (&sfp_scan_timer);
    if (rc) {
        LOG_ERROR ("PM_INTF QSFP    %2d: in stopping sfp-scan timer\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }
    rc = bf_sys_timer_stop (&sfp_fsm_timer);
    if (rc) {
        LOG_ERROR ("PM_INTF QSFP    %2d: in stopping sfp-fsm timer\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }
#endif

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_pm_init (
    bf_dev_init_mode_t init_mode)
{
    bf_pltfm_status_t sts;
    bf_dev_id_t dev_id = 0;
    int conn_id;
    bf_sys_timer_status_t rc;

    /* Real QSFPs of current platforms. */
    bf_pm_num_qsfp = bf_qsfp_get_max_qsfp_ports();
    bf_pm_num_mac  = platform_num_ports_get ();

    for (conn_id = 0; conn_id < bf_pm_num_qsfp;
         conn_id++) {
        qsfp_info_clear (conn_id);
    }

    qsfp_reset_pres_mask ();

    qsfp_quick_rmv_pres_mask[0] = 0xffffffff;
    qsfp_quick_rmv_pres_mask[1] = 0xffffffff;
    qsfp_quick_rmv_pres_mask[2] = 0xffffffff;

    if (platform_is_hw()) {
        rc = bf_sys_timer_create (&qsfp_scan_timer,
                                  QSFP_SCAN_TMR_PERIOD_MS,
                                  QSFP_SCAN_TMR_PERIOD_MS,
                                  qsfp_scan_timer_cb,
                                  (void *) (intptr_t)dev_id);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating qsfp-scan timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
    }

    if (platform_is_hw()) {
        if ((init_mode != BF_DEV_WARM_INIT_FAST_RECFG) &&
            (init_mode != BF_DEV_WARM_INIT_HITLESS)) {
            /* edit by tsihang, 2019.8.1 */
            qsfp_deassert_all_reset_pins();
        }
        rc = bf_sys_timer_create (&qsfp_fsm_timer,
                                  QSFP_FSM_TMR_PERIOD_MS,
                                  QSFP_FSM_TMR_PERIOD_MS,
                                  qsfp_fsm_timer_cb,
                                  (void *) (intptr_t)dev_id);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating qsfp-fsm timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
    }

#if defined(HAVE_SFP)
    bf_pm_num_sfp = bf_sfp_get_max_sfp_ports();

    for (conn_id = 0; conn_id < bf_pm_num_sfp;
         conn_id++) {
        sfp_info_clear (conn_id);
    }

    sfp_reset_pres_mask ();

    sfp_quick_rmv_pres_mask[0] = 0xffffffff;
    sfp_quick_rmv_pres_mask[1] = 0xffffffff;

    if (platform_is_hw()) {
        rc = bf_sys_timer_create (&sfp_scan_timer,
                                  SFP_SCAN_TMR_PERIOD_MS,
                                  SFP_SCAN_TMR_PERIOD_MS,
                                  sfp_scan_timer_cb,
                                  (void *) (intptr_t)dev_id);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating sfp-scan timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
    }

    if (platform_is_hw()) {
        if ((init_mode != BF_DEV_WARM_INIT_FAST_RECFG) &&
            (init_mode != BF_DEV_WARM_INIT_HITLESS)) {
            sfp_deassert_all_reset_pins();
        }
        rc = bf_sys_timer_create (&sfp_fsm_timer,
                                  SFP_FSM_TMR_PERIOD_MS,
                                  SFP_FSM_TMR_PERIOD_MS,
                                  sfp_fsm_timer_cb,
                                  (void *) (intptr_t)dev_id);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating sfp-fsm timer\n",
                       rc);
            return BF_PLTFM_COMM_FAILED;
        }
    }
#endif

    // Register the qsfp type get function with the bd_cfg lib
    sts = bf_bd_cfg_qsfp_type_fn_reg (
              bf_pltfm_pm_port_qsfp_type_get);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to register the qsfp type get function with bd_cfg lib : %s "
            "(%d)",
            bf_pltfm_err_str (sts),
            sts);
        return sts;
    }

    bf_pltfm_qsfp_load_conf ();
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_pm_deinit()
{
    bf_sys_timer_status_t rc;

    // Stop the timers only if the board type is not Emulator/Model
    if (platform_is_hw()) {
        rc = bf_sys_timer_stop (&qsfp_scan_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating qsfp-scan timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }
        rc = bf_sys_timer_stop (&qsfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in creating qsfp-fsm timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }

        rc = bf_sys_timer_del (&qsfp_scan_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in deleting sfp-scan timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }

        rc = bf_sys_timer_del (&qsfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in deleting sfp-fsm timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }
    }

#if defined(HAVE_SFP)
    // Stop the timers only if the board type is not Emulator/Model
    if (platform_is_hw()) {
        rc = bf_sys_timer_stop (&sfp_scan_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in stopping sfp-scan timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }
        rc = bf_sys_timer_stop (&sfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in stopping sfp-fsm timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }

        rc = bf_sys_timer_del (&sfp_scan_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in deleting sfp-scan timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }

        rc = bf_sys_timer_del (&sfp_fsm_timer);
        if (rc) {
            LOG_ERROR ("PM_INTF QSFP    %2d: in deleting sfp-fsm timer\n",
                       rc);
            return BF_HW_COMM_FAIL;
        }
    }
#endif
    return BF_PLTFM_SUCCESS;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_notify_bf_pltfm (bf_dev_id_t dev_id,
                               int conn_id)
{
    //fprintf(stdout, "A bug ?  %d\n", conn_id);
    bf_dev_port_t dev_port = (bf_dev_port_t)0L;
    bf_pal_front_port_handle_t port_hdl;
    uint32_t chnl;
    bool is_luxtera = bf_qsfp_is_luxtera_optic (
                          conn_id);
    bf_dev_id_t dev_id_of_port = 0;

    // notify bf_pltfm of new QSFP
    pm_qsfp_info_arr[conn_id].is_present = true;

    /* Luxtera modules require some special handling during bring-up.
    * They seem to prefer a higher loop bandwidth setting.
    * This code takes care of the case where a port was defined on the
    * connector before a QSFP was detected (so we couldn't determine
    * the setting in the pre_port_enable callback). Now that the QSFP
    * has been detected we can set it properly.
    *
    * Note: Since we don't eally know what all ports are defined here
    *       we set the loop bandwidth for all four possible dev_ports
    *       specifying "lane 0" for each. The API dutifully converts
    *       this to the correct serdes structure for later application.
    *
    * drv-1347
    */
    port_hdl.conn_id = conn_id;
    for (chnl = 0; chnl < 4; chnl++) {
        port_hdl.chnl_id = chnl;
        bfn_fp2_dp (dev_id_of_port, &port_hdl, &dev_port);
        dev_id = dev_id_of_port;

        if (is_luxtera) {
            bf_serdes_tx_loop_bandwidth_set (
                dev_id, dev_port, 0,
                BF_SDS_TOF_TX_LOOP_BANDWIDTH_9MHZ);
        } else {
            bf_serdes_tx_loop_bandwidth_set (
                dev_id, dev_port, 0,
                BF_SDS_TOF_TX_LOOP_BANDWIDTH_DEFAULT);
        }
    }

    qsfp_detection_actions (dev_id, conn_id);
}

/*****************************************************************
*
*****************************************************************/
void bf_pltfm_pm_qsfp_simulate_all_removed (void)
{
    int conn_id;

    for (conn_id = 1; conn_id < bf_pm_num_qsfp;
         conn_id++) {
        if (pm_qsfp_info_arr[conn_id].is_present) {
            qsfp_scan_removed (0, conn_id);
        }
    }
    qsfp_pres_mask[0] = 0xffffffff;
    qsfp_pres_mask[1] = 0xffffffff;
    qsfp_pres_mask[2] = 0xffffffff;
    bf_qsfp_debug_clear_all_presence_bits();


    /* by tsihang, 2023-05-10. */
    for (conn_id = 1; conn_id < bf_pm_num_sfp;
         conn_id++) {
        if (pm_sfp_info_arr[conn_id].is_present) {
            sfp_scan_removed (0, conn_id);
        }
    }
    sfp_pres_mask[0] = 0xffffffff;
    sfp_pres_mask[1] = 0xffffffff;
    bf_sfp_debug_clear_all_presence_bits();
}

bf_pltfm_status_t bf_pltfm_pm_ha_mode_set()
{
    bf_pltfm_pm_ha_mode = true;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_pm_ha_mode_clear()
{
    bf_pltfm_pm_ha_mode = false;
    return BF_PLTFM_SUCCESS;
}

bool bf_pltfm_pm_is_ha_mode()
{
    return bf_pltfm_pm_ha_mode;
}

bf_pm_qsfp_info_t *bf_pltfm_get_pm_qsfp_info_ptr (
    int connector)
{
    return &pm_qsfp_info_arr[connector];
}
