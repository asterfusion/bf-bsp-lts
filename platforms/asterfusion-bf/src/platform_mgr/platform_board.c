/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdint.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm.h>
#include <bf_bd_cfg/bf_bd_cfg_bd_map.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_bd_cfg.h>
#include <bf_mav_board.h>

/* A board has least one version and the bd_map may different.
 * by tsihang, 2022-06-20. */
pltfm_bd_map_t pltfm_board_maps[] = {
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU,
        /*  this type is used on harlyn model also */
        mavericks_P0B_emu_bd_map,
        ROWS (mavericks_P0B_emu_bd_map)
    },
    {
        /* X564P-T v1.0 has dropped support. */
        BF_PLTFM_BD_ID_X564PT_V1DOT0,
        mavericks_P0B_bd_map_x564p,
        ROWS (mavericks_P0B_bd_map_x564p)
    },
    {
        BF_PLTFM_BD_ID_X564PT_V1DOT1,
        mavericks_P0B_bd_map_x564p,
        ROWS (mavericks_P0B_bd_map_x564p)
    },
    {
        BF_PLTFM_BD_ID_X564PT_V1DOT2,
        mavericks_P0B_bd_map_x564p,
        ROWS (mavericks_P0B_bd_map_x564p)
    },
    {
        /* X532P-T v1.0 has dropped support. */
        BF_PLTFM_BD_ID_X532PT_V1DOT0,
        mavericks_P0B_bd_map_x532p,
        ROWS (mavericks_P0B_bd_map_x532p)
    },
    {
        BF_PLTFM_BD_ID_X532PT_V1DOT1,
        mavericks_P0B_bd_map_x532p,
        ROWS (mavericks_P0B_bd_map_x532p)
    },
    {
        BF_PLTFM_BD_ID_X532PT_V2DOT0,
        mavericks_P0B_bd_map_x532p,
        ROWS (mavericks_P0B_bd_map_x532p)
    },
    {
        BF_PLTFM_BD_ID_X308PT_V1DOT0,
        mavericks_P0B_bd_map_x308p_v1dot0,
        ROWS (mavericks_P0B_bd_map_x308p_v1dot0)
    },
    {
        BF_PLTFM_BD_ID_X308PT_V1DOT1,
        mavericks_P0B_bd_map_x308p_v1dot1,
        ROWS (mavericks_P0B_bd_map_x308p_v1dot1)
    },
    {
        /* X312P-T both v1.0 and v1.1 have dropped support. */
        BF_PLTFM_BD_ID_X312PT_V1DOT0,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        BF_PLTFM_BD_ID_X312PT_V1DOT1,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        BF_PLTFM_BD_ID_X312PT_V2DOT0,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        BF_PLTFM_BD_ID_X312PT_V3DOT0,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        BF_PLTFM_BD_ID_X312PT_V4DOT0,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        /* HC36Y24C v1.0 has dropped support. */
        BF_PLTFM_BD_ID_HC36Y24C_V1DOT0,
        mavericks_P0B_bd_map_hc36y24c,
        ROWS (mavericks_P0B_bd_map_hc36y24c)
    },
    {
        BF_PLTFM_BD_ID_HC36Y24C_V1DOT1,
        mavericks_P0B_bd_map_hc36y24c,
        ROWS (mavericks_P0B_bd_map_hc36y24c)
    },
};

/******************************************************************************
*
******************************************************************************/
pltfm_bd_map_t *platform_pltfm_bd_map_get (
    int *rows)
{
    bf_pltfm_board_id_t bd_id;
    uint32_t idx;
    bf_pltfm_status_t sts =
        bf_pltfm_chss_mgmt_bd_type_get (&bd_id);

    if (sts != BF_PLTFM_SUCCESS) {
        return NULL;
    }
    if (!rows) {
        return NULL;
    }
    *rows = ROWS (pltfm_board_maps);
    for (idx = 0; idx < ROWS (pltfm_board_maps);
         idx++) {
        if (pltfm_board_maps[idx].bd_id == bd_id) {
            return &pltfm_board_maps[idx];
        }
    }
    return NULL;
}

int platform_name_get_str (char *name,
                           size_t name_size)
{
    bf_pltfm_status_t sts;
    bf_pltfm_board_id_t bd_id;
    sts = bf_pltfm_chss_mgmt_bd_type_get (&bd_id);
    if (sts != BF_PLTFM_SUCCESS) {
        snprintf (name, name_size, "Error");
        name[name_size - 1] = '\0';
        return sts;
    }
    switch (bd_id) {
        case BF_PLTFM_BD_ID_X532PT_V1DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X532PT_V1DOT0");
            break;
        case BF_PLTFM_BD_ID_X532PT_V1DOT1:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X532PT_V1DOT1");
            break;
        case BF_PLTFM_BD_ID_X532PT_V2DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X532PT_V2DOT0");
            break;
        case BF_PLTFM_BD_ID_X564PT_V1DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X564PT_V1DOT0");
            break;
        case BF_PLTFM_BD_ID_X564PT_V1DOT1:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X564PT_V1DOT1");
            break;
        case BF_PLTFM_BD_ID_X564PT_V1DOT2:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X564PT_V1DOT2");
            break;
        case BF_PLTFM_BD_ID_X308PT_V1DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X308PT_V1DOT0");
            break;
        case BF_PLTFM_BD_ID_X308PT_V1DOT1:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X308PT_V1DOT1");
            break;
        case BF_PLTFM_BD_ID_X312PT_V1DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X312PT_V1DOT0");
            break;
        case BF_PLTFM_BD_ID_X312PT_V2DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X312PT_V2DOT0");
            break;
        case BF_PLTFM_BD_ID_X312PT_V3DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X312PT_V3DOT0");
            break;
        case BF_PLTFM_BD_ID_X312PT_V4DOT0:
            snprintf (name, name_size,
                      "BF_PLTFM_BD_ID_X312PT_V4DOT0");
            break;
        default:
            snprintf (name, name_size, "Unknown");
            break;
    }
    name[name_size - 1] = '\0';
    return 0;
}

int platform_num_ports_get (void)
{
    pltfm_bd_map_t *bd_map;
    int bd_map_rows;

    bd_map = platform_pltfm_bd_map_get (&bd_map_rows);
    if (!bd_map) {
        return 0;
    }
    return (bd_map->rows / QSFP_NUM_CHN);
}

// Returns mac_addr from platform
bf_pltfm_status_t platform_port_mac_addr_get (
    bf_pltfm_port_info_t *port_info,
    uint8_t *mac_addr)
{
    return bf_pltfm_chss_mgmt_port_mac_addr_get (
               port_info, mac_addr);
}
