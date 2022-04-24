/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rptr_ucli.c
 * @date
 *
 * Unit testing cli for repeaters
 *
 */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

/* Module includes */
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_rptr.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bfsys/bf_sal/bf_sys_intf.h>

/* Local header includes */
#include "rptr.h"

extern uint8_t rptr_debug;

/**
 * Reads/write register from repeater's channel register
 * @param dev cp2112 handle
 * @param rptr_i2c_addr info about repeater's i2c sw channel and addr
 * @param rptr_info repeater and channel number
 * @return function call status
 */
bf_pltfm_status_t rptr_reg_rd_wr (
    bf_pltfm_cp2112_device_ctx_t *dev,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info,
    uint8_t *data,
    uint8_t operation,
    uint8_t reg_type)
{
    uint8_t addr;
    uint8_t write_buf[2];
    uint8_t read_buf;
    uint8_t reg_num;
    uint32_t write_buf_size;
    uint32_t read_buf_size;
    bf_pltfm_status_t res;

    if (data == NULL) {
        LOG_ERROR ("Invalid argument\n");
        return BF_PLTFM_INVALID_ARG;
    }

    reg_num = data[0];

    /* Select channel */
    addr = rptr_i2c_addr->dev_i2c_addr;
    write_buf[0] = RPTR_CH_SELECT_REG;
    write_buf[1] = 0x1 << rptr_info->chnl_num;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("repeater channel select failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Enable register read/write */
    write_buf[0] = RPTR_CH_CNTRL_REG;
    if (reg_type == RPTR_CHANNEL_REG) {
        write_buf[1] = RPTR_EN_CH_RW;
    } else {
        write_buf[1] = RPTR_EN_GBL_RW;
    }
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("select channel register failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    if (operation == RPTR_REG_READ) {
        // Get register value
        write_buf[0] = reg_num;
        read_buf = 0;
        write_buf_size = 1;
        read_buf_size = 1;

        res = bf_pltfm_cp2112_write_read_unsafe (dev,
                addr,
                &write_buf[0],
                &read_buf,
                write_buf_size,
                read_buf_size,
                TIMEOUT_MS);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Getting EQ failed\n");
            return BF_PLTFM_COMM_FAILED;
        }

        data[1] = read_buf;

        LOG_DEBUG ("Register read value 0x%x : 0x%x \n",
                   reg_num, read_buf);
        printf ("0x%x ==> 0x%x \n", reg_num, read_buf);
    } else if (operation == RPTR_REG_WRITE) {
        /* write to register */
        write_buf[0] = reg_num;
        write_buf[1] = data[1];
        write_buf_size = 2;

        res =
            bf_pltfm_cp2112_write (dev, addr, write_buf,
                                   write_buf_size, TIMEOUT_MS);
        printf ("Register write value 0x%x : 0x%x \n",
                reg_num, write_buf[1]);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("select channel register failed\n");
            return BF_PLTFM_COMM_FAILED;
        }
    } else {
        printf ("Invalid operation requested during read or write \n");
        return BF_PLTFM_COMM_FAILED;
    }
    return BF_PLTFM_SUCCESS;
}

/**
 * Reads BST values from repeater's channel register
 * @param dev cp2112 handle
 * @param rptr_i2c_addr info about repeater's i2c sw channel and addr
 * @param rptr_info repeater and channel number
 * @return function call status
 */
