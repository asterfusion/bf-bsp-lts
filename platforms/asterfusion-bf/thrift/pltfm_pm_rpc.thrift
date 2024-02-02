/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*
        pltfm_pm thrift file
*/

namespace py pltfm_pm_rpc
namespace cpp pltfm_pm_rpc

typedef i32 pltfm_pm_dev_port_t

exception InvalidPltfmPmOperation {
  1:i32 code
}

enum pltfm_pm_board_type_t {
    /* Override bf_pltfm_board_id_e to private board powered by Asterfusion.
     * by tsihang, 2022-06-20. */
    /* X308P-T and its subtype. */
    BF_PLTFM_BD_ID_X308PT_V1DOT0 = 0x3080,
    BF_PLTFM_BD_ID_X308PT_V1DOT1 = 0x3081,
    BF_PLTFM_BD_ID_X308PT_V2DOT0 = 0x3081
    BF_PLTFM_BD_ID_X308PT_V3DOT0 = 0x3083
    /* X312P-T and its subtype. */
    BF_PLTFM_BD_ID_X312PT_V1DOT0 = 0x3120,
    BF_PLTFM_BD_ID_X312PT_V1DOT1 = 0x3120,
    BF_PLTFM_BD_ID_X312PT_V1DOT2 = 0x3122,
    BF_PLTFM_BD_ID_X312PT_V1DOT3 = 0x3123,
    BF_PLTFM_BD_ID_X312PT_V2DOT0 = 0x3122,
    BF_PLTFM_BD_ID_X312PT_V3DOT0 = 0x3123,
    BF_PLTFM_BD_ID_X312PT_V4DOT0 = 0x3124,
    BF_PLTFM_BD_ID_X312PT_V5DOT0 = 0x3125,
    /* X532P-T and its subtype. */
    BF_PLTFM_BD_ID_X532PT_V1DOT0 = 0x5320,
    BF_PLTFM_BD_ID_X532PT_V1DOT1 = 0x5321,
    BF_PLTFM_BD_ID_X532PT_V2DOT0 = 0x5322,
    BF_PLTFM_BD_ID_X532PT_V3DOT0 = 0x5323,
    /* X564P-T and its subtype. */
    BF_PLTFM_BD_ID_X564PT_V1DOT0 = 0x5640,
    BF_PLTFM_BD_ID_X564PT_V1DOT1 = 0x5641,
    BF_PLTFM_BD_ID_X564PT_V1DOT2 = 0x5642,
    BF_PLTFM_BD_ID_X564PT_V2DOT0 = 0x5643,
    BF_PLTFM_BD_ID_X732QT_V1DOT0 = 0x7320,
    /* HC36Y24C-T and its subtype. */
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT0 = 0x2400,
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT1 = 0x2401,

    /* AFN_X564PT-T and its subtype. */
    AFN_BD_ID_X564PT_V1P0 = 0x5640,
    AFN_BD_ID_X564PT_V1P1 = 0x5641,
    AFN_BD_ID_X564PT_V1P2 = 0x5642,
    AFN_BD_ID_X564PT_V2P0 = 0x5643,
    /* AFN_X532PT-T and its subtype. */
    AFN_BD_ID_X532PT_V1P0 = 0x5320,
    AFN_BD_ID_X532PT_V1P1 = 0x5321,
    AFN_BD_ID_X532PT_V2P0 = 0x5322,
    AFN_BD_ID_X532PT_V3P0 = 0x5323,
    /* AFN_X308PT-T and its subtype. */
    AFN_BD_ID_X308PT_V1P0 = 0x3080,
    AFN_BD_ID_X308PT_V1P1 = 0x3081,
    AFN_BD_ID_X308PT_V2P0 = 0x3082,  /* Announced as HW V2 to customer. */
    AFN_BD_ID_X308PT_V3P0 = 0x3083,  /* Announced as HW v3.0 with PTP hwcomp. */
    /* AFN_X312PT-T and its subtype. */
    AFN_BD_ID_X312PT_V1P0 = 0x3120,
    AFN_BD_ID_X312PT_V1P1 = 0x3120,
    AFN_BD_ID_X312PT_V1P2 = 0x3122,
    AFN_BD_ID_X312PT_V1P3 = 0x3123,
    AFN_BD_ID_X312PT_V2P0 = 0x3122,
    AFN_BD_ID_X312PT_V3P0 = 0x3123,
    AFN_BD_ID_X312PT_V4P0 = 0x3124,
    AFN_BD_ID_X312PT_V5P0 = 0x3125,
    AFN_BD_ID_X732QT_V1P0 = 0x7320,  /* Since Dec 2023. */
    /* HC36Y24C-T and its subtype. */
    AFN_BD_ID_HC36Y24C_V1P0 = 0x2400,
    AFN_BD_ID_HC36Y24C_V1P1 = 0x2401,

    BF_PLTFM_BD_ID_UNKNOWN = 0XFFFF
}

service pltfm_pm_rpc {
    /* init */
    pltfm_pm_board_type_t pltfm_pm_board_type_get() throws (1:InvalidPltfmPmOperation ouch);
}
