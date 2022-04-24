/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rtmr_priv.h
 * @date
 *
 * Local header for retimer API's
 *
 */
#ifndef _BF_PLTFM_RTMR_PRIV_H
#define _BF_PLTFM_RTMR_PRIV_H

#include <bfsys/bf_sal/bf_sys_intf.h>

/* typedef for retimer i2c address mapping */
typedef struct bf_pltfm_rtmr_i2c_addr_t {
    uint8_t rtmr_num;     /* Retimer Number */
    uint8_t cp2112_num;   /* CP2112 device Number */
    uint8_t sw_chnl_num;  /* I2C switch channel number */
    uint8_t dev_i2c_addr; /* Retimer I2C adress */
    uint8_t pca9535_id;   /* Retimer reset pca9535 id */
    uint8_t pca9535_chan; /* Retimer reset pca9535 channel */
} bf_pltfm_rtmr_i2c_addr_t;

typedef struct bf_pltfm_rtmr_chan_prof_t {
    bool tx_pol_swap;
    bool rx_pol_swap;
    uint8_t dfe_tap3;
} bf_pltfm_rtmr_chan_prof_t;

typedef struct bf_pltfm_rtmr_reg_val_t {
    uint16_t reg_offset;
    uint16_t reg_value;
} bf_pltfm_rtmr_reg_val_t;

#define RTMR_SWAP_BYTE(buffer) \
  ((((buffer) >> 8) & 0xff) | (((buffer)&0xff) << 8))

/* Time out signifies after how much cp2112 should give up on i2c transaction */
#define TIMEOUT_MS 500
#define PCA9548_I2C_ADDR 0xea
#define RTMR_NOT_AVAILABLE 0xff

/* Retimer supported operations */
#define RTMR_READ 0x0
#define RTMR_WRITE 0x1
#define RTMR_FW_DL 0x2

#define RTMR_GLOBAL_REG 0
#define RTMR_CHANNEL_REG 1

/* Channel numbers on I2C switch */
#define RTMR_PCA9548_CH_0 0x0
#define RTMR_PCA9548_CH_1 0x1
#define RTMR_PCA9548_CH_2 0x2
#define RTMR_PCA9548_CH_3 0x3
#define RTMR_PCA9548_CH_4 0x4
#define RTMR_PCA9548_CH_5 0x5
#define RTMR_PCA9548_CH_6 0x6
#define RTMR_PCA9548_CH_7 0x7

#define RTMR_I2C_ADDR_20 0x20 /* Device at address 0x20 */
#define RTMR_I2C_ADDR_22 0x22 /* Device at address 0x22 */
#define RTMR_I2C_ADDR_24 0x24 /* Device at address 0x24 */
#define RTMR_I2C_ADDR_26 0x26 /* Device at address 0x26 */

/* Retimer as per Hardware document */
#define RTMR_0 0
#define RTMR_1 1
#define RTMR_2 2
#define RTMR_3 3
#define RTMR_4 4
#define RTMR_5 5
#define RTMR_6 6
#define RTMR_7 7
#define RTMR_8 8
#define RTMR_9 9
#define RTMR_10 10
#define RTMR_11 11
#define RTMR_12 12
#define RTMR_13 13
#define RTMR_14 14
#define RTMR_15 15
#define RTMR_16 16
#define RTMR_17 17
#define RTMR_18 18
#define RTMR_19 19
#define RTMR_20 20
#define RTMR_21 21
#define RTMR_22 22
#define RTMR_23 23
#define RTMR_24 24
#define RTMR_25 25
#define RTMR_26 26
#define RTMR_27 27
#define RTMR_28 28
#define RTMR_29 29
#define RTMR_30 30
#define RTMR_31 31

#define MIN_MVR_RTMR_PORTS 33
#define MAX_MVR_RTMR_PORTS 64
#define MAX_RTMR 32
#define MAX_PRBS_MODE 3
#define BF_PLTFM_GET_RTMR_INDEX(port) ((port)-MAX_RTMR - 1)
#define BF_PLTFM_GET_PORT_INDEX(rtmr) ((rtmr) + MAX_RTMR + 1)
#define MAX_RTMR_CHAN 4
#define RTMR_DEVICE_ID 0x40