bf_pltfm_status_t rptr_read (
    bf_pltfm_cp2112_device_ctx_t *dev,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info)
{
    uint8_t addr;
    uint8_t write_buf[2];
    uint8_t read_buf;
    uint8_t regF;
    uint8_t high_gain;
    uint32_t write_buf_size;
    uint32_t read_buf_size;
    bf_pltfm_status_t res;

    /* Select channel */
    addr = rptr_i2c_addr->dev_i2c_addr;
    write_buf[0] = RPTR_CH_SELECT_REG;
    write_buf[1] = 0x1 << rptr_info->chnl_num;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("repeater channel select failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Enable register read/write */
    write_buf[0] = RPTR_CH_CNTRL_REG;
    write_buf[1] = RPTR_EN_CH_RW;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("select channel register failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    // Get BST1 BST2 BW eq
    write_buf[0] = RPTR_CH_BST_CNTRL_REG;
    read_buf = 0;
    write_buf_size = 1;
    read_buf_size = 1;

    res = bf_pltfm_cp2112_write_read_unsafe (dev,
            addr,
            &write_buf[0],
            &read_buf,
            write_buf_size,
            read_buf_size,
            TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Getting EQ failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    printf ("BST1: 0x%x BST2: 0x%x BW: 0x%x ",
            (read_buf & 0x7),
            ((read_buf & 0x38) >> 3),
            (read_buf & 0xC0) >> 6);

    /* Get BYPASS BST */
    write_buf[0] = RPTR_CH_BYPASS_CNTRL_REG;
    read_buf = 0;
    write_buf_size = 1;
    read_buf_size = 1;

    res = bf_pltfm_cp2112_write_read_unsafe (dev,
            addr,
            &write_buf[0],
            &read_buf,
            write_buf_size,
            read_buf_size,
            TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Getting EQ failed\n");
        return BF_PLTFM_COMM_FAILED;
    }
    high_gain = (read_buf & 0x20) >> 5;
    printf ("BYPASS: 0x%x", (read_buf & 0x1));

    /* Get REGF. */
    write_buf[0] = 0x0F;
    read_buf = 0;
    write_buf_size = 1;
    read_buf_size = 1;

    res = bf_pltfm_cp2112_write_read_unsafe (dev,
            addr,
            &write_buf[0],
            &read_buf,
            write_buf_size,
            read_buf_size,
            TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Getting REGF driver setting failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    regF = read_buf;

    /* Get VOD. Mute register controls VOD as well */
    write_buf[0] = RPTR_CH_MUTE_CNTRL_REG;
    read_buf = 0;
    write_buf_size = 1;
    read_buf_size = 1;

    res = bf_pltfm_cp2112_write_read_unsafe (dev,
            addr,
            &write_buf[0],
            &read_buf,
            write_buf_size,
            read_buf_size,
            TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Getting VOD driver setting failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    printf ("VOD: 0x%x REGF: 0x%x HIGH GAIN: 0x%x\n",
            ((read_buf & 0xC0) >> 6),
            regF,
            high_gain);

    return BF_PLTFM_SUCCESS;
}

static ucli_status_t rptr_ucli_eq_show (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode)
{
    bf_pltfm_rptr_info_t rptr_info;
    bf_pltfm_status_t res;

    res = bf_pltfm_bd_cfg_rptr_info_get (port_info,
                                         mode, 0, &rptr_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Check the parameters passed \n");
        LOG_ERROR ("Retrieving Repeater info failed \n");
        return UCLI_STATUS_E_ERROR;
    }

    LOG_DEBUG ("RPTR INFO from cfg rptr_num: 0x%x chnl_num: 0x0%x \n",
               rptr_info.rptr_num,
               rptr_info.chnl_num);

    if (rptr_info.rptr_num == RPTR_NOT_AVAILABLE) {
        LOG_ERROR ("Repeater not available for port: %d\n",
                   port_info->conn_id);
        return UCLI_STATUS_E_ERROR;
    }

    if (rptr_info.rptr_num >= MAX_RPTR) {
        LOG_ERROR ("Repeater number is greater than maximum repeaters \n");
        return UCLI_STATUS_E_ERROR;
    }

    res = bf_pltfm_rptr_operation (&rptr_info, mode,
                                   RPTR_READ, NULL, 0);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Repeater get failed \n");
        printf ("Repeater EQ show failed\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rptr_ig_eq_show", 2,
                       "<conn_id> <channel_no>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    return (rptr_ucli_eq_show (&port_info,
                               BF_PLTFM_RPTR_INGRESS));
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rptr_eg_eq_show", 2,
                       "<conn_id> <channel_no>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    return (rptr_ucli_eq_show (&port_info,
                               BF_PLTFM_RPTR_EGRESS));
}

static ucli_status_t rptr_ucli_eq_set (
    ucli_context_t *uc,
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t eq_bst1,
    uint8_t eq_bst2,
    uint8_t eq_bw,
    uint8_t eq_bypass_bst1,
    uint8_t vod,
    uint8_t gain,
    uint8_t high_gain,
    bf_pltfm_rptr_mode_t mode)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_info_t rptr_info;
    bf_pltfm_status_t res;

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* API to reterive repeater info */
    res = bf_pltfm_bd_cfg_rptr_info_get (&port_info,
                                         mode, 0, &rptr_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Check the parameters passed \n");
        LOG_ERROR ("Retrieving Repeater info failed \n");
        return UCLI_STATUS_E_ERROR;
    }

    LOG_DEBUG ("RPTR INFO from cfg rptr_num: 0x%x chnl_num: 0x0%x \n",
               rptr_info.rptr_num,
               rptr_info.chnl_num);

    if (rptr_info.rptr_num == RPTR_NOT_AVAILABLE) {
        LOG_ERROR ("Repeater not available for port: %d\n",
                   port_info.conn_id);
        return UCLI_STATUS_E_ERROR;
    }

    if (rptr_info.rptr_num >= MAX_RPTR) {
        LOG_ERROR ("Repeater number is greater than maximum repeaters \n");
        return UCLI_STATUS_E_ERROR;
    }

    rptr_info.eq_bst1 = eq_bst1;
    rptr_info.eq_bst2 = eq_bst2;
    rptr_info.eq_bw = eq_bw;
    rptr_info.eq_bypass_bst1 = eq_bypass_bst1;
    rptr_info.vod = vod;
    rptr_info.regF = gain;
    rptr_info.high_gain = high_gain;

    res = bf_pltfm_rptr_operation (&rptr_info, mode,
                                   RPTR_WRITE, NULL, 0);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Repeater set failed \n");
        aim_printf (&uc->pvs, "Repeater EQ set failed\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rptr_ig_eq_set",
                       9,
                       "<conn_id> <channel_no> <BST1> <BST2> <BW> <BST_BYPASS> "
                       "<VOD> <REG 0xF GAIN> <HIGH GAIN>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t eq_bst1;
    uint8_t eq_bst2;
    uint8_t eq_bw;
    uint8_t eq_bypass_bst1;
    uint8_t vod;
    uint8_t gain;
    uint8_t high_gain;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    eq_bst1 = atoi (uc->pargs->args[2]);
    eq_bst2 = atoi (uc->pargs->args[3]);
    eq_bw = atoi (uc->pargs->args[4]);
    eq_bypass_bst1 = atoi (uc->pargs->args[5]);
    vod = atoi (uc->pargs->args[6]);
    gain = atoi (uc->pargs->args[7]);
    high_gain = atoi (uc->pargs->args[8]);

    return (rptr_ucli_eq_set (uc,
                              port_num,
                              channel_num,
                              eq_bst1,
                              eq_bst2,
                              eq_bw,
                              eq_bypass_bst1,
                              vod,
                              gain,
                              high_gain,
                              BF_PLTFM_RPTR_INGRESS));
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rptr_eg_eq_set",
                       9,
                       "<conn_id> <channel_no> <BST1> <BST2> <BW> <BST_BYPASS> "
                       "<VOD> <REG 0xF GAIN> <HIGH GAIN>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t eq_bst1;
    uint8_t eq_bst2;
    uint8_t eq_bw;
    uint8_t eq_bypass_bst1;
    uint8_t vod;
    uint8_t gain;
    uint8_t high_gain;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    eq_bst1 = atoi (uc->pargs->args[2]);
    eq_bst2 = atoi (uc->pargs->args[3]);
    eq_bw = atoi (uc->pargs->args[4]);
    eq_bypass_bst1 = atoi (uc->pargs->args[5]);
    vod = atoi (uc->pargs->args[6]);
    gain = atoi (uc->pargs->args[7]);
    high_gain = atoi (uc->pargs->args[8]);

    return (rptr_ucli_eq_set (uc,
                              port_num,
                              channel_num,
                              eq_bst1,
                              eq_bst2,
                              eq_bw,
                              eq_bypass_bst1,
                              vod,
                              gain,
                              high_gain,
                              BF_PLTFM_RPTR_EGRESS));
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rptr_eg_eq_set_all",
        7,
        "<BST1> <BST2> <BW> <BST_BYPASS> <VOD> <REG 0xF GAIN> <HIGH_GAIN>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t eq_bst1;
    uint8_t eq_bst2;
    uint8_t eq_bw;
    uint8_t eq_bypass_bst1;
    uint8_t vod;
    uint8_t gain;
    uint8_t high_gain;
    uint8_t i, j;

    uint8_t port_w_rptr[24] = {33, 34, 35, 36, 37, 38, 39, 40, 45, 46, 47, 48,
                               49, 50, 51, 52, 57, 58, 59, 60, 61, 62, 63, 64
                              };

    eq_bst1 = atoi (uc->pargs->args[0]);
    eq_bst2 = atoi (uc->pargs->args[1]);
    eq_bw = atoi (uc->pargs->args[2]);
    eq_bypass_bst1 = atoi (uc->pargs->args[3]);
    vod = atoi (uc->pargs->args[4]);
    gain = atoi (uc->pargs->args[5]);
    high_gain = atoi (uc->pargs->args[6]);

    // Set all ports with repeater
    for (i = 0; i < 24; i++) {
        port_num = port_w_rptr[i];
        // Set all channels with in repeater
        for (j = 0; j < 4; j++) {
            channel_num = j;
            rptr_ucli_eq_set (uc,
                              port_num,
                              channel_num,
                              eq_bst1,
                              eq_bst2,
                              eq_bw,
                              eq_bypass_bst1,
                              vod,
                              gain,
                              high_gain,
                              BF_PLTFM_RPTR_EGRESS);
        }
        aim_printf (&uc->pvs, "Port eq set %d \n",
                    port_num);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rptr_ig_eq_set_all",
        7,
        "<BST1> <BST2> <BW> <BST_BYPASS> <VOD> <REG 0xF GAIN> <HIGH GAIN>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t eq_bst1;
    uint8_t eq_bst2;
    uint8_t eq_bw;
    uint8_t eq_bypass_bst1;
    uint8_t vod;
    uint8_t gain;
    uint8_t high_gain;
    uint8_t i, j;

    uint8_t port_w_rptr[24] = {33, 34, 35, 36, 37, 38, 39, 40, 45, 46, 47, 48,
                               49, 50, 51, 52, 57, 58, 59, 60, 61, 62, 63, 64
                              };

    eq_bst1 = atoi (uc->pargs->args[0]);
    eq_bst2 = atoi (uc->pargs->args[1]);
    eq_bw = atoi (uc->pargs->args[2]);
    eq_bypass_bst1 = atoi (uc->pargs->args[3]);
    vod = atoi (uc->pargs->args[4]);
    gain = atoi (uc->pargs->args[5]);
    high_gain = atoi (uc->pargs->args[6]);

    // Set all ports with repeater
    for (i = 0; i < 24; i++) {
        port_num = port_w_rptr[i];
        // Set all channels with in repeater
        for (j = 0; j < 4; j++) {
            channel_num = j;
            rptr_ucli_eq_set (uc,
                              port_num,
                              channel_num,
                              eq_bst1,
                              eq_bst2,
                              eq_bw,
                              eq_bypass_bst1,
                              vod,
                              gain,
                              high_gain,
                              BF_PLTFM_RPTR_INGRESS);
        }
        aim_printf (&uc->pvs, "Port eq set %d\n",
                    port_num);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_debug__
(ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "debug_mode", 1,
        "enable (1) or disable (0) debug <value>");

    rptr_debug = atoi (uc->pargs->args[0]);

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_default_eq_set__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "default_eq_set",
                       1,
                       "Set the default value of eq for a port <port number>");

    uint32_t port_num;

    port_num = atoi (uc->pargs->args[0]);

    if (bf_pltfm_rptr_conn_eq_set (port_num) !=
        BF_PLTFM_SUCCESS) {
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_eq_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "port_eq_show", 1,
        "Show eq values for a port <port number>");

    uint8_t i;
    bf_pltfm_port_info_t port_info;

    port_info.conn_id = atoi (uc->pargs->args[0]);

    printf ("INGRESS \n");
    for (i = 0; i < 4; i++) {
        port_info.chnl_id = i;
        printf ("Channel %d:  ", i);
        rptr_ucli_eq_show (&port_info,
                           BF_PLTFM_RPTR_INGRESS);
    }

    printf ("EGRESS \n");
    for (i = 0; i < 4; i++) {
        port_info.chnl_id = i;
        printf ("Channel %d:  ", i);
        rptr_ucli_eq_show (&port_info,
                           BF_PLTFM_RPTR_EGRESS);
    }

    return 0;
}

static ucli_status_t rptr_ucli_reg_rd_wr (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    uint8_t *data,
    uint8_t operation,
    uint8_t reg_type)
{
    bf_pltfm_rptr_info_t rptr_info;
    bf_pltfm_status_t res;

    res = bf_pltfm_bd_cfg_rptr_info_get (port_info,
                                         mode, 0, &rptr_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Check the parameters passed \n");
        LOG_ERROR ("Retrieving Repeater info failed \n");
        return UCLI_STATUS_E_ERROR;
    }

    LOG_DEBUG ("RPTR INFO from cfg rptr_num: 0x%x chnl_num: 0x0%x \n",
               rptr_info.rptr_num,
               rptr_info.chnl_num);

    if (rptr_info.rptr_num == RPTR_NOT_AVAILABLE) {
        LOG_ERROR ("Repeater not available for port: %d\n",
                   port_info->conn_id);
        return UCLI_STATUS_E_ERROR;
    }

    if (rptr_info.rptr_num >= MAX_RPTR) {
        LOG_ERROR ("Repeater number is greater than maximum repeaters \n");
        return UCLI_STATUS_E_ERROR;
    }

    res = bf_pltfm_rptr_operation (&rptr_info, mode,
                                   operation, data, reg_type);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Repeater get failed \n");
        printf ("Repeater rd=2 wr=3: operation==> %d failed\n",
                operation);
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_reg_read__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rptr_reg_read",
                       5,
                       "<conn_id> <channel_no> <mode: ig or eg> <reg_addr in hex> "
                       "<g: globle register/ c: channel register >");

    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_mode_t mode;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t data[2];
    uint8_t reg_type;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    data[0] = (uint8_t)strtol (uc->pargs->args[3],
                               NULL, 16);

    if (!strncmp (uc->pargs->args[2], "ig", 2)) {
        mode = BF_PLTFM_RPTR_INGRESS;
    } else if (!strncmp (uc->pargs->args[2], "eg",
                         2)) {
        mode = BF_PLTFM_RPTR_EGRESS;
    } else {
        printf ("Invalid mode passed \n");
        return 0;
    }

    if (!strncmp (uc->pargs->args[4], "g", 1)) {
        reg_type = RPTR_GLOBAL_REG;
    } else if (!strncmp (uc->pargs->args[4], "c",
                         1)) {
        reg_type = RPTR_CHANNEL_REG;
    } else {
        printf ("invalid register type passed: 'g' or 'c'\n");
        return 0;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    return (rptr_ucli_reg_rd_wr (&port_info, mode,
                                 data, RPTR_REG_READ, reg_type));
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_reg_read_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rptr_reg_read_all", 1,
                       "<conn_id>");

    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_mode_t mode;
    uint8_t port_num;
    uint8_t data[2];
    uint8_t reg_type;
    uint8_t i, j;
    port_num = atoi (uc->pargs->args[0]);

    port_info.conn_id = port_num;
    port_info.chnl_id = 0;

    reg_type = RPTR_GLOBAL_REG;
    mode = BF_PLTFM_RPTR_INGRESS;
    printf ("INGRESS : GLOBAL REG\n");
    for (i = 0; i < 12; i++) {
        data[0] = i;
        rptr_ucli_reg_rd_wr (&port_info, mode, data,
                             RPTR_REG_READ, reg_type);
    }

    reg_type = RPTR_GLOBAL_REG;
    mode = BF_PLTFM_RPTR_EGRESS;
    printf ("EGRESS : GLOBAL REG\n");
    for (i = 0; i < 12; i++) {
        data[0] = i;
        rptr_ucli_reg_rd_wr (&port_info, mode, data,
                             RPTR_REG_READ, reg_type);
    }

    for (i = 0; i < 4; i++) {
        port_info.chnl_id = i;
        reg_type = RPTR_CHANNEL_REG;
        mode = BF_PLTFM_RPTR_INGRESS;
        printf ("INGRESS : Channel %d\n", i);
        for (j = 0; j < 16; j++) {
            data[0] = j;
            rptr_ucli_reg_rd_wr (&port_info, mode, data,
                                 RPTR_REG_READ, reg_type);
        }
    }
    for (i = 0; i < 4; i++) {
        port_info.chnl_id = i;
        reg_type = RPTR_CHANNEL_REG;
        mode = BF_PLTFM_RPTR_EGRESS;
        printf ("EGRESS : Channel %d\n", i);
        for (j = 0; j < 16; j++) {
            data[0] = j;
            rptr_ucli_reg_rd_wr (&port_info, mode, data,
                                 RPTR_REG_READ, reg_type);
        }
    }

    return 0;
}
static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_reg_write__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rptr_reg_write",
                       6,
                       "<conn_id> <channel_no> <mode: ig or eg> <reg_addr in hex> "
                       "<data> <reg_type: g/c>");

    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_mode_t mode;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t data[2];
    uint8_t reg_type;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    data[0] = (uint8_t)strtol (uc->pargs->args[3],
                               NULL, 16);
    data[1] = (uint8_t)strtol (uc->pargs->args[4],
                               NULL, 16);

    if (!strncmp (uc->pargs->args[2], "ig", 2)) {
        mode = BF_PLTFM_RPTR_INGRESS;
    } else if (!strncmp (uc->pargs->args[2], "eg",
                         2)) {
        mode = BF_PLTFM_RPTR_EGRESS;
    } else {
        printf ("Invalid mode passed \n");
        return 0;
    }

    if (!strncmp (uc->pargs->args[5], "g", 1)) {
        reg_type = RPTR_GLOBAL_REG;
    } else if (!strncmp (uc->pargs->args[5], "c",
                         1)) {
        reg_type = RPTR_CHANNEL_REG;
    } else {
        printf ("invalid register type passed: 'g' or 'c'\n");
        return 0;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    return (
               rptr_ucli_reg_rd_wr (&port_info, mode, data,
                                    RPTR_REG_WRITE, reg_type));
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__rptr_port_reg_write_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rptr_reg_write_all",
        4,
        "<mode: ig or eg> <reg_addr in hex> <data> <reg_type: g/c>");

    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_mode_t mode;
    uint8_t data[2];
    uint8_t reg_type, i, j;

    uint8_t port_w_rptr[24] = {33, 34, 35, 36, 37, 38, 39, 40, 45, 46, 47, 48,
                               49, 50, 51, 52, 57, 58, 59, 60, 61, 62, 63, 64
                              };

    data[0] = (uint8_t)strtol (uc->pargs->args[1],
                               NULL, 16);
    data[1] = (uint8_t)strtol (uc->pargs->args[2],
                               NULL, 16);

    if (!strncmp (uc->pargs->args[0], "ig", 2)) {
        mode = BF_PLTFM_RPTR_INGRESS;
    } else if (!strncmp (uc->pargs->args[0], "eg",
                         2)) {
        mode = BF_PLTFM_RPTR_EGRESS;
    } else {
        printf ("Invalid mode passed \n");
        return 0;
    }

    if (!strncmp (uc->pargs->args[3], "g", 1)) {
        reg_type = RPTR_GLOBAL_REG;
    } else if (!strncmp (uc->pargs->args[3], "c",
                         1)) {
        reg_type = RPTR_CHANNEL_REG;
    } else {
        printf ("invalid register type passed: 'g' or 'c'\n");
        return 0;
    }

    // Set all ports with repeater
    for (i = 0; i < 24; i++) {
        port_info.conn_id = port_w_rptr[i];
        // Set all channels with in repeater
        for (j = 0; j < 4; j++) {
            port_info.chnl_id = j;
            rptr_ucli_reg_rd_wr (&port_info, mode, data,
                                 RPTR_REG_WRITE, reg_type);
        }
    }
    return 0;
}

static ucli_command_handler_f
bf_pltfm_rptr_ucli_ucli_handlers__[] = {
    bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_set__,
    bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_set__,
    bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_set_all__,
    bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_set_all__,
    bf_pltfm_rptr_ucli_ucli__rptr_ig_eq_show__,
    bf_pltfm_rptr_ucli_ucli__rptr_eg_eq_show__,
    bf_pltfm_rptr_ucli_ucli__rptr_debug__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_default_eq_set__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_eq_show__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_reg_read__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_reg_read_all__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_reg_write__,
    bf_pltfm_rptr_ucli_ucli__rptr_port_reg_write_all__,
    NULL
};

static ucli_module_t bf_pltfm_rptr_ucli_module__
= {
    "bf_pltfm_rptr_ucli", NULL, bf_pltfm_rptr_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_rptr_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_rptr_ucli_module__);
    n = ucli_node_create ("rptr", m,
                          &bf_pltfm_rptr_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("rptr"));
    return n;
}
