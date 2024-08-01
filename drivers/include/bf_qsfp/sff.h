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
typedef enum {
    /* Shared CMIS, QSFP and SFP fields: */
    IDENTIFIER, /* Type of Transceiver */
    SPEC_REV,
    STATUS, /* Support flags for upper pages */
    SFF8636_LANE_STATUS_FLAGS,
    SFF8636_LANE_MONITOR_FLAGS,
    MODULE_FLAGS,
    TEMPERATURE_ALARMS,
    VCC_ALARMS, /* Voltage */
    CHANNEL_RX_PWR_ALARMS,
    CHANNEL_TX_BIAS_ALARMS,
    CHANNEL_TX_PWR_ALARMS,
    TEMPERATURE,
    VCC, /* Voltage */
    CHANNEL_RX_PWR,
    CHANNEL_TX_BIAS,
    CHANNEL_TX_PWR,
    CHANNEL_TX_DISABLE, /* Field 16 */
    POWER_CONTROL,
    CDR_CONTROL,        /* sff-8636 only, Field 18 */
    ETHERNET_COMPLIANCE,
    PWR_CLASS,
    PWR_REQUIREMENTS,
    PAGE_SELECT_BYTE,
    CONN_TYPE,
    LENGTH_SM_KM, /* Single mode, in km */
    LENGTH_SM,    /* Single mode in 100m (not in QSFP) */
    LENGTH_OM5,
    LENGTH_OM4,
    LENGTH_OM3,
    LENGTH_OM2,
    LENGTH_OM1,
    LENGTH_CBLASSY,
    VENDOR_NAME,     /* QSFP Vendor Name (ASCII) */
    VENDOR_OUI,      /* QSFP Vendor IEEE company ID */
    PART_NUMBER,     /* Part NUmber provided by QSFP vendor (ASCII) */
    REVISION_NUMBER, /* Revision number */
    ETHERNET_EXTENDED_COMPLIANCE,  /* ethernet extended compliance code */
    ETHERNET_SECONDARY_COMPLIANCE, /* ethernet secondary ext compliance code */
    VENDOR_SERIAL_NUMBER,          /* Vendor Serial Number (ASCII) */
    MFG_DATE,                      /* Manufacturing date code */
    TEMPERATURE_THRESH,
    VCC_THRESH,
    RX_PWR_THRESH,
    TX_BIAS_THRESH,
    TX_PWR_THRESH,
    OPTIONS,

    /* CMIS-specific fields */
    /* If these change, also update ApplicationAdversiting below */
    MODULE_STATE,
    FIRMWARE_VER_ACTIVE,
    MODULE_FAULT_CAUSE,
    MODULE_MEDIA_TYPE,
    APSEL1_ALL,
    APSEL1_HOST_ID,
    APSEL1_MEDIA_ID,
    APSEL1_LANE_COUNTS,
    APSEL1_HOST_LANE_OPTION_MASK,
    FIRMWARE_VER_INACTIVE,
    HARDWARE_VER,
    APSEL1_MEDIA_LANE_OPTION_MASK,  // this is one contiguous list
    APSEL9_ALL,
    APSEL9_HOST_ID,
    APSEL9_MEDIA_ID,
    APSEL9_LANE_COUNTS,
    APSEL9_HOST_LANE_OPTION_MASK,
    DATAPATH_DEINIT,        /* Field 62, Pg16 128. */
    APPLY_DATAPATHINIT_SS0, /*         , Pg16 143. */
    DATAPATH_CFG_ALL_STAGED_SET0,
    DATAPATH_CFG_ALL_STAGED_SET1,
    DATAPATH_CFG_ALL_ACTIVE_SET,
    APPLICATION_SELECT_LN1,
    DATAPATH_STATE_ALL,
    DATAPATH_STATE_LN1AND2,
    CMIS_LANE_FLAGS,
    MEDIA_INT_TECH,
    DATAPATH_CFG_STATUS,
    FLAG_SUPPORT,
    LN1_ACTIVE_SET,

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

typedef enum {
    QSFP_BANKNA = -2,
    QSFP_BANKCH = -1,  // the bank number is based on the channel
    QSFP_BANK0 = 0,
    QSFP_BANK1,
    QSFP_BANK2,
    QSFP_BANK3
} Qsfp_bank;

typedef enum {
    QSFP_PAGE0_LOWER = -1,
    QSFP_PAGE0_UPPER,
    QSFP_PAGE1,
    QSFP_PAGE2,
    QSFP_PAGE3,
    QSFP_PAGE16 = 16,
    QSFP_PAGE17 = 17,
    QSFP_PAGE18 = 18,
    QSFP_PAGE19 = 19,
    QSFP_PAGE47 = 47,

    /* for SFP */
    SFP_A0 = -1,
    SFP_A2,
    SFP_PAGE0_LOWER,
    SFP_PAGE0_UPPER,
    SFP_PAGE2,
} Qsfp_page;

// this strucutre describes the byte location of memory map fields
typedef struct {
    Qsfp_bank bank;
    Qsfp_page page;
    int offset;
    int length;
    bool in_cache;
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
    POWER_OVERRIDE = 1 << 0,       // SFF-8636
    POWER_SET = 1 << 1,            // SFF-8636
    HIGH_POWER_OVERRIDE = 1 << 2,  // SFF-8636
    FORCELOWPWR = 1 << 4,          // CMIS, all versions
    CMIS4_LOWPWR = 1 << 6,         // CMIS 4.0 and later
};

enum ExternalIdentifer {
    EXT_ID_SHIFT = 6,
    EXT_ID_MASK = 0xc0,
    EXT_ID_HI_POWER_MASK = 0x03,
};

/* following compliance codes are derived from SFF-8436 document */
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
    DR_100GBASE = 0x25,     /* 100GBASE-DR (Clause 140), CAUI-4 (no FEC). by Hang Tsi, 2024/03/18. */
    FR_100GBASE = 0x26,     /* 100G-FR or 100GBASE-FR1 (Clause 140), CAUI-4 (no FEC), by Hang Tsi, 2024/03/18. */
    LR_100GBASE = 0x27,     /* 100G-LR or 100GBASE-LR1 (Clause 140), CAUI-4 (no FEC), by Hang Tsi, 2024/03/18. */
    ACC_200G_BER_6 = 0x30,
    AOC_200G_BER_6 = 0x31,
    ACC_200G_BER_4 = 0x32,
    AOC_200G_BER_4 = 0x33,
} Ethernet_extended_compliance;

