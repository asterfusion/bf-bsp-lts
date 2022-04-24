/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <iostream>

#include "pltfm_pm_rpc.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
extern "C" {
//#include <bf_pltfm_bd_cfg/bf_pltfm_bd_cfg_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
}

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::pltfm_pm_rpc;


class pltfm_pm_rpcHandler : virtual public pltfm_pm_rpcIf {
 public:
  pltfm_pm_rpcHandler() {}

  pltfm_pm_board_type_t::type pltfm_pm_board_type_get () {
    
    bf_pltfm_board_id_t bd_id;
    bf_status_t sts;
    pltfm_pm_board_type_t::type bd;

    bf_pltfm_chss_mgmt_bd_type_get(&bd_id);
    
    switch (bd_id) {
      case BF_PLTFM_BD_ID_MAVERICKS_P0A:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MAVERICKS_P0A;
        break;
      case BF_PLTFM_BD_ID_MAVERICKS_P0B:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MAVERICKS_P0B;
        break;
      case BF_PLTFM_BD_ID_MAVERICKS_P0C:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MAVERICKS_P0C;
        break;
      case BF_PLTFM_BD_ID_MONTARA_P0A:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MONTARA_P0A;
        break;
      case BF_PLTFM_BD_ID_MONTARA_P0B:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MONTARA_P0B;
        break;
      case BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU;
        break;
      case BF_PLTFM_BD_ID_MONTARA_P0C:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_MONTARA_P0C;
        break;
      case BF_PLTFM_BD_ID_UNKNOWN:
      default:
        bd = pltfm_pm_board_type_t::BF_PLTFM_BD_ID_UNKNOWN;
        break;
    }
    return bd; 
  }

};
