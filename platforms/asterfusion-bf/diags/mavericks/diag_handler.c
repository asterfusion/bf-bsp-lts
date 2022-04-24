/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file diag_handler.c
 * @date
 *
 * Contains implementation of diag commands servicing
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //strlen
#include <signal.h>
#include <inttypes.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <lld/lld_err.h>
#include <lld/lld_sku.h>
#include <lld/bf_dma_if.h>
#include <tofino/bf_pal/bf_pal_types.h>
#include <bf_pm/bf_pm_intf.h>
#include <port_mgr/bf_port_if.h>
#include <port_mgr/bf_serdes_if.h>
#include <pipe_mgr/pipe_mgr_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_led/bf_led.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_mgr/pltfm_mgr_handlers.h>
#include "diag_handler.h"
#include "diag_server.h"

extern mav_diag_info_t mav_diag_info;

static bf_diag_sess_hdl_t diag_snake_sess_hdl = 0;

#define MAV_DIAG_GET_LINESPEED_PARAMS (mav_diag_info.linespeed.params)
#define MAV_DIAG_LINESPEED_MEMSET \
  memset(&(mav_diag_info.linespeed), 0, sizeof(mav_diag_linespeed_info_t))
#define MAV_DIAG_GET_PORT_STATS(port, interval) \
  (mav_diag_info.port_stats[port][interval])
#define MAV_DIAG_PORT_STATS_MEMSET \
  memset(&(mav_diag_info.port_stats), 0, sizeof(mav_diag_info.port_stats))
#define MAV_DIAG_DEF_PORT_SPEED BF_SPEED_100G
#define MAV_DIAG_DEF_PORT_FEC BF_FEC_TYP_REED_SOLOMON
#define MAV_DIAG_CPU_PORT 64

