/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_CHSS_MGMT_H
#define _BF_PLTFM_CHSS_MGMT_H

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <dvm/bf_drv_intf.h>
#include <port_mgr/bf_port_if.h>
#include <port_mgr/bf_serdes_if.h>
#include <port_mgr/port_mgr_intf.h>
#include <lld/lld_reg_if.h>
#include <bf_pltfm.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize all the sub modules like EEPROM, Fan, etc.
 */
bf_pltfm_status_t bf_pltfm_chss_mgmt_init();

// Board EEPROM functions
#define BFN_EEPROM_F_MAGIC 2
#define BFN_EEPROM_F_VERSION 1
#define BFN_EEPROM_F_PRODUCT_NAME 12
#define BFN_EEPROM_F_PRODUCT_NUMBER 16
#define BFN_EEPROM_F_ASSEMBLY_NUMBER 12
#define BFN_EEPROM_F_BAREFOOT_PCBA_NUMBER 12
#define BFN_EEPROM_F_BAREFOOT_PCB_NUMBER 12
#define BFN_EEPROM_F_ODM_PCBA_NUMBER 13
#define BFN_EEPROM_F_ODM_PCBA_SERIAL 12
#define BFN_EEPROM_F_PRODUCT_STATE 1
#define BFN_EEPROM_F_PRODUCT_VERSION 1
#define BFN_EEPROM_F_PRODUCT_SUBVERSION 8
#define BFN_EEPROM_F_PRODUCT_SERIAL 12
#define BFN_EEPROM_F_PRODUCT_ASSET 48
#define BFN_EEPROM_F_SYSTEM_MANUFACTURER 12
#define BFN_EEPROM_F_SYSTEM_MANUFACTURING_DATA 19
#define BFN_EEPROM_F_SYSTEM_MANU_DATE 4
#define BFN_EEPROM_F_PRODUCT_VENDOR 12
#define BFN_EEPROM_F_ASSEMBLED 8
#define BFN_EEPROM_F_LOCAL_MAC 12
#define BFN_EEPROM_F_EXT_MAC_BASE 12
#define BFN_EEPROM_F_EXT_MAC_SIZE 2
#define BFN_EEPROM_F_LOCATION 8
#define BFN_EEPROM_F_CRC8 1

#define BF_PLTFM_SYS_MAC_ADDR_MAX 8

#define BF_SNSR_OUT_BUF_SIZE 4096

typedef struct bf_pltfm_eeprom_t {

    /* Product name. Must be the first element */
    /* Product Name(0x21): X532P-T-S/E/A. */
    char bf_pltfm_product_name[BFN_EEPROM_F_PRODUCT_NAME
                               + 1];

    /* Product Part Number(0x22): ONBP1U-2Y32C-S.*/
    char bf_pltfm_product_number[BFN_EEPROM_F_PRODUCT_NUMBER
                                 + 1];

    /* Product Serial Number(0x23): 201000011 */
    char bf_pltfm_product_serial[BFN_EEPROM_F_PRODUCT_SERIAL
                                 + 1];

    /* Extended MAC Address(0x24): XX:XX:XX:XX:XX:XX */
    uint8_t bf_pltfm_mac_base[6];

    /* System Manufacturing Date(0x25): MM/DD/YY hh:mm:ss */
    char bf_pltfm_system_manufacturing_date[BFN_EEPROM_F_SYSTEM_MANUFACTURING_DATA
                                            + 1];

    /* Product Version(0x26): X */
    uint8_t bf_pltfm_product_version;

    /* Product Sub Version(0x27): X */
    char bf_pltfm_product_subversion[BFN_EEPROM_F_PRODUCT_SUBVERSION + 1];

    /* Product platform arch(0x28): x86_64-asterfusion_x532p_t-r0 */
    char bf_pltfm_product_platform_arch[BFN_EEPROM_F_PRODUCT_ASSET
                                        + 1];

    /* ONIE version(0x29): master-202008111723-dirty*/
    char bf_pltfm_onie_version[BFN_EEPROM_F_ODM_PCBA_SERIAL
                               + 1];

    /* Extended MAC Address Size(0x2a):  1*/
    uint16_t bf_pltfm_mac_size;

    /* System Manufacturer(0x2b): Asterfusion */
    char bf_pltfm_system_manufacturer[BFN_EEPROM_F_SYSTEM_MANUFACTURER
                                      + 1];

    /* Country code(0x2c): CN */
    char bf_pltfm_cc[BFN_EEPROM_F_LOCATION + 1];

    /* Product Manufacturer(0x2d): Asterfusion */
    char bf_pltfm_product_vendor[BFN_EEPROM_F_PRODUCT_VENDOR
                                 + 1];

    /* Diag Version(0x2e): 1.0 */
    char bf_pltfm_diag_version[8 + 1];

    /* Serv Tag(0x2f): X */
    char bf_pltfm_serv_tag[1 + 1];

    /* ASIC Vendor(0x30): Intel-BF */
    char bf_pltfm_asic_vendor[BFN_EEPROM_F_PRODUCT_VENDOR
                              + 1];

    /* Main board version(0x31): APNS320T-A1-V1.1 */
    char bf_pltfm_main_board_version[32 + 1];

    /* CPU subsystem version(0x32): CME5008-16GB-HV-CGT */
    char bf_pltfm_come_version[32 + 1];

    /* Computing subsystem version(0x33): */
    char bf_pltfm_ghc_bd0_version[32 + 1];

    /* Computing subsystem version(0x34): */
    char bf_pltfm_ghc_bd1_version[32 + 1];

    /* Resv(0xFD) */
    char bf_pltfm_resv;

    /* CRC32(0xFE) */
    uint32_t bf_pltfm_crc32;
} bf_pltfm_eeprom_t;

