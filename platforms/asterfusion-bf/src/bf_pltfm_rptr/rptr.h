/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file rptr.h
 * @date
 *
 * Local header for repeaters API's
 *
 */
#ifndef _RPTR_H
#define _RPTR_H

#include <bfsys/bf_sal/bf_sys_intf.h>

/* typedef for repeater i2c address mapping */
typedef struct bf_pltfm_rptr_i2c_addr_t {
    uint8_t rptr_num;     /* Repeater Number */
    uint8_t sw_chnl_num;  /* I2C switch channel number */
    uint8_t dev_i2c_addr; /* Repeater I2C adress */
} bf_pltfm_rptr_i2c_addr_t;

/* Time out signifies after how much cp2112 should give up on i2c transaction */
#define TIMEOUT_MS 500
#define PCA9548_I2C_ADDR 0xea
#define RPTR_NOT_AVAILABLE 0xff

#define RPTR_READ 0x0
#define RPTR_WRITE 0x1
#define RPTR_REG_READ 0x2
#define RPTR_REG_WRITE 0x3

#define RPTR_GLOBAL_REG 0
#define RPTR_CHANNEL_REG 1

/* Channel numbers on I2C switch */
#define PCA9548_CH_0 0x0
#define PCA9548_CH_1 0x1
#define PCA9548_CH_3 0x3
#define PCA9548_CH_4 0x4
#define PCA9548_CH_6 0x6
#define PCA9548_CH_7 0x7

#define I2C_ADDR_30 0x30 /* Device at address 0x30 */
#define I2C_ADDR_36 0x36 /* Device at address 0x36 */
#define I2C_ADDR_48 0x48 /* Device at address 0x48 */
#define I2C_ADDR_4E 0x4E /* Device at address 0x4E */

/* Repeater as per Hardware document */
#define REPEATER_0 0
#define REPEATER_1 1
#define REPEATER_2 2
#define REPEATER_3 3
#define REPEATER_4 4
#define REPEATER_5 5
#define REPEATER_6 6
#define REPEATER_7 7
#define REPEATER_12 12
#define REPEATER_13 13
#define REPEATER_14 14
#define REPEATER_15 15
#define REPEATER_16 16
#define REPEATER_17 17
#define REPEATER_18 18
#define REPEATER_19 19
#define REPEATER_24 24
#define REPEATER_25 25
#define REPEATER_26 26
#define REPEATER_27 27
#define REPEATER_28 28
#define REPEATER_29 29
#define REPEATER_30 30
#define REPEATER_31 31

#define MAX_RPTR 32
#define RPTR_DEVICE_ID 0x40

/* Repeater Registers */
#define RPTR_DEV_ID_REG 0xF1
#define RPTR_CH_CNTRL_REG 0xFF
#define RPTR_CALIBRATION_CLK_REG 0x07
#define RPTR_CH_SELECT_REG 0xFC
#define RPTR_CH_RESET_REG 0x00
#define RPTR_CH_MUTE_CNTRL_REG 0x06
#define RPTR_CH_BST_CNTRL_REG 0x03
#define RPTR_CH_BYPASS_CNTRL_REG 0x04

/* Register constant values */
#define RPTR_EN_NON_CMMN_SHARE 0x30
#define RPTR_DISABLE_CAL_CLCK 0x01
#define RPTR_EN_CH_RW 0x01
#define RPTR_EN_GBL_RW 0x00
#define RPTR_CH_MUTE 0x12
#define RPTR_CH_UNMUTE 0x00
#define RPTR_VOD2 0x80
#define RPTR_VOD3 0xC0
#define RPTR_MAX_VOD_VAL 3

bf_pltfm_status_t bf_pltfm_rptr_operation (
    bf_pltfm_rptr_info_t *rptr_info,
    bf_pltfm_rptr_mode_t mode,
    uint8_t op,
    uint8_t *data,
    uint8_t reg_type);

bf_pltfm_status_t rptr_read (
    bf_pltfm_cp2112_device_ctx_t *dev,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info);

bf_pltfm_status_t rptr_reg_rd_wr (
    bf_pltfm_cp2112_device_ctx_t *dev,
    bf_pltfm_rptr_i2c_addr_t *rptr_i2c_addr,
    bf_pltfm_rptr_info_t *rptr_info,
    uint8_t *data,
    uint8_t operation,
    uint8_t reg_type);

#endif /* _RPTR_H */
