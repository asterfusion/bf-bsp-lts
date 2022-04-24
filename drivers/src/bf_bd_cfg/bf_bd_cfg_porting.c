/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

// Module includes
#include <tofino/bf_pal/bf_pal_types.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>

// Local includes
#include "bf_bd_cfg.h"

#define FIRST_CONN_ID 1
#define FIRST_CHNL_ID 0

bf_status_t bf_bd_cfg_front_panel_port_get_first (
    bf_pal_front_port_info_t *first_port_info)
{
    bf_pltfm_status_t sts = BF_PLTFM_SUCCESS;
    uint32_t log_mac_id, log_mac_lane;
    bf_pltfm_port_info_t port_info;
    bf_dev_id_t dev_id;

    // Safety checks
    if (!first_port_info) {
        return BF_INVALID_ARG;
    }

    // Since this is the first port in the system, conn_id=1 & chnl_id=0
    port_info.conn_id = FIRST_CONN_ID;
    port_info.chnl_id = FIRST_CHNL_ID;

    sts = bf_bd_cfg_port_mac_get (&port_info,
                                  &log_mac_id, &log_mac_lane);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to get the mac id and chnl for front port %d/%d : %s (%d)",
            port_info.conn_id,
            port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
        return sts;
    }

    dev_id = (bf_dev_id_t)bf_bd_get_dev_id (
                 port_info.conn_id, port_info.chnl_id);
    first_port_info->dev_id = dev_id;
    first_port_info->port_hdl.conn_id =
        port_info.conn_id;
    first_port_info->port_hdl.chnl_id =
        port_info.chnl_id;
    first_port_info->log_mac_id = log_mac_id;
    first_port_info->log_mac_lane = log_mac_lane;
    pltfm_port_info_to_str (
        &port_info, first_port_info->port_str,
        sizeof (first_port_info->port_str));

    return BF_SUCCESS;
}

bf_status_t bf_bd_cfg_front_panel_port_get_next (
    bf_pal_front_port_info_t *curr_port_info,
    bf_pal_front_port_info_t *next_port_info)
{
    bf_pltfm_status_t sts = BF_PLTFM_SUCCESS;
    bf_pltfm_port_info_t curr_pltfm_port_info,
                         next_pltfm_port_info;
    int num_ports;
    uint32_t log_mac_id, log_mac_lane;
    bf_dev_id_t dev_id;

    // Safety checks
    if (!next_port_info) {
        return BF_INVALID_ARG;
    }

    if (!curr_port_info) {
        // Return the first port in the system
        return bf_bd_cfg_front_panel_port_get_first (
                   next_port_info);
    }

    // Get the total number of ports on the platform
    num_ports = bf_bd_cfg_bd_num_port_get();

    // Derive the current pltfm port info
    curr_pltfm_port_info.conn_id =
        curr_port_info->port_hdl.conn_id;
    curr_pltfm_port_info.chnl_id =
        curr_port_info->port_hdl.chnl_id;

    // Derive the next pltfm port info
    next_pltfm_port_info.conn_id =
        curr_pltfm_port_info.conn_id;
    next_pltfm_port_info.chnl_id =
        curr_pltfm_port_info.chnl_id + 1;

    /* If channels are spread into multiple connectors, chnl_ids are not
     * incremental. On non-existent channel, bd-map will be NULL,
     * land to next connector with chan:0
     */
    if (!bf_bd_is_this_channel_valid (
            next_pltfm_port_info.conn_id,
            next_pltfm_port_info.chnl_id)) {
        if ((next_pltfm_port_info.conn_id + 1) > ((
                    uint32_t)num_ports)) {
            // last entry
            return BF_OBJECT_NOT_FOUND;
        }
        /* reset the channel and check for next connector */
        next_pltfm_port_info.conn_id =
            next_pltfm_port_info.conn_id + 1;
        next_pltfm_port_info.chnl_id = 0;
    }

    sts = bf_bd_cfg_port_mac_get (
              &next_pltfm_port_info, &log_mac_id,
              &log_mac_lane);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to get the mac id and chnl for front port %d/%d : %s (%d)",
            next_pltfm_port_info.conn_id,
            next_pltfm_port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
        return sts;
    }

    dev_id = (bf_dev_id_t)bf_bd_get_dev_id (
                 next_pltfm_port_info.conn_id,
                 next_pltfm_port_info.chnl_id);
    next_port_info->dev_id = dev_id;
    next_port_info->port_hdl.conn_id =
        next_pltfm_port_info.conn_id;
    next_port_info->port_hdl.chnl_id =
        next_pltfm_port_info.chnl_id;
    next_port_info->log_mac_id = log_mac_id;
    next_port_info->log_mac_lane = log_mac_lane;
    pltfm_port_info_to_str (&next_pltfm_port_info,
                            next_port_info->port_str,
                            sizeof (next_port_info->port_str));

    return BF_SUCCESS;
}