/* Set loopback mode */
bf_status_t mav_diag_get_loopback_mode_from_str (
    char *type, bf_diag_port_lpbk_mode_e *loop_mode)
{
    if (!strncmp (type, "INTMI", strlen ("INTMI"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_MAC;
    } else if (!strncmp (type, "INTMO",
                         strlen ("INTMO"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_PCS;
    } else if (!strncmp (type, "INTPI",
                         strlen ("INTPI"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_PHY;
    } else if (!strncmp (type, "INTPO",
                         strlen ("INTPO"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_PHY;
    } else if (!strncmp (type, "EXT1",
                         strlen ("EXT1"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_EXT;
    } else if (!strncmp (type, "EXT2",
                         strlen ("EXT2"))) {
        *loop_mode = BF_DIAG_PORT_LPBK_EXT;
    } else {
        *loop_mode = BF_DIAG_PORT_LPBK_NONE;
    }

    return BF_SUCCESS;
}

/* Get the Speed string */
char *mav_diag_helper_get_speed_string (
    bf_port_speed_t speed,
    char *ret_buffer)
{
    char *buffer;

    ret_buffer[0] = '\0';

    switch (speed) {
        case BF_SPEED_NONE:
            buffer = "Unkn";
            break;
        case BF_SPEED_1G:
            buffer = "1G";
            break;
        case BF_SPEED_10G:
            buffer = "10G";
            break;
        case BF_SPEED_25G:
            buffer = "25G";
            break;
        case BF_SPEED_40G:
            buffer = "40G";
            break;
        case BF_SPEED_40G_NB:
            buffer = "40G_NB";
            break;
        case BF_SPEED_50G:
            buffer = "50G";
            break;
        case BF_SPEED_100G:
            buffer = "100G";
            break;
        default:
            buffer = "Unkn";
            break;
    }
    strncpy (ret_buffer, buffer, strlen (buffer));

    return ret_buffer;
}

/* Get the FEC string */
char *mav_diag_helper_get_fec_string (
    bf_fec_type_t fec, char *ret_buffer)
{
    char *buffer;

    ret_buffer[0] = '\0';

    switch (fec) {
        case BF_FEC_TYP_NONE:
            buffer = "None";
            break;
        case BF_FEC_TYP_FIRECODE:
            buffer = "FC";
            break;
        case BF_FEC_TYP_REED_SOLOMON:
            buffer = "RS";
            break;
        default:
            buffer = "Unkn";
            break;
    }
    strncpy (ret_buffer, buffer, strlen (buffer));

    return ret_buffer;
}

/* Get the Auto-neg string */
char *mav_diag_helper_get_autoneg_string (
    bf_an_state_e an_state,
    char *ret_buffer)
{
    char *buffer;

    ret_buffer[0] = '\0';

    switch (an_state) {
        case BF_AN_ST_NONE:
            buffer = "None";
            break;
        case BF_AN_ST_RUNNING:
            buffer = "Runn";
            break;
        case BF_AN_ST_GOOD:
            buffer = "Good";
            break;
        case BF_AN_ST_FAILED:
            buffer = "Fail";
            break;
        case BF_AN_ST_RESTARTED:
            buffer = "Restd";
            break;
        case BF_AN_ST_COMPLETE:
            buffer = "Done";
            break;
        default:
            buffer = "None";
            break;
    }

    strncpy (ret_buffer, buffer, strlen (buffer));

    return ret_buffer;
}

/* Get the qsfp code string */
char *mav_diag_helper_get_qsfp_code_string (
    Ethernet_compliance qsfp_code,
    char *ret_buffer)
{
    char *buffer;

    ret_buffer[0] = '\0';

    switch (qsfp_code) {
        case ACTIVE_CABLE:
            buffer = "Cable";
            break;
        case LR4_40GBASE:
            buffer = "LR4_40G";
            break;
        case SR4_40GBASE:
            buffer = "SR4_40G";
            break;
        case CR4_40GBASE:
            buffer = "CR4_40G";
            break;
        case SR_10GBASE:
            buffer = "SR_10G";
            break;
        case LR_10GBASE:
            buffer = "LR_10G";
            break;
        case LRM_10GBASE:
            buffer = "LRM_10G";
            break;
        case COMPLIANCE_RSVD:
            buffer = "Rsvd";
            break;
        default:
            buffer = "None";
            break;
    }

    strncpy (ret_buffer, buffer, strlen (buffer));

    return ret_buffer;
}
/* Get the qsfp code string */
char *mav_diag_helper_get_qsfp_ext_code_string (
    Ethernet_extended_compliance qsfp_code,
    char *ret_buffer)
{
    char *buffer;

    ret_buffer[0] = '\0';

    switch (qsfp_code) {
        case AOC_100G_BER_5:
        case AOC_100G_BER_12:
            buffer = "AOC_100/25G";
            break;
        case ACC_100G_BER_5:
        case ACC_100G_BER_12:
            buffer = "ACC_100/25G";
            break;
        case SR4_100GBASE:
            buffer = "SR_100/25G";
            break;
        case LR4_100GBASE:
            buffer = "LR_100/25G";
            break;
        case CR4_100GBASE:
            buffer = "CR4_100G";
            break;
        case ER4_100GBASE:
            buffer = "ER_100/25G";
            break;
        case SR10_100GBASE:
            buffer = "SR10_100G";
            break;
        case CWDM4_100G:
            buffer = "CWDM4_100G";
            break;
        case CLR4_100G:
            buffer = "CLR4_100G";
            break;
        case ER4_40GBASE:
            buffer = "ER4_40G";
            break;
        case CR_25GBASE_CA_S:
        case CR_25GBASE_CA_N:
            buffer = "CR_25G";
            break;
        case SR_10GBASE_4:
            buffer = "SR_10G_4";
            break;
        default:
            buffer = "Other";
            break;
    }

    strncpy (ret_buffer, buffer, strlen (buffer));

    return ret_buffer;
}

/* Get the Port info string */
char *mav_diag_helper_get_port_string (
    bf_pal_front_port_handle_t *port_info,
    char *ret_buffer,
    int max_len)
{
    ret_buffer[0] = '\0';

    snprintf (
        ret_buffer, max_len, "%d/%d", port_info->conn_id,
        port_info->chnl_id);

    return ret_buffer;
}

int mav_diag_get_num_active_pipes (
    bf_dev_id_t dev_id)
{
    uint32_t num_pipes = 0;

    lld_sku_get_num_active_pipes (dev_id, &num_pipes);
    return num_pipes;
}

/* Save the linespeed init command */
bf_status_t mav_diag_save_linespeed_init_cmd (
    bf_dev_id_t dev_id,
    bf_diag_sess_hdl_t sess_hdl,
    int start_port,
    int num_ports,
    bf_diag_port_lpbk_mode_e loop_mode)
{
    MAV_DIAG_PORT_STATS_MEMSET;
    MAV_DIAG_LINESPEED_MEMSET;
    MAV_DIAG_GET_LINESPEED_PARAMS.dev_id = dev_id;
    MAV_DIAG_GET_LINESPEED_PARAMS.sess_hdl = sess_hdl;
    MAV_DIAG_GET_LINESPEED_PARAMS.start_port =
        start_port;
    MAV_DIAG_GET_LINESPEED_PARAMS.num_ports =
        num_ports;
    MAV_DIAG_GET_LINESPEED_PARAMS.loop_mode =
        loop_mode;
    MAV_DIAG_GET_LINESPEED_PARAMS.valid = true;

    return BF_SUCCESS;
}

/* Set linespeed */
bf_status_t mav_diag_service_linespeed_init (
    bf_dev_id_t dev_id,
    int start_port,
    int num_ports,
    char *type)
{
    bf_diag_port_lpbk_mode_e loop_mode =
        BF_DIAG_PORT_LPBK_NONE;
    diag_front_port_t front_port = 0;
    bf_dev_port_t port_arr[MAV_DIAG_MAX_PORTS];
    int port_index = 0;
    bf_pal_front_port_handle_t port_info;
    bf_diag_sess_hdl_t sess_hdl = 0;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    /* Cleanup old config */
    if (MAV_DIAG_GET_LINESPEED_PARAMS.valid) {
        mav_diag_service_linespeed_end (dev_id);
        MAV_DIAG_LINESPEED_MEMSET;
        MAV_DIAG_PORT_STATS_MEMSET;
    }

    memset (&port_arr[0], 0, sizeof (port_arr));
    memset (&port_info, 0, sizeof (port_info));

    mav_diag_get_loopback_mode_from_str (type,
                                         &loop_mode);
    // MAV_DIAG_PRINT("Start Port %d, Num-ports %d \n", start_port, num_ports);

    for (front_port = start_port;
         front_port < (start_port + num_ports);
         front_port++) {
        port_info.conn_id = front_port;
        FP2DP (dev_id_of_port, &port_info,
               &port_arr[port_index]);
        // MAV_DIAG_PRINT(" FRONT %d , DEV-port %d \n", front_port,
        // port_arr[port_index]);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (
                " Failed to get dev-port for front-port %d, "
                "Skipping port %d \n",
                front_port,
                front_port);
            continue;
        }
        port_index++;
    }
    /* Turn off FEC if port is in mac, phy or pcs loopback
       Do this temporarily as test fails with fec enabled
    */
    if ((loop_mode != BF_DIAG_PORT_LPBK_EXT) &&
        (loop_mode != BF_DIAG_PORT_LPBK_NONE)) {
        bf_dev_port_t dev_port = 0;
        for (front_port = start_port;
             front_port < (start_port + num_ports);
             front_port++) {
            port_info.conn_id = front_port;
            FP2DP (dev_id_of_port, &port_info,
                   &dev_port);
            if (status != BF_PLTFM_SUCCESS) {
                continue;
            }
            if (front_port < BF_DIAG_MAX_PORTS) {
                bf_pm_port_fec_set (dev_id, &port_info,
                                    BF_FEC_TYP_NONE);
            }
        }
    }

    status = bf_diag_loopback_pair_test_setup (
                 dev_id, &port_arr[0], port_index, loop_mode,
                 &sess_hdl);
    if (status == BF_SUCCESS) {
        /* Save the command */
        mav_diag_save_linespeed_init_cmd (
            dev_id, sess_hdl, start_port, num_ports,
            loop_mode);
    }
    return status;
}

/* Get the mac counters */
bf_status_t mav_diag_linespeed_counter_get (
    int interval)
{
    bf_status_t status = BF_SUCCESS;
    diag_front_port_t front_port = 0;
    bf_dev_port_t dev_port = 0;
    unsigned int pipe, port;
    uint32_t num_pipes = 0;
    bf_pal_front_port_handle_t port_info;
    bf_rmon_counter_array_t counters;
    bf_dev_id_t dev_id = 0;
    bf_dev_id_t dev_id_of_port = 0;

    if (interval >= MAV_DIAG_STATS_MAX_INTERVALS) {
        return BF_INVALID_ARG;
    }

    memset (&port_info, 0, sizeof (port_info));

    dev_id = MAV_DIAG_GET_LINESPEED_PARAMS.dev_id;
    num_pipes = mav_diag_get_num_active_pipes (
                    dev_id);
    for (front_port =
             MAV_DIAG_GET_LINESPEED_PARAMS.start_port;
         front_port <
         (MAV_DIAG_GET_LINESPEED_PARAMS.start_port +
          MAV_DIAG_GET_LINESPEED_PARAMS.num_ports);
         front_port++) {
        port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            continue;
        }
        pipe = DEV_PORT_TO_PIPE (dev_port);
        port = DEV_PORT_TO_LOCAL_PORT (dev_port);

        if ((pipe >= num_pipes) ||
            (port >= BF_PIPE_PORT_COUNT)) {
            continue;
        }

        memset (&counters, 0, sizeof (counters));
        bf_pm_port_all_stats_get (dev_id, &port_info,
                                  (uint64_t *)&counters);
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).rx_bytes =
                                     counters.format.ctr_ids.OctetsReceived;
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).rx_pkts =
                                     counters.format.ctr_ids.FramesReceivedAll;
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).rx_err_pkts =
                                     counters.format.ctr_ids.FrameswithanyError;
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).rx_fcs_err_pkts =
                                     counters.format.ctr_ids.FramesReceivedwithFCSError;
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).tx_pkts =
                                     counters.format.ctr_ids.FramesTransmittedAll;
        MAV_DIAG_GET_PORT_STATS (front_port,
                                 interval).tx_err_pkts =
                                     counters.format.ctr_ids.FramesTransmittedwithError;
    }

    return BF_SUCCESS;
}

/* Run the linespeed packet test */
bf_status_t mav_diag_service_linespeed_run (
    int time,
    int pkt_size,
    int num_packet)
{
    bf_status_t status = BF_SUCCESS;
    bool bidir = false;

    status = bf_diag_loopback_pair_test_start (
                 MAV_DIAG_GET_LINESPEED_PARAMS.sess_hdl,
                 num_packet, pkt_size, bidir);

    if (status == BF_SUCCESS) {
        MAV_DIAG_GET_LINESPEED_PARAMS.time = time;
        MAV_DIAG_GET_LINESPEED_PARAMS.pkt_size = pkt_size;
        MAV_DIAG_GET_LINESPEED_PARAMS.num_packet =
            num_packet;

        MAV_DIAG_PORT_STATS_MEMSET;
        /* Sleep for sometime to make sure pkts have been injected */
        bf_sys_usleep (1500000);

        /* Cache mac stats at start of test */
        mav_diag_linespeed_counter_get (0);

        /* Time (in seconds) here is the runtime for the test,
           Sleep for that time
         */
        bf_sys_sleep (time);
        /* Get the mac stats after the test */
        mav_diag_linespeed_counter_get (1);
    }

    return status;
}

/* Stop the linespeed packet test */
bf_status_t mav_diag_service_linespeed_stop()
{
    return bf_diag_loopback_pair_test_stop (
               MAV_DIAG_GET_LINESPEED_PARAMS.sess_hdl);
}

/* End the linespeed packet test */
bf_status_t mav_diag_service_linespeed_end()
{
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    /* Cleanup the loopback test  */
    status = bf_diag_loopback_pair_test_cleanup (
                 MAV_DIAG_GET_LINESPEED_PARAMS.sess_hdl);
    if (status == BF_SUCCESS) {
        if ((MAV_DIAG_GET_LINESPEED_PARAMS.loop_mode !=
             BF_DIAG_PORT_LPBK_EXT) &&
            (MAV_DIAG_GET_LINESPEED_PARAMS.loop_mode !=
             BF_DIAG_PORT_LPBK_NONE)) {
            /* Turn on FEC back after the test if it was a loopback test */
            int front_port = 0;
            bf_pal_front_port_handle_t port_info;
            bf_dev_port_t dev_port = 0;
            bf_status_t fp_status = BF_SUCCESS;

            memset (&port_info, 0, sizeof (port_info));
            for (front_port =
                     MAV_DIAG_GET_LINESPEED_PARAMS.start_port;
                 front_port <
                 (MAV_DIAG_GET_LINESPEED_PARAMS.start_port +
                  MAV_DIAG_GET_LINESPEED_PARAMS.num_ports);
                 front_port++) {
                port_info.conn_id = front_port;
                fp_status = FP2DP (dev_id_of_port, &port_info,
                                   &dev_port);
                if (fp_status != BF_PLTFM_SUCCESS) {
                    continue;
                }
                /* Skip FEC enable on CPU port as it is not supported */
                if ((front_port < BF_DIAG_MAX_PORTS) &&
                    (dev_port != MAV_DIAG_CPU_PORT)) {
                    bf_pm_port_fec_set (
                        MAV_DIAG_GET_LINESPEED_PARAMS.dev_id,
                        &port_info,
                        MAV_DIAG_DEF_PORT_FEC);
                }
            }
        }
        MAV_DIAG_LINESPEED_MEMSET;
        MAV_DIAG_PORT_STATS_MEMSET;
    }
    return status;
}

/* Show the linespeed packet test results */
bf_status_t mav_diag_service_linespeed_show (
    char *resp_str, int max_str_len)
{
    int c_len = strlen (resp_str), port_cnt = 0;
    diag_front_port_t front_port = 0;
    bf_dev_port_t dev_port = 0;
    unsigned int pipe, port;
    uint32_t num_pipes = 0, mbps = 0;
    bf_port_speed_t speed = 0;
    bf_fec_type_t fec = 0;
    bool state = false;
    char speed_str[200], port_info_str[200];
    bf_pal_front_port_handle_t port_info;
    bf_status_t status = BF_SUCCESS,
                ret_status = BF_SUCCESS;
    bf_diag_test_status_e test_status;
    bf_diag_loopback_pair_test_stats_t cpu_stats;
    uint64_t rx_pkts = 0, rx_err_pkts = 0,
             tx_pkts = 0, tx_err_pkts = 0;
    uint64_t rx_fcs_err_pkts = 0;
    int preamble = 8;
    double per = 0;
    bf_dev_id_t dev_id = 0;
    bf_dev_id_t dev_id_of_port = 0;

    (void)port_cnt;
    memset (speed_str, 0, sizeof (speed_str));
    memset (&port_info, 0, sizeof (port_info));
    memset (&port_info_str, 0,
            sizeof (port_info_str));
    memset (&cpu_stats, 0, sizeof (cpu_stats));

    c_len += snprintf (
                 resp_str + c_len,
                 (c_len < max_str_len) ? (max_str_len - c_len - 1)
                 : 0,
                 "Port     Rx-Pkt(Err)    Tx-Pkt(Err)  Mbps     Status        PER\n");
    c_len += snprintf (
                 resp_str + c_len,
                 (c_len < max_str_len) ? (max_str_len - c_len - 1)
                 : 0,
                 "---------------------------------------------------------------\n");
    if (!MAV_DIAG_GET_LINESPEED_PARAMS.valid) {
        MAV_DIAG_PRINT ("Run linespeed init to specify ports first \n");
        return BF_INVALID_ARG;
    }
    dev_id = MAV_DIAG_GET_LINESPEED_PARAMS.dev_id;
    /* Get the test status */
    test_status =
        bf_diag_loopback_pair_test_status_get (
            MAV_DIAG_GET_LINESPEED_PARAMS.sess_hdl,
            &cpu_stats);
    if (test_status == BF_DIAG_TEST_STATUS_PASS) {
        ret_status = 0;
    } else {
        ret_status = 1;
    }
    num_pipes = mav_diag_get_num_active_pipes (
                    dev_id);
    for (front_port =
             MAV_DIAG_GET_LINESPEED_PARAMS.start_port;
         front_port <
         (MAV_DIAG_GET_LINESPEED_PARAMS.start_port +
          MAV_DIAG_GET_LINESPEED_PARAMS.num_ports);
         front_port++, port_cnt++) {
        port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            continue;
        }
        pipe = DEV_PORT_TO_PIPE (dev_port);
        port = DEV_PORT_TO_LOCAL_PORT (dev_port);

        if ((pipe >= num_pipes) ||
            (port >= BF_PIPE_PORT_COUNT)) {
            continue;
        }
        /* Get oper state and speed */
        bf_pm_port_oper_status_get (dev_id, &port_info,
                                    &state);
        bf_pm_port_speed_get (dev_id, &port_info, &speed);
        bf_pm_port_fec_get (dev_id, &port_info, &fec);
        /* Convert to strings */
        mav_diag_helper_get_speed_string (speed,
                                          speed_str);
        mav_diag_helper_get_port_string (
            &port_info, port_info_str,
            sizeof (port_info_str));

#ifdef DEVICE_IS_SW_MODEL
        rx_pkts = (port_cnt % 2) ?
                  cpu_stats.pairs[port_cnt / 2].rx_good : 0;
        rx_err_pkts = (port_cnt % 2) ?
                      cpu_stats.pairs[port_cnt / 2].rx_bad : 0;
        rx_fcs_err_pkts = (port_cnt % 2) ?
                          cpu_stats.pairs[port_cnt / 2].rx_bad : 0;
        tx_pkts = (port_cnt % 2) ? 0 :
                  cpu_stats.pairs[port_cnt / 2].tx_total;
        tx_err_pkts = 0;
#else
        rx_pkts = MAV_DIAG_GET_PORT_STATS (front_port,
                                           1).rx_pkts -
                  MAV_DIAG_GET_PORT_STATS (front_port, 0).rx_pkts;
        rx_err_pkts = MAV_DIAG_GET_PORT_STATS (front_port,
                                               1).rx_err_pkts -
                      MAV_DIAG_GET_PORT_STATS (front_port,
                                               0).rx_err_pkts;
        rx_fcs_err_pkts = MAV_DIAG_GET_PORT_STATS (
                              front_port, 1).rx_fcs_err_pkts -
                          MAV_DIAG_GET_PORT_STATS (front_port,
                                  0).rx_fcs_err_pkts;
        tx_pkts = MAV_DIAG_GET_PORT_STATS (front_port,
                                           1).tx_pkts -
                  MAV_DIAG_GET_PORT_STATS (front_port, 0).tx_pkts;
        tx_err_pkts = MAV_DIAG_GET_PORT_STATS (front_port,
                                               1).tx_err_pkts -
                      MAV_DIAG_GET_PORT_STATS (front_port,
                                               0).tx_err_pkts;
#endif

        /* Calculate packet speed */
        if (MAV_DIAG_GET_LINESPEED_PARAMS.time) {
            /* convert to mega bits/s */
            mbps = ((rx_pkts) *
                    (MAV_DIAG_GET_LINESPEED_PARAMS.pkt_size +
                     preamble) *
                    8) /
                   (MAV_DIAG_GET_LINESPEED_PARAMS.time * 1000000);
        } else {
            mbps = 0;
        }
        /* Calculate PER */
        if (rx_pkts) {
            per = (double)rx_fcs_err_pkts / rx_pkts;
        } else {
            per = 0;
        }

        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%-5s %11" PRIu64 "(%" PRIu64 ") %11" PRIu64 "(%"
                           PRIu64
                           ") %5u %-2s %-4s %-2s %10.8g\n",
                           port_info_str,
                           rx_pkts,
                           rx_err_pkts,
                           tx_pkts,
                           tx_err_pkts,
                           mbps,
                           (state) ? "up" : "dn",
                           speed_str,
                           "FD",
                           per);
    }
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "Test Period = %" PRIx64
                       " seconds\n"
                       "Packet length = %d bytes\n"
                       "Num of Packets = %d \n"
                       "%s\n",
                       MAV_DIAG_GET_LINESPEED_PARAMS.time,
                       MAV_DIAG_GET_LINESPEED_PARAMS.pkt_size,
                       MAV_DIAG_GET_LINESPEED_PARAMS.num_packet,
                       ret_status ? "FAIL" : "PASS");
    /* Return the status */
    return ret_status;
}