bf_pltfm_status_t bf_pltfm_chss_mgmt_bd_type_get (
    bf_pltfm_board_id_t *board_id);
bf_pltfm_status_t bf_pltfm_bd_eeprom_get (
    bf_pltfm_eeprom_t *ee_data);

/*
 * Power Supply unit
 */

typedef enum bf_pltfm_pwr_supply_e {
    POWER_SUPPLY1 = 1,
    POWER_SUPPLY2 = 2
} bf_pltfm_pwr_supply_t;

#define PSU_INFO_VALID_TEMP         (1 << 0)
#define PSU_INFO_VALID_FFAULT       (1 << 1)
#define PSU_INFO_VALID_LOAD_SHARING (1 << 2)
#define PSU_INFO_VALID_MODEL        (1 << 3)
#define PSU_INFO_VALID_SERIAL       (1 << 4)
#define PSU_INFO_VALID_REV          (1 << 5)
/* Has PSU fan, so fan rota is valid. */
#define PSU_INFO_VALID_FAN_ROTA     (1 << 6)
/* AC or DC, if not AC so DC. */
#define PSU_INFO_AC                 (1 << 7)

typedef struct bf_pltfm_pwr_supply_info_t {
    uint32_t vin;      /* Input voltage in milli Volts */
    uint32_t vout;     /* Output voltage in milli Volts */
    uint32_t iin;      /* Input current in milli Amperes */
    uint32_t iout;     /* Output current in milli Amperes */
    uint32_t pwr_out;  /* Output power in milli watts */
    uint32_t pwr_in;   /* Input power in milli watts */
    bool presence;     /* Power supply present or not */
    bool power;        /* Is there power to PSU */

    uint8_t fvalid;    /* Valid flags for the following units */
    uint32_t fspeed;   /* Fan speed in RPM */
    bool ffault;       /* Fan fault TRUE/FALSE */
    bool load_sharing; /* load sharing TRUE/FALSE */
    char model[32];    /* Model number */
    char serial[32];   /* Serial number */
    char rev[32];      /* Revision number */
    uint8_t temp;      /* Temperature */

} bf_pltfm_pwr_supply_info_t;

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_supply_get (
    bf_pltfm_pwr_supply_t pwr,
    bf_pltfm_pwr_supply_info_t *info);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_supply_prsnc_get (
    bf_pltfm_pwr_supply_t pwr, bool *present);