/* Retimer Registers */
#define FW0 0x9f00
#define FW1 0x980d
#define FW2 0x9814
#define RTMR_6X_REG_BASE 0x6000
#define RTMR_7X_REG_BASE 0x7000
#define RTMR_REG_BASE 0x8000
#define RX_NRZ_SD_REG 0x8003
#define RX_READ_EYE_MARGIN_REG 0x802a
#define RX_DFETAP1_CURR_VAL_REG 0x802b
#define RX_DFETAP2_3_CURR_VAL_REG 0x802c
#define RX_DFETAP_MINUS_1_CURR_VAL_REG 0x802d
#define RX_CTLE_OVERRIDE 0x804d
#define RX_CTLE_OVERRIDE_VAL 0x804e
#define RX_PRBS_CFG_REG 0x8061
#define RX_PRBS_ERROR_CNT_HI_REG 0x8066
#define RX_PRBS_ERROR_CNT_LO_REG 0x8067
#define TIMING_CTRL_REG 0x8075
#define READ_REF_DAC_CURR_CTL_REG 0x807f
#define RX_NRZ_SM_RESET_REG 0x8081
#define TX_TEST_PATTERN_CFG 0x80a0
#define TX_POST_CURSOR_TAP3_COEFF 0x80a4
#define TX_POST_CURSOR_TAP2_COEFF 0x80a5
#define TX_POST_CURSOR_TAP1_COEFF 0x80a6
#define TX_MAIN_TAP_COEFF 0x80a7
#define TX_MAIN_PRE_CURSOR_COEFF 0x80a8
#define PLL_MULTIPLIER 0x80fe
#define PLL_REFCLK_DIV 0x80ff
#define LB_CNTRL_REG 0x980c

#define RTMR_FW_IND_VALUE_REG 0x9812
#define RTMR_FW_AN_MODE_REG 0x9815
#define RTMR_FW_OPT_MOD_REG 0x9816
#define RTMR_FW_IND_CTRL_REG 0x9818
#define RTMR_FW_IND_OFFSET_REG 0x9819

/* Retimer register constant values */
#define BF_RTMR_START_FW_LOAD 0xfff0
#define BF_RTMR_RESET_MCU 0x0aaa
#define BF_START_FW 0x4000
#define RTMR_RD_CRC 0xf001
#define RTMR_RD_CRC_CFM 0xf01
#define RTMR_INIT_REG_MODE 0x5000
#define RTMR_INIT_REG_CFM 0x500
#define RTMR_INIT_25G_NO_AN 0x5024
#define RTMR_INIT_STATE_MODE 0x5040
#define RTMR_INIT_STATE_MODE_ALL (RTMR_INIT_STATE_MODE | RTMR_APPLY_ALL_CHAN)
#define RTMR_APPLY_ALL_CHAN 0x4
#define RTMR_INIT_MODE_ALL (RTMR_INIT_REG_MODE | RTMR_APPLY_ALL_CHAN)
#define RTMR_OPT_PRES_BIT_POS 15

#define BF_RTMR_MAX_FW (32768 + 20)

#define TX_COEFF_DATA_SET(data) (((data)&0x3f) << 8)
#define TX_COEFF_DATA_GET(data) (((data) >> 8) & 0x3f)
#define RTMR_CHAN_ADDR_SET(channel) (((channel)&0xf) << 8)

/* Retimer PRBS macros */
#define RTMR_DIR_INGRESS 0
#define RTMR_DIR_EGRESS 1
#define RTMR_MAX_DIR 2
#define RTMR_PRBS_GEN_SET 0xe800
#define RTMR_PRBS_GEN_MASK 0x300
#define RTMR_PRBS_GEN_MODE_SET(mode) (((mode)&0x3) << 8)
#define RTMR_PRBS_CKR_SET 0x400
#define RTMR_PRBS_CKR_MASK 0x3000
#define RTMR_PRBS_CKR_MODE_SET(mode) (((mode)&0x3) << 12)
#define RTMR_PRBS_CKR_RST_CNT_MASK 0x8000