/* Port-Status show */
bf_status_t mav_diag_service_port_status_show (
    bf_dev_id_t dev_id,
    diag_front_port_t input_front_port,
    char *resp_str,
    int max_str_len)
{
    int lanes = 0, num_ports = 0;
    diag_front_port_t front_port = 0;
    bf_dev_port_t dev_port = 0;
    bf_port_speed_t speed = 0;
    bf_fec_type_t fec = 0;
    bf_an_state_e an_state = 0;
    bool tx_pause = false, rx_pause = false,
         in_loop = false, state = false;
    uint32_t tx_mtu = 0, rx_mtu = 0;
    char speed_str[200], autoneg_str[200],
         qsfp_code_str[200];
    char qsfp_ext_code_str[32], fec_str[200];
    char port_info_str[200], pause_str[200];
    int tx_pre = 0, tx_main = 0, tx_post = 0;
    int c_len = strlen (resp_str);
    bf_diag_port_lpbk_mode_e diag_loop_mode;
    Ethernet_compliance qsfp_code;
    Ethernet_extended_compliance qsfp_ext_code;
    bf_pal_front_port_handle_t port_info;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    memset (speed_str, 0, sizeof (speed_str));
    memset (fec_str, 0, sizeof (fec_str));
    memset (autoneg_str, 0, sizeof (autoneg_str));
    memset (qsfp_code_str, 0, sizeof (qsfp_code_str));
    memset (qsfp_ext_code_str, 0,
            sizeof (qsfp_ext_code_str));
    memset (&pause_str, 0, sizeof (pause_str));
    memset (&port_info, 0, sizeof (port_info));
    memset (&port_info_str, 0,
            sizeof (port_info_str));

    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "Port  link lns speed dlpx a-neg pause fec  txPre txMain "
                       "txPost mtu   loop Intf    Ext-intf  \n");
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "--------------------------------------------------------"
                       "----------------------------------\n");

    num_ports = bf_bd_cfg_bd_num_port_get();
    for (front_port = 1; front_port <= num_ports;
         front_port++) {
        if ((input_front_port != -1) &&
            (input_front_port != front_port)) {
            continue;
        }
        port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            continue;
        }
        memset (speed_str, 0, sizeof (speed_str));
        memset (fec_str, 0, sizeof (fec_str));
        memset (autoneg_str, 0, sizeof (autoneg_str));
        memset (qsfp_code_str, 0, sizeof (qsfp_code_str));
        memset (qsfp_ext_code_str, 0,
                sizeof (qsfp_ext_code_str));
        memset (&port_info_str, 0,
                sizeof (port_info_str));

        /* Call the get APIs */
        bf_pm_port_oper_status_get (dev_id, &port_info,
                                    &state);
        bf_port_num_lanes_get (dev_id, dev_port, &lanes);
        bf_pm_port_speed_get (dev_id, &port_info, &speed);
        bf_pm_port_fec_get (dev_id, &port_info, &fec);
        bf_port_autoneg_state_get (dev_id, dev_port,
                                   &an_state);
        bf_port_autoneg_pause_resolution_get (
            dev_id, front_port, &tx_pause, &rx_pause);
        bf_port_mtu_get (dev_id, dev_port, &tx_mtu,
                         &rx_mtu);

        /* Convert to strings */
        mav_diag_helper_get_port_string (
            &port_info, port_info_str,
            sizeof (port_info_str));
        mav_diag_helper_get_speed_string (speed,
                                          speed_str);
        mav_diag_helper_get_fec_string (fec, fec_str);
        mav_diag_helper_get_autoneg_string (an_state,
                                            autoneg_str);
        /* Is port in loop */
        bf_diag_port_loopback_mode_get (dev_id, dev_port,
                                        &diag_loop_mode);
        in_loop = false;
        if ((diag_loop_mode != BF_DIAG_PORT_LPBK_NONE) &&
            (diag_loop_mode != BF_DIAG_PORT_LPBK_EXT)) {
            in_loop = true;
        }
        /* Get qsfp code */
        qsfp_code = 0;
        bf_qsfp_get_eth_compliance (front_port,
                                    &qsfp_code);
        mav_diag_helper_get_qsfp_code_string (qsfp_code,
                                              qsfp_code_str);
        qsfp_ext_code = 0;
        bf_qsfp_get_eth_ext_compliance (front_port,
                                        &qsfp_ext_code);
        mav_diag_helper_get_qsfp_ext_code_string (
            qsfp_ext_code, qsfp_ext_code_str);
        if ((tx_pause) || (rx_pause)) {
            snprintf (
                pause_str, 200, "%s%s", tx_pause ? "TX" : "",
                rx_pause ? "RX" : "");
        } else {
            snprintf (pause_str, 200, "None");
        }
        tx_pre = 0;
        tx_main = 0;
        tx_post = 0;
        bf_serdes_tx_drv_attn_get (dev_id, dev_port, 0,
                                   &tx_main, &tx_post, &tx_pre);

        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%-5s %-4s %-3d %-5s %-4s %-5s %-5s %-4s %-5d %-6d %-6d "
                           "%-5d %-4s %-7s %-10s\n",
                           port_info_str,
                           state ? "up" : "down",
                           lanes,
                           speed_str,
                           "FD",
                           autoneg_str,
                           pause_str,
                           fec_str,
                           tx_pre,
                           tx_main,
                           tx_post,
                           tx_mtu,
                           in_loop ? "Yes" : "No",
                           qsfp_code_str,
                           qsfp_ext_code_str);
    }
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "--------------------------------------------------------"
                       "----------------------------------\n");

    return BF_SUCCESS;
}

