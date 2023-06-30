/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_BD_CFG_BD_MAP_H_
#define _BF_BD_CFG_BD_MAP_H_

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_QSFP_TYPES 6
#define MAX_QSFPDD_TYPES 8

#ifndef MAX_TF_SERDES_LANES_PER_CHAN
#define MAX_TF_SERDES_LANES_PER_CHAN 2
#endif

typedef struct serdes_lane_tx_eq_ {
    int32_t tx_main[MAX_QSFPDD_TYPES];
    int32_t tx_pre1[MAX_QSFPDD_TYPES];
    int32_t tx_pre2[MAX_QSFPDD_TYPES];
    int32_t tx_post1[MAX_QSFPDD_TYPES];
    int32_t tx_post2[MAX_QSFPDD_TYPES];
} serdes_lane_tx_eq_t;

typedef struct multi_lane_map_ {
    uint32_t tx_lane;
    uint32_t tx_pn_swap;
    uint32_t rx_lane;
    uint32_t rx_pn_swap;
    uint32_t qsfp_channel;        // head channel
    uint32_t qsfp_other_channel;  // incase of multi-lane
    // expand for serdes eq.
} multi_lane_map_t;

typedef struct bd_map_ent_t {
    uint32_t connector;
    uint32_t channel;
    uint32_t mac_block;
    uint32_t mac_ch;
    uint32_t tx_lane;
    uint32_t tx_pn_swap;
    uint32_t rx_lane;
    uint32_t rx_pn_swap;
    uint32_t tx_eq_pre[MAX_QSFP_TYPES];
    uint32_t tx_eq_post[MAX_QSFP_TYPES];
    uint32_t tx_eq_attn[MAX_QSFP_TYPES];
    uint32_t has_repeater;
    uint32_t eg_rptr_nbr;
    uint32_t eg_rptr_i2c_addr;
    uint32_t eg_rptr_port;
    uint32_t eg_rptr_eq_bst1[MAX_QSFP_TYPES];
    uint32_t eg_rptr_eq_bst2[MAX_QSFP_TYPES];
    uint32_t eg_rptr_eq_bw[MAX_QSFP_TYPES];
    uint32_t eg_rptr_eq_bst_bypass[MAX_QSFP_TYPES];
    uint32_t eg_rptr_vod[MAX_QSFP_TYPES];
    uint32_t eg_rptr_regF[MAX_QSFP_TYPES];
    uint32_t eg_rptr_high_gain[MAX_QSFP_TYPES];
    uint32_t ig_rptr_nbr;
    uint32_t ig_rptr_i2c_addr;
    uint32_t ig_rptr_port;
    uint32_t ig_rptr_eq_bst1[MAX_QSFP_TYPES];
    uint32_t ig_rptr_eq_bst2[MAX_QSFP_TYPES];
    uint32_t ig_rptr_eq_bw[MAX_QSFP_TYPES];
    uint32_t ig_rptr_eq_bst_bypass[MAX_QSFP_TYPES];
    uint32_t ig_rptr_vod[MAX_QSFP_TYPES];
    uint32_t ig_rptr_regF[MAX_QSFP_TYPES];
    uint32_t ig_rptr_high_gain[MAX_QSFP_TYPES];

    /* Re-timer params for Mavericks P0C board */
    uint32_t has_rtmr;
    uint32_t rtmr_nbr;
    uint32_t rtmr_i2c_addr;
    uint32_t rtmr_host_side_lane;
    uint32_t rtmr_host_side_tx_pn_swap;
    uint32_t rtmr_host_side_rx_pn_swap;
    uint32_t rtmr_line_side_lane;
    uint32_t rtmr_line_side_tx_pn_swap;
    uint32_t rtmr_line_side_rx_pn_swap;
    uint32_t rtmr_tx_main_opt;
    uint32_t rtmr_tx_post_1_opt;
    uint32_t rtmr_tx_precursor_opt;

    /* Internal port for Montara P0C board */
    uint32_t is_internal_port;

    // Device-id
    uint32_t device_id;

    // Serdes params for DD modules
    serdes_lane_tx_eq_t
    *tx_eq_for_qsfpdd[2];  // 0: NRZ 1:PAM4
    serdes_lane_tx_eq_t *tx_eq_for_internal_port[2];
} bd_map_ent_t;

typedef struct pltfm_bd_map_t {
    bf_pltfm_board_id_t bd_id;
    bd_map_ent_t *bd_map;
    uint32_t rows;
    uint32_t num_of_connectors;
} pltfm_bd_map_t;

#define ROWS(x) (sizeof(x) / sizeof(x[0]))

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_BD_CFG_BD_MAP_H_ */
