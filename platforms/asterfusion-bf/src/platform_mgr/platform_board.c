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

#if 0
static pltfm_bd_map_t pltfm_board_maps[] = {

    //{BF_PLTFM_BD_ID_MONTARA_P0A, montara_P0A_bd_map,
    // ROWS(montara_P0A_bd_map)},
    {BF_PLTFM_BD_ID_MONTARA_P0B, montara_P0B_bd_map, ROWS (montara_P0B_bd_map)},
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0A,
        mavericks_P0A_bd_map,
        ROWS (mavericks_P0A_bd_map)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B,
        mavericks_P0B_bd_map,
        ROWS (mavericks_P0B_bd_map)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0C,
        mavericks_P0C_bd_map,
        ROWS (mavericks_P0C_bd_map)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU,
        /*  this type is used on harlyn model also */
        mavericks_P0B_emu_bd_map,
        ROWS (mavericks_P0B_emu_bd_map)
    },
    {BF_PLTFM_BD_ID_MONTARA_P0C, montara_P0C_bd_map, ROWS (montara_P0C_bd_map)}
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
#else
/* bf_pltfm_type as the index.
 * by tsihang, 2021-07-14. */
pltfm_bd_map_t pltfm_board_maps[] = {
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU,
        /*  this type is used on harlyn model also */
        mavericks_P0B_emu_bd_map,
        ROWS (mavericks_P0B_emu_bd_map)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B,
        mavericks_P0B_bd_map_x564p,
        ROWS (mavericks_P0B_bd_map_x564p)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B,
        mavericks_P0B_bd_map_x532p,
        ROWS (mavericks_P0B_bd_map_x532p)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B,
        mavericks_P0B_bd_map_x308p,
        ROWS (mavericks_P0B_bd_map_x308p)
    },
    {
        BF_PLTFM_BD_ID_MONTARA_P0B,
        montara_P0B_bd_map_x312p,
        ROWS (montara_P0B_bd_map_x312p)
    },
    {
        BF_PLTFM_BD_ID_MAVERICKS_P0B,
        mavericks_P0B_bd_map_hc36y24c,
        ROWS (mavericks_P0B_bd_map_hc36y24c)
    },
};
#endif
pltfm_bd_map_t *platform_pltfm_bd_map_get (
    int *rows)
{
    if (platform_type_equal (X564P)) {
        return &pltfm_board_maps[X564P];
    } else if (platform_type_equal (X532P)) {
        return &pltfm_board_maps[X532P];
    } else if (platform_type_equal (X308P)) {
        return &pltfm_board_maps[X308P];
    } else if (platform_type_equal (X312P)) {
        return &pltfm_board_maps[X312P];
    } else if (platform_type_equal (HC)) {
        return &pltfm_board_maps[HC];
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
        snprintf (name, name_size, "Board type: Error");
        name[name_size - 1] = '\0';
        return sts;
    }
    switch (bd_id) {
        case BF_PLTFM_BD_ID_MAVERICKS_P0A:
            snprintf (name, name_size,
                      "Board type: MAVERICKS_P0A");
            break;
        case BF_PLTFM_BD_ID_MAVERICKS_P0B:
            snprintf (name, name_size,
                      "Board type: MAVERICKS_P0B");
            break;
        case BF_PLTFM_BD_ID_MAVERICKS_P0C:
            snprintf (name, name_size,
                      "Board type: MAVERICKS_P0C");
            break;
        case BF_PLTFM_BD_ID_MONTARA_P0A:
            snprintf (name, name_size,
                      "Board type: MONTARA_P0A");
            break;
        case BF_PLTFM_BD_ID_MONTARA_P0B:
            snprintf (name, name_size,
                      "Board type: MONTARA_P0B");
            break;
        case BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU:
            snprintf (name, name_size,
                      "Board type: MAVERICKS_P0B_EMU");
            break;
        case BF_PLTFM_BD_ID_MONTARA_P0C:
            snprintf (name, name_size,
                      "Board type: MONTARA_P0C");
            break;
        default:
            snprintf (name, name_size, "Board type: Unknown");
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
