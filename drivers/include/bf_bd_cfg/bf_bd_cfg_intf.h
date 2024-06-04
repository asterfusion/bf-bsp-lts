/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef BF_PLTFM_BD_CFG_INTF_H_INCLUDED
#define BF_PLTFM_BD_CFG_INTF_H_INCLUDED

#include <bf_pltfm_types/bf_pltfm_types.h>

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

//#define QSFP_NUM_CHN 4

typedef bf_pltfm_status_t (
    *bf_pltfm_bd_cfg_qsfp_type_fn) (
        bf_pltfm_port_info_t *port_info,
        bf_pltfm_qsfp_type_t *qsfp_type);

/*
 * Encapsulates the information of the MAC on the board
 */
typedef struct bf_pltfm_mac_lane_info_t {
    uint32_t tx_phy_lane_id;  // Identifies the TX physical lane within the MAC
    uint32_t rx_phy_lane_id;  // Identifies the RX physical lane within the MAC
    bool tx_phy_pn_swap;
    bool rx_phy_pn_swap;
    uint32_t tx_attn;
    uint32_t tx_pre;
    uint32_t tx_post;
} bf_pltfm_mac_lane_info_t;

/*
 * Repeater information for a port and lane combination
 */
typedef struct bf_pltfm_rptr_info_t {
    uint8_t rptr_num;        /* Repeater Number */
    uint8_t chnl_num;        /* Channnel number */
    uint32_t eq_bst1;        /* Equalization boost 1 */
    uint32_t eq_bst2;        /* Equalization boost 2 */
    uint32_t eq_bw;          /* Equalization bandwidth */
    uint32_t eq_bypass_bst1; /* Equalization bypass boost */
    uint32_t vod;            /* VOD for signal attenuation */
    uint32_t regF;           /* Gain for the channel */
    uint32_t high_gain;      /* High Gain for the channel*/
} bf_pltfm_rptr_info_t;

/*
 * Repeater mode as Egress or Ingress
 */
typedef enum bf_pltfm_rptr_mode_e {
    BF_PLTFM_RPTR_EGRESS = 0,
    BF_PLTFM_RPTR_INGRESS = 1,
} bf_pltfm_rptr_mode_t;

/*
 * Retimer information for a port and lane combination
 */
typedef struct bf_pltfm_rtmr_info_t {
    uint32_t num;                  /* Retimer number */
    uint32_t i2c_addr;             /* Retimer i2c addr */
    uint32_t host_side_lane;       /* Host side lane */
    uint32_t host_side_tx_pn_swap; /* Host side tx polarity swap */
    uint32_t host_side_rx_pn_swap; /* Host side rx polarity swap */
    uint32_t line_side_lane;       /* Line side lane */
    uint32_t line_side_tx_pn_swap; /* Line side tx polarity swap */
    uint32_t line_side_rx_pn_swap; /* Line side rx polarity swap */
    uint32_t tx_main_opt;          /* Tx eq main */
    uint32_t tx_precursor_opt;     /* Tx eq precursor */
    uint32_t tx_post_1_opt;        /* Tx eq post 1 */
} bf_pltfm_rtmr_info_t;

/*
 * bf_pltfm_bd_cfg_rptr_info_get
 * @param port_info info struct of the port on the board
 * @param mode mode of the repeater lane i.e egress or ingress
 * @param qsfp_type type of the QSFP connected to the port
 * @return rptr_info info struct of repeater for particular port and channel
 *
 */
bf_pltfm_status_t bf_pltfm_bd_cfg_rptr_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    bf_pltfm_qsfp_type_t qsfp_type,
    bf_pltfm_rptr_info_t *rptr_info);

/*
 * bf_bd_cfg_rptr_addr_get
 * @param port_info info struct of the port on the board
 * @param mode mode of the repeater lane i.e egress or ingress
 * @return addr i2c address of the repeater lane
 */
bf_pltfm_status_t bf_bd_cfg_rptr_addr_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    uint8_t *i2c_addr);

