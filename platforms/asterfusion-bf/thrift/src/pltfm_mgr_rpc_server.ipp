/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <iostream>

#include "pltfm_mgr_rpc.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
extern "C" {
#include <bf_pltfm_mgr/pltfm_mgr_handlers.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_qsfp/sff.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
}

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using namespace ::pltfm_mgr_rpc;


class pltfm_mgr_rpcHandler : virtual public pltfm_mgr_rpcIf {
 public:
  pltfm_mgr_rpcHandler() {}

  pltfm_mgr_status_t pltfm_mgr_dummy(const pltfm_mgr_device_t device) {
    /* Disable this call to prevent link issue on Debian 11. */
    //pltfm_mgr_dummy_call();
    return 0;
  }

  void pltfm_mgr_sys_tmp_get(pltfm_mgr_sys_tmp_t &_return) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_temperature_info_t bf_sys_tmp;

    sts = bf_pltfm_chss_mgmt_temperature_get(&bf_sys_tmp);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    _return.tmp1 = bf_sys_tmp.tmp1;
    _return.tmp2 = bf_sys_tmp.tmp2;
    _return.tmp3 = bf_sys_tmp.tmp3;
    _return.tmp4 = bf_sys_tmp.tmp4;
    _return.tmp5 = bf_sys_tmp.tmp5;
    _return.tmp6 = bf_sys_tmp.tmp6;
    _return.tmp7 = bf_sys_tmp.tmp7;
    _return.tmp8 = bf_sys_tmp.tmp8;
    _return.tmp9 = bf_sys_tmp.tmp9;
    _return.tmp10 = bf_sys_tmp.tmp10;

  }

  void pltfm_mgr_sys_eeprom_get(pltfm_mgr_eeprom_t &_return) {
    // pltfm_mgr_eeprom_t eeprom;
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_eeprom_t bf_eeprom;
    char str1[20];

    sts = bf_pltfm_bd_eeprom_get(&bf_eeprom);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    _return.prod_name =
        std::string(reinterpret_cast<char *>(bf_eeprom.bf_pltfm_product_name),
                    strlen(bf_eeprom.bf_pltfm_product_name));
    _return.prod_part_num =
        std::string(reinterpret_cast<char *>(bf_eeprom.bf_pltfm_product_number),
                    strlen(bf_eeprom.bf_pltfm_product_number));

    _return.prod_ser_num =
        std::string(reinterpret_cast<char *>(bf_eeprom.bf_pltfm_product_serial),
                    strlen(bf_eeprom.bf_pltfm_product_serial));

    memset(str1, 0, sizeof(str1));
    sprintf(str1,
            "%02x:%02x:%02x:%02x:%02x:%02x",
            bf_eeprom.bf_pltfm_mac_base[0],
            bf_eeprom.bf_pltfm_mac_base[1],
            bf_eeprom.bf_pltfm_mac_base[2],
            bf_eeprom.bf_pltfm_mac_base[3],
            bf_eeprom.bf_pltfm_mac_base[4],
            bf_eeprom.bf_pltfm_mac_base[5]);
    _return.ext_mac_addr.assign((const char *)(str1), sizeof(str1));


    _return.sys_mfg_date = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_system_manufacturing_date),
        strlen(bf_eeprom.bf_pltfm_system_manufacturing_date));


    _return.prod_ver = bf_eeprom.bf_pltfm_product_version;
    _return.prod_sub_ver = bf_eeprom.bf_pltfm_product_subversion;

    _return.prod_arch = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_product_platform_arch),
        strlen(bf_eeprom.bf_pltfm_product_platform_arch));

    _return.onie_version = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_onie_version),
        strlen(bf_eeprom.bf_pltfm_onie_version));

    _return.ext_mac_addr_size = bf_eeprom.bf_pltfm_mac_size;

    _return.sys_mfger = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_system_manufacturer),
        strlen(bf_eeprom.bf_pltfm_system_manufacturer));

    _return.country_code =
        std::string(reinterpret_cast<char *>(bf_eeprom.bf_pltfm_cc),
                    strlen(bf_eeprom.bf_pltfm_cc));

    _return.vendor_name = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_product_vendor),
        strlen(bf_eeprom.bf_pltfm_product_vendor));

    _return.diag_version = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_diag_version),
        strlen(bf_eeprom.bf_pltfm_diag_version));

    _return.serv_tag = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_serv_tag),
        strlen(bf_eeprom.bf_pltfm_serv_tag));

    _return.asic_vendor = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_asic_vendor),
        strlen(bf_eeprom.bf_pltfm_asic_vendor));

    _return.main_bd_version = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_main_board_version),
        strlen(bf_eeprom.bf_pltfm_main_board_version));

    _return.come_version = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_come_version),
        strlen(bf_eeprom.bf_pltfm_come_version));

    _return.ghc_bd0 = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_ghc_bd0_version),
        strlen(bf_eeprom.bf_pltfm_ghc_bd0_version));

    _return.ghc_bd1 = std::string(
        reinterpret_cast<char *>(bf_eeprom.bf_pltfm_ghc_bd1_version),
        strlen(bf_eeprom.bf_pltfm_ghc_bd1_version));

    /* Reserved 0xFD */

    /* last one, 0xFE */
    _return.crc32 = bf_eeprom.bf_pltfm_crc32;

    // return eeprom;
  }

  bool pltfm_mgr_pwr_supply_present_get(const pltfm_mgr_ps_num_t ps_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bool ps_present = false;

    sts = bf_pltfm_chss_mgmt_pwr_supply_prsnc_get((bf_pltfm_pwr_supply_t)ps_num,
                                                  &ps_present);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    return ps_present;
  }

  void pltfm_mgr_pwr_supply_info_get(pltfm_mgr_pwr_supply_info_t &_return,
                                     const pltfm_mgr_ps_num_t ps_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_pwr_supply_info_t bf_ps_info;

    sts = bf_pltfm_chss_mgmt_pwr_supply_get((bf_pltfm_pwr_supply_t)ps_num,
                                            &bf_ps_info);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    /* Raw PSU info returned to the callers. While, how to convert
     * VIN/VOUT/IIN/IOUT/PIN/POUT to be readable is another thing.
     * We strongly recommand the information provided by
     * this API like below and must keep same for all X-T platforms.
     * By tsihang, 2022-04/14. */
    _return.vin = bf_ps_info.vin;
    _return.vout = bf_ps_info.vout;
    _return.iin = bf_ps_info.iin;
    _return.iout = bf_ps_info.iout;
    _return.pwr_in = bf_ps_info.pwr_in;
    _return.pwr_out = bf_ps_info.pwr_out;
    _return.fspeed = bf_ps_info.fspeed;
    _return.ffault = bf_ps_info.ffault;
    _return.load_sharing = bf_ps_info.load_sharing;
    _return.model = bf_ps_info.model;
    _return.serial = bf_ps_info.serial;
    _return.rev = bf_ps_info.rev;
    _return.temp = bf_ps_info.temp;

    // return ps_info;
  }

  void pltfm_mgr_pwr_rail_info_get(pltfm_mgr_pwr_rail_info_t &_return,
                                   const pltfm_mgr_ps_num_t pwr_supply_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_pwr_rails_info_t bf_pwr_rail_info;

    sts = bf_pltfm_chss_mgmt_pwr_rails_get(&bf_pwr_rail_info);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    _return.vrail1 = bf_pwr_rail_info.vrail1;
    _return.vrail2 = bf_pwr_rail_info.vrail2;
    _return.vrail3 = bf_pwr_rail_info.vrail3;
    _return.vrail4 = bf_pwr_rail_info.vrail4;
    _return.vrail5 = bf_pwr_rail_info.vrail5;
    _return.vrail6 = bf_pwr_rail_info.vrail6;
    _return.vrail7 = bf_pwr_rail_info.vrail7;
    _return.vrail8 = bf_pwr_rail_info.vrail8;
    _return.vrail9 = bf_pwr_rail_info.vrail9;
    _return.vrail10 = bf_pwr_rail_info.vrail10;
    _return.vrail11 = bf_pwr_rail_info.vrail11;
    _return.vrail12 = bf_pwr_rail_info.vrail12;
    _return.vrail13 = bf_pwr_rail_info.vrail13;
    _return.vrail14 = bf_pwr_rail_info.vrail14;
    _return.vrail15 = bf_pwr_rail_info.vrail15;
    _return.vrail16 = bf_pwr_rail_info.vrail16;
  }

  pltfm_mgr_status_t pltfm_mgr_fan_speed_set(const pltfm_mgr_fan_num_t fan_num,
                                             const pltfm_mgr_fan_percent_t percent) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_fan_info_t bf_fan_info;

    bf_fan_info.fan_num = fan_num;
    bf_fan_info.percent = percent;

    sts = bf_pltfm_chss_mgmt_fan_speed_set(&bf_fan_info);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    return 0;
  }

  void pltfm_mgr_fan_info_get(pltfm_mgr_fan_info_t &_return,
                              const pltfm_mgr_fan_num_t fan_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    bf_pltfm_fan_data_t fan_data;

    sts = bf_pltfm_chss_mgmt_fan_data_get(&fan_data);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }
    /* FAN ID (fan num) is 1 based, and FAN DATA in fan_data.F is 0 based.
     * This must keep same for all X-T platforms.
     * By tsihang, 2022-04/14. */
    _return.fan_num   = fan_data.F[fan_num - 1].fan_num;
    _return.front_rpm = fan_data.F[fan_num - 1].front_speed;
    _return.rear_rpm  = fan_data.F[fan_num - 1].rear_speed;
    _return.percent   = fan_data.F[fan_num - 1].percent;
    _return.model     = fan_data.F[fan_num - 1].model;
    _return.serial    = fan_data.F[fan_num - 1].serial;
  }

  pltfm_mgr_max_port_t pltfm_mgr_sfp_get_max_port(void) {
    return bf_sfp_get_max_sfp_ports ();
  }

  bool pltfm_mgr_sfp_presence_get(const pltfm_mgr_port_num_t port_num) {
    return bf_sfp_is_present (port_num);
  }

  void pltfm_mgr_sfp_info_get(std::string &_return,
                              const pltfm_mgr_port_num_t port_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    uint8_t buf[512] = {0};
    uint32_t i;
    char str[1024] = {0};

    if (!bf_sfp_is_present (port_num)) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
    }

    /* idprom */
    sts = bf_sfp_get_cached_info (port_num, 0, buf);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    /* a2h. */
    sts = bf_sfp_get_cached_info (port_num, 1, (buf + 256));
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    // Page A0h
    for (i = 0; i < 256; i++) {
      sprintf(&(str[i * 2]), "%02x", buf[i]);
    }
    // Page A2h
    for (i = 256; i < 512; i++) {
      sprintf(&(str[i * 2]), "%02x", buf[i]);
    }

    _return.assign((const char *)(str), sizeof(str));
  }

  bool pltfm_mgr_qsfp_presence_get(const pltfm_mgr_port_num_t port_num) {
    return bf_qsfp_is_present (port_num);
  }

  void pltfm_mgr_qsfp_info_get(std::string &_return,
                               const pltfm_mgr_port_num_t port_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    uint8_t buf[6272] = {0};
    uint32_t i;
    char str[12544] = {0};

    if (!bf_qsfp_is_present (port_num)) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
    }

    sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE0_LOWER, buf);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }
    sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE0_UPPER, (buf + 128));
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }
    sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE1, (buf + 128 + MAX_QSFP_PAGE_SIZE));
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }
    if (bf_qsfp_is_cmis(port_num)) {
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE2, (buf + 128 + MAX_QSFP_PAGE_SIZE * 2));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE3, (buf + 128 + MAX_QSFP_PAGE_SIZE * 3));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE4, (buf + 128 + MAX_QSFP_PAGE_SIZE * 4));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      // Banked Pages
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE17, (buf + 128 + MAX_QSFP_PAGE_SIZE * 17));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE18, (buf + 128 + MAX_QSFP_PAGE_SIZE * 18));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE20, (buf + 128 + MAX_QSFP_PAGE_SIZE * 20));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE21, (buf + 128 + MAX_QSFP_PAGE_SIZE * 21));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE22, (buf + 128 + MAX_QSFP_PAGE_SIZE * 22));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE23, (buf + 128 + MAX_QSFP_PAGE_SIZE * 23));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE32, (buf + 128 + MAX_QSFP_PAGE_SIZE * 32));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE33, (buf + 128 + MAX_QSFP_PAGE_SIZE * 33));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE34, (buf + 128 + MAX_QSFP_PAGE_SIZE * 34));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE35, (buf + 128 + MAX_QSFP_PAGE_SIZE * 35));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE36, (buf + 128 + MAX_QSFP_PAGE_SIZE * 36));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE37, (buf + 128 + MAX_QSFP_PAGE_SIZE * 37));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE38, (buf + 128 + MAX_QSFP_PAGE_SIZE * 38));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE39, (buf + 128 + MAX_QSFP_PAGE_SIZE * 39));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE40, (buf + 128 + MAX_QSFP_PAGE_SIZE * 40));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE41, (buf + 128 + MAX_QSFP_PAGE_SIZE * 41));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE42, (buf + 128 + MAX_QSFP_PAGE_SIZE * 42));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE43, (buf + 128 + MAX_QSFP_PAGE_SIZE * 43));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE44, (buf + 128 + MAX_QSFP_PAGE_SIZE * 44));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE45, (buf + 128 + MAX_QSFP_PAGE_SIZE * 45));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE47, (buf + 128 + MAX_QSFP_PAGE_SIZE * 47));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
    } else {
      sts = bf_qsfp_get_cached_info(port_num, QSFP_PAGE3, (buf + 128 + MAX_QSFP_PAGE_SIZE * 3));
      if (sts != BF_SUCCESS) {
        InvalidPltfmMgrOperation iop;
        iop.code = sts;
        throw iop;
      }
    }

    for (i = 0; i < 6272; i++) {
      sprintf(&(str[i * 2]), "%02x", buf[i]);
    }

    _return.assign((const char *)(str), sizeof(str));
  }

  pltfm_mgr_max_port_t pltfm_mgr_qsfp_get_max_port(void) {
    return bf_qsfp_get_max_qsfp_ports ();
  }

  pltfm_mgr_status_t pltfm_mgr_qsfp_reset(const pltfm_mgr_port_num_t port_num,
                                          const bool reset) {
    return bf_qsfp_reset (port_num, reset);
  }

  bool pltfm_mgr_qsfp_lpmode_get(const pltfm_mgr_port_num_t port_num) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    uint32_t lower_ports = 0;
    uint32_t upper_ports = 0;
    uint32_t cpu_ports = 0;
    bool lp_mode = false;

    sts =
        bf_qsfp_get_transceiver_lpmode(&lower_ports, &upper_ports, &cpu_ports);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    if (port_num < 33) {
      lp_mode = ((lower_ports >> (port_num - 1)) & (0x1));
    } else if (port_num < 65) {
      lp_mode = ((upper_ports >> (port_num - 33)) & (0x1));
    } else if (port_num == 65) {
      lp_mode = cpu_ports;
    } else {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }
    return lp_mode;
  }

  pltfm_mgr_status_t pltfm_mgr_qsfp_lpmode_set(const pltfm_mgr_port_num_t port_num,
                                               const bool lpmode) {
    return bf_qsfp_set_transceiver_lpmode(port_num, (lpmode ? true : false));
  }

  void pltfm_mgr_sensor_info_get(std::string &_return,
                                 const std::string &options) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    char buf[BF_SNSR_OUT_BUF_SIZE] = {0};

    sts = pltfm_mgr_sensor_out_get(&options[0u], buf, sizeof(buf));
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    _return.assign((const char *)(buf), sizeof(buf));
  }

  void pltfm_mgr_bmc_version_get(std::string &_return) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    char bmc_ver[32] = {0};

    sts = bf_pltfm_get_bmc_ver(bmc_ver, false);
    if (sts != BF_SUCCESS) {
      InvalidPltfmMgrOperation iop;
      iop.code = sts;
      throw iop;
    }

    _return.assign((const char *)(bmc_ver), sizeof(bmc_ver));
  }

  void pltfm_mgr_cpld_info_get(pltfm_mgr_cpld_info_t &_return) {
    bf_pltfm_status_t sts = BF_SUCCESS;
    char cpld_ver[MAX_SYSCPLDS][8] = {{0}};

    for (int i = 0; i < (int)bf_pltfm_mgr_ctx()->cpld_count; i++) {
        bf_pltfm_get_cpld_ver((uint8_t)(i + 1), cpld_ver[i], true);
    }

    _return.cpld_count = bf_pltfm_mgr_ctx() -> cpld_count;
    _return.cpld1_ver =
        std::string(reinterpret_cast<char *>(cpld_ver[0]),
                    strlen(cpld_ver[0]));
    _return.cpld2_ver =
        std::string(reinterpret_cast<char *>(cpld_ver[1]),
                    strlen(cpld_ver[1]));
    _return.cpld3_ver =
        std::string(reinterpret_cast<char *>(cpld_ver[2]),
                    strlen(cpld_ver[2]));
    _return.cpld4_ver =
        std::string(reinterpret_cast<char *>(cpld_ver[3]),
                    strlen(cpld_ver[3]));
    _return.cpld5_ver =
        std::string(reinterpret_cast<char *>(cpld_ver[4]),
                    strlen(cpld_ver[4]));
  }

};
