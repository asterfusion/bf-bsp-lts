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
    /* legacy start */
    BF_PLTFM_BD_ID_MAVERICKS_P0A     = 0x0234,
    BF_PLTFM_BD_ID_MAVERICKS_P0B     = 0x1234,
    BF_PLTFM_BD_ID_MONTARA_P0A       = 0x2234,
    BF_PLTFM_BD_ID_MONTARA_P0B       = 0x3234,
    BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU = 0x4234,
    BF_PLTFM_BD_ID_MAVERICKS_P0C     = 0x5234,
    BF_PLTFM_BD_ID_MONTARA_P0C       = 0x6234,
    BF_PLTFM_BD_ID_NEWPORT_P0A       = 0x1134,
    BF_PLTFM_BD_ID_NEWPORT_P0B       = 0x2134,
    /* legacy end */

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
    /* HC36Y24C-T and its subtype. */
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT0 = 0x2400,
    BF_PLTFM_BD_ID_HC36Y24C_V1DOT1 = 0x2401,

    BF_PLTFM_BD_ID_UNKNOWN = 0XFFFF
}

service pltfm_pm_rpc {
    /* init */
    pltfm_pm_board_type_t pltfm_pm_board_type_get() throws (1:InvalidPltfmPmOperation ouch);
}