/* Show phy info */
bf_status_t mav_diag_service_phy_info_show (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len)
{
    int c_len = strlen (resp_str);
    int num_ports = 0;
    diag_front_port_t front_port = 0;
    bf_pal_front_port_handle_t port_info;
    bf_pltfm_port_info_t pltfm_port_info;
    char port_info_str[200], speed_str[200];
    bf_dev_port_t dev_port = 0;
    int lanes = 0, chnl = 0;
    uint32_t phy_mac_blk = 0;
    bf_port_speed_t speed = 0;
    uint32_t tx_lane = 0, rx_lane = 0;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_info, 0, sizeof (port_info));
    memset (&pltfm_port_info, 0,
            sizeof (pltfm_port_info));
    memset (&port_info_str, 0,
            sizeof (port_info_str));
    memset (&speed_str, 0, sizeof (speed_str));

    c_len +=
        snprintf (resp_str + c_len,
                  (c_len < max_str_len) ? (max_str_len - c_len - 1)
                  : 0,
                  "Port dev-id dev-port phy-mac-blk speed lane tx-lane rx-lane\n");
    c_len +=
        snprintf (resp_str + c_len,
                  (c_len < max_str_len) ? (max_str_len - c_len - 1)
                  : 0,
                  "----------------------------------------------------------\n");

    num_ports = bf_bd_cfg_bd_num_port_get();
    for (front_port = 1; front_port <= num_ports;
         front_port++) {
        port_info.conn_id = front_port;
        pltfm_port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            continue;
        }
        bf_pm_port_speed_get (dev_id, &port_info, &speed);
        bf_port_num_lanes_get (dev_id, dev_port, &lanes);
        bf_port_map_dev_port_to_mac (dev_id, dev_port,
                                     &phy_mac_blk, &chnl);
        bf_bd_cfg_port_tx_phy_lane_get (&pltfm_port_info,
                                        &tx_lane);
        bf_bd_cfg_port_rx_phy_lane_get (&pltfm_port_info,
                                        &rx_lane);

        /* Convert to strings */
        mav_diag_helper_get_speed_string (speed,
                                          speed_str);
        mav_diag_helper_get_port_string (
            &port_info, port_info_str,
            sizeof (port_info_str));

        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%-5s %-6d %-8d %-11d %-4s %-4d %-7d %-7d\n",
                           port_info_str,
                           dev_id,
                           dev_port,
                           phy_mac_blk,
                           speed_str,
                           lanes,
                           tx_lane,
                           rx_lane);
    }
    c_len +=
        snprintf (resp_str + c_len,
                  (c_len < max_str_len) ? (max_str_len - c_len - 1)
                  : 0,
                  "----------------------------------------------------------\n");
    return BF_SUCCESS;
}

