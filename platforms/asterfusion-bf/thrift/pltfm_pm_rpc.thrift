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

typedef i32 pltfm_pm_status_t
typedef i32 pltfm_pm_dev_port_t
typedef byte pltfm_pm_device_t

exception InvalidPltfmPmOperation {
  1:i32 code
}

enum pltfm_pm_board_type_t {
  BF_PLTFM_BD_ID_MAVERICKS_P0A = 0x0234,
  BF_PLTFM_BD_ID_MAVERICKS_P0B = 0x1234,
  BF_PLTFM_BD_ID_MAVERICKS_P0C = 0x5234,
  BF_PLTFM_BD_ID_MONTARA_P0A = 0x2234,
  BF_PLTFM_BD_ID_MONTARA_P0B = 0x3234,
  BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU = 0x4234,
  BF_PLTFM_BD_ID_MONTARA_P0C = 0x6234,
  BF_PLTFM_BD_ID_UNKNOWN = 0XFFFF
}

service pltfm_pm_rpc {
    /* init */
    pltfm_pm_board_type_t pltfm_pm_board_type_get() throws (1:InvalidPltfmPmOperation ouch);
}