/* Temperature sensors */
typedef struct bf_pltfm_temperature_info_t {
    /* x312p-t -> LM75 */
    /* x308p-t -> Mainboard Front Left */
    /* x532p-t -> Mainboard Front Left */
    /* x564p-t -> Mainboard Front Left */
    float tmp1;  /* tempearture of sensor 1 in C */
    /* x312p-t -> LM63 */
    /* x308p-t -> Mainboard Front Right */
    /* x532p-t -> Mainboard Front Right */
    /* x564p-t -> Mainboard Front Right */
    float tmp2;  /* tempearture of sensor 2 in C */
    /* x312p-t -> LM86 */
    /* x308p-t -> FAN1 <Save temp of FAN2 to temp10. > */
    /* x532p-t -> ASIC Ambient */
    /* x564p-t -> ASIC Ambient */
    float tmp3;  /* tempearture of sensor 3 in C */
    /* x312p-t -> DPU-1 Junction */
    /* x308p-t -> DPU-1 Junction */
    /* x532p-t -> ASIC Junction */
    /* x564p-t -> ASIC Junction */
    float tmp4;  /* tempearture of sensor 4 in C */
    /* x312p-t -> DPU-1 Ambient */
    /* x308p-t -> DPU-1 Ambient */
    /* x532p-t -> Fan 1 */
    /* x564p-t -> Fan 1 */
    float tmp5;  /* tempearture of sensor 5 in C */
    /* x312p-t -> DPU-2 Junction */
    /* x308p-t -> DPU-2 Junction */
    /* x532p-t -> Fan 2 */
    /* x564p-t -> Fan 2 */
    float tmp6;  /* tempearture of sensor 6 in C */
    /* x312p-t -> DPU-2 Ambient */
    /* x308p-t -> DPU-2 Ambient */
    /* x532p-t -> Not Defined */
    /* x564p-t -> Mainboard Rear Left (available on V2.0) */
    float tmp7;  /* tempearture of sensor 7 in C */
    /* x312p-t -> BF Junction */
    /* x308p-t -> BF Junction */
    /* x532p-t -> Not Defined */
    /* x564p-t -> Mainboard Rear Right (available on V2.0) */
    float tmp8;  /* tempearture of sensor 8 in C */
    /* x312p-t -> BF Ambient */
    /* x308p-t -> BF Ambient */
    /* x532p-t -> Not Defined */
    /* x564p-t -> Not Defined */
    float tmp9;  /* tempearture of sensor 9 in C */
    /* x312p-t -> Not Defined */
    /* x308p-t -> Not Defined */
    /* x532p-t -> Not Defined */
    /* x564p-t -> Not Defined */
    float tmp10; /* tempearture of sensor 10 in C */
} bf_pltfm_temperature_info_t;

/* Switch temperature sensors */
typedef struct
    bf_pltfm_switch_temperature_info_t {
    uint32_t remote_sensor;
    uint32_t main_sensor;
} bf_pltfm_switch_temperature_info_t;

bf_pltfm_status_t
bf_pltfm_chss_mgmt_temperature_get (
    bf_pltfm_temperature_info_t *t);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_switch_temperature_get (
    bf_dev_id_t dev_id, int sensor,
    bf_pltfm_switch_temperature_info_t *tmp);

/* Power Rails */
typedef struct bf_pltfm_pwr_rails_info_t {
    /* Barefoot_Core */
    uint32_t vrail1;  /* Voltage of rail 1 in mV */
    /* Barefoot_AVDD_0_9V */
    uint32_t vrail2;  /* Voltage of rail 2 in mV */
    /* Payload_12V */
    uint32_t vrail3;  /* Voltage of rail 3 in mV */
    /* Payload_3_3V */
    uint32_t vrail4;  /* Voltage of rail 4 in mV */
    /* Payload_5V */
    uint32_t vrail5;  /* Voltage of rail 5 in mV */
    /* Payload_2_5V */
    uint32_t vrail6;  /* Voltage of rail 6 in mV */
    /* 88E6131_1_9V */
    uint32_t vrail7;  /* Voltage of rail 7 in mV */
    /* 88E6131_1_2V */
    uint32_t vrail8;  /* Voltage of rail 8 in mV */

    /* Not defined. */
    uint32_t vrail9;  /* Voltage of rail 9 in mV */
    uint32_t vrail10; /* Voltage of rail 10 in mV */
    uint32_t vrail11; /* Voltage of rail 11 in mV */
    uint32_t vrail12; /* Voltage of rail 12 in mV */
    uint32_t vrail13; /* Voltage of rail 13 in mV */
    uint32_t vrail14; /* Voltage of rail 14 in mV */
    uint32_t vrail15; /* Voltage of rail 15 in mV */
    uint32_t vrail16; /* Voltage of rail 16 in mV */
} bf_pltfm_pwr_rails_info_t;

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_rails_get (
    bf_pltfm_pwr_rails_info_t *pwr_rails);

