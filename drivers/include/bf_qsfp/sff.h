/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

/*******************************************************************************
 *  the contents of this file is partially derived from
 *  https://github.com/facebook/fboss
 *
 *  Many changes have been applied all thru to derive the contents in this
 *  file.
 *  Please refer to the licensing information on the link proved above.
 *
 ******************************************************************************/
#ifndef __SFF_H
#define __SFF_H

#include "sff_standards.h"

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/*
 * Management interface spec for well-known module form factors.
 * by tsihang, 2021-07-15.
 *
 * SFP+/SFP28 and later    -> SFF-8472
 * QSFP+                   -> SFF-8436
 * QSFP+/QSFP28 and later  -> SFF-8636 or CMIS
 * QSFP-DD/QSFP-DD800      -> CMIS
 * OSFP                    -> CMIS
 * SFP-DD                  -> SFP-DD MGMT interface spec
 * MicroQSFP               -> SFF-8436
 *
 */

#define MAX_SFF_PAGE_SIZE 128


typedef enum {
    /* Shared QSFP and SFP fields: */
    IDENTIFIER, /* Type of Transceiver */
    STATUS,     /* Support flags for upper pages */
    TEMPERATURE_ALARMS,
    VCC_ALARMS, /* Voltage */
    CHANNEL_RX_PWR_ALARMS,
    CHANNEL_TX_BIAS_ALARMS,
    TEMPERATURE,
    VCC, /* Voltage */
    CHANNEL_RX_PWR,
    CHANNEL_TX_BIAS,
    CHANNEL_TX_DISABLE,
    POWER_CONTROL,
    ETHERNET_COMPLIANCE,
    EXTENDED_IDENTIFIER,
    PAGE_SELECT_BYTE,
    LENGTH_SM_KM, /* Single mode, in km */
    LENGTH_SM,    /* Single mode in 100m (not in QSFP) */
    LENGTH_OM3,
    LENGTH_OM2,
    LENGTH_OM1,
    LENGTH_COPPER,
    VENDOR_NAME,     /* QSFP Vendor Name (ASCII) */
    VENDOR_OUI,      /* QSFP Vendor IEEE company ID */
    PART_NUMBER,     /* Part NUmber provided by QSFP vendor (ASCII) */
    REVISION_NUMBER, /* Revision number */
    ETHERNET_EXTENDED_COMPLIANCE, /* ethernet extended compliance code */
    VENDOR_SERIAL_NUMBER,         /* Vendor Serial Number (ASCII) */
    MFG_DATE,                     /* Manufacturing date code */
    DIAGNOSTIC_MONITORING_TYPE,   /* Diagnostic monitoring implemented */
    TEMPERATURE_THRESH,
    VCC_THRESH,
    RX_PWR_THRESH,
    TX_BIAS_THRESH,

    /* SFP-specific Fields */
    /* 0xA0 Address Fields */
    EXT_IDENTIFIER,        /* Extended type of transceiver */
    CONNECTOR_TYPE,        /* Code for Connector Type */
    TRANSCEIVER_CODE,      /* Code for Electronic or optical capability */
    ENCODING_CODE,         /* High speed Serial encoding algo code */
    SIGNALLING_RATE,       /* nominal signalling rate */
    RATE_IDENTIFIER,       /* type of rate select functionality */
    TRANCEIVER_CAPABILITY, /* Code for Electronic or optical capability */
    WAVELENGTH,            /* laser wavelength */
    CHECK_CODE_BASEID,     /* Check code for the above fields */
    /* Extended options */
    ENABLED_OPTIONS, /* Indicates the optional transceiver signals enabled */
    UPPER_BIT_RATE_MARGIN,   /* upper bit rate margin */
    LOWER_BIT_RATE_MARGIN,   /* lower but rate margin */
    ENHANCED_OPTIONS,        /* Enhanced options implemented */
    SFF_COMPLIANCE,          /* revision number of SFF compliance */
    CHECK_CODE_EXTENDED_OPT, /* check code for the extended options */
    VENDOR_EEPROM,

    /* 0xA2 address Fields */
    /* Diagnostics */
    ALARM_THRESHOLD_VALUES,  /* diagnostic flag alarm and warning thresh values */
    EXTERNAL_CALIBRATION,    /* diagnostic calibration constants */
    CHECK_CODE_DMI,          /* Check code for base Diagnostic Fields */
    DIAGNOSTICS,             /* Diagnostic Monitor Data */
    STATUS_CONTROL,          /* Optional Status and Control bits */
    ALARM_WARN_FLAGS,        /* Diagnostic alarm and warning flag */
    EXTENDED_STATUS_CONTROL, /* Extended status and control bytes */
    /* General Purpose */
    VENDOR_MEM_ADDRESS, /* Vendor Specific memory address */
    USER_EEPROM,        /* User Writable NVM */
    VENDOR_CONTROL,     /* Vendor Specific Control */

    SFF_FIELD_MAX /* keep this the last */
} Sff_field;

