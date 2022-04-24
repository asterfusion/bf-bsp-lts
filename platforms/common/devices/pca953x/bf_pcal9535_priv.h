/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PCAL9535_PRIV_H
#define _BF_PCAL9535_PRIV_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
    pcal9535a
    0x00: Input port 0 (0-7)
    0x01: Input port 1 (8-15)
    0x02: Output port 0
    0x03: Output port 1
    0x04: Polarity inversion port 0 (1: inversion for input, default=0)
    0x05: Polarity inversion port 1
    0x06: Configuration port 0 (1: Input, 0: output, default=1)
    0x07: Configuration port 1
    0x44: Input latch enable port 0 (0: not latch, generates interrupt, read
   input clears interrupt. 1: enable latch)
    0x45: Input latch enable port 1 (0: not latch, generates interrupt, read
   input clears interrupt. 1: enable latch)
    0x4A: Interrupt mask port 0 (1: disable, default:1)
    0x4B: Interrupt mask port 1
    0x4C: Interrupt status port 0
    0x4D: Interrupt status port 1
*/
enum {
    // Backward compatible with PCA9535
    PCAL9535_INPUT_PORT_REG_0 = 0,
    PCAL9535_INPUT_PORT_REG_1,
    PCAL9535_OUTPUT_PORT_REG_0,
    PCAL9535_OUTPUT_PORT_REG_1,
    PCAL9535_POL_INV_PORT_REG_0,
    PCAL9535_POL_INV_PORT_REG_1,
    PCAL9535_CONF_PORT_REG_0,
    PCAL9535_CONF_PORT_REG_1,

    // PCAL9535 specific
    PCAL9535_INPUT_LATCH_PORT_REG_0 = 0x44,
    PCAL9535_INPUT_LATCH_PORT_REG_1 = 0x45,
};

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !_BF_PCAL9535_PRIV_H */