// Commonly used QSFP variants
/* Defined in SFF-8024_R4.9.pdf Chapter 4.2.
 * See https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8024&items_per_page=20
 * by tsihang, 2021-07-15. */
typedef enum {
    /* identifier byte 0x00 (SFF-8636) */
    UNKNOWN = 0,
    SFP = 0x03,         /* SFF8472_IDENT_SFP */
    SFP_PLUS = SFP,
    SFP_28 = SFP,
    QSFP_DW_DM = 0x0B,  /* SFF8472_IDENT_DWDM_SFP */
    QSFP = 0x0C,        /* SFF8436_IDENT_QSFP */
    QSFP_PLUS = 0x0D,   /* SFF8436_IDENT_QSFP_PLUS */
    QSFP_28 = 0x11,     /* SFF8636_IDENT_QSFP28 */
    QSFP_DD = 0x18,
    OSFP = 0x19,
    QSFP_CMIS = 0x1E,
} Module_identifier;

/* Defined in SFF-8024_R4.9.pdf Chapter 4.4.
 * See https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8024&items_per_page=20
 * by tsihang, 2021-07-15. */
typedef enum {
    QSFP_CONN_TYPE_SC = 0x01,       /* SFF8472_CONN_SC */
    QSFP_CONN_TYPE_LC = 0x07,       /* SFF8472_CONN_LC */
    QSFP_CONN_TYPE_MPO_1x12 = 0x0C, /* SFF8472_CONN_MPO_1X12 */
    QSFP_CONN_TYPE_MPO_2x16 = 0x0D, /* SFF8472_CONN_MPO_2X16 */
    QSFP_CONN_TYPE_MPO_1x16 = 0x28, /* SFF8472_CONN_MPO_1X16 */
    QSFP_CONN_TYPE_NO_SEPARABLE_CAB = 0x23,
} Connector_type;

// Media encoding type : Byte 85
typedef enum {
    MEDIA_TYPE_UNDEF = 0x00,
    MEDIA_TYPE_MMF = 0x01,
    MEDIA_TYPE_SMF = 0x02,
    MEDIA_TYPE_PASSIVE_CU = 0x03,
    MEDIA_TYPE_ACTIVE_CBL = 0x04,
    MEDIA_TYPE_BASE_T = 0x05,   // by Hang Tsi, 2024/02/26.
} Media_type_enc_for_CMIS;