/* any updates to this list also must update qsfp_flag_common in
 * bf_qsfp_comm.c */
typedef enum {
    /* Shared CMIS, QSFP and SFP fault/warning flags: */
    FLAG_TX_LOS,
    FLAG_RX_LOS,
    FLAG_TX_ADAPT_EQ_FAULT,
    FLAG_TX_FAULT,
    FLAG_TX_LOL,
    FLAG_RX_LOL,
    FLAG_TEMP_HIGH_ALARM,
    FLAG_TEMP_LOW_ALARM,
    FLAG_TEMP_HIGH_WARN,
    FLAG_TEMP_LOW_WARN,
    FLAG_VCC_HIGH_ALARM,
    FLAG_VCC_LOW_ALARM,
    FLAG_VCC_HIGH_WARN,
    FLAG_VCC_LOW_WARN,
    FLAG_VENDOR_SPECIFIC,
    FLAG_RX_PWR_HIGH_ALARM,
    FLAG_RX_PWR_LOW_ALARM,
    FLAG_RX_PWR_HIGH_WARN,
    FLAG_RX_PWR_LOW_WARN,
    FLAG_TX_BIAS_HIGH_ALARM,
    FLAG_TX_BIAS_LOW_ALARM,
    FLAG_TX_BIAS_HIGH_WARN,
    FLAG_TX_BIAS_LOW_WARN,
    FLAG_TX_PWR_HIGH_ALARM,
    FLAG_TX_PWR_LOW_ALARM,
    FLAG_TX_PWR_HIGH_WARN,
    FLAG_TX_PWR_LOW_WARN,

    /* CMIS-only flags */
    FLAG_DATAPATH_FW_FAULT,
    FLAG_MODULE_FW_FAULT,
    FLAG_MODULE_STATE_CHANGE,
    FLAG_DATAPATH_STATE_CHANGE,
    FLAG_AUX1_HIGH_ALARM,
    FLAG_AUX1_LOW_ALARM,
    FLAG_AUX1_HIGH_WARN,
    FLAG_AUX1_LOW_WARN,
    FLAG_AUX2_HIGH_ALARM,
    FLAG_AUX2_LOW_ALARM,
    FLAG_AUX2_HIGH_WARN,
    FLAG_AUX2_LOW_WARN,
    FLAG_VEND_HIGH_ALARM,
    FLAG_VEND_LOW_ALARM,
    FLAG_VEND_HIGH_WARN,
    FLAG_VEND_LOW_WARN,
    FLAG_AUX3_HIGH_ALARM,
    FLAG_AUX3_LOW_ALARM,
    FLAG_AUX3_HIGH_WARN,
    FLAG_AUX3_LOW_WARN,
    SFF_FLAG_MAX, /* keep this last on list */
} Sff_flag;

typedef enum { QSFP_PAGE0_LOWER, QSFP_PAGE0_UPPER, QSFP_PAGE3,
               /* for SFP */
               SFP_BASE_REG_LOWER,
               SFP_BASE_REG_UPPER,
               SFP_DIAG_REG_LOWER,
               SFP_DIAG_REG_PAGE0,
               SFP_DIAG_REG_PAGE1,
               SFP_DIAG_REG_PAGE2,
             } Qsfp_page;

typedef struct {
    Qsfp_page page;
    int offset;
    int length;
} sff_field_info_t;

typedef struct {
    Sff_field field;
    sff_field_info_t info;
} sff_field_map_t;

