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
#include <bf_pltfm_bd_cfg.h>

static bf_pltfm_bd_cfg_qsfp_type_fn qsfp_type_get
    = NULL;

static int num_board_ports;

/******************************************************************************
*
******************************************************************************/
static bd_map_ent_t *pltfm_bd_map_find (
    uint32_t connector, uint32_t channel)
{
    pltfm_bd_map_t *bd_map;
    int bd_map_rows;
    uint32_t row;

    bd_map = platform_pltfm_bd_map_get (&bd_map_rows);
    if (!bd_map) {
        return NULL;
    }
    for (row = 0; row < bd_map->rows; row++) {
        if ((bd_map->bd_map[row].connector == connector)
            &&
            (bd_map->bd_map[row].channel == channel)) {
            return & (bd_map->bd_map[row]);
        }
    }
    return NULL;
}

// Public functions
bf_pltfm_status_t bf_bd_cfg_port_mac_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *mac_id,
    uint32_t *mac_chnl_id)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *mac_id = bd_map->mac_block;
    *mac_chnl_id = bd_map->mac_ch;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_info_get (
    uint32_t mac_id, uint32_t mac_chnl_id,
    bf_pltfm_port_info_t *port_info)
{
    pltfm_bd_map_t *bd_map;
    int bd_map_rows;
    uint32_t row;

    bd_map = platform_pltfm_bd_map_get (&bd_map_rows);
    if (!bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }
    for (row = 0; row < bd_map->rows; row++) {
        if ((bd_map->bd_map[row].mac_block == mac_id) &&
            (bd_map->bd_map[row].mac_ch == mac_chnl_id)) {
            port_info->conn_id =
                bd_map->bd_map[row].connector;
            port_info->chnl_id = bd_map->bd_map[row].channel;
            return BF_PLTFM_SUCCESS;
        }
    }
    return BF_PLTFM_OBJECT_NOT_FOUND;
}

