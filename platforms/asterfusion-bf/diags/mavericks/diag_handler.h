/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file mav_diag_handler.h
 * @date
 *
 * Contains definitions of diag commands servicing
 *
 */
#ifndef _DIAG_HANDLER_H
#define _DIAG_HANDLER_H

/* Module header includes */
#include "diag_server.h"

int mav_diag_get_num_active_pipes (
    bf_dev_id_t dev_id);
bf_status_t mav_diag_service_linespeed_init (
    bf_dev_id_t dev_id,
    int start_port,
    int num_ports,
    char *type);
bf_status_t mav_diag_service_linespeed_run (
    int time,
    int pkt_size,
    int num_packet);
bf_status_t mav_diag_service_linespeed_stop();
bf_status_t mav_diag_service_linespeed_end();
bf_status_t mav_diag_service_linespeed_show (
    char *resp_str, int max_str_len);

bf_status_t mav_diag_service_snake_init (
    bf_dev_id_t dev_id,
    int start_port,
    int num_ports,
    char *type);
bf_status_t mav_diag_service_snake_run (
    int pkt_size, int num_packet);
bf_status_t mav_diag_service_snake_stop();
bf_status_t mav_diag_service_snake_end();
bf_status_t mav_diag_service_snake_status();
bf_status_t mav_diag_service_port_status_show (
    bf_dev_id_t dev_id,
    diag_front_port_t front_port,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_phy_info_show (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_show_pmap (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_show_temp (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_vlan_destroy (
    bf_dev_id_t dev_id, int vlan_id);
bf_status_t mav_diag_service_vlan_show (
    bf_dev_id_t dev_id,
    int input_vlan_id,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_vlan_create (
    bf_dev_id_t dev_id,
    int vlan_id,
    int pbm[],
    int num_pbm_ports,
    int ubm[],
    int num_ubm_ports);
bf_status_t mav_diag_service_vlan_add (
    bf_dev_id_t dev_id,
    int vlan_id,
    int pbm[],
    int num_pbm_ports,
    int ubm[],
    int num_ubm_ports);
bf_status_t mav_diag_service_eyescan (
    bf_dev_id_t dev_id,
    diag_front_port_t front_port,
    int ber,
    char *mode,
    int lane,
    char *resp_str,
    int max_str_len);
bf_status_t mav_diag_service_led_set (
    bf_dev_id_t dev_id,
    diag_front_port_t front_port,
    char *color);
bf_status_t mav_diag_ports_add (bf_dev_id_t
                                dev_id);
bf_status_t mav_diag_service_fec (bf_dev_id_t
                                  dev_id, int port, char *fec);
bf_status_t mav_diag_service_port_speed (
    bf_dev_id_t dev_id,
    int port,
    char *speed);
bf_status_t mav_diag_service_port_autoneg (
    bf_dev_id_t dev_id,
    int front_port,
    char *autoneg);
bf_status_t mav_diag_service_port_intf (
    bf_dev_id_t dev_id,
    int port,
    char *intf);
bf_status_t mav_diag_service_pre_emphasis (
    bf_dev_id_t dev_id,
    int port,
    int value,
    bool tx_pre,
    bool tx_post,
    bool tx_main,
    bool tx_post2,
    bool tx_post3);
bf_status_t mav_diag_service_prbs_set (
    bf_dev_id_t dev_id,
    int port,
    uint32_t p_value,
    uint32_t value);
bf_status_t mav_diag_service_prbs_clean (
    bf_dev_id_t dev_id, int front_port);
bf_status_t mav_diag_service_prbs_show (
    bf_dev_id_t dev_id,
    int front_port,
    char *resp_str,
    int max_str_len);

#endif