typedef struct {
    Sff_flag flag;
    Sff_field field;
    int firstbyte;     // byte location of lane 1
    int firstbit;      // bit location of lane 1
    int numbits;       // number of bits, per lane if applicable
    bool perlaneflag;  // true if this flag is per lane
    int laneincr;      // bit count between lanes
    bool inbyteinv;    // true if lane order in a byte is opposite of bit order
} sff_flag_map_t;

typedef struct {
    Sff_field field;
    uint32_t value;
} sff_field_mult_t;

enum PowerControl {
    POWER_OVERRIDE = 1 << 0,
    POWER_SET = 1 << 1,
    HIGH_POWER_OVERRIDE = 1 << 2,
};

enum ExternalIdentifer {
    EXT_ID_SHIFT = 6,
    EXT_ID_MASK = 0xc0,
    EXT_ID_HI_POWER_MASK = 0x03,
};

/* following complianbce codes are derived from SFF-8436 document */
typedef enum {
    COMPLIANCE_NONE = 0,
    ACTIVE_CABLE = 1 << 0,
    LR4_40GBASE = 1 << 1,
    SR4_40GBASE = 1 << 2,
    CR4_40GBASE = 1 << 3,
    SR_10GBASE = 1 << 4,
    LR_10GBASE = 1 << 5,
    LRM_10GBASE = 1 << 6,
    COMPLIANCE_RSVD = 1 << 7,
} Ethernet_compliance;

/* following complianbce codes are derived from SFF-8024 document */
typedef enum {
    EXT_COMPLIANCE_NONE = 0,
    AOC_100G_BER_5 = 0x01, /* 100G AOC or 25G AUI C2M AOC 5 * 10^^-5 BER */
    SR4_100GBASE = 0x02,   /* or SR-25GBASE */
    LR4_100GBASE = 0x03,   /* or LR-25GBASE */
    ER4_100GBASE = 0x04,   /* or ER-25GBASE */
    SR10_100GBASE = 0x05,
    CWDM4_100G = 0x06,
    PSM4_100G_SMF = 0x07,
    ACC_100G_BER_5 = 0x08, /* 100G ACC or 25G AUI C2M ACC 5 * 10^^-5 BER */
    EXT_COMPLIANCE_OBSOLETE = 0x09,
    EXT_COMPLIANCE_RSVD1 = 0x0A,
    CR4_100GBASE = 0x0B, /* or CR-25GBASE CA-L */
    CR_25GBASE_CA_S = 0x0C,
    CR_25GBASE_CA_N = 0x0D,
    EXT_COMPLIANCE_RSVD2 = 0x0E,
    EXT_COMPLIANCE_RSVD3 = 0x0F,
    ER4_40GBASE = 0x10,
    SR_10GBASE_4 = 0x11,
    PSM4_40G_SMF = 0x12,
    G959_P1I1_2D1 = 0x13, /* 10709 Mbd, 2 km, 1310nm SM */
    G959_P1S1_2D2 = 0x14, /* 10709 Mbd, 40 km, 1550nm SM */
    G959_P1L1_2D2 = 0x15, /* 10709 Mbd, 80 km, 1550nm SM */
    T_10BASE_SFI = 0x16,  /* 10BASE-T with SFI electrical interface */
    CLR4_100G = 0x17,
    AOC_100G_BER_12 = 0x18, /* 100G AOC or 25G AUI C2M AOC 10^^-12 BER */
    ACC_100G_BER_12 = 0x19, /* 100G ACC or 25G AUI C2M ACC 10^^-12 BER */
    DWDM2_100GE = 0x1A,     /* DMWM module using 1550nm, 80 km */

    /* Rev 4.9, May 24, 2021.
     * by tsihang, 2021-07-29. */
    WDM4_100GE = 0x1B,
    SR_10GBASE_T = 0x1C,
    T_5GBASE = 0x1D,
    T_2P5GBASE = 0x1E,
    SWDM4_40G = 0x1F,
    SWDM4_100G = 0x20,
    PAM4_BiDi_100G = 0x21,
} Ethernet_extended_compliance;

// Commonly used QSFP variants
/* Defined in SFF-8024_R4.9.pdf Chapter 4.2.
 * See https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8024&items_per_page=20
 * by tsihang, 2021-07-15. */