bf_pltfm_status_t bf_bd_cfg_port_tx_phy_lane_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *tx_phy_lane_id)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *tx_phy_lane_id = bd_map->tx_lane;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_rx_phy_lane_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *rx_phy_lane_id)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *rx_phy_lane_id = bd_map->rx_lane;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_tx_pn_swap_get (
    bf_pltfm_port_info_t *port_info,
    bool *tx_phy_pn_swap)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *tx_phy_pn_swap = (bd_map->tx_pn_swap ? true :
                       false);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_rx_pn_swap_get (
    bf_pltfm_port_info_t *port_info,
    bool *rx_phy_pn_swap)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *rx_phy_pn_swap = (bd_map->rx_pn_swap ? true :
                       false);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_attn_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *tx_attn)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    *tx_attn = bd_map->tx_eq_attn[qsfp_type];

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_pre_emp_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *pre_emp)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    *pre_emp = bd_map->tx_eq_pre[qsfp_type];

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_post_emp_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *post_emp)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    *post_emp = bd_map->tx_eq_post[qsfp_type];

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_bd_cfg_port_mac_lane_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    bf_pltfm_mac_lane_info_t *lane_info)
{
    if (NULL == port_info || NULL == lane_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    lane_info->tx_phy_lane_id = bd_map->tx_lane;
    lane_info->rx_phy_lane_id = bd_map->rx_lane;
    lane_info->tx_phy_pn_swap = (bd_map->tx_pn_swap ?
                                 true : false);
    lane_info->rx_phy_pn_swap = (bd_map->rx_pn_swap ?
                                 true : false);
    lane_info->tx_attn =
        bd_map->tx_eq_attn[qsfp_type];
    lane_info->tx_pre = bd_map->tx_eq_pre[qsfp_type];
    lane_info->tx_post =
        bd_map->tx_eq_post[qsfp_type];

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_bd_cfg_port_mac_lane_info_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    bf_pltfm_mac_lane_info_t *lane_info)
{
    if (NULL == port_info || NULL == lane_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    bd_map->tx_lane = lane_info->tx_phy_lane_id;
    bd_map->rx_lane = lane_info->rx_phy_lane_id;
    bd_map->tx_pn_swap = lane_info->tx_phy_pn_swap ?
                         1 : 0;
    bd_map->rx_pn_swap = lane_info->rx_phy_pn_swap ?
                         1 : 0;
    bd_map->tx_eq_attn[qsfp_type] =
        lane_info->tx_attn;
    bd_map->tx_eq_pre[qsfp_type] = lane_info->tx_pre;
    bd_map->tx_eq_post[qsfp_type] =
        lane_info->tx_post;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_bd_cfg_port_mac_lane_info_update (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_mac_lane_info_t *lane_info)
{
    int qsfp_id;

    if (NULL == port_info || NULL == lane_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    for (qsfp_id = 0; qsfp_id < MAX_QSFP_TYPES;
         qsfp_id++) {
        bd_map->tx_lane = lane_info->tx_phy_lane_id;
        bd_map->rx_lane = lane_info->rx_phy_lane_id;
        bd_map->tx_pn_swap = lane_info->tx_phy_pn_swap ?
                             1 : 0;
        bd_map->rx_pn_swap = lane_info->rx_phy_pn_swap ?
                             1 : 0;
        bd_map->tx_eq_attn[qsfp_id] = lane_info->tx_attn;
        bd_map->tx_eq_pre[qsfp_id] = lane_info->tx_pre;
        bd_map->tx_eq_post[qsfp_id] = lane_info->tx_post;
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_has_rptr (
    bf_pltfm_port_info_t *port_info,
    bool *has_rptr)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *has_rptr = (bd_map->has_repeater ? true : false);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_rptr_addr_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    uint8_t *i2c_addr)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (bd_map->has_repeater == 0) {
        *i2c_addr = 0xff;
        return BF_PLTFM_SUCCESS;
    }

    if (mode == BF_PLTFM_RPTR_EGRESS) {
        *i2c_addr = bd_map->eg_rptr_i2c_addr;
    } else if (mode == BF_PLTFM_RPTR_INGRESS) {
        *i2c_addr = bd_map->ig_rptr_i2c_addr;
    } else {
        return BF_PLTFM_INVALID_ARG;
    }
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_cfg_rptr_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    bf_pltfm_qsfp_type_t qsfp_type,
    bf_pltfm_rptr_info_t *rptr_info)
{
    if (NULL == port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (qsfp_type >= BF_PLTFM_QSFP_UNKNOWN) {
        qsfp_type =
            BF_PLTFM_QSFP_CU_LOOP; /* Default to loopback */
    }

    if (bd_map->has_repeater == 0) {
        rptr_info->rptr_num = 0xff;
        rptr_info->chnl_num = 0xff;
        rptr_info->eq_bst1 = 0xff;
        rptr_info->eq_bst2 = 0xff;
        rptr_info->eq_bw = 0xff;
        rptr_info->eq_bypass_bst1 = 0xff;
        rptr_info->vod = 0xff;
        rptr_info->regF = 0xff;
        rptr_info->high_gain = 0xff;
        return BF_PLTFM_SUCCESS;
    }

    if (mode == BF_PLTFM_RPTR_EGRESS) {
        rptr_info->rptr_num = bd_map->eg_rptr_nbr;
        rptr_info->chnl_num = bd_map->eg_rptr_port;
        rptr_info->eq_bst1 =
            bd_map->eg_rptr_eq_bst1[qsfp_type];
        rptr_info->eq_bst2 =
            bd_map->eg_rptr_eq_bst2[qsfp_type];
        rptr_info->eq_bw =
            bd_map->eg_rptr_eq_bw[qsfp_type];
        rptr_info->eq_bypass_bst1 =
            bd_map->eg_rptr_eq_bst_bypass[qsfp_type];
        rptr_info->vod = bd_map->eg_rptr_vod[qsfp_type];
        rptr_info->regF = bd_map->eg_rptr_regF[qsfp_type];
        rptr_info->high_gain =
            bd_map->eg_rptr_high_gain[qsfp_type];
    } else if (mode == BF_PLTFM_RPTR_INGRESS) {
        rptr_info->rptr_num = bd_map->ig_rptr_nbr;
        rptr_info->chnl_num = bd_map->ig_rptr_port;
        rptr_info->eq_bst1 =
            bd_map->ig_rptr_eq_bst1[qsfp_type];
        rptr_info->eq_bst2 =
            bd_map->ig_rptr_eq_bst2[qsfp_type];
        rptr_info->eq_bw =
            bd_map->ig_rptr_eq_bw[qsfp_type];
        rptr_info->eq_bypass_bst1 =
            bd_map->ig_rptr_eq_bst_bypass[qsfp_type];
        rptr_info->vod = bd_map->ig_rptr_vod[qsfp_type];
        rptr_info->regF = bd_map->ig_rptr_regF[qsfp_type];
        rptr_info->high_gain =
            bd_map->ig_rptr_high_gain[qsfp_type];
    } else {
        return BF_PLTFM_INVALID_ARG;
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_port_has_rtmr (
    bf_pltfm_port_info_t *port_info,
    bool *has_rtmr)
{
    if ((NULL == port_info) || (NULL == has_rtmr)) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *has_rtmr = (bd_map->has_rtmr ? true : false);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_rtmr_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rtmr_info_t *rtmr_info)
{
    if ((NULL == port_info) || (NULL == rtmr_info)) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (bd_map->has_rtmr == 0) {
        rtmr_info->num = 0xff;
        rtmr_info->i2c_addr = 0xff;
        rtmr_info->host_side_lane = 0xff;
        rtmr_info->host_side_tx_pn_swap = 0xff;
        rtmr_info->host_side_rx_pn_swap = 0xff;
        rtmr_info->line_side_lane = 0xff;
        rtmr_info->line_side_tx_pn_swap = 0xff;
        rtmr_info->line_side_rx_pn_swap = 0xff;
        rtmr_info->tx_main_opt = 0xff;
        rtmr_info->tx_precursor_opt = 0xff;
        rtmr_info->tx_post_1_opt = 0xff;

        return BF_PLTFM_INVALID_ARG;
    }

    rtmr_info->num = bd_map->rtmr_nbr;
    rtmr_info->i2c_addr = bd_map->rtmr_i2c_addr;
    rtmr_info->host_side_lane =
        bd_map->rtmr_host_side_lane;
    rtmr_info->host_side_tx_pn_swap =
        bd_map->rtmr_host_side_tx_pn_swap;
    rtmr_info->host_side_rx_pn_swap =
        bd_map->rtmr_host_side_rx_pn_swap;
    rtmr_info->line_side_lane =
        bd_map->rtmr_line_side_lane;
    rtmr_info->line_side_tx_pn_swap =
        bd_map->rtmr_line_side_tx_pn_swap;
    rtmr_info->line_side_rx_pn_swap =
        bd_map->rtmr_line_side_rx_pn_swap;
    rtmr_info->tx_main_opt = bd_map->rtmr_tx_main_opt;
    rtmr_info->tx_precursor_opt =
        bd_map->rtmr_tx_precursor_opt;
    rtmr_info->tx_post_1_opt =
        bd_map->rtmr_tx_post_1_opt;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_cfg_init()
{
    num_board_ports = platform_num_ports_get();
    if (num_board_ports == 0) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }
    return BF_PLTFM_SUCCESS;
}

int bf_bd_cfg_bd_num_port_get()
{
    return num_board_ports;
}

void pltfm_port_info_to_str (bf_pltfm_port_info_t
                             *port_info,
                             char *str_hdl,
                             uint32_t str_len)
{
    if (!str_hdl || !port_info) {
        return;
    }

    snprintf (str_hdl, str_len, "%d/%d",
              port_info->conn_id, port_info->chnl_id);

    return;
}

bf_pltfm_status_t pltfm_port_str_to_info (
    const char *str,
    bf_pltfm_port_info_t *port_info)
{
    int len, i;
    char *tmp_str, *ptr;
    bf_pltfm_status_t sts = BF_PLTFM_SUCCESS;

    if (!str || !port_info) {
        return BF_PLTFM_INVALID_ARG;
    }

    len = strlen (str);
    tmp_str = (char *)bf_sys_malloc (sizeof (str) *
                                     (len + 1));
    if (tmp_str == NULL) {
        return BF_PLTFM_INVALID_ARG;
    }
    strncpy (tmp_str, str, len + 1);
    tmp_str[len] = '\0';

    ptr = tmp_str;
    i = 0;
    while (*ptr != '/' && i < len) {
        ptr++;
        i++;
    }
    if (i == len) {
        sts = BF_PLTFM_INVALID_ARG;
        goto end;
    }
    *ptr = '\0';
    if (*tmp_str == '-') {
        port_info->conn_id = -1;
    } else {
        port_info->conn_id = strtoul (tmp_str, NULL, 10);
    }
    ptr++;
    if (*ptr == '-') {
        port_info->chnl_id = -1;
    } else {
        port_info->chnl_id = strtoul (ptr, NULL, 10);
    }
end:
    bf_sys_free (tmp_str);
    return sts;
}

bf_pltfm_status_t bf_bd_cfg_qsfp_type_fn_reg (
    bf_pltfm_bd_cfg_qsfp_type_fn fn)
{
    if (!fn) {
        return BF_PLTFM_INVALID_ARG;
    }
    qsfp_type_get = fn;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t pltfm_port_qsfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type)
{
    if (!qsfp_type_get) {
        assert (0);
    }
    return qsfp_type_get (port_info, qsfp_type);
}

bf_pltfm_status_t bf_bd_cfg_serdes_info_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_mac_lane_info_t *serdes_info)
{
    bf_pltfm_status_t sts;
    bf_pltfm_qsfp_type_t qsfp_type =
        BF_PLTFM_QSFP_UNKNOWN;
    bf_pltfm_mac_lane_info_t mac_lane_info;

    // Safety checks
    if (!port_info) {
        return BF_INVALID_ARG;
    }
    if (!serdes_info) {
        return BF_INVALID_ARG;
    }

    // Get the type of the QSFP connected to the port
    sts = pltfm_port_qsfp_type_get (port_info,
                                    &qsfp_type);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Unable to get the type of the QSFP connected to front port %d/%d : %s "
            "(%d)",
            port_info->conn_id,
            port_info->chnl_id,
            bf_pltfm_err_str (sts),
            sts);
    }

    /* Get current values */
    sts = bf_bd_cfg_port_mac_lane_info_get (
              port_info, qsfp_type, &mac_lane_info);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Unable to get the mac lane info for front port %d/%d : %s (%d)",
                   port_info->conn_id,
                   port_info->chnl_id,
                   bf_pltfm_err_str (sts),
                   sts);
        return sts;
    }

    /* Set the new values */
    mac_lane_info.tx_attn = serdes_info->tx_attn;
    mac_lane_info.tx_pre = serdes_info->tx_pre;
    mac_lane_info.tx_post = serdes_info->tx_post;

    sts = bf_pltfm_bd_cfg_port_mac_lane_info_set (
              port_info, qsfp_type, &mac_lane_info);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Unable to get the mac lane info for front port %d/%d : %s (%d)",
                   port_info->conn_id,
                   port_info->chnl_id,
                   bf_pltfm_err_str (sts),
                   sts);
        return sts;
    }

    return BF_SUCCESS;
}