/*
 * bf_bd_cfg_port_has_rtmr
 * @param port_info info struct of the port on the board
 * @return has_rtmr flag to indicate if a retimer is present
 */
bf_pltfm_status_t bf_bd_cfg_port_has_rtmr (
    bf_pltfm_port_info_t *port_info,
    bool *has_rtmr);

/*
 * bf_bd_cfg_rtmr_info_get
 * @param port_info info struct of the port on the board
 * @return rtmr_info info struct of retimer for particular port and channel
 *
 */
bf_pltfm_status_t bf_bd_cfg_rtmr_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rtmr_info_t *rtmr_info);

/*
 * bf_bd_cfg_port_mac_get
 * @param port_info info struct of the port on the board
 * @return mac_id mac block id connected to the port on the board
 * @return mac_chnl_id channel id within the mac block connected to the port on
 * the board
 */
bf_pltfm_status_t bf_bd_cfg_port_mac_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *mac_id,
    uint32_t *mac_chnl_id);
uint32_t bf_bd_first_conn_ch_get (uint32_t
                                  connector);

/*
 * bf_bd_cfg_port_info_get
 * @param mac_id block id connected to the port on the board
 * @param mac_chnl_id channel id within the mac block connected to the port on
 * the board
 * @return port_info info struct of the port on the board
 */
bf_pltfm_status_t bf_bd_cfg_port_info_get (
    uint32_t mac_id,
    uint32_t mac_chnl_id,
    bf_pltfm_port_info_t *port_info);

/*
 * bf_bd_cfg_port_tx_phy_lane_get
 * @param port_info info struct of the port on the board
 * @return tx_phy_lane_id tx lane id within the mac block connected to the port
 * on the board
 */
bf_pltfm_status_t bf_bd_cfg_port_tx_phy_lane_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *tx_phy_lane_id);
/*
 * bf_bd_cfg_port_rx_phy_lane_get
 * @param port_info info struct of the port on the board
 * @return rx_phy_lane_id rx lane id within the mac block connected to the port
 * on the board
 */
bf_pltfm_status_t bf_bd_cfg_port_rx_phy_lane_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *rx_phy_lane_id);

/*
 * bf_bd_cfg_port_tx_pn_swap_get
 * @param port_info info struct of the port on the board
 * @return tx_phy_pn_swap polarity inversion of the tx lane in a MAC block
 * connected to the port on the board
 */
bf_pltfm_status_t bf_bd_cfg_port_tx_pn_swap_get (
    bf_pltfm_port_info_t *port_info,
    bool *tx_phy_pn_swap);

/*
 * bf_bd_cfg_port_rx_pn_swap_get
 * @param port_info info struct of the port on the board
 * @return rx_phy_pn_swap polarity inversion of the rx lane in a MAC block
 * connected to the port on the board
 */
bf_pltfm_status_t bf_bd_cfg_port_rx_pn_swap_get (
    bf_pltfm_port_info_t *port_info,
    bool *rx_phy_pn_swap);

/*
 * bf_bd_cfg_port_tx_lane_attn_get
 * @param port_info info struct of the port on the board
 * @param qsfp_type type of the QSFP connected to the port
 * @return tx_attn tx attenuation of the lane on the board
 */
bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_attn_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *tx_attn);

/*
 * bf_bd_cfg_port_tx_lane_pre_emp_get
 * @param port_info info struct of the port on the board
 * @param qsfp_type type of the QSFP connected to the port
 * @return pre_emp pre-emphasis of the TX lane on the board
 */
bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_pre_emp_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *pre_emp);

/*
 * bf_bd_cfg_port_tx_lane_post_emp_get
 * @param port_info info struct of the port on the board
 * @param qsfp_type type of the QSFP connected to the port
 * @return post_emp post-emphasis of the TX lane on the board
 */
bf_pltfm_status_t
bf_bd_cfg_port_tx_lane_post_emp_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    uint32_t *post_emp);

/*
 * bf_bd_cfg_port_mac_lane_info_get
 * @param port_info info struct of the port on the board
 * @param qsfp_type type of the QSFP connected to the port
 * @return lane_info info struct containing info about the tx, rx lane id and
 * their polarities
 */
