/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_SI5342_H
#define _BF_PLTFM_SI5342_H

#include <bfutils/uCli/ucli.h>

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#define I2C_ADDR_SI5342 (0x6B << 1)
#define SI5342_FIXED_PAGE_REG 0x01
#define SI5342_NVM_WR_REG 0x00E3
#define SI5342_DEV_RDY_REG 0x00FE
#define SI5342_DEV_RDY 0x0F

int bf_pltfm_si5342_init (void *arg);
int bf_pltfm_si5342_i2c_wr (int chip_id,
                            uint32_t reg, uint8_t data);
int bf_pltfm_si5342_i2c_rd (int chip_id,
                            uint32_t reg, uint8_t *data);
int bf_pltfm_si5342_ppm_set (int chip_id,
                             int ppm);
int bf_pltfm_si5342_ppm_get (int chip_id,
                             int *ppm);
int bf_pltfm_si5342_156mhz_ppm_config_set (
    int chip_id);
ucli_node_t *bf_pltfm_si5342_ucli_node_create (
    ucli_node_t *m);
#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_SI5342_H */