/* Show port map */
bf_status_t mav_diag_service_show_pmap (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len)
{
    int c_len = strlen (resp_str);
    int num_ports = 0;
    diag_front_port_t front_port = 0;
    bf_dev_port_t dev_port = 0;
    bf_pal_front_port_handle_t port_info;
    char port_info_str[200];
    uint32_t pipe_id = 0, phy_pipe_id = 0;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_info, 0, sizeof (port_info));
    memset (&port_info_str, 0,
            sizeof (port_info_str));

    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "Port  dev-port logical-pipe physical-pipe\n");
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "------------------------------------------\n");

    num_ports = bf_bd_cfg_bd_num_port_get();
    for (front_port = 1; front_port <= num_ports;
         front_port++) {
        port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            continue;
        }
        mav_diag_helper_get_port_string (
            &port_info, port_info_str,
            sizeof (port_info_str));
        pipe_id = DEV_PORT_TO_PIPE (dev_port);
        lld_sku_map_pipe_id_to_phy_pipe_id (dev_id,
                                            pipe_id, &phy_pipe_id);
        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%-5s %-8d %-12d %-13d \n",
                           port_info_str,
                           dev_port,
                           pipe_id,
                           phy_pipe_id);
    }
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "------------------------------------------\n");
    return BF_SUCCESS;
}

/* Show temperature */
bf_status_t mav_diag_service_show_temp (
    bf_dev_id_t dev_id,
    char *resp_str,
    int max_str_len)
{
    int c_len = strlen (resp_str), max_sensors = 11;
    float total_curr = 0, max_peak = 0;
    bf_pltfm_temperature_info_t info, peak;

    (void)dev_id;
    memset (&info, 0, sizeof (info));
    memset (&peak, 0, sizeof (peak));
    /* Call the API */
    bf_pltfm_temperature_get (&info, &peak);

    /* tmp1 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       1,
                       info.tmp1,
                       peak.tmp1);
    total_curr += info.tmp1;
    if (peak.tmp1 > max_peak) {
        max_peak = peak.tmp1;
    }

    /* tmp2 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       2,
                       info.tmp2,
                       peak.tmp2);
    total_curr += info.tmp2;
    if (peak.tmp2 > max_peak) {
        max_peak = peak.tmp2;
    }

    /* tmp3 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       3,
                       info.tmp3,
                       peak.tmp3);
    total_curr += info.tmp3;
    if (peak.tmp3 > max_peak) {
        max_peak = peak.tmp3;
    }

    /* tmp4 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       4,
                       info.tmp4,
                       peak.tmp4);
    total_curr += info.tmp4;
    if (peak.tmp4 > max_peak) {
        max_peak = peak.tmp4;
    }

    /* tmp5 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       5,
                       info.tmp5,
                       peak.tmp5);
    total_curr += info.tmp5;
    if (peak.tmp5 > max_peak) {
        max_peak = peak.tmp5;
    }

    /* tmp6 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       6,
                       info.tmp6,
                       peak.tmp6);
    total_curr += info.tmp6;
    if (peak.tmp6 > max_peak) {
        max_peak = peak.tmp6;
    }

    /* tmp7 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       7,
                       info.tmp7,
                       peak.tmp7);
    total_curr += info.tmp7;
    if (peak.tmp7 > max_peak) {
        max_peak = peak.tmp7;
    }

    /* tmp8 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       8,
                       info.tmp8,
                       peak.tmp8);
    total_curr += info.tmp8;
    if (peak.tmp8 > max_peak) {
        max_peak = peak.tmp8;
    }

    /* tmp9 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       9,
                       info.tmp9,
                       peak.tmp9);
    total_curr += info.tmp9;
    if (peak.tmp9 > max_peak) {
        max_peak = peak.tmp9;
    }

    /* tmp10 */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "temperature monitor %d: current=%f, peak=%f\n",
                       10,
                       info.tmp10,
                       peak.tmp10);
    total_curr += info.tmp10;
    if (peak.tmp10 > max_peak) {
        max_peak = peak.tmp10;
    }

    /* Average temp */
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "average current temperature= %f\n",
                       total_curr / max_sensors);
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "maximum peak temperature= %f\n",
                       max_peak);
    return BF_SUCCESS;
}

/* Create vlan */
bf_status_t mav_diag_service_vlan_create (
    bf_dev_id_t dev_id,
    int vlan_id,
    int pbm[],
    int num_pbm_ports,
    int ubm[],
    int num_ubm_ports)
{
    bf_status_t status = BF_SUCCESS;
    status = bf_diag_vlan_create (dev_id, vlan_id);
    if (status != BF_SUCCESS) {
        return status;
    }
    return mav_diag_service_vlan_add (
               dev_id, vlan_id, pbm, num_pbm_ports, ubm,
               num_ubm_ports);
}

/* Add ports to vlan */
bf_status_t mav_diag_service_vlan_add (
    bf_dev_id_t dev_id,
    int vlan_id,
    int pbm[],
    int num_pbm_ports,
    int ubm[],
    int num_ubm_ports)
{
    int index = 0;
    //diag_front_port_t port = 0;
    bf_dev_port_t dev_port = 0;
    bf_status_t status = BF_SUCCESS;
    bf_pal_front_port_handle_t port_info;
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_info, 0, sizeof (port_info));

    for (index = 0; index < num_pbm_ports; index++) {
        port_info.conn_id = pbm[index];
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                            pbm[index]);
            continue;
        }
        status |= bf_diag_port_vlan_add (dev_id, dev_port,
                                         vlan_id);
    }
    for (index = 0; index < num_ubm_ports; index++) {
        port_info.conn_id = ubm[index];
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                            ubm[index]);
            continue;
        }
        status |= bf_diag_port_default_vlan_set (dev_id,
                  dev_port, vlan_id);
    }
    return status;
}

/* Destroy vlan */
bf_status_t mav_diag_service_vlan_destroy (
    bf_dev_id_t dev_id, int vlan_id)
{
    return bf_diag_vlan_destroy (dev_id, vlan_id);
}

/* Vlan show helper */
bf_status_t mav_diag_vlan_show_helper (
    bf_dev_id_t dev_id,
    int vlan_id,
    char *resp_str,
    int max_str_len,
    int *curr_len)
{
    int c_len = *curr_len;
    bf_dev_port_t dev_port = 0, next_port = 0;
    diag_front_port_t front_port = 0;
    bf_status_t status = BF_SUCCESS;
    bool newline_added = false;

    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "%-7d",
                       vlan_id);

    status = bf_diag_vlan_port_get_first (dev_id,
                                          vlan_id, &dev_port);
    while (dev_port != -1) {
        bf_pal_front_port_handle_t port_info;
        status = bf_pm_port_dev_port_to_front_panel_port_get (
                     dev_id, dev_port, &port_info);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (" Failed to get front-port from dev-port %d \n",
                            dev_port);
            continue;
        }
        front_port = port_info.conn_id;
        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%d(%s) ",
                           front_port,
                           "T");

        status =
            bf_diag_vlan_port_get_next (dev_id, vlan_id,
                                        dev_port, 1, &next_port);
        dev_port = next_port;
    }

    status = bf_diag_default_vlan_port_get_first (
                 dev_id, vlan_id, &dev_port);
    while (dev_port != -1) {
        bf_pal_front_port_handle_t port_info;
        status = bf_pm_port_dev_port_to_front_panel_port_get (
                     dev_id, dev_port, &port_info);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (" Failed to get front-port from dev-port %d \n",
                            dev_port);
            continue;
        }
        front_port = port_info.conn_id;

        if (!newline_added) {
            c_len += snprintf (resp_str + c_len,
                               (c_len < max_str_len) ? (max_str_len - c_len - 1)
                               : 0,
                               "\n       ");
            newline_added = true;
        }
        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%d(%s) ",
                           front_port,
                           "U");

        status = bf_diag_default_vlan_port_get_next (
                     dev_id, vlan_id, dev_port, 1, &next_port);
        dev_port = next_port;
    }

    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "\n");

    *curr_len = c_len;
    return BF_SUCCESS;
}