typedef enum {
    /* identifier byte 0x00 (SFF-8636) */
    UNKNOWN = 0,
    SFP = 0x03,
    SFP_PLUS = SFP,
    SFP_28 = SFP,
    QSFP = 0x0C,
    QSFP_PLUS = 0x0D,
    QSFP_28 = 0x11,
    QSFP_DD = 0x18,
    OSFP = 0x19,
    DSFP = 0x1B,
    QSFP_CMIS = 0x1E,
} Module_identifier;

/* Defined in SFF-8024_R4.9.pdf Chapter 4.4.
 * See https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8024&items_per_page=20
 * by tsihang, 2021-07-15. */
typedef enum {
    QSFP_CONN_TYPE_LC = 0x07,
    QSFP_CONN_TYPE_MTP = 0x0C,
    QSFP_CONN_TYPE_NO_SEPARABLE_CAB = 0x23,
} Connector_type;

// Media encoding type : Byte 85
typedef enum {
    MEDIA_TYPE_UNDEF = 0X00,
    MEDIA_TYPE_MMF = 0x01,
    MEDIA_TYPE_SMF = 0x02,
    MEDIA_TYPE_PASSIVE_CU = 0x03,
    MEDIA_TYPE_ACTIVE_CBL = 0x04,
} Media_type_enc_for_CMIS;

typedef enum {
    QSFPDD_400GBASE_AOC = 0x03,
    QSFPDD_400GBASE_SR8 = 0x10,
    QSFPDD_400GBASE_SR4 = 0x11,
    QSFPDD_400GBASE_FR8 = 0x1A,
    QSFPDD_400GBASE_LR8 = 0x1B,
    QSFPDD_400GBASE_DR4 = 0x1C,
    QSFPDD_400GBASE_FR4 = 0x1D,
    QSFPDD_400GBASE_LR4 = 0x1E,

    QSFPDD_200GBASE_DR4 = 0x17,
    QSFPDD_200GBASE_FR4 = 0x18,
    QSFPDD_200GBASE_LR4 = 0x19,
} Module_media_interface_code;

// memory map formats (IDENTIFIER_OFFSET field is used to determine this)
typedef enum {
    MMFORMAT_UNKNOWN,
    MMFORMAT_SFF8636,
    MMFORMAT_CMIS3P0,
    MMFORMAT_CMIS4P0,
    MMFORMAT_SFF8472,

    /*
     * SFF-8636        : https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8636&items_per_page=20
     * QSFP-DD CMIS3.0 : http://www.qsfp-dd.com/wp-content/uploads/2018/09/QSFP-DD-mgmt-rev3p0-final-9-18-18-Default-clean.pdf
     * QSFP-DD HDW3.0  : http://www.qsfp-dd.com/wp-content/uploads/2017/09/QSFP-DD-Hardware-rev3p0.pdf
     * QSFP-DD CMIS4.0 : http://www.qsfp-dd.com/wp-content/uploads/2019/05/QSFP-DD-CMIS-rev4p0.pdf
     *
     * by tsihang, 2021-07-14.
     */
} MemMap_Format;

typedef enum {
    MODULE_ST_LOWPWR = 1,
    MODULE_ST_PWRUP,
    MODULE_ST_READY,
    MODULE_ST_PWRDN,
    MODULE_ST_FAULT,
} Module_State;

typedef enum {
    DATAPATH_ST_DEACTIVATED = 1,
    DATAPATH_ST_INIT,
    DATAPATH_ST_DEINIT,
    DATAPATH_ST_ACTIVATED,
    DATAPATH_ST_TXTURNON,
    DATAPATH_ST_TXTURNOFF,
    DATAPATH_ST_INITIALIZED,
} DataPath_State;

typedef enum {
    STAGED_SET0 = 0,
    STAGED_SET1,
    ACTIVE_SET = 0XFF,
} Control_Sets;

/* by tsihang, 2021-07-12 */
typedef struct {
    bool low;
    bool high;
} sff_flag_t;

typedef struct {
    sff_flag_t warn;
    sff_flag_t alarm;
} sff_flags_level_t;

typedef struct {
    double value;
    sff_flags_level_t flags;
    struct {
        bool value;
        bool flags;
    } _isset;
} sff_sensor_t;

typedef struct {
    uint8_t ext_code;
    char ext_code_desc[250];
} sff_ext_code_map_t;

/* by tsihang, 2021-07-12 end */


#ifdef __cplusplus
}
#endif /* C++ */

#endif /* __SFF_H */