// Host Electrical Interface ID referring to SFF8024 Table 4.5 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 22
typedef enum {
    HOST_TYPE_UNDEF = 0x00,
    HOST_TYPE_1000BASE_CX = 0x1,
    HOST_TYPE_XAUI  = 0x2,
    HOST_TYPE_XFI = 0x3,
    HOST_TYPE_SFI = 0x4,
    HOST_TYPE_25GAUI_C2M = 0x5,
    HOST_TYPE_XLAUI_C2M = 0x6,
    HOST_TYPE_XLPPI_C2M = 0x7,
    HOST_TYPE_LAUI_2_C2M = 0x8,
    HOST_TYPE_50GAUI_2_C2M = 0x9,
    HOST_TYPE_50GAUI_1_C2M = 0xA,
    HOST_TYPE_CAUI_4_C2M = 0xB,
    HOST_TYPE_100GAUI_4_C2M = 0xC,
    HOST_TYPE_100GAUI_2_C2M = 0xD,
    HOST_TYPE_200GAUI_8_C2M = 0xE,
    HOST_TYPE_200GAUI_4_C2M = 0xF,
    HOST_TYPE_400GAUI_16_C2M = 0x10,
    HOST_TYPE_400GAUI_8_C2M = 0x11,
    HOST_TYPE_RESERVED_0x12 = 0x12,
    HOST_TYPE_10GBASE_CX4 = 0x13,
    HOST_TYPE_25GBASE_CR_CA_L = 0x14,
    HOST_TYPE_25GBASE_CR_CA_S = 0x15,
    HOST_TYPE_25GBASE_CR_CA_N = 0x16,
    HOST_TYPE_40GBASE_CR4 = 0x17,
    HOST_TYPE_50GBASE_CR = 0x18,
    HOST_TYPE_100GBASE_CR10 = 0x19,
    HOST_TYPE_100GBASE_CR4 = 0x1A,
    HOST_TYPE_100GBASE_CR2 = 0x1B,
    HOST_TYPE_200GBASE_CR4 = 0x1C,
    HOST_TYPE_400GBASE_CR8 = 0x1D,
    HOST_TYPE_RESERVED_0x1E_0x24 = 0x1E,
    HOST_TYPE_RESERVED_0x24_0x1E = 0x24,
    /* 0x25 - 0x40 for other kind of ethernet */
    HOST_TYPE_IB_EDR = 0x30,
    HOST_TYPE_IB_HDR = 0x31,
    HOST_TYPE_CAUI_4_C2M_NO_FEC = 0x41,
    HOST_TYPE_CAUI_4_C2M_RS_FEC = 0x42,
    HOST_TYPE_50GBASE_CR2_RS_FEC = 0x43,
    HOST_TYPE_50GBASE_CR2_FC_FEC = 0x44,
    HOST_TYPE_50GBASE_CR2_NO_FEC = 0x45,
    HOST_TYPE_100GBASE_CR1 = 0x46,
    HOST_TYPE_200GBASE_CR2 = 0x47,
    HOST_TYPE_400GBASE_CR4 = 0x48,
    HOST_TYPE_800GBASE_CR8 = 0x49,
    HOST_TYPE_100GAUI_1_S_C2M = 0x4B,
    HOST_TYPE_100GAUI_1_L_C2M = 0x4C,
    HOST_TYPE_200GAUI_2_S_C2M = 0x4D,
    HOST_TYPE_200GAUI_2_L_C2M = 0x4E,
    HOST_TYPE_400GAUI_4_S_C2M = 0x4F,
    HOST_TYPE_400GAUI_4_L_C2M = 0x50,
} Host_type_interface_code;

