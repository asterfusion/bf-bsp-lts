/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*
        pltfm_mgr thrift file
*/

namespace py pltfm_mgr_rpc
namespace cpp pltfm_mgr_rpc

typedef i8  pltfm_mgr_device_t
typedef i32 pltfm_mgr_status_t
typedef i32 pltfm_mgr_ps_num_t
typedef i32 pltfm_mgr_fan_num_t
typedef i32 pltfm_mgr_port_num_t
typedef i32 pltfm_mgr_max_port_t
typedef i32 pltfm_mgr_fan_percent_t

struct pltfm_mgr_sys_tmp_t {
   1 : double tmp1,
   2 : double tmp2,
   3 : double tmp3,
   4 : double tmp4,
   5 : double tmp5,
   6 : double tmp6,
   7 : double tmp7,
   8 : double tmp8,
   9 : double tmp9,
  10 : double tmp10,
}

struct pltfm_mgr_eeprom_t {
   1 : string prod_name,
   2 : string prod_part_num,
   3 : string prod_ser_num,
   4 : string ext_mac_addr,
   5 : string sys_mfg_date,
   6 : i8     prod_ver,
   7 : string prod_sub_ver,
   8 : string prod_arch,
   9 : string onie_version,
  10 : i16    ext_mac_addr_size,
  11 : string sys_mfger,
  12 : string country_code,
  13 : string vendor_name,
  14 : string diag_version,
  15 : string serv_tag,
  16 : string asic_vendor,
  17 : string main_bd_version,
  18 : string come_version,
  19 : string ghc_bd0,
  20 : string ghc_bd1,
  21 : string resv,
  22 : i32    crc32,
}

struct pltfm_mgr_pwr_supply_info_t {
   1 : i32    vin,
   2 : i32    vout,
   3 : i32    iout,
   4 : i32    pwr_out,
   5 : i32    fspeed,
   6 : bool   ffault,
   7 : bool   load_sharing,
   8 : string model,
   9 : string serial,
  10 : string rev,
  11 : i32    iin,
  12 : i32    pwr_in,
  13 : i32    temp,
}

struct pltfm_mgr_pwr_rail_info_t {
   1 : i32 vrail1,
   2 : i32 vrail2,
   3 : i32 vrail3,
   4 : i32 vrail4,
   5 : i32 vrail5,
   6 : i32 vrail6,
   7 : i32 vrail7,
   8 : i32 vrail8,
   9 : i32 vrail9,
  10 : i32 vrail10,
  11 : i32 vrail11,
  12 : i32 vrail12,
  13 : i32 vrail13,
  14 : i32 vrail14,
  15 : i32 vrail15,
  16 : i32 vrail16,
}

struct pltfm_mgr_fan_info_t {
  1 : i32    fan_num,
  2 : i32    front_rpm,
  3 : i32    rear_rpm,
  4 : i32    percent,
  5 : string model,
  6 : string serial,
}

struct pltfm_mgr_cpld_info_t {
  1 : i32    cpld_count,
  2 : string cpld1_ver,
  3 : string cpld2_ver,
  4 : string cpld3_ver,
  5 : string cpld4_ver,
  6 : string cpld5_ver,
}

exception InvalidPltfmMgrOperation {
  1 : i32 code
}

service pltfm_mgr_rpc {
    /* init */
    /* type                     func                             args                                                              exception */
    pltfm_mgr_status_t          pltfm_mgr_dummy                  (1:pltfm_mgr_device_t device);
    pltfm_mgr_sys_tmp_t         pltfm_mgr_sys_tmp_get            ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_eeprom_t          pltfm_mgr_sys_eeprom_get         ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
    bool                        pltfm_mgr_pwr_supply_present_get (1:pltfm_mgr_ps_num_t ps_num)                                     throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_pwr_supply_info_t pltfm_mgr_pwr_supply_info_get    (1:pltfm_mgr_ps_num_t ps_num)                                     throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_pwr_rail_info_t   pltfm_mgr_pwr_rail_info_get      (1:pltfm_mgr_ps_num_t ps_num)                                     throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_status_t          pltfm_mgr_fan_speed_set          (1:pltfm_mgr_fan_num_t fan_num 2:pltfm_mgr_fan_percent_t percent) throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_fan_info_t        pltfm_mgr_fan_info_get           (1:pltfm_mgr_fan_num_t fan_num)                                   throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_max_port_t        pltfm_mgr_sfp_get_max_port       ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
    bool                        pltfm_mgr_sfp_presence_get       (1:i32 port_num)                                                  throws (1:InvalidPltfmMgrOperation ouch);
    string                      pltfm_mgr_sfp_info_get           (1:i32 port_num)                                                  throws (1:InvalidPltfmMgrOperation ouch);
    bool                        pltfm_mgr_qsfp_presence_get      (1:i32 port_num)                                                  throws (1:InvalidPltfmMgrOperation ouch);
    string                      pltfm_mgr_qsfp_info_get          (1:i32 port_num)                                                  throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_max_port_t        pltfm_mgr_qsfp_get_max_port      ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_status_t          pltfm_mgr_qsfp_reset             (1:i32 port_num 2:bool reset)                                     throws (1:InvalidPltfmMgrOperation ouch);
    bool                        pltfm_mgr_qsfp_lpmode_get        (1:i32 port_num)                                                  throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_status_t          pltfm_mgr_qsfp_lpmode_set        (1:i32 port_num 2:bool lpmode)                                    throws (1:InvalidPltfmMgrOperation ouch);
    string                      pltfm_mgr_sensor_info_get        (1:string options)                                                throws (1:InvalidPltfmMgrOperation ouch);
    string                      pltfm_mgr_bmc_version_get        ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
    pltfm_mgr_cpld_info_t       pltfm_mgr_cpld_info_get          ()                                                                throws (1:InvalidPltfmMgrOperation ouch);
}
