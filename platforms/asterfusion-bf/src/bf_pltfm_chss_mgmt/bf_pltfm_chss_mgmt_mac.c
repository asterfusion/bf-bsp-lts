/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_mac.c
 * @date
 *
 * Returns MAC address of a port
 *
 */

/* Standard includes */
#include <stdio.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_bd_map.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"

/**********************************************************************
 * Assume base MAC address in EEPROM is 60:EB:5A:00:37:D9.
 *
 * There are 8 (BF_PLTFM_SYS_MAC_ADDR_MAX) reserved addresses in pool
 * for sys manangement, and they start from 60:EB:5A:00:37:D9 to
 * (60:EB:5A:00:37:D9 + BF_PLTFM_SYS_MAC_ADDR_MAX - 1). Let's try to
 * handle them like below:
 * - 60:EB:5A:00:37:D9,     for CME carrier NIC (well-known as ma1)
 * - 60:EB:5A:00:37:D9 + 1, for virtual NIC (well-known as macvlan0)
 * - ...
 * - No more details for remaining MAC addresses.
 * - ...
 * (let's remark the number as A)
 *
 * The starting MAC address for tofino port is (60:EB:5A:00:37:D9 + BF_PLTFM_SYS_MAC_ADDR_MAX).
 * Considering of applying breakout mode to a given tofino port, it
 * requires amount of MAC addresses and the allocation could be like:
 * - One tofino1 port requires 4 MAC addresses.
 * - One tofino2 port requires 8 MAC addresses.
 *
 * So the number of required MAC address for TOFINO PORT ONLY are
 * different and listed below:
 * - For X732Q-T, requires 32 x8 + 1 x4        = 260 MAC addresses.
 * - For X564P-T, requires 64 x4 + 1 x4        = 260 MAC addresses.
 * - For X532P-T, requires 32 x4 + 1 x4        = 132 MAC addresses.
 * - For X308P-T, requires  8 x4 + 48x1 + 1 x4 =  84 MAC addresses.
 * - For X312P-T, requires 12 x4 + 48x1 + 1 x4 = 100 MAC addresses.
 * (let's remark the number as B)
 *
 * Finally,
 * the number of total required MAC addresses for given X-T = (A + B)
 *
 *
 *                                            by Hang Tsi, 1 Apr 2024
 **********************************************************************/

/**
 * Get base MAC address from EEPROM for sys management
 */
bf_pltfm_status_t
bf_pltfm_chss_mgmt_sys_mac_addr_get (
    uint8_t *mac_info, uint8_t *num_sys_addresses)
{
    uint8_t i;

    if (mac_info == NULL) {
        LOG_ERROR ("Invalid argument\n");
        return BF_PLTFM_INVALID_ARG;
    }

    *num_sys_addresses = BF_PLTFM_SYS_MAC_ADDR_MAX;

    for (i = 0; i < 6; i++) {
        //mac_info[i] = eeprom.bf_pltfm_local_mac[i];
        mac_info[i] = eeprom.bf_pltfm_mac_base[i];
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Get base MAC address from EEPROM.
 */
bf_pltfm_status_t
bf_pltfm_chss_mgmt_port_mac_addr_get (
    bf_pltfm_port_info_t *port_info,
    uint8_t *mac_info)
{
    uint8_t i;
    uint32_t index, t;
    bf_pltfm_board_id_t board_id = BF_PLTFM_BD_ID_UNKNOWN;
    uint32_t max_conn_id = 33, max_chnl_id = 4;  // default
    uint32_t num_lanes = 1;
    bf_pltfm_status_t st;

    if (port_info == NULL) {
        LOG_ERROR ("Invalid argument\n");
        return BF_PLTFM_INVALID_ARG;
    }

    bf_pltfm_chss_mgmt_bd_type_get (&board_id);
    if (board_id == BF_PLTFM_BD_ID_UNKNOWN) {
        LOG_ERROR("Invalid board id 0x%0x\n", board_id);
        return BF_PLTFM_INVALID_ARG;
    }

    if (platform_type_equal (AFN_X732QT)) {
        max_chnl_id = 8;
    }
    if (platform_type_equal (AFN_X564PT)) {
        max_conn_id = 65;
    }

    /* num_lanes will always be 1 for tof/tof2. */
    st = bf_bd_cfg_port_nlanes_per_ch_get(port_info, &num_lanes);
    if (st != BF_PLTFM_SUCCESS) {
        LOG_ERROR("Unable to get number of lanes conn_id: %d chnl_id: %d\n",
                port_info->conn_id,
                port_info->chnl_id);
        return st;
    }

    if ((port_info->conn_id > max_conn_id) ||
        (port_info->chnl_id >= max_chnl_id)) {
        LOG_ERROR ("Invalid argument conn_id: %d chnl_id: %d\n",
                   port_info->conn_id,
                   port_info->chnl_id);
        return BF_PLTFM_INVALID_ARG;
    }

    index = port_info->conn_id * max_chnl_id +
            (port_info->chnl_id / num_lanes);

    for (i = 0; i < 6; i++) {
        mac_info[i] = eeprom.bf_pltfm_mac_base[i];
    }

    t = mac_info[5] | (mac_info[4] << 8) | (mac_info[3] << 16);

    // Let's take base MAC 60:EB:5A:00:37:D9 as an example.
    // [60:EB:5A:00:37:D9, 60:EB:5A:00:37:E0], total 8 (refer to BF_PLTFM_SYS_MAC_ADDR_MAX) MACs are reserved for sysmac.
    // The following next MAC (60:EB:5A:00:37:E1) is the first MAC for port 1/0.
    //t = t + index;
    t = t + BF_PLTFM_SYS_MAC_ADDR_MAX + index - max_chnl_id;

    mac_info[5] = t & 0xFF;
    mac_info[4] = (t >> 8) & 0xFF;
    mac_info[3] = (t >> 16) & 0xFF;

    LOG_DEBUG ("Extended Mac addr: %x:%x:%x:%x:%x:%x",
               mac_info[0],
               mac_info[1],
               mac_info[2],
               mac_info[3],
               mac_info[4],
               mac_info[5]);

    return BF_PLTFM_SUCCESS;
}