// MMF Interface ID referring to SFF8024 Table 4.6 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 25
typedef enum {
    MEDIA_TYPE_MMF_UNDEF = 0x00,
    MEDIA_TYPE_MMF_10GBASE_SW = 0x01,
    MEDIA_TYPE_MMF_10GBASE_SR = 0x02,
    MEDIA_TYPE_MMF_25GBASE_SR = 0x03,
    MEDIA_TYPE_MMF_40GBASE_SR4 = 0x04,
    MEDIA_TYPE_MMF_40GE_SWDM4 = 0x05,
    MEDIA_TYPE_MMF_40GE_BiDi = 0x06,
    MEDIA_TYPE_MMF_50GBASE_SR = 0x07,
    MEDIA_TYPE_MMF_100GBASE_SR10 = 0x08,
    MEDIA_TYPE_MMF_100GBASE_SR4 = 0x09,
    MEDIA_TYPE_MMF_100GE_SWDM4 = 0x0A,
    MEDIA_TYPE_MMF_100GE_BiDi = 0x0B,
    MEDIA_TYPE_MMF_100GBASE_SR2 = 0x0C,
    MEDIA_TYPE_MMF_100GBASE_SR = 0x0D,
    MEDIA_TYPE_MMF_200GBASE_SR4 = 0x0E,
    MEDIA_TYPE_MMF_400GBASE_SR16 = 0x0F,
    MEDIA_TYPE_MMF_400GBASE_SR8 = 0x10,
    MEDIA_TYPE_MMF_400GBASE_SR4 = 0x11,
    MEDIA_TYPE_MMF_800GBASE_SR8 = 0x12,
    MEDIA_TYPE_MMF_400GBASE_SR4_2 = 0x1A,
    MEDIA_TYPE_MMF_400GE_BiDi = MEDIA_TYPE_MMF_400GBASE_SR4_2,
    MEDIA_TYPE_MMF_200GBASE_SR2 = 0x1B,
    MEDIA_TYPE_MMF_100GBASE_VR = 0x1D,
    MEDIA_TYPE_MMF_200GBASE_VR2 = 0x1E,
    MEDIA_TYPE_MMF_400GBASE_VR4 = 0x1F,

    QSFPDD_200GBASE_SR4 = MEDIA_TYPE_MMF_200GBASE_SR4,
    QSFPDD_400GBASE_SR8 = MEDIA_TYPE_MMF_400GBASE_SR8,
    QSFPDD_400GBASE_SR4 = MEDIA_TYPE_MMF_400GBASE_SR4,
} Module_MMF_media_interface_code;

// SMF Interface ID referring to SFF8024 Table 4.7 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 26
typedef enum {
    MEDIA_TYPE_SMF_UNDEF = 0x00,
    MEDIA_TYPE_SMF_10GBASE_LW = 0x01,
    MEDIA_TYPE_SMF_10GBASE_EW = 0x02,
    MEDIA_TYPE_SMF_10G_ZW = 0x03,
    MEDIA_TYPE_SMF_10GBASE_LR = 0x04,
    MEDIA_TYPE_SMF_10GBASE_ER = 0x05,
    MEDIA_TYPE_SMF_10G_ZR = 0x06,
    MEDIA_TYPE_SMF_25GBASE_LR = 0x07,
    MEDIA_TYPE_SMF_25GBASE_ER = 0x08,
    MEDIA_TYPE_SMF_40GBASE_LR4 = 0x09,
    MEDIA_TYPE_SMF_40GBASE_FR = 0x0A,
    MEDIA_TYPE_SMF_50GBASE_FR = 0x0B,
    MEDIA_TYPE_SMF_50GBASE_LR = 0x0C,
    MEDIA_TYPE_SMF_100GBASE_LR4 = 0x0D,
    MEDIA_TYPE_SMF_100GBASE_ER4 = 0x0E,
    MEDIA_TYPE_SMF_100G_PSM4 = 0x0F,
    MEDIA_TYPE_SMF_100G_CWDM4 = 0x10,
    MEDIA_TYPE_SMF_100G_4WDM10 = 0x11,
    MEDIA_TYPE_SMF_100G_4WDM20 = 0x12,
    MEDIA_TYPE_SMF_100G_4WDM40 = 0x13,
    MEDIA_TYPE_SMF_100GBASE_DR = 0x14,
    MEDIA_TYPE_SMF_100G_FR = 0x15,
    MEDIA_TYPE_SMF_100GBASE_FR1 = MEDIA_TYPE_SMF_100G_FR,
    MEDIA_TYPE_SMF_100G_LR = 0x16,
    MEDIA_TYPE_SMF_100GBASE_LR1 = MEDIA_TYPE_SMF_100G_LR,
    MEDIA_TYPE_SMF_200GBASE_DR4 = 0x17,
    MEDIA_TYPE_SMF_200GBASE_FR4 = 0x18,
    MEDIA_TYPE_SMF_200GBASE_LR4 = 0x19,
    MEDIA_TYPE_SMF_400GBASE_FR8 = 0x1A,
    MEDIA_TYPE_SMF_400GBASE_LR8 = 0x1B,
    MEDIA_TYPE_SMF_400GBASE_DR4 = 0x1C,
    MEDIA_TYPE_SMF_400GBASE_FR4 = 0x1D,
    MEDIA_TYPE_SMF_400G_FR4 = MEDIA_TYPE_SMF_400GBASE_FR4,
    MEDIA_TYPE_SMF_400G_LR4_10 = 0x1E,

    MEDIA_TYPE_SMF_100G_CWDM4_OCP = 0x34,
    MEDIA_TYPE_SMF_50GBASE_ER = 0x40,
    MEDIA_TYPE_SMF_200GBASE_ER4 = 0x41,
    MEDIA_TYPE_SMF_400GBASE_ER8 = 0x42,
    MEDIA_TYPE_SMF_400GBASE_LR4_6 = 0x43,
    MEDIA_TYPE_SMF_100GBASE_ZR = 0x44,

    MEDIA_TYPE_SMF_100G_LR1_20 = 0x4A,
    MEDIA_TYPE_SMF_100G_ER1_30 = 0x4B,
    MEDIA_TYPE_SMF_100G_ER1_40 = 0x4C,

    MEDIA_TYPE_SMF_400GBASE_ZR = 0x4D,

    MEDIA_TYPE_SMF_10GBASE_BR = 0x4E,
    MEDIA_TYPE_SMF_25GBASE_BR = 0x4F,
    MEDIA_TYPE_SMF_50GBASE_BR = 0x50,

    QSFPDD_100GBASE_DR = MEDIA_TYPE_SMF_100GBASE_DR,
    QSFPDD_200GBASE_DR4 = MEDIA_TYPE_SMF_200GBASE_DR4,
    QSFPDD_200GBASE_FR4 = MEDIA_TYPE_SMF_200GBASE_FR4,
    QSFPDD_200GBASE_LR4 = MEDIA_TYPE_SMF_200GBASE_LR4,
    QSFPDD_400GBASE_FR8 = MEDIA_TYPE_SMF_400GBASE_FR8,
    QSFPDD_400GBASE_LR8 = MEDIA_TYPE_SMF_400GBASE_LR8,
    QSFPDD_400GBASE_DR4 = MEDIA_TYPE_SMF_400GBASE_DR4,
    QSFPDD_400GBASE_FR4 = MEDIA_TYPE_SMF_400GBASE_FR4,
    QSFPDD_400GBASE_LR4 = MEDIA_TYPE_SMF_400G_LR4_10,
} Module_SMF_media_interface_code;