/* Fan tray presence */
#define FAN_INFO_VALID_MODEL        (1 << 0)
#define FAN_INFO_VALID_SERIAL       (1 << 1)
#define FAN_INFO_VALID_MAX_SPEED    (1 << 2)

typedef struct bf_pltfm_fan_info_t {
    uint32_t present;     /* Fan present */
    uint32_t fan_num;     /* Fan number */
    uint32_t front_speed; /* Front fan speed */
    uint32_t rear_speed;  /* Rear fan speed */
    uint32_t direction;   /* Fan direction */
    uint32_t percent;     /* Percentage of Max speed
                         * at which both fans are
                         * running */
    uint32_t speed_level;

    /* 1 based group and keep the maximum group to bf_pltfm_mgr_ctx()->fan_group_count.
     * left->right at rear.
     * added by tsihang, 2022-07-08. */
    uint8_t group;

    uint8_t fvalid;       /* Valid flags for the following units */
    char model[32];       /* Model number */
    char serial[32];      /* Serial number */
    uint32_t max_speed;   /* Max fan speed */
} bf_pltfm_fan_info_t;

typedef struct bf_pltfm_fan_data_t {
    bf_pltfm_fan_info_t
    F[12]; /* 12 is Max number of fans*/
    uint8_t fantray_present;   /* Fan tray presence */
} bf_pltfm_fan_data_t;

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_init();
bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_init();
bf_pltfm_status_t
bf_pltfm_chss_mgmt_tmp_init();
bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_rails_init();

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_data_get (
    bf_pltfm_fan_data_t *fdata);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_speed_set (
    bf_pltfm_fan_info_t *fdata);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_sys_mac_addr_get (
    uint8_t *mac_info, uint8_t *num_sys_addresses);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_port_mac_addr_get (
    bf_pltfm_port_info_t *port_info,
    uint8_t *mac_info);

bf_pltfm_status_t pltfm_mgr_sensor_out_get (
    const char *options,
    char *info,
    size_t info_size);

bf_pltfm_status_t
bf_pltfm_chss_mgmt_platform_type_get (
    uint8_t *type, uint8_t *subtype);

bool platform_type_equal (uint8_t type);
bool platform_subtype_equal (uint8_t subtype);

#define BMC_COMM_INTERVAL_US    500000

void bf_pltfm_start_312_i2c_wdt(void);
void bf_pltfm_set_312_bmc_comm_interval(uint32_t us);
uint32_t bf_pltfm_get_312_bmc_comm_interval(void);

typedef enum COME_type {
    COME_UNKNOWN,
    CME3000,
    CME7000,
    CG1508,
    CG1527,
    ADV1508,
    ADV1527,
    ADV1548,
    S021508,
    S021527,
} COME_type;

struct x86_carrier_board_t {
    const char *desc;
    COME_type type;
};

extern uint8_t bmc_i2c_bus;
extern unsigned char bmc_i2c_addr;
extern COME_type global_come_type;

typedef enum cpld_path_e {
    VIA_NULL,
    VIA_CGOS,
    VIA_CP2112,
    VIA_SIO,
} cpld_path_e;

cpld_path_e bf_pltfm_find_path_to_cpld();

bf_pltfm_status_t bf_pltfm_get_bmc_ver(char *bmc_ver, bool forced);
bf_pltfm_status_t bf_pltfm_compare_bmc_ver(char *cmp_ver);

/* Porting CMEXXX, by tsihang, 2019-09-18. */
#define is_HVXXX (\
    (global_come_type == CME3000) ||\
    (global_come_type == CME7000))

/* Porting CGT15XX, by tsihang, 2020-02-18. */
#define is_CG15XX (\
    (global_come_type == CG1508) ||\
    (global_come_type == CG1527))

/* Porting YH15XX. by tsihang, 2022-06-22. */
#define is_ADV15XX (\
    (global_come_type == ADV1508) ||\
    (global_come_type == ADV1527) ||\
    (global_come_type == ADV1548))

/* Porting S02XX. by tsihang, 2022-06-22. */
#define is_S02XXX (\
    (global_come_type == S021508) ||\
    (global_come_type == S021527))

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_pltfm_chss_mgmt_ucli_node_create (
    ucli_node_t *m);
#endif

#ifdef __cplusplus
}
#endif /* C++ */

#endif
