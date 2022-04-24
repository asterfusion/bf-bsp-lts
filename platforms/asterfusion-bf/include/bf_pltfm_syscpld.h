/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PLTFM_SYSCPLD_H
#define _BF_PLTFM_SYSCPLD_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#define BF_MAV_SYSCPLD_I2C_ADDR   (0x40)   /* syscpld i2c_addr on cp2112 */
#define BF_MAV_SYSCPLD1_I2C_ADDR  (0x40)   /* syscpld */
#define BF_MAV_SYSCPLD2_I2C_ADDR  (0x40)
#define BF_MAV_SYSCPLD3_I2C_ADDR  (0x40)

#define BF_MON_SYSCPLD1_I2C_ADDR  (0x60)
#define BF_MON_SYSCPLD2_I2C_ADDR  (0x32)
#define BF_MON_SYSCPLD3_I2C_ADDR  (0x62)
#define BF_MON_SYSCPLD4_I2C_ADDR  (0x64)
#define BF_MON_SYSCPLD5_I2C_ADDR  (0x66)

enum {
    BF_MAV_SYSCPLD1 = 1,
    BF_MAV_SYSCPLD2,
    BF_MAV_SYSCPLD3,
};

enum {
    BF_MON_SYSCPLD1 = 1,
    BF_MON_SYSCPLD2,
    BF_MON_SYSCPLD3,
    BF_MON_SYSCPLD4,
    BF_MON_SYSCPLD5,
};

typedef enum {
    X312P_QSFP_RST_1 = 0x00,
    X312P_QSFP_RST_2 = 0x01,
    X312P_QSFP_PRS_1 = 0x02,
    X312P_QSFP_PRS_2 = 0x03,
    X312P_QSFP_INT_1 = 0x04,
    X312P_QSFP_INT_2 = 0x05,
    X312P_QSFP_LPM_1 = 0x06,
    X312P_QSFP_LPM_2 = 0x07,
    X312P_QSFP_MS_1 = 0x08,
    X312P_QSFP_MS_2 = 0x09,
    X312P_CPLD1_MODULE_CHANGE_STATUS = 0x0A,
    X312P_PWR_OSC   = 0x0B,   /* System Power Alert, R/W */
    X312P_SYS_RST   = 0x0C,   /* System Rest, R/W */
    X312P_TEST_CORE = 0x0D,
    X312P_SYS_GPIO  = 0x0E,
    X312P_BARE_SYNC = 0x0F,
    X312P_TF_ROV    = 0x10,
} x312p_cpld1_reg_t;

typedef enum {
    X312P_SFP_PRS_01_08 = 0x00,
    X312P_SFP_PRS_09_15 = 0x01,
    X312P_SFP_PRS_YM_YN = 0x02,
    X312P_SFP_LOS_01_08 = 0x03,
    X312P_SFP_LOS_09_15 = 0x04,
    X312P_SFP_LOS_YM_YN = 0x05,
    X312P_SFP_ENB_01_08 = 0x06,
    X312P_SFP_ENB_09_15 = 0x07,
    X312P_SFP_ENB_YM_YN = 0x08,
    X312P_SFP_TX_FAULT_01_08 = 0x09,
    X312P_SFP_TX_FAULT_09_15 = 0x0A,
    X312P_SFP_TX_FAULT_YM_YN = 0x0B,
    X312P_CPLD3_MODULE_CHANGE_STATUS = 0x0C,
    X312P_CPLD3_RST     = 0x0D,
    X312P_CPLD3_CODE_VERSION = 0x0E
} x312p_cpld3_reg_t;

typedef enum {
    X312P_SFP_PRS_16_23 = 0x00,
    X312P_SFP_PRS_24_31 = 0x01,
    X312P_SFP_PRS_32_32 = 0x02,
    X312P_SFP_LOS_16_23 = 0x03,
    X312P_SFP_LOS_24_31 = 0x04,
    X312P_SFP_LOS_32_32 = 0x05,
    X312P_SFP_ENB_16_23 = 0x06,
    X312P_SFP_ENB_24_31 = 0x07,
    X312P_SFP_ENB_32_32 = 0x08,
    X312P_SFP_TX_FAULT_16_23 = 0x09,
    X312P_SFP_TX_FAULT_24_31 = 0x0A,
    X312P_SFP_TX_FAULT_32_32 = 0x0B,
    X312P_CPLD4_MODULE_CHANGE_STATUS = 0x0C,
    X312P_CPLD4_RST     = 0x0D,
    X312P_CPLD4_CODE_VERSION = 0x0E
} x312p_cpld4_reg_t;

typedef enum {
    X312P_SFP_PRS_33_40 = 0x00,
    X312P_SFP_PRS_41_48 = 0x01,
    X312P_SFP_LOS_33_40 = 0x02,
    X312P_SFP_LOS_41_48 = 0x03,
    X312P_SFP_ENB_33_40 = 0x04,
    X312P_SFP_ENB_41_48 = 0x05,
    X312P_SFP_TX_FAULT_33_40 = 0x06,
    X312P_SFP_TX_FAULT_41_48 = 0x07,
    X312P_CPLD5_MODULE_CHANGE_STATUS = 0x08,
    X312P_CPLD5_RST     = 0x09,
    X312P_CPLD5_CODE_VERSION = 0x0A
} x312p_cpld5_reg_t;

typedef enum {
    X312P_PCA9548_L1_0X71 = 0x71,
    X312P_PCA9548_L2_0x72 = 0x72,
    X312P_PCA9548_L2_0x73 = 0x73,
    X312P_PCA9548_L2_0x74 = 0x74
} x312p_pca9548_addr_t;

int bf_pltfm_cpld_read_bytes (
    int cpld_index,
    uint8_t *buf,
    uint8_t rdlen);

int bf_pltfm_cpld_write_bytes (
    int cpld_index,
    uint8_t offset,
    uint8_t *buf,
    uint8_t wrlen);

int bf_pltfm_cpld_read_byte (
    int cpld_index,
    uint8_t offset,
    uint8_t *buf);

int bf_pltfm_cpld_write_byte (
    int cpld_index,
    uint8_t offset,
    uint8_t val);

int select_cpld (int cpld);
int unselect_cpld();


int bf_pltfm_syscpld_init();

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_SYSCPLD_H */