// Passive Copper/Loopback Interface ID referring to SFF8024 Table 4.8 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 29
typedef enum {
    MEDIA_PASSIVE_LOOPBACK = 0xBF,
} Media_Passive_cbl_interface_code;

// Active Copper/Loopback Interface ID referring to SFF8024 Table 4.9 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 29
typedef enum {
    MEDIA_ACTIVE_CBL_BER_12 = 0x1,
    MEDIA_ACTIVE_CBL_BER_5 = 0x2,
    MEDIA_ACTIVE_CBL_BER_4 = 0x3,
    MEDIA_ACTIVE_CBL_BER_6 = 0x4,
    MEDIA_ACTIVE_LOOPBACK = 0xBF,
} Media_Active_cbl_interface_code;

// BASE-T media Interface ID referring to SFF8024 Table 4.10 in Chapter 4.6
// SFF-8024 Rev 4.9 Page 29
typedef enum {
    MEDIA_BASET_1000BASET = 0x2,
    MEDIA_BASET_2P5GBASET = 0x3,
    MEDIA_BASET_10GBASET = 0x4,
    MEDIA_BASET_25GBASET = 0x5,
    MEDIA_BASET_40GBASET = 0x6,
    MEDIA_BASET_50GBASET = 0x7,
} Media_BaseT_interface_code;

typedef enum {
    // Innolight Specific Media ID
    QSFPDD_200GBASE_DR2 = 0xFB,
    QSFPDD_800GBASE_DR8 = 0xFC,
} Custom_Module_SMF_media_interface_code;

/* by Hang Tsi, 2024/01/23. */
typedef enum {
    QSFPDD_400G_SR8 = 0x07, /* Asterfusion QFDD-8G400-01 50GBASE-SR (Clause 138) */
} Custom_Module_MMF_media_interface_code;

typedef enum {
    QSFPDD_400GBASE_AOC = 0x03,
} Module_media_interface_code;

