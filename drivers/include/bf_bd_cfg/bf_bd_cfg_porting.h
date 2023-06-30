/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_BD_CFG_PORTING_H
#define _BF_PLTFM_BD_CFG_PORTING_H

#include <bf_types/bf_types.h>
#include <tofino/bf_pal/bf_pal_types.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the first port in the system. This function is registered with
 * SDE and the SDE PM will call this on demand
 *
 * @param first_port_info First front panel port in the system
 *
 * @return Status of the API call
 */
bf_status_t bf_bd_cfg_front_panel_port_get_first (
    bf_pal_front_port_info_t *first_port_info);

/**
 * @brief Return the next port in the system. This function is registered with
 * SDE and the SDE PM will call this on demand
 * curr_port_info == NULL -> Return the first front panel port in the system
 * return == BF_OBJECT_NOT_FOUND -> curr_port_hdl is the last front panel port
 *in the
 * system
 *
 * @param curr_port_info Current front panel port in the system
 * @param next_port_info Next front panel port in the system
 *
 * @return Status of the API call
 */
bf_status_t bf_bd_cfg_front_panel_port_get_next (
    bf_pal_front_port_info_t *curr_port_info,
    bf_pal_front_port_info_t *next_port_info);

/**
 * @brief Return the port handle corresponding to the port str. If the str
 * contains wildcard param, return the port hdl conn/chnl as -1
 *
 * @param port_str String representing the port
 * @param port_hdl Corresponding front panel port handle
 *
 * @return Status of the API call
 */
bf_status_t bf_bd_cfg_port_str_to_handle (
    const char *port_str,
    bf_pal_front_port_handle_t *port_hdl);

/**
 * @brief Given a port handle, return the serdes Tx/Rx lane connected to the
 * corresponding MAC lane. This funvtion is registered with the SDE and the
 * SDE PM wil call this on demand
 *
 * @param port_hdl Front panel port handle
 * @param mac_blk_map MAC lane to serdes Tx/Rx lane mapping
 *
 * @return Status of the API call
 */
bf_status_t bf_bd_cfg_mac_to_serdes_map_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_mac_to_serdes_lane_map_t *mac_blk_map);

/**
 * @brief Given a port handle, get parameters of the corresponding serdes lane
 *
 * @param port_hdl Front panel port handle
 * @param serdes_info Serdes lane info
 *
 * @return Status of the API call
 */
bf_status_t bf_bd_cfg_serdes_info_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_serdes_info_t *serdes_info);

/**
 * @brief Given a port handle, return the serdes Tx/Rx lane/lanes connected to
 * the corresponding MAC lane. This funvtion is registered with the SDE and the
 * SDE PM wil call this on demand. In Tofino3, there could be two serdes lanes
 * per MAC lane. Use this for Tofino3 onwards
 *
 * @param port_hdl Front panel port handle
 * @param mac_blk_map MAC lane to serdes Tx/Rx lane mapping
 *
 * @return Status of the API call
 */
#if SDE_VERSION_GT(970) /* since 9.9.x. */
bf_status_t
bf_bd_cfg_mac_to_multi_serdes_map_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_mac_to_multi_serdes_lane_map_t
    *mac_blk_map);
#endif
#ifdef __cplusplus
}
#endif /* C++ */

#endif