/* Show vlan */
bf_status_t mav_diag_service_vlan_show (
    bf_dev_id_t dev_id,
    int input_vlan_id,
    char *resp_str,
    int max_str_len)
{
    int vlan_id = 0, next_vlan_id = 0;
    int c_len = strlen (resp_str);
    bf_status_t status = BF_SUCCESS;

    (void)status;
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       "Vlan   Ports        \n");
    c_len += snprintf (
                 resp_str + c_len,
                 (c_len < max_str_len) ? (max_str_len - c_len - 1)
                 : 0,
                 "------------------------------------------------------------\n");
    /* Dump all vlans */
    if (input_vlan_id != -1) {
        status = mav_diag_vlan_show_helper (
                     dev_id, input_vlan_id, resp_str, max_str_len,
                     &c_len);
        if (status == BF_INVALID_ARG) {
            MAV_DIAG_PRINT ("Vlan %d does not exist \n",
                            input_vlan_id);
        }
    } else {
        status = bf_diag_vlan_get_first (dev_id,
                                         &vlan_id);
        while (vlan_id != -1) {
            mav_diag_vlan_show_helper (dev_id, vlan_id,
                                       resp_str, max_str_len, &c_len);
            status = bf_diag_vlan_get_next (dev_id, vlan_id,
                                            1, &next_vlan_id);
            vlan_id = next_vlan_id;
        }
    }

    return BF_SUCCESS;
}

/* Run eyescan command  */
bf_status_t mav_diag_service_eyescan (
    bf_dev_id_t dev_id,
    diag_front_port_t front_port,
    int ber,
    char *mode,
    int lane,
    char *resp_str,
    int max_str_len)
{
    int c_len = strlen (resp_str);
    bf_sds_rx_eye_meas_mode_t meas_mode =
        BF_SDS_RX_EYE_MEAS_HEIGHT;
    bf_sds_rx_eye_meas_ber_t meas_ber = ber;
    bf_dev_port_t dev_port = 0;
    bf_pal_front_port_handle_t port_info;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    (void)meas_mode;
    memset (&port_info, 0, sizeof (port_info));

    if (mode &&
        (!strncmp (mode, "line", strlen ("line")))) {
        meas_mode = BF_SDS_RX_EYE_MEAS_WIDTH;
    }
    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       " ---------- EyeScan ----------- \n");

    port_info.conn_id = front_port;
    status = FP2DP (dev_id_of_port, &port_info,
                    &dev_port);
    if (status != BF_PLTFM_SUCCESS) {
        MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                        front_port);
        return status;
    }
    bf_serdes_rx_eye_3d_get (
        dev_id, dev_port, lane, meas_ber, resp_str,
        max_str_len - c_len - 1);

    return BF_SUCCESS;
}

/* Set LED color */
bf_status_t mav_diag_service_led_set (
    bf_dev_id_t dev_id,
    diag_front_port_t input_front_port,
    char *color)
{
    int num_ports = 0, chnl = 0;
    bf_led_condition_t led_color =
        BF_LED_POST_PORT_DEL;
    bf_pltfm_port_info_t port_info;
    diag_front_port_t front_port = 0;

    memset (&port_info, 0, sizeof (port_info));
    if (!color) {
        return BF_INVALID_ARG;
    }

    if (!strncmp (color, "red", strlen ("red"))) {
        led_color = BF_LED_PORT_RED;
    } else if (!strncmp (color, "green",
                         strlen ("green"))) {
        led_color = BF_LED_PORT_GREEN;
    } else if (!strncmp (color, "blue",
                         strlen ("blue"))) {
        led_color = BF_LED_PORT_BLUE;
    } else if (!strncmp (color, "off",
                         strlen ("off"))) {
        led_color = BF_LED_POST_PORT_DEL;
    } else {
        return BF_INVALID_ARG;
    }

    if (input_front_port != -1) {
        for (chnl = 0; chnl < 4; chnl++) {
            port_info.conn_id = input_front_port;
            port_info.chnl_id = chnl;
            bf_port_led_set (dev_id, &port_info, led_color);
        }
        return BF_SUCCESS;
    }

    num_ports = bf_bd_cfg_bd_num_port_get();
    for (front_port = 1; front_port <= num_ports;
         front_port++) {
        for (chnl = 0; chnl < 4; chnl++) {
            port_info.conn_id = front_port;
            port_info.chnl_id = chnl;
            bf_port_led_set (dev_id, &port_info, led_color);
        }
    }

    return BF_SUCCESS;
}

/* Setup snake */
bf_status_t mav_diag_service_snake_init (
    bf_dev_id_t dev_id,
    int start_port,
    int num_ports,
    char *type)
{
    bf_diag_port_lpbk_mode_e loop_mode =
        BF_DIAG_PORT_LPBK_NONE;
    diag_front_port_t front_port = 0;
    bf_dev_port_t dev_port = 0,
                  port_arr[MAV_DIAG_MAX_PORTS];
    int port_index = 0;
    bf_pal_front_port_handle_t port_info;
    bf_status_t status = BF_SUCCESS;
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_arr[0], 0, sizeof (port_arr));
    memset (&port_info, 0, sizeof (port_info));

    mav_diag_get_loopback_mode_from_str (type,
                                         &loop_mode);

    for (front_port = start_port;
         front_port < (start_port + num_ports);
         front_port++) {
        port_info.conn_id = front_port;
        status = FP2DP (dev_id_of_port, &port_info,
                        &dev_port);
        if (status != BF_PLTFM_SUCCESS) {
            MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                            front_port);
            return status;
        }
        port_arr[port_index] = dev_port;
        port_index++;
    }

    if (diag_snake_sess_hdl != 0) {
        MAV_DIAG_PRINT (
            " Another snake session with hdl %u in progress, cleaning that \n",
            diag_snake_sess_hdl);
        bf_diag_loopback_snake_test_cleanup (
            diag_snake_sess_hdl);
        diag_snake_sess_hdl = 0;
    }

    return bf_diag_loopback_snake_test_setup (
               dev_id, &port_arr[0], port_index, loop_mode,
               &diag_snake_sess_hdl);
}

/* Run the snake packet test */
bf_status_t mav_diag_service_snake_run (
    int pkt_size, int num_packet)
{
    bool bidir = false;
    return bf_diag_loopback_snake_test_start (
               diag_snake_sess_hdl, num_packet, pkt_size, bidir);
}

/* Stop the snake packet test */
bf_status_t mav_diag_service_snake_stop()
{
    return bf_diag_loopback_snake_test_stop (
               diag_snake_sess_hdl);
}

/* End the snake packet test */
bf_status_t mav_diag_service_snake_end()
{
    bf_status_t sts = BF_SUCCESS;
    sts = bf_diag_loopback_snake_test_cleanup (
              diag_snake_sess_hdl);
    diag_snake_sess_hdl = 0;
    return sts;
}

/* Snake status */
bf_status_t mav_diag_service_snake_status()
{
    bf_diag_port_stats_t stats;
    bf_diag_test_status_e test_status;

    memset (&stats, 0, sizeof (stats));
    test_status =
        bf_diag_loopback_snake_test_status_get (
            diag_snake_sess_hdl, &stats);
    if (test_status == BF_DIAG_TEST_STATUS_PASS) {
        return 0;
    }
    return 1;
}