// memory map formats (IDENTIFIER_OFFSET field is used to determine this)
typedef enum {
    MMFORMAT_UNKNOWN,
    MMFORMAT_SFF8472,
    MMFORMAT_SFF8636,
    MMFORMAT_CMIS3P0,
    MMFORMAT_CMIS4P0,
    MMFORMAT_CMIS5P0,
    MMFORMAT_CMIS5P1,
    /*
     * SFF-8636        : https://www.snia.org/technology-communities/sff/specifications?field_doc_status_value=All&combine=8636&items_per_page=20
     * QSFP-DD CMIS3.0 : http://www.qsfp-dd.com/wp-content/uploads/2018/09/QSFP-DD-mgmt-rev3p0-final-9-18-18-Default-clean.pdf
     * QSFP-DD HDW3.0  : http://www.qsfp-dd.com/wp-content/uploads/2017/09/QSFP-DD-Hardware-rev3p0.pdf
     * QSFP-DD CMIS4.0 : http://www.qsfp-dd.com/wp-content/uploads/2019/05/QSFP-DD-CMIS-rev4p0.pdf
     * QSFP-DD CMIS5.0 : http://www.qsfp-dd.com/wp-content/uploads/2021/05/CMIS5p0.pdf
     * QSFP-DD CMIS5.1 : http://www.qsfp-dd.com/wp-content/uploads/2021/11/CMIS5p1.pdf
     *
     * by tsihang, 2021-07-14.
     */
} MemMap_Format;

typedef enum {
    MODULE_FAULT_NO_FAULT = 0, /* Or not supported */
    MODULE_FAULT_TEC_RUNAWAY = 1,
    MODULE_FAULT_DATA_MEM_CORRUPTED = 2,
    MODULE_FAULT_PROG_MEM_CORRUPTED = 3,
    /* Reserved 4-31 */
    /* Custom 32-63 */
    /* Reserved 64-255 */
} Module_Fault_Cause;

typedef enum {
    MODULE_ST_RESV0 = 0,
    MODULE_ST_LOWPWR = 1,
    MODULE_ST_PWRUP,
    MODULE_ST_READY,
    MODULE_ST_PWRDN,
    MODULE_ST_FAULT,
    MODULE_ST_RESV6,
    MODULE_ST_RESV7,
} Module_State;

typedef enum {
    DATAPATH_ST_RESERVED = 0,
    DATAPATH_ST_DEACTIVATED = 1,
    DATAPATH_ST_INIT,
    DATAPATH_ST_DEINIT,
    DATAPATH_ST_ACTIVATED,
    DATAPATH_ST_TXTURNON,
    DATAPATH_ST_TXTURNOFF,
    DATAPATH_ST_INITIALIZED,
} DataPath_State;

// Configuration Command Status definition as per CMIS 5.0
// Table 8-81
typedef enum {
    DATAPATH_CONF_UNDEF = 0x0,
    DATAPATH_CONF_SUCCESS = 0x1,
    DATAPATH_CONF_REJECT = 0x2,
    DATAPATH_CONF_REJECT_INVALID_APSEL = 0x3,
    DATAPATH_CONF_REJECT_INVALID_DP = 0x4,
    DATAPATH_CONF_REJECT_INVALID_SI = 0x5,
    DATAPATH_CONF_REJECT_LANE_INUSE = 0x6,
    DATAPATH_CONF_REJECT_PARTIAL_DP = 0x7,
    DATAPATH_CONF_REJECT_RESERVED_0x8 = 0x8,
    DATAPATH_CONF_REJECT_RESERVED_0x9 = 0x9,
    DATAPATH_CONF_REJECT_RESERVED_0xA = 0xA,
    DATAPATH_CONF_REJECT_RESERVED_0xB = 0xB,
    DATAPATH_CONF_INPROGRESS = 0xC,
    DATAPATH_CONF_REJECT_CUSTOM_0xD = 0xD,
    DATAPATH_CONF_REJECT_CUSTOM_0xE = 0xE,
    DATAPATH_CONF_REJECT_CUSTOM_0xF = 0xF,
} DataPath_Config_Status;

typedef enum {
    STAGED_SET0 = 0,
    STAGED_SET1,
    ACTIVE_SET = 0XFF,
} Control_Sets;

// Media Interface Technology : Byte 212
// Definition as per CMIS 5.1
typedef enum {
    MEDIA_TECH_CU_UNEQ = 0x0A,
    MEDIA_TECH_CU_PASSIVE_EQ = 0x0B,
    MEDIA_TECH_CU_NEAR_FAR_END_ACTIVE_EQ = 0x0C,
    MEDIA_TECH_CU_FAR_END_ACTIVE_EQ = 0x0D,
    MEDIA_TECH_CU_NEAR_END_ACTIVE_EQ = 0x0E,
    MEDIA_TECH_CU_LINEAR_ACTIVE_EQ = 0x0F,
} Media_Interface_Technology_CMIS;

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
