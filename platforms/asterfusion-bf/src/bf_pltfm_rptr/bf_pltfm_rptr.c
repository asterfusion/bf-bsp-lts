/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rptr.c
 * @date
 *
 * API's for reading and writing to serdes repeaters for Mavericks
 *
 */

/* Standard includes */
#include <stdio.h>

/* Module includes */
#include <bf_pltfm_rptr.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_mav_qsfp_i2c_lock.h>

/* Local header includes */
#include "rptr.h"

/* Repeater I2C address map for Mavricks P0A and P0B */
static bf_pltfm_rptr_i2c_addr_t
rptr_map[2][MAX_RPTR] = {
    {

        {REPEATER_0, PCA9548_CH_0, I2C_ADDR_30},
        {REPEATER_1, PCA9548_CH_0, I2C_ADDR_36},
        {REPEATER_2, PCA9548_CH_0, I2C_ADDR_48},
        {REPEATER_3, PCA9548_CH_0, I2C_ADDR_4E},
        {REPEATER_4, PCA9548_CH_1, I2C_ADDR_30},
        {REPEATER_5, PCA9548_CH_1, I2C_ADDR_36},
        {REPEATER_6, PCA9548_CH_1, I2C_ADDR_48},
        {REPEATER_7, PCA9548_CH_1, I2C_ADDR_4E},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {REPEATER_14, PCA9548_CH_3, I2C_ADDR_48},
        {REPEATER_15, PCA9548_CH_3, I2C_ADDR_4E},
        {REPEATER_16, PCA9548_CH_4, I2C_ADDR_30},
        {REPEATER_17, PCA9548_CH_4, I2C_ADDR_36},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {REPEATER_24, PCA9548_CH_6, I2C_ADDR_30},
        {REPEATER_25, PCA9548_CH_6, I2C_ADDR_36},
        {REPEATER_26, PCA9548_CH_6, I2C_ADDR_48},
        {REPEATER_27, PCA9548_CH_6, I2C_ADDR_4E},
        {REPEATER_28, PCA9548_CH_7, I2C_ADDR_30},
        {REPEATER_29, PCA9548_CH_7, I2C_ADDR_36},
        {REPEATER_30, PCA9548_CH_7, I2C_ADDR_48},
        {REPEATER_31, PCA9548_CH_7, I2C_ADDR_4E},
    },
    {
        {REPEATER_0, PCA9548_CH_0, I2C_ADDR_30},
        {REPEATER_1, PCA9548_CH_0, I2C_ADDR_36},
        {REPEATER_2, PCA9548_CH_0, I2C_ADDR_48},
        {REPEATER_3, PCA9548_CH_0, I2C_ADDR_4E},
        {REPEATER_4, PCA9548_CH_1, I2C_ADDR_30},
        {REPEATER_5, PCA9548_CH_1, I2C_ADDR_36},
        {REPEATER_6, PCA9548_CH_1, I2C_ADDR_48},
        {REPEATER_7, PCA9548_CH_1, I2C_ADDR_4E},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {REPEATER_12, PCA9548_CH_3, I2C_ADDR_30},
        {REPEATER_13, PCA9548_CH_3, I2C_ADDR_36},
        {REPEATER_14, PCA9548_CH_3, I2C_ADDR_48},
        {REPEATER_15, PCA9548_CH_3, I2C_ADDR_4E},
        {REPEATER_16, PCA9548_CH_4, I2C_ADDR_30},
        {REPEATER_17, PCA9548_CH_4, I2C_ADDR_36},
        {REPEATER_18, PCA9548_CH_4, I2C_ADDR_48},
        {REPEATER_19, PCA9548_CH_4, I2C_ADDR_4E},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff},
        {REPEATER_24, PCA9548_CH_6, I2C_ADDR_30},
        {REPEATER_25, PCA9548_CH_6, I2C_ADDR_36},
        {REPEATER_26, PCA9548_CH_6, I2C_ADDR_48},
        {REPEATER_27, PCA9548_CH_6, I2C_ADDR_4E},
        {REPEATER_28, PCA9548_CH_7, I2C_ADDR_30},
        {REPEATER_29, PCA9548_CH_7, I2C_ADDR_36},
        {REPEATER_30, PCA9548_CH_7, I2C_ADDR_48},
        {REPEATER_31, PCA9548_CH_7, I2C_ADDR_4E},
    }
};