static bf_status_t mav_diag_port_add_enable (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port,
    bf_port_speed_t speed,
    bf_fec_type_t fec)
{
    bf_status_t sts = BF_SUCCESS;
    bf_pal_front_port_handle_t port_info;
    bool is_internal = false;

    sts =
        bf_pm_port_dev_port_to_front_panel_port_get (
            dev_id, dev_port, &port_info);
    if (sts != BF_SUCCESS) {
        LOG_ERROR ("ERROR:%d Failed to get port info for dev_id=%d, dev_port=%d\n",
                   sts,
                   dev_id,
                   dev_port);
        return sts;
    }

    sts = bf_pm_is_port_internal (dev_id, &port_info,
                                  &is_internal);
    if (sts != BF_SUCCESS) {
        LOG_ERROR (
            "ERROR:%d Failed to get internal port info for dev_id=%d, "
            "dev_port=%d\n",
            sts,
            dev_id,
            dev_port);
        return sts;
    }
    /* Skip internal ports */
    if (is_internal) {
        return BF_SUCCESS;
    }

    sts = bf_pm_port_add (dev_id, &port_info, speed,
                          fec);
    if (sts != BF_SUCCESS) {
        LOG_ERROR ("ERROR: port_add failed(%d) for dev_id=%d, dev_port=%d\n",
                   sts,
                   dev_id,
                   dev_port);
    } else {
        /* Enable the port */
        sts = bf_pm_port_enable (dev_id, &port_info);
        if (sts != BF_SUCCESS) {
            LOG_ERROR (
                "ERROR: port_enable failed(%d) for dev_port=%d "
                "port_id=%d\n",
                sts,
                dev_id,
                dev_port);
        }
    }

    return sts;
}

#ifdef DEVICE_IS_SW_MODEL
static bf_status_t mav_diag_port_delete (
    bf_dev_id_t dev_id,
    bf_dev_port_t dev_port)
{
    bf_status_t sts = BF_SUCCESS;
    bf_pal_front_port_handle_t port_info;

    sts =
        bf_pm_port_dev_port_to_front_panel_port_get (
            dev_id, dev_port, &port_info);
    if (sts != BF_SUCCESS) {
        LOG_ERROR ("ERROR:%d Failed to get port info for dev_id=%d, dev_port=%d\n",
                   sts,
                   dev_id,
                   dev_port);
        return sts;
    }

    sts = bf_pm_port_delete (dev_id, &port_info);
    if (sts != BF_SUCCESS) {
        LOG_ERROR ("ERROR: port_delete failed(%d) for dev_id=%d, dev_port=%d\n",
                   sts,
                   dev_id,
                   dev_port);
    }

    return sts;
}
#endif

/* Ports add */
bf_status_t mav_diag_ports_add (bf_dev_id_t
                                dev_id)
{
    unsigned int pipe, port;
    uint32_t num_pipes;
    bf_port_speed_t speed = MAV_DIAG_DEF_PORT_SPEED;
    bf_fec_type_t fec = MAV_DIAG_DEF_PORT_FEC;

#ifdef DEVICE_IS_SW_MODEL
    /* Delete all existing ports (added by default in sw-model) */
    lld_sku_get_num_active_pipes (dev_id, &num_pipes);
    for (pipe = 0; pipe < num_pipes; pipe++) {
        for (port = 0; port < 64; port += 1) {
            mav_diag_port_delete (dev_id, MAKE_DEV_PORT (pipe,
                                  port));
        }
    }
#endif

    /* Add all ports in 100G (for both sw-model and asic) */
    lld_sku_get_num_active_pipes (dev_id, &num_pipes);
    for (pipe = 0; pipe < num_pipes; pipe++) {
        for (port = 0; port < 64; port += 4) {
            mav_diag_port_add_enable (dev_id,
                                      MAKE_DEV_PORT (pipe, port), speed, fec);
        }

#if !defined(DEVICE_IS_SW_MODEL)
        /* Add the 64th port on pipe 0 if it has mac */
        if (pipe == 0) {
            bool has_mac = false;
            bf_status_t sts = BF_SUCCESS;
            port = MAV_DIAG_CPU_PORT;
            sts = bf_port_has_mac (dev_id,
                                   MAKE_DEV_PORT (pipe, port), &has_mac);
            if (sts != BF_SUCCESS) {
                has_mac = false;
            }
            if (has_mac) {
                mav_diag_port_add_enable (
                    dev_id, MAKE_DEV_PORT (pipe, port), speed,
                    BF_FEC_TYP_NONE);
            }
        }
#endif
    }

    return BF_SUCCESS;
}

/* Set fec */
bf_status_t mav_diag_service_fec (bf_dev_id_t
                                  dev_id,
                                  int front_port,
                                  char *mode)
{
    bf_status_t status = BF_SUCCESS;
    bf_fec_type_t fec;
    bf_pal_front_port_handle_t port_info;

    if (!mode) {
        return BF_INVALID_ARG;
    }

    if (!strncmp (mode, "ON", strlen ("ON"))) {
        fec = MAV_DIAG_DEF_PORT_FEC;
    } else if (!strncmp (mode, "OFF",
                         strlen ("OFF"))) {
        fec = BF_FEC_TYP_NONE;
    } else {
        return BF_INVALID_ARG;
    }

    memset (&port_info, 0, sizeof (port_info));
    port_info.conn_id = front_port;

    status = bf_pm_port_fec_set (dev_id, &port_info,
                                 fec);

    return status;
}

/* Set port speed */
bf_status_t mav_diag_service_port_speed (
    bf_dev_id_t dev_id,
    int front_port,
    char *speed)
{
    bf_status_t status = BF_SUCCESS;
    bf_port_speed_t speed_val = 0;
    bf_pal_front_port_handle_t port_info;

    if (!speed) {
        return BF_INVALID_ARG;
    }

    if (!strncmp (speed, "100000",
                  strlen ("100000"))) {
        speed_val = BF_SPEED_100G;
    } else if (!strncmp (speed, "50000",
                         strlen ("50000"))) {
        speed_val = BF_SPEED_50G;
    } else if (!strncmp (speed, "40000",
                         strlen ("40000"))) {
        speed_val = BF_SPEED_40G;
    } else if (!strncmp (speed, "25000",
                         strlen ("25000"))) {
        speed_val = BF_SPEED_25G;
    } else if (!strncmp (speed, "10000",
                         strlen ("10000"))) {
        speed_val = BF_SPEED_10G;
    } else if (!strncmp (speed, "1000",
                         strlen ("1000"))) {
        speed_val = BF_SPEED_1G;
    } else {
        return BF_INVALID_ARG;
    }

    memset (&port_info, 0, sizeof (port_info));
    port_info.conn_id = front_port;

    status = bf_pm_port_speed_set (dev_id, &port_info,
                                   speed_val);

    return status;
}

/* Set port autoneg */
bf_status_t mav_diag_service_port_autoneg (
    bf_dev_id_t dev_id,
    int front_port,
    char *autoneg)
{
    bf_status_t status = BF_SUCCESS;
    bf_pm_port_autoneg_policy_e autoneg_val =
        PM_AN_DEFAULT;
    bf_pal_front_port_handle_t port_info;

    if (!autoneg) {
        return BF_INVALID_ARG;
    }

    if (!strncmp (autoneg, "DEFAULT",
                  strlen ("DEFAULT"))) {
        autoneg_val = PM_AN_DEFAULT;
    } else if (!strncmp (autoneg, "ON",
                         strlen ("ON"))) {
        autoneg_val = PM_AN_FORCE_ENABLE;
    } else if (!strncmp (autoneg, "OFF",
                         strlen ("OFF"))) {
        autoneg_val = PM_AN_FORCE_DISABLE;
    } else {
        return BF_INVALID_ARG;
    }

    memset (&port_info, 0, sizeof (port_info));
    port_info.conn_id = front_port;

    /* Disable port */
    bf_pm_port_disable (dev_id, &port_info);

    /* Sleep for sometime for changes to take effect */
    bf_sys_usleep (500000);
    /* Apply new autoneg setting */
    status = bf_pm_port_autoneg_set (dev_id,
                                     &port_info, autoneg_val);
    if (status != BF_PLTFM_SUCCESS) {
        MAV_DIAG_PRINT (" Failed to set auto-neg for front-port %d \n",
                        front_port);
    }
    bf_sys_usleep (500000);

    /* Enable port */
    bf_pm_port_enable (dev_id, &port_info);

    return status;
}

