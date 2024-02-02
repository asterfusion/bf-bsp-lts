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
/* Currently maximum is 5.
 * Modify this if the fact changes in the future.
 * by SunZheng, 2023-09-05. */
#define MAX_SYSCPLDS 5

enum {
    BF_MAV_SYSCPLD1 = 1,
    BF_MAV_SYSCPLD2,
    BF_MAV_SYSCPLD3,
    BF_MAV_SYSCPLD4,
    BF_MAV_SYSCPLD5,
    /* Add max syscpld enum for your platform and update bf_pltfm_mgr_ctx()->cpld_count. */
};

typedef enum {
    HC36Y24C_SYSCPLD1_I2C_ADDR = 0x40,
    HC36Y24C_SYSCPLD2_I2C_ADDR = 0x40,
    HC36Y24C_SYSCPLD3_I2C_ADDR = 0x40,
} hc36y24c_cpld_addr_t;

typedef enum {
    X532P_SYSCPLD1_I2C_ADDR = 0x40,
    X532P_SYSCPLD2_I2C_ADDR = 0x40,
} x532p_cpld_addr_t;

typedef enum {
    X564P_SYSCPLD1_I2C_ADDR = 0x40,
    X564P_SYSCPLD2_I2C_ADDR = 0x40,
    X564P_SYSCPLD3_I2C_ADDR = 0x40,
} x564p_cpld_addr_t;

typedef enum {
    X308P_SYSCPLD1_I2C_ADDR = 0x40,
    X308P_SYSCPLD2_I2C_ADDR = 0x40,
} x308p_cpld_addr_t;

typedef enum {
    X312P_SYSCPLD1_I2C_ADDR = 0x60,
    /* Only BMC can read 0x32. */
    X312P_SYSCPLD2_I2C_ADDR = 0x32,
    X312P_SYSCPLD3_I2C_ADDR = 0x62,
    X312P_SYSCPLD4_I2C_ADDR = 0x64,
    X312P_SYSCPLD5_I2C_ADDR = 0x66,
} x312p_cpld_addr_t;

typedef enum {
    X732Q_SYSCPLD1_I2C_ADDR = 0x40,
    X732Q_SYSCPLD2_I2C_ADDR = 0x40,
} x732q_cpld_addr_t;

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
    X312P_PCA9548_L1_0x71 = 0x71,
    X312P_PCA9548_L2_0x72 = 0x72,
    X312P_PCA9548_L2_0x73 = 0x73,
    X312P_PCA9548_L2_0x74 = 0x74
} x312p_pca9548_addr_t;

/** read bytes from slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to read
*  @param val
*   value to read into
*  @return
*   0 on success and otherwise in error
*/
int bf_pltfm_cp2112_reg_read_block (
    uint8_t addr,
    uint8_t reg,
    uint8_t *read_buf,
    uint32_t read_buf_size);

/** write bytes to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_block (
    uint8_t addr,
    uint8_t reg,
    uint8_t *write_buf,
    uint32_t write_buf_size);

/** write bytes to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_byte (
    uint8_t addr,
    uint8_t reg,
    uint8_t val);

/** read a byte from syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to read to
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_read_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t *buf);

/** write a byte to syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to write from
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_write_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t val);

/** write PCA9548 to select channel which connects to syscpld
*
*  @param cpld_index
*   index of cpld for a given platform
*  @return
*   0 on success otherwise in error
*/
int select_cpld (uint8_t cpld_index);

/** write PCA9548 back to zero to de-select syscpld
*
*  @return
*   0 on success otherwise in error
*/
int unselect_cpld();

int bf_pltfm_get_max_cplds ();
int bf_pltfm_get_cpld_ver (uint8_t cpld_index, char *version, bool forced);
int bf_pltfm_syscpld_init();

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_SYSCPLD_H */
