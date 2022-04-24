/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef __TOFINO_SPI_IF_H_
#define __TOFINO_SPI_IF_H_

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

#define BF_SPI_MAX_OUTDATA 8
#define BF_SPI_MAX_INDATA 4

#define BF_SPI_PERIOD_NS 200 /* SPI clock period in nano seconds */
/* SPI device OPcodes */
#define BF_SPI_OPCODE_WR_EN 6
#define BF_SPI_OPCODE_WR_DI 4
#define BF_SPI_OPCODE_WR_DATA 2
#define BF_SPI_OPCODE_RD_DATA 3

/* SPI EEPROM constants */
#define BF_SPI_EEPROM_SIZE (512 * 1024 / 8) /* 512 Kbits part */
#define BF_SPI_EEPROM_WR_SIZE 4             /* write 4 bytes at a time */
#define BF_SPI_EEPROM_RD_CMD_SIZE 2 /* bytes cnt to issue a eeprom read */
#define BF_SPI_EEPROM_RD_SIZE 4     /* write 4 bytes at a time */

/* SPI FSM Instructions */
#define BF_SPI_FSM_START 0x01
#define BF_SPI_FSM_SBUS_SINGLE 0x17
#define BF_SPI_FSM_SBUS_STREAM 0x25
#define BF_SPI_FSM_WAIT 0x32
#define BF_SPI_FSM_REG_WR 0x45
#define BF_SPI_FSM_I2CBYTE_WR 0x71
#define BF_SPI_FSM_END 0x61

/* Tofino SPI register offsets */
#define SPI_OUTDATA0_REG 0x400C8UL
#define SPI_OUTDATA1_REG 0x400CCUL
#define SPI_CMD_REG 0x400D0UL
#define SPI_INDATA_REG 0x400D4UL
#define SPI_IDCODE_REG 0x400D8UL

/* porting layer APIs */
int tofino_porting_spi_init (int dev_id,
                             void *arg);
int tofino_porting_spi_finish (int dev_id);
int tofino_porting_spi_reg_wr (int dev_id,
                               uint32_t reg, uint32_t data);
int tofino_porting_spi_reg_rd (int dev_id,
                               uint32_t reg, uint32_t *data);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