/* Set port interface */
bf_status_t mav_diag_service_port_intf (
    bf_dev_id_t dev_id,
    int port,
    char *intf)
{
    if (!intf) {
        return BF_INVALID_ARG;
    }

    (void)dev_id;
    (void)port;

    /* Set port interface not supported */
    return BF_INVALID_ARG;
}

/* Set pre-emphasis */
bf_status_t mav_diag_service_pre_emphasis (
    bf_dev_id_t dev_id,
    int front_port,
    int value,
    bool tx_pre,
    bool tx_post,
    bool tx_main,
    bool tx_post2,
    bool tx_post3)
{
    bf_status_t status = BF_SUCCESS;
    bf_pal_front_port_handle_t port_info;
    uint32_t chnl = 0;

    memset (&port_info, 0, sizeof (port_info));
    /* Apply the setting on all four channels */
    for (chnl = 0; chnl < 4; chnl++) {
        port_info.conn_id = front_port;
        port_info.chnl_id = chnl;

        if (tx_pre) {
            status |= bf_pm_port_serdes_tx_eq_pre_set (dev_id,
                      &port_info, value);
        } else if (tx_post) {
            status |= bf_pm_port_serdes_tx_eq_post_set (
                          dev_id, &port_info, value);
        } else if (tx_main) {
            status |= bf_pm_port_serdes_tx_eq_main_set (
                          dev_id, &port_info, value);
        } else {
            /* tx_post2 and tx_post3 are not supported */
            return BF_INVALID_ARG;
        }
    }

    return status;
}

/* Prbs Set */
bf_status_t mav_diag_service_prbs_set (
    bf_dev_id_t dev_id,
    int front_port,
    uint32_t p_value,
    uint32_t speed_value)
{
    bf_status_t status = BF_SUCCESS;
    bf_port_prbs_mode_t prbs_mode = 0;
    bf_dev_port_t dev_port = 0;
    bf_pal_front_port_handle_t port_info;
    bf_port_speed_t speed = MAV_DIAG_DEF_PORT_SPEED;
    bf_fec_type_t fec = MAV_DIAG_DEF_PORT_FEC;
    bf_port_prbs_speed_t prbs_speed = 0;
    int chnl = 0;
    bf_pal_front_port_handle_t port_list[4];
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_info, 0, sizeof (port_info));
    port_info.conn_id = front_port;
    status = FP2DP (dev_id_of_port, &port_info,
                    &dev_port);
    if (status != BF_PLTFM_SUCCESS) {
        MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                        front_port);
        return status;
    }

    /* Get prbs mode from value */
    if (p_value == 0) {
        prbs_mode = BF_PORT_PRBS_MODE_7;
    } else if (p_value == 1) {
        prbs_mode = BF_PORT_PRBS_MODE_15;
    } else if (p_value == 3) {
        prbs_mode = BF_PORT_PRBS_MODE_31;
    } else if (p_value == 4) {
        prbs_mode = BF_PORT_PRBS_MODE_9;
    } else if (p_value == 5) {
        prbs_mode = BF_PORT_PRBS_MODE_11;
    } else {
        MAV_DIAG_PRINT (
            "PRBS set not supported for p=%d (port %d)\n",
            p_value, front_port);
        return BF_INVALID_ARG;
    }

    if (speed_value == 10000) {
        prbs_speed = BF_PORT_PRBS_SPEED_10G;
    } else if (speed_value == 25000) {
        prbs_speed = BF_PORT_PRBS_SPEED_25G;
    } else {
        MAV_DIAG_PRINT ("PRBS set not supported for speed=%d (port %d)\n",
                        prbs_speed,
                        front_port);
        return BF_INVALID_ARG;
    }

    /* Delete the port */
    bf_pm_port_delete (dev_id, &port_info);
    bf_sys_usleep (500000);

    /* Turn on the laser on all channels as prbs runs on all channels */
    for (chnl = 0; chnl < 4; chnl++) {
        bf_qsfp_tx_disable_single_lane (front_port, chnl,
                                        false);
        /* Delay in 12c access is required after writing register */
        bf_sys_usleep (15000);
    }

    /* Set the prbs mode */
    for (chnl = 0; chnl < 4; chnl++) {
        port_list[chnl].conn_id = front_port;
        port_list[chnl].chnl_id = chnl;
    }
    status = bf_pm_port_prbs_set (dev_id,
                                  &port_list[0],
                                  sizeof (port_list) / sizeof (port_list[0]),
                                  prbs_speed,
                                  prbs_mode);
    if (status != BF_PLTFM_SUCCESS) {
        MAV_DIAG_PRINT (
            "PRBS set failed for front port %d, status=%d \n",
            front_port, status);
        mav_diag_port_add_enable (dev_id, dev_port, speed,
                                  fec);
    }

    return status;
}

/* Prbs Clean */
bf_status_t mav_diag_service_prbs_clean (
    bf_dev_id_t dev_id, int front_port)
{
    bf_status_t status = BF_SUCCESS;
    bf_dev_port_t dev_port = 0;
    bf_port_speed_t speed = MAV_DIAG_DEF_PORT_SPEED;
    bf_fec_type_t fec = MAV_DIAG_DEF_PORT_FEC;
    bf_pal_front_port_handle_t port_info;
    int chnl = 0;
    bf_pal_front_port_handle_t port_list[4];
    bf_dev_id_t dev_id_of_port = 0;

    memset (&port_info, 0, sizeof (port_info));
    port_info.conn_id = front_port;
    status = FP2DP (dev_id_of_port, &port_info,
                    &dev_port);
    if (status != BF_PLTFM_SUCCESS) {
        MAV_DIAG_PRINT (" Failed to get dev-port for front-port %d \n",
                        front_port);
        return status;
    }

    /* Cleanup prbs */
    for (chnl = 0; chnl < 4; chnl++) {
        port_list[chnl].conn_id = front_port;
        port_list[chnl].chnl_id = chnl;
    }
    status = bf_pm_port_prbs_cleanup (
                 dev_id, &port_list[0],
                 sizeof (port_list) / sizeof (port_list[0]));
    if (status != BF_SUCCESS) {
        MAV_DIAG_PRINT ("PRBS cleanup failed for front port %d, status=%d \n",
                        front_port,
                        status);
    }
    bf_sys_usleep (500000);

    /* Add the port back, Enable it */
    status |= mav_diag_port_add_enable (dev_id,
                                        dev_port, speed, fec);

    return status;
}

/* Prbs Show */
bf_status_t mav_diag_service_prbs_show (
    bf_dev_id_t dev_id,
    int front_port,
    char *resp_str,
    int max_str_len)
{
    int c_len = strlen (resp_str);
    int chnl = 0;
    bf_sds_debug_stats_per_quad_t stats;
    bf_sds_debug_stats_t *chnl_stats = NULL;
    bf_pal_front_port_handle_t port_hdl;

    memset (&stats, 0, sizeof (stats));

    c_len += snprintf (resp_str + c_len,
                       (c_len < max_str_len) ? (max_str_len - c_len - 1)
                       : 0,
                       " ---------- PRBS for port %d ----------- \n",
                       front_port);

    c_len +=
        snprintf (resp_str + c_len,
                  (c_len < max_str_len) ? (max_str_len - c_len - 1)
                  : 0,
                  "Channel Attn-main Attn-post Attn-pre Errors Eye-Metric \n");

    for (chnl = 0; chnl < 4; chnl++) {
        port_hdl.conn_id = front_port;
        port_hdl.chnl_id = chnl;
        bf_pm_port_prbs_stats_get (dev_id, &port_hdl,
                                   &stats.chnl_stats[chnl]);
    }
    for (chnl = 0; chnl < 4; chnl++) {
        chnl_stats = &stats.chnl_stats[chnl];
        c_len += snprintf (resp_str + c_len,
                           (c_len < max_str_len) ? (max_str_len - c_len - 1)
                           : 0,
                           "%-7d %-9d %-9d %-8d %-6d %-10d\n",
                           chnl,
                           chnl_stats->attn_main,
                           chnl_stats->attn_post,
                           chnl_stats->attn_pre,
                           chnl_stats->errors,
                           chnl_stats->eye_metric);
    }

    return BF_SUCCESS;
}