#define RTMR_LB_MASK 0xc000
#define RTMR_LB_EN_MASK 0x40
#define RTMR_LB_TIMING_BIT 0x8000
#define RTMR_LB_RX_SER_BIT 0x100

#define RTMR_RX_SWAP_SET(swap) ((swap & 0x1) << 14)
#define RTMR_TX_SWAP_SET(swap) ((swap & 0x1) << 7)

/* Retimer tuning macros */
#define RTMR_RX_EYE_MARGIN_GET(value) \
  (int16_t)(((value)&0x4000) ? ((value)&0x7fff) | (0x8000) : ((value)&0x7fff))
#define RTMR_DFE_TAP_1_GET(value) ((value)&0x7f)
#define RTMR_DFE_TAP_2_GET(value) (((value) >> 8) & 0x7f)
#define RTMR_DFE_TAP_3_GET(value) ((value)&0x7f)
#define RTMR_DFE_TAP_MINUS1_GET(value) (((value) >> 8) & 0x7f)
#define RTMR_DFE_TAP_CALC(tap, sel) ((tap) * (((sel)*50) + 100) / 64)
#define RTMR_GET_DAC_SEL(value) (((value) >> 12) & 0xf)
#define RTMR_EYE_MARGIN_CALC(margin, sel) \
  (int16_t)((margin) * ((sel)*50 + 100) / 2048)

#define MAX_CTLE_VAL 7
#define RTMR_CTLE_VAL_SET(value) (((value)&0x7) << 11)
#define RTMR_CTLE_VAL_GET(value) (((value) >> 11) & 0x7)
#define RTMR_CTLE_OVERRIDE_ENABLE_BIT 0x8000

#define RTMR_TWO_CMP_BIT(val, cnt) \
  (((val) ^ (1 << ((cnt)-1))) - (1 << ((cnt)-1)))

/* Retimer PCA9535 reset macros */
#define BF_RTMR_PCA9535_MAX 2
#define ADDR_SWITCH_COMM 0xe8
#define DEFAULT_TIMEOUT_MS 200
#define MAV_PCA9535_REG_OUT 2
#define MAV_PCA9535_REG_CFG 6

#define BF_RTMR_PCA9535_DEV_40 0
#define BF_RTMR_PCA9535_DEV_42 1

#define BF_RTMR_PCA9535_CH_0 0
#define BF_RTMR_PCA9535_CH_1 1
#define BF_RTMR_PCA9535_CH_2 2
#define BF_RTMR_PCA9535_CH_3 3
#define BF_RTMR_PCA9535_CH_4 4
#define BF_RTMR_PCA9535_CH_5 5
#define BF_RTMR_PCA9535_CH_6 5
#define BF_RTMR_PCA9535_CH_7 7
#define BF_RTMR_PCA9535_CH_8 8
#define BF_RTMR_PCA9535_CH_9 9
#define BF_RTMR_PCA9535_CH_10 10
#define BF_RTMR_PCA9535_CH_11 11
#define BF_RTMR_PCA9535_CH_12 12
#define BF_RTMR_PCA9535_CH_13 13
#define BF_RTMR_PCA9535_CH_14 14
#define BF_RTMR_PCA9535_CH_15 15

/* Retimer firmware download macros */
#define RTMR_FW_FILE_LOC "bin/credo_firmware.bin"
#define RTMR_FW_CRC_OFFSET 0x1004
#define RTMR_FW_IMAGE_LEN_OFFSET 12
#define RTMR_FW_ENTER_POINT_OFFSET 8
#define RTMR_FW_RAM_ADDR_OFFSET 16
#define RTMR_FW_DATA_START_OFFSET 20

#define RTMR_FW_DL_ADDR_HI_OFFSET 12
#define RTMR_FW_DL_ADDR_LO_OFFSET 13
#define RTMR_FW_DL_CHECKSUM_OFFSET 14
#define RTMR_FW_DL_CTRL_OFFSET 15
#define RTMR_FW_DL_TRIGGER_CMD 0x800c

#define RTMR_INSTALL_DIR_MAX 100
#define RTMR_INSTALL_DIR_FULL_MAX 200
#endif /* _BF_PLTFM_RTMR_PRIV_H */