bf_pltfm_status_t
bf_bd_cfg_port_mac_lane_info_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t qsfp_type,
    bf_pltfm_mac_lane_info_t *lane_info);

/*
 * bf_bd_cfg_port_mac_lane_info_update
 * @param port_info info struct of the port on the board
 * @param lane_info info struct containing info about the MAC
 * @return Status of the API call
 */
bf_pltfm_status_t
bf_bd_cfg_port_mac_lane_info_update (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_mac_lane_info_t *lane_info);

/*
 * bf_bd_cfg_port_has_rptr
 * @param port_info info struct of the port on the board
 * @return has_rptr flag to indicate if a repeater is present
 */
bf_pltfm_status_t bf_bd_cfg_port_has_rptr (
    bf_pltfm_port_info_t *port_info,
    bool *has_rptr);

/*
 * bf_bd_cfg_init
 */
bf_pltfm_status_t bf_bd_cfg_init();

/*
 * bf_bd_cfg_bd_num_port_get
 * @return number of ports on the underlying platform
 */
int bf_bd_cfg_bd_num_port_get();

int bf_bd_cfg_bd_port_num_nlanes_get(
    uint32_t conn_id);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_bd_cfg_ucli_node_create (
    ucli_node_t *m);
#endif

/*
 * bf_bd_cfg_qsfp_type_fn_reg
 */
bf_pltfm_status_t bf_bd_cfg_qsfp_type_fn_reg (
    bf_pltfm_bd_cfg_qsfp_type_fn fn);

/*
 * bf_bd_cfg_serdes_info_set
 */
bf_pltfm_status_t bf_bd_cfg_serdes_info_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_mac_lane_info_t *serdes_info);

/*
 * bf_bd_is_this_port_internal
 */
int bf_bd_is_this_port_internal (uint32_t conn_id,
                                 uint32_t chnl_id);

/*
 * Return true if channel is found on board-map
 */
int bf_bd_is_this_channel_valid (uint32_t conn_id,
                                 uint32_t chnl_id);

/*
 * Returns serdes tx coefficients
 */
bf_pltfm_status_t
bf_bd_port_serdes_tx_params_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfpdd_type_t qsfpdd_type,
    bf_pltfm_serdes_lane_tx_eq_t *tx_eq_dst,
    bf_pltfm_encoding_type_t encoding);

/*
 * Returns serdes tx coefficients
 */
bf_pltfm_status_t bf_bd_port_serdes_polarity_get (
    bf_pltfm_port_info_t *port_info, int *rx_inv,
    int *tx_inv);

/*
 * Return device id
 */
uint32_t bf_bd_get_dev_id (uint32_t conn_id,
                           uint32_t chnl_id);

/*
* Retuns num of serdes lanes per logical channel
*/
bf_pltfm_status_t
bf_bd_cfg_port_nlanes_per_ch_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *num_lanes);

/*
* Returns whether channel has multi-lanes or not
*/
bf_pltfm_status_t
bf_bd_cfg_port_is_multilane_channel_get (
    bf_pltfm_port_info_t *port_info,
    bool *is_multi_lane_conn);

bf_pltfm_status_t bf_bd_cfg_qsfp_ch_get (
    bf_pltfm_port_info_t *port_info,
    uint32_t *qsfp_ch);

bf_pltfm_status_t bf_bd_cfg_pin_name_get (
    bf_pltfm_port_info_t *port_info,
    char *pin_name);

/*
 * returns 1, if conn/channel is found in the board-map,
 * but non functional like temp non-operational or
 * some mac chans are not routed * on hardware.
 *
 * Note: Mark this port is_internal_port = 1 as well to avoid qsfp-fsm
 *
 */
int bf_bd_is_this_port_non_func(uint32_t conn_id, uint32_t chnl_id);

#ifdef __cplusplus
}
#endif /* C++ */

#endif  // bf_pltfm_bd_intf_h_included
