/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file diag_server.h
 * @date
 *
 * Contains definitions of diag server
 *
 */
#ifndef _MAV_DIAG_SERVER_H
#define _MAV_DIAG_SERVER_H

#include <stdint.h>
#include "stdbool.h"
#include <bf_types/bf_types.h>
#include <diags/bf_diag_api.h>
#include <tofino/pdfixed/pd_common.h>

/* Module header includes */

#define MAV_DIAG_DEF_DEV_ID 0
#define MAV_DIAG_FILE_RESP_MAX_SIZE 64 * 1024
/* Default TCP port base. Can be overridden by command-line */
#define MAV_DIAG_TCP_PORT_BASE_DEFAULT 17000
#define MAV_DIAG_CLIENT_CMD_MAX_SIZE 256
#define MAV_DIAG_SUBTOKEN_CNT_LIMIT 15
#define MAV_DIAG_SUBTOKEN_STR_SIZE 80
#define MAV_DIAG_CMD_OUT_FILENAME "/tmp/mav_diag_result"
#define MAV_DIAG_PRINT printf
/* include internal ports that start from port 600 */
#define MAV_DIAG_MAX_PORTS 700

typedef int diag_front_port_t;

/* LED colors */
typedef enum mav_diag_led_color_e {
    MAV_DIAG_LED_OFF = 0,
    MAV_DIAG_LED_RED,
    MAV_DIAG_LED_GREEN,
    MAV_DIAG_LED_BLUE,
} mav_diag_led_color_t;

/* Linespeed init command params */
typedef struct mav_diag_linespeed_params_s {
    bf_dev_id_t dev_id;                 /* init arg */
    bf_diag_sess_hdl_t sess_hdl;        /* init arg */
    int start_port;                     /* init arg */
    int num_ports;                      /* init arg */
    bf_diag_port_lpbk_mode_e loop_mode; /* init arg */
    uint64_t time;                      /* run arg */
    uint32_t pkt_size;                  /* run arg */
    int num_packet;                     /* run arg */
    bool valid;
} mav_diag_linespeed_params_t;

/* Linespeed command related info */
typedef struct mav_diag_linespeed_info_s {
    mav_diag_linespeed_params_t params;
} mav_diag_linespeed_info_t;

#define MAV_DIAG_STATS_MAX_INTERVALS 2
typedef struct mav_diag_port_stats_s {
    uint64_t rx_bytes;
    uint64_t rx_pkts;
    uint64_t rx_err_pkts;
    uint64_t rx_fcs_err_pkts;
    uint64_t tx_pkts;
    uint64_t tx_err_pkts;
} mav_diag_port_stats_t;

typedef struct mav_diag_info_s {
    int client_sock;
    int server_sock;
    FILE *resp_filep;
    char file_resp_str[MAV_DIAG_FILE_RESP_MAX_SIZE];
    /* State info */
    mav_diag_linespeed_info_t
    linespeed; /* linespeed cmd info */
    mav_diag_port_stats_t
    port_stats[MAV_DIAG_MAX_PORTS][MAV_DIAG_STATS_MAX_INTERVALS];
} mav_diag_info_t;

#endif