/*
 * returns true if port is internal.
 */
int bf_bd_is_this_port_internal (uint32_t conn_id,
                                 uint32_t chnl_id)
{

    bd_map_ent_t *bd_map = pltfm_bd_map_find (conn_id,
                           chnl_id);

    if (NULL == bd_map) {
        return 0;
    }

    return (bd_map->is_internal_port ? 1 : 0);
}

/*
 * returns 1, if conn/channel is found in the board-map
 */
int bf_bd_is_this_channel_valid (uint32_t conn_id,
                                 uint32_t chnl_id)
{

    bd_map_ent_t *bd_map = pltfm_bd_map_find (conn_id,
                           chnl_id);

    if (NULL == bd_map) {
        return 0;
    }

    return 1;
}

bf_pltfm_status_t
bf_bd_port_serdes_tx_params_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfpdd_type_t qsfpdd_type,
    bf_pltfm_serdes_lane_tx_eq_t *tx_eq_dst,
    bf_pltfm_encoding_type_t encoding)
{

    if (NULL == port_info || NULL == tx_eq_dst) {
        return BF_PLTFM_INVALID_ARG;
    }

    if (qsfpdd_type >= BF_PLTFM_QSFPDD_UNKNOWN) {
        LOG_ERROR ("QSFPDD: Serdes TX params requested for unknown module type "
                   "connector:%d/%d encoding:%d, qsfpdd_type:%d",
                   port_info->conn_id, port_info->chnl_id, encoding,
                   qsfpdd_type);
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    if (!bd_map->tx_eq_for_qsfpdd[encoding]) {
        LOG_ERROR ("QSFPDD: Serdes TX params entry does not exist "
                   "in board-map for connector:%d/%d "
                   "encoding %d, qsfpdd_type %d",
                   port_info->conn_id, port_info->chnl_id, encoding,
                   qsfpdd_type);
        return BF_PLTFM_INVALID_ARG;
    }
    tx_eq_dst->tx_main =
        bd_map->tx_eq_for_qsfpdd[encoding]->tx_main[qsfpdd_type];
    tx_eq_dst->tx_pre1 =
        bd_map->tx_eq_for_qsfpdd[encoding]->tx_pre1[qsfpdd_type];
    tx_eq_dst->tx_pre2 =
        bd_map->tx_eq_for_qsfpdd[encoding]->tx_pre2[qsfpdd_type];
    tx_eq_dst->tx_post1 =
        bd_map->tx_eq_for_qsfpdd[encoding]->tx_post1[qsfpdd_type];
    tx_eq_dst->tx_post2 =
        bd_map->tx_eq_for_qsfpdd[encoding]->tx_post2[qsfpdd_type];

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_bd_port_serdes_polarity_get (
    bf_pltfm_port_info_t *port_info, int *rx_inv,
    int *tx_inv)
{

    if (NULL == port_info || NULL == rx_inv ||
        NULL == tx_inv) {
        return BF_PLTFM_INVALID_ARG;
    }

    bd_map_ent_t *bd_map =
        pltfm_bd_map_find (port_info->conn_id,
                           port_info->chnl_id);

    if (NULL == bd_map) {
        return BF_PLTFM_OBJECT_NOT_FOUND;
    }

    *tx_inv = bd_map->tx_pn_swap;
    *rx_inv = bd_map->rx_pn_swap;

    return BF_PLTFM_SUCCESS;
}

/*
 * returns dev-id
 */
uint32_t bf_bd_get_dev_id (uint32_t conn_id,
                           uint32_t chnl_id)
{

    bd_map_ent_t *bd_map = pltfm_bd_map_find (conn_id,
                           chnl_id);

    if (NULL == bd_map) {
        return 0;
    }

    return bd_map->device_id;
}

