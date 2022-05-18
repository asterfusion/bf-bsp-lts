/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_SLAVE_I2C_H
#define _BF_PLTFM_SLAVE_I2C_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TOFINO_I2c_BAR0 = 0,    /* regular PCIe space */
    TOFINO_I2c_BAR_MSI = 1, /* msix capability register space */
    TOFINO_I2c_BAR_CFG = 2, /* 4k pcie cap + 4K config space */
} Tofino_I2C_BAR_SEG;

#define I2C_ADDR_TOFINO (0x58 << 1)

typedef enum {
    TOFINO_I2C_OPCODE_LOCAL_WR = (0 << 5),
    TOFINO_I2C_OPCODE_LOCAL_RD = (1 << 5),
    TOFINO_I2C_OPCODE_IMM_WR = (2 << 5),
    TOFINO_I2C_OPCODE_IMM_RD = (3 << 5),
    TOFINO_I2C_OPCODE_DIR_WR = (4 << 5),
    TOFINO_I2C_OPCODE_DIR_RD = (5 << 5),
    TOFINO_I2C_OPCODE_INDIR_WR = (6 << 5),
    TOFINO_I2C_OPCODE_INDIR_RD = (7 << 5)
} TOFINO_I2C_CMD_OPCODE;

#define TOFINO_I2C_CMD_OPCODE_MASK 0xE0
#define TOFINO_I2C_BAR_SEG_SHFT 28

/* i2c access to tofino register space using indirect addressing mode */
int bf_pltfm_ind_i2c_rd (int chip_id,
                         uint64_t indir_addr, uint8_t *data);
int bf_pltfm_ind_i2c_wr (int chip_id,
                         uint64_t indir_addr, uint8_t *data);
/* i2c access to tofino register space using direct addressing mode */
int bf_pltfm_dir_i2c_rd (int chip_id,
                         Tofino_I2C_BAR_SEG seg,
                         uint32_t dir_addr,
                         uint32_t *data);
int bf_pltfm_dir_i2c_wr (int chip_id,
                         Tofino_I2C_BAR_SEG seg,
                         uint32_t dir_addr,
                         uint32_t data);
int bf_pltfm_switchd_dir_i2c_rd (int chip_id,
                                 uint32_t dir_addr, uint32_t *data);
int bf_pltfm_switchd_dir_i2c_wr (int chip_id,
                                 uint32_t dir_addr, uint32_t data);
/* i2c access to tofino local register space (diagnostic use only) */
int bf_pltfm_loc_i2c_rd (int chip_id,
                         uint8_t local_reg, uint8_t *data);
int bf_pltfm_loc_i2c_wr (int chip_id,
                         uint8_t local_reg, uint8_t data);
int bf_pltfm_slave_i2c_init (void *arg);
ucli_node_t *bf_pltfm_slave_i2c_ucli_node_create (
    ucli_node_t *m);
#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_SLAVE_I2C_H */