/* Board ID */
bf_pltfm_board_id_t bd_id;

/* Repeater debug flag */
uint8_t rptr_debug;

/**
 * Initailizes Repeater channel register write. It reads device id,
 * enable write to register, reset register and mute channel
 * @param dev cp2112 handle
 * @param dev_i2c_addr repeater's i2c addr
 * @param channel repeater channel number
 * @return function call status
 */
static bf_pltfm_status_t rptr_config_init (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t dev_i2c_addr,
    uint8_t channel)
{
    uint8_t addr;
    uint8_t write_buf[2];
    uint8_t read_buf;
    uint32_t write_buf_size;
    uint32_t read_buf_size;
    bf_pltfm_status_t res;

    /* READ Device ID */
    addr = dev_i2c_addr;
    write_buf[0] = RPTR_DEV_ID_REG;
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
        LOG_ERROR ("Device ID read failed \n");
        return BF_PLTFM_COMM_FAILED;
    }

    LOG_DEBUG ("Device ID read : %x \n", read_buf);

    if (read_buf != RPTR_DEVICE_ID) {
        LOG_ERROR ("Device ID doesn't match \n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Enable Non Common share registers */
    write_buf[0] = RPTR_CH_CNTRL_REG;
    write_buf[1] = RPTR_EN_NON_CMMN_SHARE;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Enable non common share register failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Disable calibration clock output */
    write_buf[0] = RPTR_CALIBRATION_CLK_REG;
    write_buf[1] = RPTR_DISABLE_CAL_CLCK;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR (" Disable calibration clock output failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Select channel for init */
    write_buf[0] = RPTR_CH_SELECT_REG;
    write_buf[1] = (0x01 << channel);
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Select channel for init failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Enable channel register writes */
    write_buf[0] = RPTR_CH_CNTRL_REG;
    write_buf[1] = RPTR_EN_CH_RW;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Enable channel register writes failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Reset channel registers */
    write_buf[0] = RPTR_CH_RESET_REG;
    write_buf[1] = 0x04;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Reset channel registers failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Mute driver output */
    write_buf[0] = RPTR_CH_MUTE_CNTRL_REG;
    write_buf[1] = RPTR_CH_MUTE;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Mute driver output failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Writes BST values from repeater's channel register in egress mode
 * @param dev cp2112 handle
 * @param dev_i2c_addr repeater's i2c addr
 * @param rptr_info repeater and channel number
 * @return function call status
 */
static bf_pltfm_status_t rptr_eg_set (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t dev_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info)
{
    uint8_t addr;
    uint8_t write_buf[2];
    uint32_t write_buf_size;
    bf_pltfm_status_t res;
    uint8_t vod = 0;

    LOG_DEBUG (
        "Setting these EQ: BST1: 0x%x BST2: 0x%x BW: "
        "0x%x BYPASS: 0x%x VOD: 0x%x REGF: 0x%x HIGH_GAIN:  0x%x \n",
        rptr_info->eq_bst1,
        rptr_info->eq_bst2,
        rptr_info->eq_bw,
        rptr_info->eq_bypass_bst1,
        rptr_info->vod,
        rptr_info->regF,
        rptr_info->high_gain);

    addr = dev_i2c_addr;

    /* Set BST1, BST2 and BW */
    write_buf[0] = RPTR_CH_BST_CNTRL_REG;
    write_buf[1] = (rptr_info->eq_bst1 & 0x07) |
                   ((rptr_info->eq_bst2 << 3) & 0x38) |
                   ((rptr_info->eq_bw << 6) & 0xC0);
    write_buf_size = 2;

    LOG_DEBUG ("EQ register value 0x%x \n",
               write_buf[1]);

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting EQ BST1 BST2 BW failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Set high gain mode and BYPASS BST1 eq */
    write_buf[0] = RPTR_CH_BYPASS_CNTRL_REG;
    write_buf[1] = 0x90 | ((rptr_info->high_gain << 5)
                           & 0x20) |
                   (rptr_info->eq_bypass_bst1 & 0x1);
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting BYPASS and gain mode failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Set register 0x0f with value depended on cable type */

    write_buf[0] = 0x0F;
    write_buf[1] = rptr_info->regF;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting gain for 0x0F register failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    if (rptr_info->vod <= RPTR_MAX_VOD_VAL) {
        vod = (rptr_info->vod & 0x03) << 6;
    } else {
        vod = RPTR_VOD3;
    }

    /* Set VOD and unmute the channel*/
    write_buf[0] = RPTR_CH_MUTE_CNTRL_REG;
    write_buf[1] = RPTR_CH_UNMUTE | vod;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting vod and unmute failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Writes BST values from repeater's channel register in Ingress mode
 * @param dev cp2112 handle
 * @param dev_i2c_addr repeater's i2c addr
 * @param rptr_info repeater and channel number
 * @return function call status
 */
static bf_pltfm_status_t rptr_ig_set (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t dev_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info)
{
    uint8_t addr;
    uint8_t write_buf[2];
    uint32_t write_buf_size;
    bf_pltfm_status_t res;
    uint8_t vod = 0;

    LOG_DEBUG (
        "Setting these EQ: BST1: 0x%x BST2: 0x%x BW:"
        "0x%x BYPASS: 0x%x VOD: 0x0%x REGF: 0x%x HIGH_GAIN:  0x%x \n",
        rptr_info->eq_bst1,
        rptr_info->eq_bst2,
        rptr_info->eq_bw,
        rptr_info->eq_bypass_bst1,
        rptr_info->vod,
        rptr_info->regF,
        rptr_info->high_gain);

    addr = dev_i2c_addr;

    /* Set BST1, BST2 and BW */
    write_buf[0] = RPTR_CH_BST_CNTRL_REG;
    write_buf[1] = (rptr_info->eq_bst1 & 0x07) |
                   ((rptr_info->eq_bst2 << 3) & 0x38) |
                   ((rptr_info->eq_bw << 6) & 0xC0);
    write_buf_size = 2;

    LOG_DEBUG ("EQ register value 0x%x \n",
               write_buf[1]);

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting EQ BST1 BST2 BW failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Set high gain mode and BYPASS BST1 eq */
    write_buf[0] = RPTR_CH_BYPASS_CNTRL_REG;
    write_buf[1] = 0x90 | ((rptr_info->high_gain << 5)
                           & 0x20) |
                   (rptr_info->eq_bypass_bst1 & 0x1);
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting BYPASS BST gain mode failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Set register 0x0f with value depended on cable type */

    write_buf[0] = 0x0F;
    write_buf[1] = rptr_info->regF;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting gain for 0x0F register failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    if (rptr_info->vod <= RPTR_MAX_VOD_VAL) {
        vod = (rptr_info->vod & 0x03) << 6;
    } else {
        vod = RPTR_VOD3;
    }

    /* Set VOD=3 and unmute channel */
    write_buf[0] = RPTR_CH_MUTE_CNTRL_REG;
    write_buf[1] = RPTR_CH_UNMUTE | vod;
    write_buf_size = 2;

    res = bf_pltfm_cp2112_write (dev, addr, write_buf,
                                 write_buf_size, TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Setting vod mode failed\n");
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t rptr_eg_mode_write (
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info)
{
    if (rptr_config_init (cp2112_handle,
                          rptr_i2c_addr->dev_i2c_addr,
                          rptr_info->chnl_num) != BF_PLTFM_SUCCESS) {
        return BF_PLTFM_COMM_FAILED;
    }

    if (rptr_eg_set (cp2112_handle,
                     rptr_i2c_addr->dev_i2c_addr, rptr_info) !=
        BF_PLTFM_SUCCESS) {
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t rptr_ig_mode_write (
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info)
{
    if (rptr_config_init (cp2112_handle,
                          rptr_i2c_addr->dev_i2c_addr,
                          rptr_info->chnl_num) != BF_PLTFM_SUCCESS) {
        return BF_PLTFM_COMM_FAILED;
    }

    if (rptr_ig_set (cp2112_handle,
                     rptr_i2c_addr->dev_i2c_addr, rptr_info) !=
        BF_PLTFM_SUCCESS) {
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Performs read and write operation on repeater
 * @param rptr_info repeater number and channel
 * @param mode INGRESS or EGRESS mode
 * @operation read or write operation
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_operation (
    bf_pltfm_rptr_info_t *rptr_info,
    bf_pltfm_rptr_mode_t mode,
    uint8_t operation,
    uint8_t *data,
    uint8_t reg_type)
{
    bf_pltfm_rptr_i2c_addr_t rptr_i2c_addr;
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;
    bf_pltfm_status_t res;
    uint8_t buf;
    uint8_t i;

    /*No check for data as it can be NULL. Only check when rptr_reg_read used */

    if (bd_id == BF_PLTFM_BD_ID_MAVERICKS_P0A) {
        i = 0;
    } else if (bd_id ==
               BF_PLTFM_BD_ID_MAVERICKS_P0B) {
        i = 1;
    } else {
        LOG_ERROR ("Invalid board id passed \n");
        return BF_PLTFM_INVALID_ARG;
    }

    if ((operation != RPTR_WRITE) &&
        (operation != RPTR_READ) &&
        (operation != RPTR_REG_READ) &&
        (operation != RPTR_REG_WRITE)) {
        LOG_ERROR ("Invalid Repeater operation\n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (reg_type > RPTR_CHANNEL_REG) {
        LOG_ERROR ("Invalid Repeater register type passed \n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (rptr_info == NULL) {
        LOG_ERROR ("Invalid pointer passed\n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (rptr_info->rptr_num == RPTR_NOT_AVAILABLE) {
        LOG_ERROR ("Repeater not available\n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (rptr_info->rptr_num >= MAX_RPTR) {
        LOG_ERROR ("Repeater number is greater than maximum repeaters \n");
        return BF_PLTFM_INVALID_ARG;
    }
    /* Find i2c addr of repeater */
    rptr_i2c_addr = rptr_map[i][rptr_info->rptr_num];

    /* Get CP2112 handle for i2c transactions */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        CP2112_ID_2);

    /* Value to be wriiten in PCA9548 device for opening the channel */
    buf = (1 << rptr_i2c_addr.sw_chnl_num);

    /* Lock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_lock();

    LOG_DEBUG ("I2C sw open: PCA i2c line: 0x%x \n",
               buf);
    LOG_DEBUG ("rptr_num %d sw_chl_num 0x%x i2c_addr 0x%x\n",
               rptr_i2c_addr.rptr_num,
               rptr_i2c_addr.sw_chnl_num,
               rptr_i2c_addr.dev_i2c_addr);

    /* Open I2c switch lane */
    res = bf_pltfm_cp2112_write (
              cp2112_handle, PCA9548_I2C_ADDR, &buf,
              sizeof (buf), TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("CP2112 write failed while opening PCA i2c switch \n");
        goto com_error;
    }

    if (operation == RPTR_WRITE) {
        /* Write eq data to repeater */
        if (mode == BF_PLTFM_RPTR_EGRESS) {
            res = rptr_eg_mode_write (cp2112_handle,
                                      &rptr_i2c_addr, rptr_info);
            if (res != BF_PLTFM_SUCCESS) {
                LOG_ERROR ("Egress mode writes to repeater failed\n");
                goto com_error;
            }
        } else {
            res = rptr_ig_mode_write (cp2112_handle,
                                      &rptr_i2c_addr, rptr_info);
            if (res != BF_PLTFM_SUCCESS) {
                LOG_ERROR ("Ingress mode writes to repeater failed\n");
                goto com_error;
            }
        }
    } else if (operation == RPTR_READ) {
        /* Read EQ setting */
        res = rptr_read (cp2112_handle, &rptr_i2c_addr,
                         rptr_info);
        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Repeater EQ read failed\n");
            goto com_error;
        }
    } else if (operation == RPTR_REG_READ) {
        if (data == NULL) {
            LOG_ERROR ("Invalid argument passed \n");
            goto com_error;
        }

        /* Read register values */
        res = rptr_reg_rd_wr (
                  cp2112_handle, &rptr_i2c_addr, rptr_info, data,
                  operation, reg_type);
        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Repeater register read failed\n");
            goto com_error;
        }
    } else if (operation == RPTR_REG_WRITE) {
        if (data == NULL) {
            LOG_ERROR ("Invalid argument passed \n");
            goto com_error;
        }

        /* write register value */
        res = rptr_reg_rd_wr (
                  cp2112_handle, &rptr_i2c_addr, rptr_info, data,
                  operation, reg_type);
        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Repeater register write failed\n");
            goto com_error;
        }
    } else {
        LOG_ERROR ("INVALID repeater operation requested \n");
        goto com_error;
    }

    /* close I2c switch lane */
    buf = 0;
    res = bf_pltfm_cp2112_write (
              cp2112_handle, PCA9548_I2C_ADDR, &buf,
              sizeof (buf), TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("CP2112 write failed while closing PCA i2c switch \n");
        goto com_error;
    }

    /* unLock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_unlock();

    return BF_PLTFM_SUCCESS;

com_error:

    bf_pltfm_rtmr_i2c_unlock();
    return BF_PLTFM_COMM_FAILED;
}

/**
 * Set particular port's egress or ingress repeater channel
 * @param port_info info struct of the port on the board
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_ch_eq_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    bf_pltfm_rptr_info_t *rptr_info)
{
    bf_pltfm_status_t res;

    if ((mode != BF_PLTFM_RPTR_EGRESS) &&
        (mode != BF_PLTFM_RPTR_INGRESS)) {
        LOG_ERROR ("Invalid mode passed \n");
        return BF_PLTFM_INVALID_ARG;
    }

    if ((rptr_info == NULL) || (port_info == NULL)) {
        LOG_ERROR ("Invalid arguments");
        return BF_PLTFM_INVALID_ARG;
    }

    if (rptr_info->rptr_num == RPTR_NOT_AVAILABLE) {
        LOG_ERROR ("Repeater not available for port: %d\n",
                   port_info->conn_id);
        return BF_PLTFM_INVALID_ARG;
    }

    if (rptr_info->rptr_num >= MAX_RPTR) {
        LOG_ERROR ("Repeater number is greater than maximum repeaters \n");
        return BF_PLTFM_INVALID_ARG;
    }

    LOG_DEBUG ("RPTR INFO from cfg rptr_num: 0x%x chnl_num: 0x0%x \n",
               rptr_info->rptr_num,
               rptr_info->chnl_num);

    res = bf_pltfm_rptr_operation (rptr_info, mode,
                                   RPTR_WRITE, NULL, 0);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Repeater write failed \n");
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Set all 4 repeater channels of a port in ingress mode
 * @param conn_id struct containing port number info
 * @param rptr_info port information for the port
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_conn_ig_eq_set (
    uint32_t conn_id)
{
    uint8_t i = 0;
    bf_pltfm_status_t res;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_info_t rptr_info;

    port_info.conn_id = conn_id;

    /* Set all 4 channels of a port with ingress equalization */
    for (; i < 4; i++) {
        port_info.chnl_id = i;

        res = bf_pltfm_bd_cfg_rptr_info_get (
                  &port_info, BF_PLTFM_RPTR_INGRESS, 0, &rptr_info);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Check the parameters passed \n");
            LOG_ERROR ("Retrieving Repeater info failed \n");
            return res;
        }

        res =
            bf_pltfm_rptr_ch_eq_set (&port_info,
                                     BF_PLTFM_RPTR_INGRESS, &rptr_info);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Setting eq failed port num: %d ch num: 0x%x \n",
                       port_info.conn_id,
                       port_info.chnl_id);
            return (res);
        }
    }

    return (res);
}

/**
 * Set all 4 repeater channels of a port in egress mode
 * @param conn_id port number
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_conn_eg_eq_set (
    uint32_t conn_id)
{
    uint8_t i = 0;
    bf_pltfm_status_t res;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rptr_info_t rptr_info;

    port_info.conn_id = conn_id;

    /* Set all 4 channels of a port with egress equalization */
    for (; i < 4; i++) {
        port_info.chnl_id = i;

        res = bf_pltfm_bd_cfg_rptr_info_get (
                  &port_info, BF_PLTFM_RPTR_EGRESS, 0, &rptr_info);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Check the parameters passed \n");
            LOG_ERROR ("Retrieving Repeater info failed \n");
            return res;
        }

        res = bf_pltfm_rptr_ch_eq_set (&port_info,
                                       BF_PLTFM_RPTR_EGRESS, &rptr_info);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Setting eq failed port num: %d ch num: 0x%x \n",
                       port_info.conn_id,
                       port_info.chnl_id);
            return (res);
        }
    }

    return (res);
}

/**
 * Set all 4 repeater channels of a port in ingress and egress mode
 * @param conn_id port number for which eq values needs to be set
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_conn_eq_set (
    uint32_t conn_id)
{
    bf_pltfm_status_t res;

    res = bf_pltfm_rptr_conn_eg_eq_set (conn_id);

    if (res != BF_PLTFM_SUCCESS) {
        return (res);
    }

    res = bf_pltfm_rptr_conn_ig_eq_set (conn_id);

    if (res != BF_PLTFM_SUCCESS) {
        return (res);
    }

    return (res);
}

/**
 * Initialize repeater library
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_init (void)
{
    bf_pltfm_chss_mgmt_bd_type_get (&bd_id);

    if ((bd_id == BF_PLTFM_BD_ID_X312PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V3DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V4DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V5DOT0)) {
        // No repeaters on board, so simply return success
        return BF_PLTFM_SUCCESS;
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_rptr_config_set (
    bf_dev_id_t dev_id,
    uint32_t conn,
    bf_pltfm_qsfp_type_t qsfp_type)
{
    bf_pltfm_rptr_info_t eg_rptr_info, ig_rptr_info;
    bf_pltfm_status_t sts;
    bf_pltfm_port_info_t port_info;
    bool has_rptr;
    uint32_t chnl;

    if ((bd_id == BF_PLTFM_BD_ID_X532PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X532PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X532PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT2) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_HC36Y24C_V1DOT0)) {
        /* No Repeaters Present, hence simply return */
        return BF_PLTFM_SUCCESS;
    }

    port_info.conn_id = conn;
    for (chnl = 0; chnl < QSFP_NUM_CHN; chnl++) {
        port_info.chnl_id = chnl;

        sts = bf_bd_cfg_port_has_rptr (&port_info,
                                       &has_rptr);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error: in getting the rptr info for port %d/%d at %d:%s",
                       conn,
                       chnl,
                       __LINE__,
                       __func__);
            return sts;
        }

        if (!has_rptr) {
            // Return success since the port does not have a repeater
            return BF_PLTFM_SUCCESS;
        }

        sts = bf_pltfm_bd_cfg_rptr_info_get (
                  &port_info, BF_PLTFM_RPTR_EGRESS, qsfp_type,
                  &eg_rptr_info);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "Error: in getting the egress mode rptr info for port %d/%d at %s:%d",
                conn,
                chnl,
                __func__,
                __LINE__);
            return sts;
        }

        sts = bf_pltfm_rptr_ch_eq_set (
                  &port_info, BF_PLTFM_RPTR_EGRESS, &eg_rptr_info);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "Error: in setting the egress repeater config for port %d/%d at "
                "%s:%d",
                conn,
                chnl,
                __func__,
                __LINE__);
            return sts;
        }

        sts = bf_pltfm_bd_cfg_rptr_info_get (
                  &port_info, BF_PLTFM_RPTR_INGRESS, qsfp_type,
                  &ig_rptr_info);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "Error: in getting the ingress mode rptr info for port %d/%d at "
                "%s:%d",
                conn,
                chnl,
                __func__,
                __LINE__);
            return sts;
        }

        sts = bf_pltfm_rptr_ch_eq_set (
                  &port_info, BF_PLTFM_RPTR_INGRESS, &ig_rptr_info);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "Error: in setting the ingress repeater config for port %d/%d at "
                "%s:%d",
                conn,
                chnl,
                __func__,
                __LINE__);
            return sts;
        }
    }
    return BF_PLTFM_SUCCESS;
}