bf_status_t bf_bd_cfg_port_str_to_handle (
    const char *port_str,
    bf_pal_front_port_handle_t *port_hdl)
{
    bf_pltfm_port_info_t port_info;
    bf_status_t sts;

    // Safety checks
    if (!port_str) {
        return BF_INVALID_ARG;
    }
    if (!port_hdl) {
        return BF_INVALID_ARG;
    }

    sts = pltfm_port_str_to_info (port_str,
                                  &port_info);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Unable to get pltfm port info for str : %s : %s (%d)",
                   port_str,
                   bf_pltfm_err_str (sts),
                   sts);
        return sts;
    }

    port_hdl->conn_id = port_info.conn_id;
    port_hdl->chnl_id = port_info.chnl_id;

    return BF_SUCCESS;
}

bf_status_t bf_bd_cfg_mac_to_serdes_map_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_mac_to_serdes_lane_map_t *mac_blk_map)
{
    bf_pltfm_status_t sts;
    bf_pltfm_port_info_t port_info;
    uint32_t tx_lane, rx_lane;

    // Safety checks
    if (!port_hdl) {
        return BF_INVALID_ARG;
    }
    if (!mac_blk_map) {
        return BF_INVALID_ARG;
    }

    // Derive the pltfm port info
    port_info.conn_id = port_hdl->conn_id;
    port_info.chnl_id = port_hdl->chnl_id;

    sts = bf_bd_cfg_port_tx_phy_lane_get (&port_info,
                                          &tx_lane);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to get the serdes Tx lane connected to front port %d/%d : %s "
            "(%d)",
            port_info.conn_id,
            port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
        return sts;
    }

    sts = bf_bd_cfg_port_rx_phy_lane_get (&port_info,
                                          &rx_lane);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to get the serdes Rx lane connected to front port %d/%d : %s "
            "(%d)",
            port_info.conn_id,
            port_info.chnl_id,
            bf_pltfm_err_str (sts),
            sts);
        return sts;
    }

    mac_blk_map->tx_lane = tx_lane;
    mac_blk_map->rx_lane = rx_lane;

    return BF_SUCCESS;
}

bf_status_t bf_bd_cfg_serdes_info_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_serdes_info_t *serdes_info)
{
    bf_pltfm_status_t sts;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_qsfp_type_t qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    bf_pltfm_mac_lane_info_t mac_lane_info;

    // Safety checks
    if (!port_hdl) {
        return BF_INVALID_ARG;
    }
    if (!serdes_info) {
        return BF_INVALID_ARG;
    }

    // Derive the pltfm port info
    port_info.conn_id = port_hdl->conn_id;
    port_info.chnl_id = port_hdl->chnl_id;

    if (bf_bd_is_this_port_internal (
            port_info.conn_id, port_info.chnl_id)) {
        /* cook this up for now. */
        qsfp_type = BF_PLTFM_QSFP_CU_3_M;
    } else {
        // Get the type of the QSFP connected to the port
        sts = pltfm_port_qsfp_type_get (&port_info,
                                        &qsfp_type);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "Unable to get the type of the QSFP connected to front port %d/%d : %s "
                "(%d)",
                port_info.conn_id,
                port_info.chnl_id,
                bf_pltfm_err_str (sts),
                sts);
        }
    }

    sts = bf_bd_cfg_port_mac_lane_info_get (
              &port_info, qsfp_type, &mac_lane_info);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Unable to get the mac lane info for front port %d/%d : %s (%d)",
                   port_info.conn_id,
                   port_info.chnl_id,
                   bf_pltfm_err_str (sts),
                   sts);
        return sts;
    }

    serdes_info->tx_inv =
        mac_lane_info.tx_phy_pn_swap;
    serdes_info->rx_inv =
        mac_lane_info.rx_phy_pn_swap;
    serdes_info->tx_attn = mac_lane_info.tx_attn;
    serdes_info->tx_pre = mac_lane_info.tx_pre;
    serdes_info->tx_post = mac_lane_info.tx_post;

    return BF_SUCCESS;
}
