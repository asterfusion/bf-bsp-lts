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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>

#define BYTES_PER_APP 4

#define MAX_BANKS 4

#define SFF8636_MODULE_FLAG_BYTES 3
#define CMIS_MODULE_FLAG_BYTES 6
#define MODULE_FLAG_BYTES                               \
  ((SFF8636_MODULE_FLAG_BYTES > CMIS_MODULE_FLAG_BYTES) \
       ? SFF8636_MODULE_FLAG_BYTES                      \
       : CMIS_MODULE_FLAG_BYTES)
#define SFF8636_LANE_STATUS_FLAG_BYTES 3
#define SFF8636_LANE_MONITOR_FLAG_BYTES 13
#define CMIS_LANE_FLAG_BYTES 19
#define LANE_FLAG_BYTES                                                   \
  ((SFF8636_LANE_STATUS_FLAG_BYTES + SFF8636_LANE_MONITOR_FLAG_BYTES >    \
    MAX_BANKS * CMIS_LANE_FLAG_BYTES)                                     \
       ? SFF8636_LANE_STATUS_FLAG_BYTES + SFF8636_LANE_MONITOR_FLAG_BYTES \
       : MAX_BANKS * CMIS_LANE_FLAG_BYTES)
#define min(a, b) ((a) < (b) ? (a) : (b))

typedef double (*qsfp_conversion_fn) (
    uint16_t value);


typedef struct bf_qsfp_special_info_ {
    bf_pltfm_qsfp_type_t qsfp_type;
    bool is_set;

    /*
     * Member vendor|pn|ctrlmask are used by /etc/transceiver-cases.conf.
     * Added by tsihang, 2023-05-11.
     */
    char vendor[16];
    char pn[16];
    /* BF_TRANS_CTRLMASK_XXXX */
    uint32_t ctrlmask;
} bf_qsfp_special_info_t;

// one supported application in CMIS or SFF-8636. In the case of 8636, each
// field is entered as if that 8636 entry was advertised as a CMIS Application
typedef struct bf_pm_qsfp_apps_t {
    uint8_t host_if_id;   // host I/F ID from CMIS or 8636 equiv
    uint8_t media_if_id;  // media I/F ID from CMIS or 8636 equiv
    uint8_t host_lane_cnt;
    uint8_t media_lane_cnt;
    uint8_t host_lane_assign_mask;  // each set bit location corresponds to
    // supported starting lane for this application
    uint8_t media_lane_assign_mask;  // each set bit location corresponds to
    // supported starting lane for this application

#if !defined (APP_ALLOC)
    /* Belowing added by Hang Tsi, 2024/02/29 */
#define BF_QSFP_MAX_APPLICATIONS    32   /* The CMIS spec supports maximum 16 applications. */
#define BF_QSFP_APP_VALID           (1 << 0)
    uint8_t ul_flags;
#endif

#if defined (APP_ALLOC)
#define BF_APP_VALID(app)   (1)
#define BF_APP_INVALID(app) (0)
#define BF_APP_VALID_SET(app)
#define BF_APP_INVALID_SET(app)
#else
#define BF_APP_VALID(app)       ((((app).ul_flags) & BF_QSFP_APP_VALID) == 1)
#define BF_APP_INVALID(app)     ((((app).ul_flags) & BF_QSFP_APP_VALID) == 0)
#define BF_APP_VALID_SET(app)   (((app).ul_flags) |= BF_QSFP_APP_VALID)
#define BF_APP_INVALID_SET(app) (((app).ul_flags) &= ~BF_QSFP_APP_VALID)
#endif
} bf_pm_qsfp_apps_t;

typedef struct bf_qsfp_info_t {
    bool present;
    bool reset;
    bool soft_removed;
    bool flat_mem;
    bool passive_cu;
    bool fsm_detected; /* by tsihang, 2023-06-26. */
    uint8_t num_ch;
    MemMap_Format memmap_format;

    /* cached QSFP values */
    bool cache_dirty;
    bool suppress_repeated_rd_fail_msgs;
    uint8_t idprom[MAX_QSFP_PAGE_SIZE];
    uint8_t page0[MAX_QSFP_PAGE_SIZE];
    uint8_t page1[MAX_QSFP_PAGE_SIZE];
    uint8_t page2[MAX_QSFP_PAGE_SIZE];
    uint8_t page3[MAX_QSFP_PAGE_SIZE];
    uint8_t page17[MAX_QSFP_PAGE_SIZE];
    uint8_t page18[MAX_QSFP_PAGE_SIZE];
    uint8_t page19[MAX_QSFP_PAGE_SIZE];
    uint8_t page47[MAX_QSFP_PAGE_SIZE];
    uint8_t module_flags[MODULE_FLAG_BYTES];
    uint32_t module_flag_cnt[MODULE_FLAG_BYTES][8];
    uint8_t lane_flags[LANE_FLAG_BYTES];
    uint32_t lane_flag_cnt[LANE_FLAG_BYTES][8];

    uint8_t media_type;
#if defined (APP_ALLOC)
    bf_pm_qsfp_apps_t *app_list;
#else
    bf_pm_qsfp_apps_t app_list[BF_QSFP_MAX_APPLICATIONS];
#endif
    int num_apps;

    bf_qsfp_special_info_t special_case_port;
    bf_sys_mutex_t qsfp_mtx;
    bool internal_port;  // via ucli for special purposes
    int rxlos_debounce_cnt;
    double module_temper_cache;
    bool checksum;
} bf_qsfp_info_t;

/* access is 1 based index, so zeroth index would be unused */
static bf_qsfp_info_t
bf_qsfp_info_arr[BF_PLAT_MAX_QSFP + 1];
static int bf_plt_max_qsfp = 0;

static bf_qsfp_vec_t bf_qsfp_vec = {0};

typedef struct bf_qsfp_flag_format {
    Sff_field fieldName;
    uint8_t high_alarm_loc;
    uint8_t low_alarm_loc;
    uint8_t high_warn_loc;
    uint8_t low_warn_loc;
    bool is_ch_flag;
    bool ch_order_opposite_bit_order;
} bf_qsfp_flag_format_t;

/* SFF-8636, QSFP TRANSCEIVER spec */

static sff_field_map_t sff8636_field_map[] = {
    /* Base page values, including alarms and sensors */
    {IDENTIFIER, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 0, 1, true}},
    {SPEC_REV, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 1, 1, true}},
    {STATUS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 2, 1, false}},
    {
        SFF8636_LANE_STATUS_FLAGS,
        {QSFP_BANKNA, QSFP_PAGE0_LOWER, 3, SFF8636_LANE_STATUS_FLAG_BYTES, false}
    },
    {
        MODULE_FLAGS,
        {QSFP_BANKNA, QSFP_PAGE0_LOWER, 6, SFF8636_MODULE_FLAG_BYTES, false}
    },
    {
        SFF8636_LANE_MONITOR_FLAGS,
        {
            QSFP_BANKNA,
            QSFP_PAGE0_LOWER,
            9,
            SFF8636_LANE_MONITOR_FLAG_BYTES,
            false
        }
    },
    {TEMPERATURE_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 6, 1, false}},
    {VCC_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 7, 1, false}},
    {CHANNEL_RX_PWR_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 9, 2, false}},
    {CHANNEL_TX_BIAS_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 11, 2, false}},
    {CHANNEL_TX_PWR_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 13, 2, false}},
    {TEMPERATURE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 22, 2, false}},
    {VCC, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 26, 2, false}},
    {CHANNEL_RX_PWR, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 34, 8, false}},
    {CHANNEL_TX_BIAS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 42, 8, false}},
    {CHANNEL_TX_PWR, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 50, 8, false}},
    {CHANNEL_TX_DISABLE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 86, 1, false}},
    {POWER_CONTROL, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 93, 1, false}},
    {CDR_CONTROL, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 98, 1, false}},
    {
        ETHERNET_SECONDARY_COMPLIANCE,
        {QSFP_BANKNA, QSFP_PAGE0_LOWER, 116, 1, true}
    },
    {PAGE_SELECT_BYTE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 127, 1, false}},

    /* Page 0 values, including vendor info: */
    {PWR_REQUIREMENTS, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 129, 1, true}},
    {EXT_IDENTIFIER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 129, 1, true}},
    {CONN_TYPE, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 130, 1, true}},
    {ETHERNET_COMPLIANCE, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 131, 1, true}},
    {LENGTH_SM_KM, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 142, 1, true}},
    {LENGTH_OM3, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 143, 1, true}},
    {LENGTH_OM2, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 144, 1, true}},
    {LENGTH_OM1, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 145, 1, true}},
    {LENGTH_CBLASSY, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 146, 1, true}},
    {VENDOR_NAME, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 148, 16, true}},
    {VENDOR_OUI, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 165, 3, true}},
    {PART_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 168, 16, true}},
    {REVISION_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 184, 2, true}},
    {
        ETHERNET_EXTENDED_COMPLIANCE,
        {QSFP_BANKNA, QSFP_PAGE0_UPPER, 192, 1, true}
    },
    {OPTIONS, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 193, 3, true}},
    {VENDOR_SERIAL_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 196, 16, true}},
    {MFG_DATE, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 212, 8, true}},

    /* Page 3 values, including alarm and warning threshold values: */
    {TEMPERATURE_THRESH, {QSFP_BANKNA, QSFP_PAGE3, 128, 8, true}},
    {VCC_THRESH, {QSFP_BANKNA, QSFP_PAGE3, 144, 8, true}},
    {RX_PWR_THRESH, {QSFP_BANKNA, QSFP_PAGE3, 176, 8, true}},
    {TX_BIAS_THRESH, {QSFP_BANKNA, QSFP_PAGE3, 184, 8, true}},
    {TX_PWR_THRESH, {QSFP_BANKNA, QSFP_PAGE3, 192, 8, true}},

};

/* CMIS (Common Management Interface Spec), used in QSFP-DD, OSFP, COBO, etc */
static sff_field_map_t cmis_field_map[] = {
    /* Base page values, including alarms and sensors */
    {IDENTIFIER, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 0, 1, true}},
    {SPEC_REV, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 1, 1, true}},
    {STATUS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 2, 1, true}},
    {MODULE_STATE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 3, 1, false}},
    {
        MODULE_FLAGS,
        {QSFP_BANKNA, QSFP_PAGE0_LOWER, 8, CMIS_MODULE_FLAG_BYTES, false}
    },
    {TEMPERATURE_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 9, 1, false}},
    {VCC_ALARMS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 9, 1, false}},
    {TEMPERATURE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 14, 2, false}},
    {VCC, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 16, 2, false}},
    {POWER_CONTROL, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 26, 1, false}}, /* note -
        LowPwr bit (6) first appeared in CMIS 4.0 */
    {FIRMWARE_VER_ACTIVE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 39, 2, false}},
    {MODULE_MEDIA_TYPE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 85, 1, true}},
    {APSEL1_ALL, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 86, BYTES_PER_APP, true}},
    {APSEL1_HOST_ID, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 86, 1, true}},
    {APSEL1_MEDIA_ID, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 87, 1, true}},
    {APSEL1_LANE_COUNTS, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 88, 1, true}},
    {
        APSEL1_HOST_LANE_OPTION_MASK,
        {QSFP_BANKNA, QSFP_PAGE0_LOWER, 89, 1, true}
    },
    {PAGE_SELECT_BYTE, {QSFP_BANKNA, QSFP_PAGE0_LOWER, 127, 1, false}},

    /* Upper Page 0 values, including vendor info: */

    // Ethernet compliance is advertised in the Application Codes
    // {ETHERNET_COMPLIANCE},
    // {ETHERNET_EXTENDED_COMPLIANCE},
    // {LENGTH_OM1},  not supported in CMIS modules
    {VENDOR_NAME, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 129, 16, true}},
    {VENDOR_OUI, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 145, 3, true}},
    {PART_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 148, 16, true}},
    {REVISION_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 164, 2, true}},
    {VENDOR_SERIAL_NUMBER, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 166, 16, true}},
    {MFG_DATE, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 182, 8, true}},
    {PWR_CLASS, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 200, 1, true}},
    {PWR_REQUIREMENTS, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 201, 1, true}},
    {LENGTH_CBLASSY, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 202, 1, true}},
    {CONN_TYPE, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 203, 1, true}},
    {MEDIA_INT_TECH, {QSFP_BANKNA, QSFP_PAGE0_UPPER, 212, 1, true}},

    /* page 1 values, active module features */
    {FIRMWARE_VER_INACTIVE, {QSFP_BANKNA, QSFP_PAGE1, 128, 2, false}},
    {HARDWARE_VER, {QSFP_BANKNA, QSFP_PAGE1, 130, 2, true}},
    {LENGTH_SM_KM, {QSFP_BANKNA, QSFP_PAGE1, 132, 1, true}},
    {LENGTH_OM5, {QSFP_BANKNA, QSFP_PAGE1, 133, 1, true}},
    {LENGTH_OM4, {QSFP_BANKNA, QSFP_PAGE1, 134, 1, true}},
    {LENGTH_OM3, {QSFP_BANKNA, QSFP_PAGE1, 135, 1, true}},
    {LENGTH_OM2, {QSFP_BANKNA, QSFP_PAGE1, 136, 1, true}},
    {FLAG_SUPPORT, {QSFP_BANKNA, QSFP_PAGE1, 157, 2, true}},
    {APSEL1_MEDIA_LANE_OPTION_MASK, {QSFP_BANKNA, QSFP_PAGE1, 176, 1, true}},
    {APSEL9_ALL, {QSFP_BANKNA, QSFP_PAGE1, 223, BYTES_PER_APP, true}},
    {APSEL9_HOST_ID, {QSFP_BANKNA, QSFP_PAGE1, 223, 1, true}},
    {APSEL9_MEDIA_ID, {QSFP_BANKNA, QSFP_PAGE1, 224, 1, true}},
    {APSEL9_LANE_COUNTS, {QSFP_BANKNA, QSFP_PAGE1, 225, 1, true}},
    {APSEL9_HOST_LANE_OPTION_MASK, {QSFP_BANKNA, QSFP_PAGE1, 226, 1, true}},

    /* Page 2 values, including alarm and warning threshold values: */
    {TEMPERATURE_THRESH, {QSFP_BANKNA, QSFP_PAGE2, 128, 8, true}},
    {VCC_THRESH, {QSFP_BANKNA, QSFP_PAGE2, 136, 8, true}},
    {RX_PWR_THRESH, {QSFP_BANKNA, QSFP_PAGE2, 192, 8, true}},
    {TX_BIAS_THRESH, {QSFP_BANKNA, QSFP_PAGE2, 184, 8, true}},
    {TX_PWR_THRESH, {QSFP_BANKNA, QSFP_PAGE2, 176, 8, true}},

    /* Page 16 (banked), data path control */
    {DATAPATH_DEINIT, {QSFP_BANKCH, QSFP_PAGE16, 128, 1, false}},
    {CHANNEL_TX_DISABLE, {QSFP_BANKCH, QSFP_PAGE16, 130, 1, false}},
    {APPLY_DATAPATHINIT_SS0, {QSFP_BANKCH, QSFP_PAGE16, 143, 1, false}},
    {DATAPATH_CFG_ALL_STAGED_SET0, {QSFP_BANKCH, QSFP_PAGE16, 145, 33, false}},
    {APPLICATION_SELECT_LN1, {QSFP_BANKCH, QSFP_PAGE16, 145, 1, false}},
    {DATAPATH_CFG_ALL_STAGED_SET1, {QSFP_BANKCH, QSFP_PAGE16, 180, 33, false}},

    /* Page 17 (banked), data path status */
    {DATAPATH_STATE_ALL, {QSFP_BANKCH, QSFP_PAGE17, 128, 4, false}},
    {DATAPATH_STATE_LN1AND2, {QSFP_BANKCH, QSFP_PAGE17, 128, 1, false}},
    {
        CMIS_LANE_FLAGS,
        {QSFP_BANKCH, QSFP_PAGE17, 134, CMIS_LANE_FLAG_BYTES, false}
    },
    {CHANNEL_TX_PWR_ALARMS, {QSFP_BANKCH, QSFP_PAGE17, 139, 4, false}},
    {CHANNEL_TX_BIAS_ALARMS, {QSFP_BANKCH, QSFP_PAGE17, 143, 4, false}},
    {CHANNEL_RX_PWR_ALARMS, {QSFP_BANKCH, QSFP_PAGE17, 149, 4, false}},
    {CHANNEL_TX_PWR, {QSFP_BANKCH, QSFP_PAGE17, 154, 16, false}},
    {CHANNEL_TX_BIAS, {QSFP_BANKCH, QSFP_PAGE17, 170, 16, false}},
    {CHANNEL_RX_PWR, {QSFP_BANKCH, QSFP_PAGE17, 186, 16, false}},
    {DATAPATH_CFG_STATUS, {QSFP_BANKCH, QSFP_PAGE17, 202, 1, false}},
    {DATAPATH_CFG_ALL_ACTIVE_SET, {QSFP_BANKCH, QSFP_PAGE17, 206, 29, false}},
    {LN1_ACTIVE_SET, {QSFP_BANKCH, QSFP_PAGE17, 206, 1, false}},
};

static sff_flag_map_t sff8636_flag_map[] = {
    /* flag, field_in, firstbyte, firstbit, numbits, perlaneflag, laneincr,
       inbyteinv */
    {FLAG_TX_LOS, SFF8636_LANE_STATUS_FLAGS, 3, 4, 1, true, 1, false},
    {FLAG_RX_LOS, SFF8636_LANE_STATUS_FLAGS, 3, 0, 1, true, 1, false},
    {
        FLAG_TX_ADAPT_EQ_FAULT,
        SFF8636_LANE_STATUS_FLAGS,
        4,
        4,
        1,
        true,
        1,
        false
    },
    {FLAG_TX_FAULT, SFF8636_LANE_STATUS_FLAGS, 4, 0, 1, true, 1, false},
    {FLAG_TX_LOL, SFF8636_LANE_STATUS_FLAGS, 5, 4, 1, true, 1, false},
    {FLAG_RX_LOL, SFF8636_LANE_STATUS_FLAGS, 5, 0, 1, true, 1, false},

    {FLAG_TEMP_HIGH_ALARM, MODULE_FLAGS, 6, 7, 1, false, 0, false},
    {FLAG_TEMP_LOW_ALARM, MODULE_FLAGS, 6, 6, 1, false, 0, false},
    {FLAG_TEMP_HIGH_WARN, MODULE_FLAGS, 6, 5, 1, false, 0, false},
    {FLAG_TEMP_LOW_WARN, MODULE_FLAGS, 6, 4, 1, false, 0, false},
    {FLAG_VCC_HIGH_ALARM, MODULE_FLAGS, 7, 7, 1, false, 0, false},
    {FLAG_VCC_LOW_ALARM, MODULE_FLAGS, 7, 6, 1, false, 0, false},
    {FLAG_VCC_HIGH_WARN, MODULE_FLAGS, 7, 5, 1, false, 0, false},
    {FLAG_VCC_LOW_WARN, MODULE_FLAGS, 7, 4, 1, false, 0, false},
    {FLAG_VENDOR_SPECIFIC, MODULE_FLAGS, 8, 0, 8, false, 0, false},

    {
        FLAG_RX_PWR_HIGH_ALARM,
        SFF8636_LANE_MONITOR_FLAGS,
        9,
        7,
        1,
        true,
        4,
        true
    },
    {FLAG_RX_PWR_LOW_ALARM, SFF8636_LANE_MONITOR_FLAGS, 9, 6, 1, true, 4, true},
    {FLAG_RX_PWR_HIGH_WARN, SFF8636_LANE_MONITOR_FLAGS, 9, 5, 1, true, 4, true},
    {FLAG_RX_PWR_LOW_WARN, SFF8636_LANE_MONITOR_FLAGS, 9, 4, 1, true, 4, true},
    {
        FLAG_TX_BIAS_HIGH_ALARM,
        SFF8636_LANE_MONITOR_FLAGS,
        11,
        7,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_BIAS_LOW_ALARM,
        SFF8636_LANE_MONITOR_FLAGS,
        11,
        6,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_BIAS_HIGH_WARN,
        SFF8636_LANE_MONITOR_FLAGS,
        11,
        5,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_BIAS_LOW_WARN,
        SFF8636_LANE_MONITOR_FLAGS,
        11,
        4,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_PWR_HIGH_ALARM,
        SFF8636_LANE_MONITOR_FLAGS,
        13,
        7,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_PWR_LOW_ALARM,
        SFF8636_LANE_MONITOR_FLAGS,
        13,
        6,
        1,
        true,
        4,
        true
    },
    {
        FLAG_TX_PWR_HIGH_WARN,
        SFF8636_LANE_MONITOR_FLAGS,
        13,
        5,
        1,
        true,
        4,
        true
    },
    {FLAG_TX_PWR_LOW_WARN, SFF8636_LANE_MONITOR_FLAGS, 13, 4, 1, true, 4, true},
};

static sff_flag_map_t cmis_flag_map[] = {
    /* flag, field_in, firstbyte, firstbit, numbits, perlaneflag, laneincr,
       inbyteinv */
    {FLAG_DATAPATH_FW_FAULT, MODULE_FLAGS, 8, 2, 1, false, 0, false},
    {FLAG_MODULE_FW_FAULT, MODULE_FLAGS, 8, 1, 1, false, 0, false},
    {FLAG_MODULE_STATE_CHANGE, MODULE_FLAGS, 8, 0, 1, false, 0, false},
    {FLAG_VCC_LOW_WARN, MODULE_FLAGS, 9, 7, 1, false, 0, false},
    {FLAG_VCC_HIGH_WARN, MODULE_FLAGS, 9, 6, 1, false, 0, false},
    {FLAG_VCC_LOW_ALARM, MODULE_FLAGS, 9, 5, 1, false, 0, false},
    {FLAG_VCC_HIGH_ALARM, MODULE_FLAGS, 9, 4, 1, false, 0, false},
    {FLAG_TEMP_LOW_WARN, MODULE_FLAGS, 9, 3, 1, false, 0, false},
    {FLAG_TEMP_HIGH_WARN, MODULE_FLAGS, 9, 2, 1, false, 0, false},
    {FLAG_TEMP_LOW_ALARM, MODULE_FLAGS, 9, 1, 1, false, 0, false},
    {FLAG_TEMP_HIGH_ALARM, MODULE_FLAGS, 9, 0, 1, false, 0, false},
    {FLAG_AUX2_LOW_WARN, MODULE_FLAGS, 10, 7, 1, false, 0, false},
    {FLAG_AUX2_HIGH_WARN, MODULE_FLAGS, 10, 6, 1, false, 0, false},
    {FLAG_AUX2_LOW_ALARM, MODULE_FLAGS, 10, 5, 1, false, 0, false},
    {FLAG_AUX2_HIGH_ALARM, MODULE_FLAGS, 10, 4, 1, false, 0, false},
    {FLAG_AUX1_LOW_WARN, MODULE_FLAGS, 10, 3, 1, false, 0, false},
    {FLAG_AUX1_HIGH_WARN, MODULE_FLAGS, 10, 2, 1, false, 0, false},
    {FLAG_AUX1_LOW_ALARM, MODULE_FLAGS, 10, 1, 1, false, 0, false},
    {FLAG_AUX1_HIGH_ALARM, MODULE_FLAGS, 10, 0, 1, false, 0, false},
    {FLAG_VEND_LOW_WARN, MODULE_FLAGS, 11, 7, 1, false, 0, false},
    {FLAG_VEND_HIGH_WARN, MODULE_FLAGS, 11, 6, 1, false, 0, false},
    {FLAG_VEND_LOW_ALARM, MODULE_FLAGS, 11, 5, 1, false, 0, false},
    {FLAG_VEND_HIGH_ALARM, MODULE_FLAGS, 11, 4, 1, false, 0, false},
    {FLAG_AUX3_LOW_WARN, MODULE_FLAGS, 11, 3, 1, false, 0, false},
    {FLAG_AUX3_HIGH_WARN, MODULE_FLAGS, 11, 2, 1, false, 0, false},
    {FLAG_AUX3_LOW_ALARM, MODULE_FLAGS, 11, 1, 1, false, 0, false},
    {FLAG_AUX3_HIGH_ALARM, MODULE_FLAGS, 11, 0, 1, false, 0, false},

    {FLAG_DATAPATH_STATE_CHANGE, CMIS_LANE_FLAGS, 134, 0, 1, true, 1, false},
    {FLAG_TX_FAULT, CMIS_LANE_FLAGS, 135, 0, 1, true, 1, false},
    {FLAG_TX_LOS, CMIS_LANE_FLAGS, 136, 0, 1, true, 1, false},
    {FLAG_TX_LOL, CMIS_LANE_FLAGS, 137, 0, 1, true, 1, false},
    {FLAG_TX_ADAPT_EQ_FAULT, CMIS_LANE_FLAGS, 138, 0, 1, true, 1, false},
    {FLAG_TX_PWR_HIGH_ALARM, CMIS_LANE_FLAGS, 139, 0, 1, true, 1, false},
    {FLAG_TX_PWR_LOW_ALARM, CMIS_LANE_FLAGS, 140, 0, 1, true, 1, false},
    {FLAG_TX_PWR_HIGH_WARN, CMIS_LANE_FLAGS, 141, 0, 1, true, 1, false},
    {FLAG_TX_PWR_LOW_WARN, CMIS_LANE_FLAGS, 142, 0, 1, true, 1, false},
    {FLAG_TX_BIAS_HIGH_ALARM, CMIS_LANE_FLAGS, 143, 0, 1, true, 1, false},
    {FLAG_TX_BIAS_LOW_ALARM, CMIS_LANE_FLAGS, 144, 0, 1, true, 1, false},
    {FLAG_TX_BIAS_HIGH_WARN, CMIS_LANE_FLAGS, 145, 0, 1, true, 1, false},
    {FLAG_TX_BIAS_LOW_WARN, CMIS_LANE_FLAGS, 146, 0, 1, true, 1, false},
    {FLAG_RX_LOS, CMIS_LANE_FLAGS, 147, 0, 1, true, 1, false},
    {FLAG_RX_LOL, CMIS_LANE_FLAGS, 148, 0, 1, true, 1, false},
    {FLAG_RX_PWR_HIGH_ALARM, CMIS_LANE_FLAGS, 149, 0, 1, true, 1, false},
    {FLAG_RX_PWR_LOW_ALARM, CMIS_LANE_FLAGS, 150, 0, 1, true, 1, false},
    {FLAG_RX_PWR_HIGH_WARN, CMIS_LANE_FLAGS, 151, 0, 1, true, 1, false},
    {FLAG_RX_PWR_LOW_WARN, CMIS_LANE_FLAGS, 152, 0, 1, true, 1, false},
};

static bf_qsfp_flag_format_t
sff8636_flag_formats[] = {
    /* Sff_field, high_alarm_loc, low_alarm_loc, high_warn_loc, low_warn_loc,
       is_ch_flag, ch_order_opposite_bit_order */
    {TEMPERATURE_ALARMS, 7, 6, 5, 4, false, false},
    {VCC_ALARMS, 7, 6, 5, 4, false, false},
    {CHANNEL_RX_PWR_ALARMS, 3, 2, 1, 0, true, true},
    {CHANNEL_TX_BIAS_ALARMS, 3, 2, 1, 0, true, true},
};

static bf_qsfp_flag_format_t cmis_flag_formats[]
= {
    /* Sff_field, high_alarm_loc, low_alarm_loc, high_warn_loc, low_warn_loc,
       is_ch_flag, ch_order_opposite_bit_order */
    {TEMPERATURE_ALARMS, 4, 5, 6, 7, false, false},
    {VCC_ALARMS, 4, 5, 6, 7, false, false},
    {CHANNEL_RX_PWR_ALARMS, 0, 1, 2, 3, true, true},   // these are byte locs
    {CHANNEL_TX_BIAS_ALARMS, 0, 1, 2, 3, true, true},  // these are byte locs
};

/* cable length multiplier values, from SFF specs */
static const sff_field_mult_t qsfp_multiplier[] =
{
    {LENGTH_SM_KM, 1000},
    {LENGTH_OM5, 2},
    {LENGTH_OM4, 2},
    {LENGTH_OM3, 2},
    {LENGTH_OM2, 1},
    {LENGTH_OM1, 1},
    {LENGTH_CBLASSY, 1},
};

// the Extended code in this array are from SFF-8024, rev 4.6, section 4.5
static sff_ext_code_map_t sff_ext_code_map[] = {
    {0x00, "Unspecified"},
    {0x01, "100G AOC or 25GAUI C2M AOC"},
    {0x02, "100GBASE-SR4 or 25GBASE-SR"},
    {0x03, "100GBASE-LR4 or 25GBASE-LR"},
    {0x04, "100GBASE-ER4 or 25GBASE-ER"},
    {0x05, "100GBASE-SR10"},
    {0x06, "100G CWDM4"},
    {0x07, "100G PSM4 Parallel SMF"},
    {0x08, "100G ACC or 25GAUI C2M ACC. BER of 5 ? 10-5"},
    // {0x09, "Obsolete"},
    // {0x0A, "Reserved"},
    {0x0B, "100GBASE-CR4, 25GBASE-CR CA-25G-L or 50GBASE-CR2 with FEC91"},
    {0x0C, "25GBASE-CR CA-25G-S or 50GBASE-CR2 with BASE-R FEC74"},
    {0x0D, "25GBASE-CR CA-25G-N or 50GBASE-CR2 with no FEC"},
    // {0x0E-0F, "Reserved"},
    {0x10, "40GBASE-ER4"},
    {0x11, "4 x 10GBASE-SR"},
    {0x12, "40G PSM4 Parallel SMF"},
    {0x13, "G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310 nm SM)"},
    {0x14, "G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550 nm SM)"},
    {0x15, "G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550 nm SM)"},
    {0x16, "10GBASE-T with SFI electrical interface"},
    {0x17, "100G CLR4"},
    {0x18, "100G AOC or 25GAUI C2M AOC. BER of 10-12 or below"},
    {0x19, "100G ACC or 25GAUI C2M ACC. BER of 10-12 or below"},
    {0x1A, "100GE-DWDM2"},
    {0x1B, "100G 1550nm WDM (4 wavelengths)"},
    {0x1C, "10GBASE-T Short Reac (30 meters)"},
    {0x1D, "5GBASE-T"},
    {0x1E, "2.5GBASE-T"},
    {0x1F, "40G SWDM4"},
    {0x20, "100G SWDM4"},
    {0x21, "100G PAM4 BiDi"},
    {
        0x22,
        "4WDM-10 MSA (10km version of 100G CWDM4 with same RS(528,514) FEC in "
        "host system)"
    },
    {
        0x23,
        "4WDM-20 MSA (20km version of 100GBASE-LR4 with RS(528,514) FEC in host "
        "system)"
    },
    {
        0x24,
        "4WDM-40 MSA (40km reac with APD receiver and RS(528,514) FEC in host "
        "system)"
    },
    {0x25, "100GBASE-DR (clause 140), CAUI-4 (no FEC)"},
    {0x26, "100G-FR or 100GBASE-FR1 (clause 140), CAUI-4 (no FEC)"},
    {0x27, "100G-LR or 100GBASE-LR1 (clause 140), CAUI-4 (no FEC)"},
    // {0x28 ? 2F, "Reserved"},
    {0x30, "ACC with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. BER of 10-6 or below"},
    {0x31, "AOC with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. BER of 10-6 or below"},
    {
        0x32,
        "ACC with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. BER of 2.6x10-4 for ACC, "
        "10-5 for AUI, or below"
    },
    {
        0x33,
        "AOC with 50GAUI, 100GAUI-2 or 200GAUI-4 C2M. BER of 2.6x10-4 for AOC, "
        "10-5 for AUI, or below"
    },
    // {0x34 ? 3F, "Reserved"},
    {0x40, "50GBASE-CR, 100GBASE-CR2, or 200GBASE-CR4"},
    {0x41, "50GBASE-SR, 100GBASE-SR2, or 200GBASE-SR4"},
    {0x42, "50GBASE-FR or 200GBASE-DR4"},
    {0x43, "200GBASE-FR4"},
    {0x44, "200G 1550 nm PSM4"},
    {0x45, "50GBASE-LR"},
    {0x46, "200GBASE-LR4"},
    // {0x47 ? 4F, "Reserved"},
    {0x50, "64GFC EA"},
    {0x51, "64GFC SW"},
    {0x52, "64GFC LW"},
    {0x53, "128GFC EA"},
    {0x54, "128GFC SW"},
    {0x55, "128GFC LW"},
    // {0x56 - FF, "Reserved"},
};

typedef struct {
    uint8_t host_id;
    bf_port_speed_t speed;
    char host_id_desc[41];
} cmis_app_host_id_map_t;

// the host_id's in this array are from SFF-8024, rev 4.6, section 4.7
static cmis_app_host_id_map_t
cmis_app_host_if_speed_map[] = {
    /* host_id, bf_port_speed_t, host_id_desc */
    /*                     -- max length -------------------------- */
    {0, BF_SPEED_NONE, "Undefined"},
    {1, BF_SPEED_1G, "1000BASE-CX      1 x 1.25Gbd NRZ"},
    //  {2, BF_SPEED_???,     "XAUI             4 x 3.125Gbd NRZ"},
    {4, BF_SPEED_10G, "SFI           1 x 9.95-11.18Gbd NRZ"},
    {5, BF_SPEED_25G, "25GAUI           1 x 25.78125Gbd NRZ"},
    {6, BF_SPEED_40G, "XLAUI            4 X 10.3125Gbd NRZ"},  // typ CFP
    {7, BF_SPEED_40G, "XLPPI            4 X 10.3125Gbd NRZ"},  // typ QSFP
    {8, BF_SPEED_50G, "LAUI-2           2 X 25.78125Gbd NRZ"},
    {9, BF_SPEED_50G, "50GAUI-2         2 X 26.5625Gbd NRZ"},  // KP FEC
    {10, BF_SPEED_50G, "50GAUI-1         1 x 26.5625Gbd PAM4"},
    {11, BF_SPEED_100G, "CAUI-4 FEC       4 x 25.78125Gbd NRZ"},  // KR FEC
    {12, BF_SPEED_100G, "100GAUI-4        4 x 26.5625Gbd NRZ"},   // KP FEC
    {13, BF_SPEED_100G, "100GAUI-2        2 x 26.5625Gbd PAM4"},
    {14, BF_SPEED_200G, "200GAUI-8        8 x 26.5625Gbd NRZ"},
    {15, BF_SPEED_200G, "200GAUI-4        4 x 26.5625Gbd PAM4"},
    //  {16, BF_SPEED_400G,    "400GAUI-16 C2M     16 x 26.5625Gbd NRZ"}, /* by Hang Tsi, 2024/02/26. */
    {17, BF_SPEED_400G, "400GAUI-8        8 x 26.5625Gbd PAM4"},
    //  {19, BF_SPEED_???,    "10GBASE-CX4      4 x 3.125Gbd NRZ"},
    {20, BF_SPEED_25G, "25GBASE-CR CA-L  1 x 25.78125 Gbd NRZ"},
    {21, BF_SPEED_25G, "25GBASE-CR CA-S  1 x 25.78125 Gbd NRZ"},
    {22, BF_SPEED_25G, "25GBASE-CR CA-N  1 x 25.78125 Gbd NRZ"},
    {23, BF_SPEED_40G, "40GBASE-CR4      4 X 10.3125Gbd NRZ"},
    {24, BF_SPEED_50G, "50GBASE-CR       1 x 26.5625Gbd PAM4"},
    {26, BF_SPEED_100G, "100GBASE-CR4     4 x 25.78125Gbd NRZ"},
    {27, BF_SPEED_100G, "100GBASE-CR2     2 x 26.5625Gbd PAM4"},
    {28, BF_SPEED_200G, "200GBASE-CR4     4 x 26.5625Gbd PAM4"},
    {29, BF_SPEED_400G, "400GBASE-CR8     8 x 26.5625Gbd PAM4"},
    {65, BF_SPEED_100G, "CAUI-4 C2M NOFEC 4 x 25.78125Gbd NRZ"}, /* by Hang Tsi, 2024/01/23. */
    {66, BF_SPEED_100G, "CAUI-4 C2M   FEC 4 x 25.78125Gbd NRZ"}, /* by Hang Tsi, 2024/02/26. */
    {70, BF_SPEED_100G, "100GBASE-CR1     1 x 53.125GBd PAM4"},
    {71, BF_SPEED_200G, "200GBASE-CR2     2 x 53.125GBd PAM4"},
    {72, BF_SPEED_400G, "400GBASE-CR4     4 x 53.125GBd PAM4"},
    //{73, BF_SPEED_800G, "800GBASE-CR8     8 x 53.125GBd PAM4"},
    {75, BF_SPEED_100G, "100GAUI-1-S      1 x 53.125GBd PAM4"},
    {76, BF_SPEED_100G, "100GAUI-1-L      1 x 53.125GBd PAM4"},
    {77, BF_SPEED_200G, "200GAUI-2-S      2 x 53.125GBd PAM4"},
    {78, BF_SPEED_200G, "200GAUI-2-L      2 x 53.125GBd PAM4"},
    {79, BF_SPEED_400G, "400GAUI-4-S      4 x 53.125GBd PAM4"},
    {80, BF_SPEED_400G, "400GAUI-4-L      4 x 53.125GBd PAM4"},
    //{81, BF_SPEED_800G, "800GAUI-8-S      8 x 53.125GBd PAM4"},
    //{82, BF_SPEED_800G, "800GAUI-8-L      8 x 53.125GBd PAM4"},
};

// the host_id's in this array are from SFF-8024, rev 4.6, section 4.7
static cmis_app_host_id_map_t
innolight_cmis_app_host_if_speed_map[] = {
    {241, BF_SPEED_100G, "100GAUI-1       1 x 53.125GBd PAM4"},
    {242, BF_SPEED_200G, "200GAUI-2       2 x 53.125GBd PAM4"},
    {244, BF_SPEED_400G, "400GAUI-4       4 x 53.125GBd PAM4"},
    //{248, BF_SPEED_800G, "800GAUI-8       8 x 53.125GBd PAM4"},
};

typedef struct {
    uint8_t media_type;
    char media_type_str[41];
} cmis_media_type_t;

static cmis_media_type_t cmis_media_type_desc[] =
{
    /* media_type, media_type_str */
    /*                       -- max length -------------------------- */
    {MEDIA_TYPE_UNDEF, "Undefined"},
    {MEDIA_TYPE_MMF, "Optical interface - MMF"},
    {MEDIA_TYPE_SMF, "Optical interface - SMF"},
    {MEDIA_TYPE_PASSIVE_CU, "Passive copper"},
    {MEDIA_TYPE_ACTIVE_CBL, "Active cable"},
    {MEDIA_TYPE_BASE_T, "Base-T"},
};

typedef struct {
    uint8_t media_type;
    uint8_t media_id;
    char media_id_desc[41];
} cmis_app_media_id_map_t;

// the host_id's in this array are from SFF-8024, rev 4.6, section 4.7
static cmis_app_media_id_map_t
cmis_app_media_if_speed_map[] = {
    /* media_type, media_id, bf_port_speed_t, media_id_desc */
    /*                     -- max length -------------------------- */
    {MEDIA_TYPE_MMF, 0, "Undefined"},
    {MEDIA_TYPE_MMF, 1, "10GBASE-SW       1 X 9.95328 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 2, "10GBASE-SR       1 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 3, "25GBASE-SR       1 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 4, "40GBASE-SR4      4 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 5, "40GE SWDM MSA    4 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 6, "40GE BiDi        2 X 20.625 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 7, "50GBASE-SR       1 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 8, "100GBASE-SR10    10 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 9, "100GBASE-SR4     4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 10, "100GE SWDM4 MSA  4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_MMF, 11, "100GE BiDi       2 X 25.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 12, "100GBASE-SR2     2 X 25.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 13, "100GBASE-SR      1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 14, "200GBASE-SR4     4 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 15, "400GBASE-SR16    16 X 25.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 16, "400GBASE-SR8     8 X 26.5625 Gbd PAM4"}, /* by Hang Tsi, 2024/01/23. */
    {MEDIA_TYPE_MMF, 17, "400GBASE-SR4     4 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 18, "800GBASE-SR8     8 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 26, "400GE BiDi       8 X 25.5625 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 27, "200GBASE-SR2     2 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 29, "100GBASE-VR      1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 30, "200GBASE-VR2     2 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_MMF, 31, "400GBASE-VR4     4 X 53.125 Gbd PAM4"},

    /*                     -- max length -------------------------- */
    {MEDIA_TYPE_SMF, 0, "Undefined"},
    {MEDIA_TYPE_SMF, 1, "10GBASE-LW       1 X 9.95328 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 2, "10GBASE-EW       1 X 9.953 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 3, "10G-ZW           1 X 9.953 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 4, "10GBASE-LR       1 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 5, "10GBASE-ER       1 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 6, "10G-ZR           1 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 7, "25GBASE-LR       1 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 8, "25GBASE-ER       1 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 9, "40GBASE-LR4      4 X 10.3125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 10, "40GBASE-FR       1 X 41.25 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 11, "50GBASE-FR       1 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 12, "50GBASE-LR       1 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 13, "100GBASE-LR4     4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 14, "100GBASE-ER4     4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 15, "100G PSM4 MSA    4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 52, "100G CWDM4-OCP   4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 16, "100G CWDM4 MSA   4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 17, "100G 4WDM-10 MSA 4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 18, "100G 4WDM-20 MSA 4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 19, "100G 4WDM-30 MSA 4 X 25.78125 Gbd NRZ"},
    {MEDIA_TYPE_SMF, 20, "100GBASE-DR      1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 21, "100G-FR          1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 22, "100G-LR          1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 23, "200GBASE-DR4     4 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 24, "200GBASE-FR4     4 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 25, "200GBASE-LR4     4 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 26, "400GBASE-FR8     8 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 27, "400GBASE-LR8     8 X 26.5625 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 28, "400GBASE-DR4     4 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 29, "400G-FR4         4 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 30, "400G-LR4-10      4 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 67, "400G-LR4-6       4 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 74, "100G-LR1-20      1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 75, "100G-ER1-30      1 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 76, "100G-ER1-40      1 X 53.125 Gbd PAM4"},

    /*                            -- max length -------------------------- */
    {MEDIA_TYPE_PASSIVE_CU, 0, "Undefined"},
    {MEDIA_TYPE_PASSIVE_CU, 1, "Copper cable assembly"},

    /*                            -- max length -------------------------- */
    {MEDIA_TYPE_ACTIVE_CBL, 0, "Undefined"},
    {MEDIA_TYPE_ACTIVE_CBL, 1, "Active cable assembly, BER < 1E-12"},
    {MEDIA_TYPE_ACTIVE_CBL, 2, "Active cable assembly, BER < 5E-5"},
    {MEDIA_TYPE_ACTIVE_CBL, 3, "Active cable assembly, BER < 2.6E-4"},
    {MEDIA_TYPE_ACTIVE_CBL, 4, "Active cable assembly, BER < 1E-6"},
};

// the host_id's in this array are from SFF-8024, rev 4.6, section 4.7
static cmis_app_media_id_map_t
innolight_cmis_app_media_if_speed_map[] = {
    {MEDIA_TYPE_SMF, 251, "200GBASE-DR2     2 X 53.125 Gbd PAM4"},
    {MEDIA_TYPE_SMF, 252, "800GBASE-DR8     8 X 53.125 Gbd PAM4"},
};
/* by Hang Tsi, 2024/01/23.
 * Temporary code and will be removed later. */
typedef enum {
    QSFPDD_400G_SR8 = 0x07, /* Asterfusion QFDD-8G400-01 50GBASE-SR (Clause 138) */
} Custom_Module_MMF_media_interface_code;

static cmis_app_host_id_map_t
*bf_qsfp_get_custom_host_if_speed_map (
    int port, uint8_t host_id)
{
    cmis_app_host_id_map_t *map_ptr;
    int array_ind = 0, array_size;
    qsfp_vendor_info_t vendor;
    bf_qsfp_get_vendor_info (port, &vendor);

    if (!strncmp (vendor.name, "INNOLIGHT",
                  strlen ("INNOLIGHT"))) {
        map_ptr = innolight_cmis_app_host_if_speed_map;
        array_size = sizeof (
                         innolight_cmis_app_host_if_speed_map) /
                     sizeof (cmis_app_host_id_map_t);
    } else {
        return NULL;
    }

    while (array_ind < array_size) {
        if (map_ptr[array_ind].host_id == host_id) {
            return &map_ptr[array_ind];
        } else {
            array_ind++;
        }
    }
    return NULL;
}

static cmis_app_media_id_map_t
*bf_qsfp_get_custom_media_if_speed_map (
    int port, uint8_t media_type, uint8_t media_id)
{
    cmis_app_media_id_map_t *map_ptr;
    int array_ind = 0, array_size;
    qsfp_vendor_info_t vendor;
    bf_qsfp_get_vendor_info (port, &vendor);

    if (!strncmp (vendor.name, "INNOLIGHT",
                  strlen ("INNOLIGHT"))) {
        map_ptr = innolight_cmis_app_media_if_speed_map;
        array_size = sizeof (
                         innolight_cmis_app_media_if_speed_map) /
                     sizeof (cmis_app_media_id_map_t);
    } else {
        return NULL;
    }

    while (array_ind < array_size) {
        if ((map_ptr[array_ind].media_type == media_type)
            &&
            (map_ptr[array_ind].media_id == media_id)) {
            return &map_ptr[array_ind];
        } else {
            array_ind++;
        }
    }
    return NULL;
}

static bf_port_speed_t bf_qsfp_decode_host_speed (
    int port, uint8_t host_id)
{
    bf_port_speed_t matchspeed = 0;

    // all Applications are stored as CMIS codes, even SFF-8636.
    // For 8636 supported compliance codes, the equivalent CMIS host id
    // is entered in the map
    int array_ind = 0,
        array_size = sizeof (cmis_app_host_if_speed_map) /
                     sizeof (cmis_app_host_id_map_t);
    bool matchfound = false;
    cmis_app_host_id_map_t *host_id_map;

    if (qsfp_needs_hi_pwr_init (port)) {
        array_size =
            1;  // hardware init uses only first app
    }

    while ((array_ind < array_size) &&
           (!matchfound)) {
        if (cmis_app_host_if_speed_map[array_ind].host_id
            == host_id) {
            matchfound = true;
            matchspeed =
                cmis_app_host_if_speed_map[array_ind].speed;
        } else {
            array_ind++;
        }
    }

    if (!matchfound) {
        // Check for custom host ID
        host_id_map =
            bf_qsfp_get_custom_host_if_speed_map (port,
                    host_id);
        if (host_id_map) {
            matchspeed = host_id_map->speed;
        }
    }
    return matchspeed;
}

static bool bf_qsfp_decode_host_id (int port,
                                    uint8_t host_id,
                                    char *host_id_desc,
                                    bool show)
{
    // all Applications are stored as CMIS codes, even SFF-8636.
    // For 8636 supported compliance codes, the equivalent CMIS host id
    // is entered in the map
    int array_ind = 0,
        array_size = sizeof (cmis_app_host_if_speed_map) /
                     sizeof (cmis_app_host_id_map_t);
    bool matchfound = false;
    cmis_app_host_id_map_t *host_id_map;

    if (!show && qsfp_needs_hi_pwr_init (port)) {
        array_size =
            1;  // hardware init uses only first app
    }

    while ((array_ind < array_size) &&
           (!matchfound)) {
        if (cmis_app_host_if_speed_map[array_ind].host_id
            == host_id) {
            matchfound = true;
            strcpy (host_id_desc,
                    cmis_app_host_if_speed_map[array_ind].host_id_desc);
        } else {
            array_ind++;
        }
    }

    if (!matchfound) {
        // Check for custom host ID
        host_id_map =
            bf_qsfp_get_custom_host_if_speed_map (port,
                    host_id);
        if (host_id_map) {
            matchfound = true;
            strcpy (host_id_desc, host_id_map->host_id_desc);
        }
    }
    if (!matchfound) {
        sprintf (host_id_desc, "Unknown (0x%X)", host_id);
    }
    return matchfound;
}

static bool bf_qsfp_decode_media_id (int port,
                                     uint8_t media_type,
                                     uint8_t media_id,
                                     char *media_id_desc,
                                     bool show)
{
    // all Applications are stored as CMIS codes, even SFF-8636.
    // For 8636 supported compliance codes, the equivalent CMIS host id
    // is entered in the map
    int array_ind = 0,
        array_size = sizeof (
                         cmis_app_media_if_speed_map) /
                     sizeof (cmis_app_media_id_map_t);
    bool matchfound = false;
    cmis_app_media_id_map_t *media_id_map;

    if (!show && qsfp_needs_hi_pwr_init (port)) {
        array_size =
            1;  // hardware init uses only first app
    }

    while ((array_ind < array_size) &&
           (!matchfound)) {
        if ((cmis_app_media_if_speed_map[array_ind].media_type
             == media_type) &&
            (cmis_app_media_if_speed_map[array_ind].media_id
             == media_id)) {
            matchfound = true;
            strcpy (media_id_desc,
                    cmis_app_media_if_speed_map[array_ind].media_id_desc);
        } else {
            array_ind++;
        }
    }

    if (!matchfound) {
        // Check for custom host ID
        media_id_map =
            bf_qsfp_get_custom_media_if_speed_map (port,
                    media_type, media_id);
        if (media_id_map) {
            matchfound = true;
            strcpy (media_id_desc,
                    media_id_map->media_id_desc);
        }
    }
    if (!matchfound) {
        sprintf (media_id_desc, "Unknown (0x%X)",
                 media_id);
    }
    return matchfound;
}

// search through qsfp_info.app_list to find a match
// returns the matching index or -1 on failure
// applicable to SFF-8636 and CMIS modules. Note ApSel =
// returned index + 1
static int bf_qsfp_id_matching_app (int port,
                                    bf_port_speed_t intf_speed,
                                    int intf_lanes,
                                    bf_qsfp_info_t *qsfp_info)
{
    if (!qsfp_info) {
        return -1;
    }

    int cur_app = 0;
    bool matchfound = false;
    while ((cur_app < qsfp_info->num_apps) &&
           (!matchfound)) {
        if (BF_APP_VALID(qsfp_info->app_list[cur_app]) &&
            (intf_lanes ==
                qsfp_info->app_list[cur_app].host_lane_cnt)) {
            if ((intf_speed != 0) &&
                (intf_speed == bf_qsfp_decode_host_speed (
                     port, qsfp_info->app_list[cur_app].host_if_id))) {
                matchfound = true;
            }
        }
        if (!matchfound) {
            cur_app++;
        }
    }
    if (!matchfound) {
        cur_app = -1;
    }
    return cur_app;
}

// returns the matching ApSel number (1-based) for the queried port
// on error, returns -1
int bf_cmis_find_matching_Application (int port,
                                       bf_port_speed_t speed,
                                       int nlanes,
                                       int firstLane)
{
    int rc = 0;

    rc = bf_qsfp_id_matching_app (port, speed, nlanes,
                                  &bf_qsfp_info_arr[port]);

    if (rc >= 0) {  // we have found a match. rc is the matching ApSel index
        if (bf_qsfp_info_arr[port].app_list[rc].host_lane_assign_mask
            &
            (0x1 << firstLane)) {  // host lane assignment options matches
            // since the spec is 1-based, add 1 to return the actual ApSel code
            rc++;
        } else {
            rc = -2;  // we found a matching app, but our firstlane was invalid
        }
    }
    return rc;
}

// returns the media lane information for the indicated Application and host
// lane
// on error, returns -1
int bf_cmis_get_Application_media_info (int port,
                                        int curr_app,
                                        int host_lane,
                                        int *media_lane,
                                        int *media_firstLane,
                                        int *media_nLanes)
{
    if ((!media_lane || !media_firstLane ||
         !media_nLanes) ||
        (port > bf_plt_max_qsfp)) {
        return -1;
    }

    // range check application
    if ((curr_app < 1) ||
        (curr_app > bf_qsfp_info_arr[port].num_apps)) {
        LOG_ERROR ("QSFP    %2d : invalid ApSel (%d)",
                   port, curr_app);
        return -1;
    }

    int app = curr_app -
              1;  // Since ApSel is 1-based but app_list is 0 indexed

    /* Not neccesarry indeed for real world. */
    if (BF_APP_INVALID(bf_qsfp_info_arr[port].app_list[app])) {
        LOG_ERROR ("QSFP    %2d : INVALID ApSel (%d)",
                   port,
                   curr_app);
        return -1;
    }

    if ((bf_qsfp_info_arr[port].app_list[app].media_lane_cnt)
        == 0) {
        LOG_ERROR ("QSFP    %2d : invalid Media-lane (%d) for ApSel (%d)",
                   port,
                   bf_qsfp_info_arr[port].app_list[app].media_lane_cnt,
                   curr_app);
        return -1;
    }

    if ((bf_qsfp_info_arr[port].app_list[app].host_lane_cnt)
        == 0) {
        LOG_ERROR ("QSFP    %2d : invalid host-lane (%d) for ApSel (%d)",
                   port,
                   bf_qsfp_info_arr[port].app_list[app].host_lane_cnt,
                   curr_app);
        return -1;
    }

    // make sure host lane count is evenly divisible by media lane count
    if ((bf_qsfp_info_arr[port].app_list[app].host_lane_cnt
         %
         bf_qsfp_info_arr[port].app_list[app].media_lane_cnt)
        != 0) {
        LOG_ERROR ("QSFP    %2d : ApSel %d: media lane count (%d) %s (%d)",
                   port,
                   app,
                   bf_qsfp_info_arr[port].app_list[app].media_lane_cnt,
                   "is not evenly divisible into host lane count",
                   bf_qsfp_info_arr[port].app_list[app].host_lane_cnt);
        return -1;
    }

    // calculate the ratio between the host lane count and media lane count
    int host_media_lane_ratio =
        (int) (bf_qsfp_info_arr[port].app_list[app].host_lane_cnt /
               bf_qsfp_info_arr[port].app_list[app].media_lane_cnt);

    *media_nLanes =
        bf_qsfp_info_arr[port].app_list[app].media_lane_cnt;

    // figure out which host lane group we're using
    int host_ln_group = 0, host_firstLane = 0,
        curlane;
    for (curlane = 0; curlane <= host_lane;
         curlane++) {
        if ((bf_qsfp_info_arr[port].app_list[app].host_lane_assign_mask
             >>
             curlane) &
            0x1) {
            host_ln_group++;
            host_firstLane = curlane;
        }
    }

    // find the corresponding media lane group
    int media_ln_group = 0;
    curlane = 0;
    while ((media_ln_group < host_ln_group) &&
           (curlane < 8)) {
        if ((bf_qsfp_info_arr[port].app_list[app].media_lane_assign_mask
             >>
             curlane) &
            0x1) {
            media_ln_group++;
            *media_firstLane = curlane;
        }
        if (media_ln_group < host_ln_group) {
            curlane++;
        }
    }

    // figure out the media lane number inside the lane group
    *media_lane = *media_firstLane +
                  (int) ((host_lane - host_firstLane) /
                         host_media_lane_ratio);
    return 0;
}

/* return the requested media type string */
int bf_qsfp_get_media_type_str (int port,
                                char *media_type_str)
{
    if (bf_qsfp_is_cmis (port)) {
        int array_ind = 0,
            array_size = sizeof (cmis_media_type_desc) /
                         sizeof (cmis_media_type_t);
        bool matchfound = false;

        while ((array_ind < array_size) &&
               (!matchfound)) {
            if (cmis_media_type_desc[array_ind].media_type ==
                bf_qsfp_info_arr[port].media_type) {
                matchfound = true;
                strcpy (media_type_str,
                        cmis_media_type_desc[array_ind].media_type_str);
            } else {
                array_ind++;
            }
        }
        if (!matchfound) {
            sprintf (media_type_str,
                     "Unknown (0x%X)",
                     cmis_media_type_desc[array_ind].media_type);
        }
    } else {
        strcpy (media_type_str, "---");
    }
    return 0;
}

/* get the number of advertsed Applications */
int bf_qsfp_get_application_count (int port)
{
    return bf_qsfp_info_arr[port].num_apps;
}

/* return the requested Application info
 * NOTE: ApSel is 1-based, to be consistent with the CMIS spec */
int bf_qsfp_get_application (int port,
                             int ApSel,
                             qsfp_application_info_t *app_info)
{
    bf_pm_qsfp_apps_t *cur_app =
        &bf_qsfp_info_arr[port].app_list[ApSel - 1];
    app_info->host_if_id = cur_app->host_if_id;
    app_info->media_if_id = cur_app->media_if_id;
    bf_qsfp_decode_host_id (port, cur_app->host_if_id,
                            app_info->host_if_id_str,
                            false);
    bf_qsfp_decode_media_id (port,
                             bf_qsfp_info_arr[port].media_type,
                             cur_app->media_if_id,
                             app_info->media_if_id_str,
                             false);
    app_info->host_lane_cnt  = cur_app->host_lane_cnt;
    app_info->media_lane_cnt = cur_app->media_lane_cnt;
    app_info->host_lane_assign_mask  = cur_app->host_lane_assign_mask;
    app_info->media_lane_assign_mask = cur_app->media_lane_assign_mask;
    return 0;
}

/* return the requested Application info
 * NOTE: ApSel is 1-based, to be consistent with the CMIS spec.
 * For ucli ONLy
 * by Hang Tsi, 2024/02/29. */
int bf_qsfp_get_application0 (int port,
                             int ApSel,
                             qsfp_application_info_t *app_info)
{
    bf_pm_qsfp_apps_t *cur_app =
        &bf_qsfp_info_arr[port].app_list[ApSel - 1];
    app_info->host_if_id = cur_app->host_if_id;
    app_info->media_if_id = cur_app->media_if_id;
    bf_qsfp_decode_host_id (port, cur_app->host_if_id,
                            app_info->host_if_id_str,
                            true);
    bf_qsfp_decode_media_id (port,
                             bf_qsfp_info_arr[port].media_type,
                             cur_app->media_if_id,
                             app_info->media_if_id_str,
                             true);
    app_info->host_lane_cnt  = cur_app->host_lane_cnt;
    app_info->media_lane_cnt = cur_app->media_lane_cnt;
    app_info->host_lane_assign_mask  = cur_app->host_lane_assign_mask;
    app_info->media_lane_assign_mask = cur_app->media_lane_assign_mask;
    return 0;
}

int bf_qsfp_rxlos_debounce_get (int port)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    return bf_qsfp_info_arr[port].rxlos_debounce_cnt;
}

int bf_qsfp_rxlos_debounce_set (int port,
                                int count)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    bf_qsfp_info_arr[port].rxlos_debounce_cnt = count;
    return 0;
}

/* converts qsfp temp sensor data to absolute temperature */
static double get_temp (const uint16_t temp)
{
    double data;
    data = temp / 256.0;
    if (data > 128) {
        data = data - 256;
    }
    return data;
}

/* converts qsfp vcc sensor data to absolute voltage */
static double get_vcc (const uint16_t temp)
{
    double data;
    data = temp / 10000.0;
    return data;
}

/* converts qsfp txbias sensor data to absolute value */
static double get_txbias (const uint16_t temp)
{
    double data;
    data = temp * 2.0 / 1000;
    return data;
}

/* converts qsfp power sensor data to absolute power value */
static double get_pwr (const uint16_t temp)
{
    double data;
    data = 10.0 * log10(temp * 0.1 / 1000);
    return data;
}

/* return sff_field_info_t for a given field in sff8636_field_map[] */
static sff_field_info_t *get_sff_field_addr (
    const Sff_field field)
{
    int i, cnt = sizeof (sff8636_field_map) / sizeof (
                     sff_field_map_t);

    for (i = 0; i < cnt; i++) {
        if (sff8636_field_map[i].field == field) {
            return (& (sff8636_field_map[i].info));
        }
    }
    return NULL;
}

/* return sff_field_info_t for a given field in cmis_field_map[] */
static sff_field_info_t *get_cmis_field_addr (
    const Sff_field field)
{
    int i, cnt = sizeof (cmis_field_map) / sizeof (
                     sff_field_map_t);

    for (i = 0; i < cnt; i++) {
        if (cmis_field_map[i].field == field) {
            return (& (cmis_field_map[i].info));
        }
    }
    return NULL;
}

/* return the contents of the  sff_field_info_t for a given field */
static int get_sff_field_info (int port,
                               Sff_field field,
                               sff_field_info_t **info)
{
    if (field >= SFF_FIELD_MAX) {
        return -1;
    }
    if (field == IDENTIFIER) {
        // we may not know if we're CMIS or SFF-8636. Luckily, this field is
        // common to both
        *info = get_sff_field_addr (field);
    } else if (bf_qsfp_is_cmis (port)) {
        *info = get_cmis_field_addr (field);
    } else {
        *info = get_sff_field_addr (field);
    }
    if (!*info) {
        return -1;
    }
    return 0;
}

/* return the qsfp multiplier for a given field in struct qsfp_multiplier[] */
/* this function does not handle the CMIS upper nibble multiplier for
 * cable assys */
static uint32_t get_qsfp_multiplier (
    Sff_field field)
{
    int i, cnt = sizeof (qsfp_multiplier) / sizeof (
                     sff_field_mult_t);

    for (i = 0; i < cnt; i++) {
        if (qsfp_multiplier[i].field == field) {
            return (qsfp_multiplier[i].value);
        }
    }
    return 0; /* invalid field value */
}

static bf_qsfp_flag_format_t
*find_sff8636_flag_format (Sff_field fieldName)
{
    int i, cnt = sizeof (sff8636_flag_formats) /
                 sizeof (bf_qsfp_flag_format_t);

    for (i = 0; i < cnt; i++) {
        if (sff8636_flag_formats[i].fieldName ==
            fieldName) {
            return (& (sff8636_flag_formats[i]));
        }
    }
    return NULL;
}

static bf_qsfp_flag_format_t
*find_cmis_flag_format (Sff_field fieldName)
{
    int i, cnt = sizeof (cmis_flag_formats) / sizeof (
                     bf_qsfp_flag_format_t);

    for (i = 0; i < cnt; i++) {
        if (cmis_flag_formats[i].fieldName == fieldName) {
            return (& (cmis_flag_formats[i]));
        }
    }
    return NULL;
}

/*
 * Given a byte, extract bit fields for various alarm flags
 * chnum is 0-based, and relative to the current data
 */
static int extract_qsfp_flags (Sff_field
                               fieldName,
                               int port,
                               const uint8_t *data,
                               int chnum,
                               qsfp_flags_level_t *flags)
{
    if (!flags) {
        return -1;
    }

    bf_qsfp_flag_format_t *flag_format;
    bool is_cmis = bf_qsfp_is_cmis (port);

    if (is_cmis) {
        flag_format = find_cmis_flag_format (fieldName);
    } else {  // sff-8636
        flag_format = find_sff8636_flag_format (
                          fieldName);
    }

    if (!flag_format) {
        return -1;
    }

    if (flag_format->is_ch_flag) {  // channel flags
        if (is_cmis) {
            // loc variables are byte locations
            // this code assumes all data in within one bank
            flags->warn.low = (data[flag_format->low_warn_loc]
                               >> chnum) & 0x1;
            flags->warn.high =
                (data[flag_format->high_warn_loc] >> chnum) & 0x1;
            flags->alarm.low =
                (data[flag_format->low_alarm_loc] >> chnum) & 0x1;
            flags->alarm.high =
                (data[flag_format->high_alarm_loc] >> chnum) &
                0x1;
        } else {  // sff-8636
            // loc variables are bit locations
            flags->warn.low = (data[chnum >> 1] >>
                               (flag_format->low_warn_loc +
                                ((1 - (chnum & 0x1)) * 4))) &
                              0x1;
            flags->warn.high = (data[chnum >> 1] >>
                                (flag_format->high_warn_loc +
                                 ((1 - (chnum & 0x1)) * 4))) &
                               0x1;
            flags->alarm.low = (data[chnum >> 1] >>
                                (flag_format->low_alarm_loc +
                                 ((1 - (chnum & 0x1)) * 4))) &
                               0x1;
            flags->alarm.high = (data[chnum >> 1] >>
                                 (flag_format->high_alarm_loc +
                                  ((1 - (chnum & 0x1)) * 4))) &
                                0x1;
        }
    } else {  // module flags
        flags->warn.low = (*data >>
                           flag_format->low_warn_loc) & 0x1;
        flags->warn.high = (*data >>
                            flag_format->high_warn_loc) & 0x1;
        flags->alarm.low = (*data >>
                            flag_format->low_alarm_loc) & 0x1;
        flags->alarm.high = (*data >>
                             flag_format->high_alarm_loc) & 0x1;
    }
    return 0;
}

static bool is_qsfp_addr_in_cache (int port,
                                   sff_field_info_t *info)
{
    // assumes calling routine is handling port availability checks
    if (bf_qsfp_info_arr[port].cache_dirty) {
        return false;
    }

    return info->in_cache;
}

/* do not call this directly, go through bf_qsfp_field_read_onebank */
static bool bf_qsfp_read_cache (int port,
                                sff_field_info_t *info,
                                uint8_t *data)
{
    // is this field in the cache? (currently, only non-banked pages are cached)
    if ((!is_qsfp_addr_in_cache (port, info)) ||
        (info->bank != QSFP_BANKNA)) {
        return false;  // not in cache
    }

    // get pointer to cache location containing data
    uint8_t *cache_loc;
    if (info->page == QSFP_PAGE0_LOWER) {
        cache_loc = &bf_qsfp_info_arr[port].idprom[0 +
                    info->offset];
    } else {
        if (info->page == QSFP_PAGE0_UPPER) {
            cache_loc =
                &bf_qsfp_info_arr[port].page0[info->offset -
                                              MAX_QSFP_PAGE_SIZE];
        } else if (!bf_qsfp_is_flat_mem (port)) {
            switch (info->page) {
                case QSFP_PAGE1:
                    cache_loc =
                        &bf_qsfp_info_arr[port].page1[info->offset -
                                                      MAX_QSFP_PAGE_SIZE];
                    break;
                case QSFP_PAGE2:
                    cache_loc =
                        &bf_qsfp_info_arr[port].page2[info->offset -
                                                      MAX_QSFP_PAGE_SIZE];
                    break;
                case QSFP_PAGE3:
                    cache_loc =
                        &bf_qsfp_info_arr[port].page3[info->offset -
                                                      MAX_QSFP_PAGE_SIZE];
                    break;
                default:
                    LOG_ERROR ("Invalid Page value %d\n", info->page);
                    return false;
            }
        } else {
            LOG_ERROR ("Invalid Page value %d\n", info->page);
            return false;
        }
    }

    // copy cached data into the memory location provided by  calling routine
    memcpy (data, cache_loc, info->length);
    return true;
}

// determine the actual bank number, from sff_field_info_t and the channel mask
static int decode_bank (int port,
                        sff_field_info_t *info,
                        int chmask,
                        int *decoded_bank)
{
    int shifted_chmask;
    if (bf_qsfp_is_cmis (port)) {
        if (info->bank ==
            QSFP_BANKNA) {  // no bank, so bank number is 0
            *decoded_bank = 0;
        } else if (info->bank ==
                   QSFP_BANKCH) {  // bank is based on channel number
            // figure out which bank chmask is pointing to
            if (!chmask) {
                LOG_ERROR ("qsfp %d no chmask provided\n", port);
                return -1;
            }
            *decoded_bank = 0;
            shifted_chmask = chmask;
            while (shifted_chmask > 0xFF) {
                shifted_chmask >>= 8;
                (*decoded_bank)++;
            }

            // make sure channel mask only points to one bank
            if (chmask != (shifted_chmask << (*decoded_bank *
                                              8))) {
                LOG_ERROR (
                    "qsfp %d chmask must be within one bank (%x)\n",
                    port, chmask);
                return -1;
            }
        } else {  // bank is defined by sff_field_info_t structure
            *decoded_bank = info->bank;
        }

    } else {  // sff-8636, make sure chmask is in the first 4 bits
        if (chmask > 0xF) {
            LOG_ERROR ("qsfp %d chmask is invalid (%x)\n",
                       port, chmask);
            return -1;
        }
        *decoded_bank = 0;
    }
    return 0;
}

// determine the actual page number, from sff_field_info_t
static int decode_page (int port,
                        sff_field_info_t *info, int *decoded_page)
{
    if (info->page == QSFP_PAGE0_LOWER) {
        *decoded_page = 0;
    } else {
        *decoded_page = info->page;
    }
    return 0;
}

/* Reads the identified qsfp memory map field on the applicable port.
 * This routine works for any memory map, because it decodes the field enum
 * to identify the page, offset, and length for the read operation.
 */
/*  @param port
 *   port
 *  @param field
 *   Sff_field enum value, identifying field to read
 *  @param chmask
 *   bitmask identifying channels to query. Used to id bank, not filter data
 *   chmask must be constrained to one bank
 *  @param addl_offset
 *   additional offset to apply on top of offset derived from field, for arrays
 *  @param max_length
 *   length of read data buffer
 *  @param data
 *   read data
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_field_read_onebank (int port,
                                Sff_field field,
                                int chmask,
                                int addl_offset,
                                int max_length,
                                uint8_t *data)
{
    sff_field_info_t *info = NULL;
    int offset;

    // retrieve the field address info
    if (get_sff_field_info (port, field, &info) < 0) {
        /* To be fixed, by tsihang. */
        //LOG_ERROR ("QSFP    %2d : field %d not decodable",
        //           port, field);
        return -1;
    }

    if (!info) {
        LOG_ERROR ("QSFP    %2d : field %d decoded to NULL",
                   port, field);
        return -1;
    }

    // make sure the buffer is big enough
    if (info->length > max_length) {
        LOG_ERROR (
            "QSFP    %2d : field %d length exceeds available buffer",
            port, field);
        return -1;
    }

    // add in any additional offset for arrays
    offset = info->offset + addl_offset;

    if ((offset < 0) ||
        ((offset + info->length) >
         MAX_QSFP_PAGE_SIZE_255)) {
        LOG_ERROR ("qsfp %d offset (%d) or length (%d) for field %d is invalid\n",
                   port,
                   offset,
                   info->length,
                   field);
        return -1;
    }

    // check cache first
    if (!bf_qsfp_read_cache (port, info, data)) {
        // data is not in cache

        // figure out which bank the channel mask is on
        int bank, page;
        if (decode_bank (port, info, chmask, &bank) < 0) {
            return -1;
        }

        if (decode_page (port, info, &page) < 0) {
            return -1;
        }

        // read that location
        if (bf_qsfp_module_read (port, bank, page, offset,
                                 info->length, data) < 0) {
            if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
                LOG_WARNING ("qsfp %d read failed\n", port);
            }
            return -1;
        }
    }
    return 0;
}

/* Writes data into the identified qsfp memory map field on the applicable port.
 * This routine works for any memory map, because it decodes the field enum
 * to identify the page, offset, and length for the write operation.
 */
/*  @param port
 *   port
 *  @param field
 *   Sff_field enum value, identifying field to read
 *  @param chmask
 *   bitmask identifying channels to query. Used to id bank, not filter data
 *   chmask must be constrained to one bank
 *  @param addl_offset
 *   additional offset to apply on top of offset derived from field, for arrays
 *  @param max_length
 *   length of read data buffer
 *  @param data
 *   read data
 *  @return
 *   0 on success and -1 on error
 */
static int bf_qsfp_field_write_onebank (int port,
                                        Sff_field field,
                                        int chmask,
                                        int addl_offset,
                                        int len,
                                        uint8_t *data)
{
    sff_field_info_t *info = NULL;
    int offset;

    // retrieve the field address info
    if (get_sff_field_info (port, field, &info) < 0) {
        LOG_ERROR ("qsfp %d field %d not decodable\n",
                   port, field);
        return -1;
    }

    // add in any additional offset for arrays
    offset = info->offset + addl_offset;

    if ((offset < 0) ||
        ((offset + info->length) >
         MAX_QSFP_PAGE_SIZE_255)) {
        LOG_ERROR ("qsfp %d offset (%d) or length (%d) for field %d is invalid\n",
                   port,
                   offset,
                   info->length,
                   field);
        return -1;
    }

    // figure out which bank the channel mask is on
    int bank, page;
    if (decode_bank (port, info, chmask, &bank) < 0) {
        return -1;
    }

    if (decode_page (port, info, &page) < 0) {
        return -1;
    }

    // write that location
    if (bf_qsfp_module_write (port, bank, page,
                              offset, info->length, data) < 0) {
        LOG_ERROR ("qsfp %d write failed\n", port);
        return -1;
    }
    return 0;
}

/* Performs a read-modify-write to a single-byte bitfield in one bank in the
 * qsfp memory map on the applicable port. This routine works for any memory
 * map, because it decodes the field enum to identify the page, offset, and
 * length for the read operation.
 */
/*  @param port
 *   port
 *  @param field
 *   Sff_field enum value, identifying field to read
 *  @param chmask
 *   bitmask identifying channels to query. Used to id bank, not filter data
 *   chmask must be constrained to one bank
 *  @param newdata
 *   new data to write, in the appropriate bit positions
 *  @return
 *   0 on success and -1 on error
 */
static int bf_qsfp_bitfield_rmw_onebank (int port,
        Sff_field field,
        uint host_chmask,
        uint8_t chmask,
        uint8_t newdata)
{
    sff_field_info_t *info = NULL;

    // retrieve the field address info
    if (get_sff_field_info (port, field, &info) < 0) {
        LOG_ERROR ("qsfp %d field %d not decodable\n",
                   port, field);
        return -1;
    }

    if (info->length > 1) {
        LOG_ERROR (
            "qsfp %d field length %d incompatible with bf_qsfp_bitfield_rmw\n",
            port,
            info->length);
        return -1;
    }

    uint8_t buf = 0;  // create buffer for read data
    // only read & modify if we are partially updating the field
    if (chmask != 0xFF) {
        // read
        if (bf_qsfp_field_read_onebank (port, field,
                                        host_chmask, 0, 1, &buf) < 0) {
            return -1;
        }

        // modify
        buf = (buf & ~chmask) | newdata;
    } else {
        buf = newdata;
    }

    // write
    if (bf_qsfp_field_write_onebank (port, field,
                                     host_chmask, 0, 1, &buf) < 0) {
        return -1;
    }
    return 0;
}

/* Performs a read-modify-write to a single-byte bitfield in the qsfp memory
 * map on the applicable port. This routine works for any memory map, because
 * it decodes the field enum to identify the page, offset, and length for the
 * read operation. This routine cycles through all banks, as needed, and
 * rmw's each, one at a time
 */
/*  @param port
 *   port
 *  @param field
 *   Sff_field enum value, identifying field to read
 *  @param chmask
 *   bitmask identifying channels to update, may span multiple banks
 *  @param newdata
 *   new data to write, in the appropriate bit positions (including across
 * banks)
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_bitfield_rmw (
    int port, Sff_field field, uint host_chmask,
    uint chmask, uint newdata)
{
    uint8_t current_bank = 0;

    if ((port > bf_plt_max_qsfp) ||
        (host_chmask >= (1UL <<
                         bf_qsfp_info_arr[port].num_ch))) {
        return -1;
    }

    /* loop through mask, 8 channels at a time since a CMIS bank is 8 channels */
    /* This routine also works for SFF-8636 */
    while (host_chmask >> (current_bank * 8)) {
        if (host_chmask &
            (0xFF << current_bank)) {  // only update banks in the mask

            if (bf_qsfp_bitfield_rmw_onebank (
                    port,
                    field,
                    (host_chmask & (0xFF << current_bank)),
                    ((chmask >> (current_bank * 8)) & 0xFF),
                    ((newdata >> (current_bank * 8)) & 0xFF)) < 0) {
                return -1;
            }
        }
        current_bank++;
    }
    return 0;
}

/** read from a specific location in the memory map
 * It is recommended that bf_qsfp_field_read_onebank be used where possible and
 * bf_qsfp_module_read be used only to read vendor proprietary data that is
 * not defined by the MSAs
 *
 *  @param port
 *   port
 *  @param bank
 *   bank number, 0 if none
 *  @param page
 *   page number
 *  @param offset
 *    offset into QSFP memory to read from
 *  @param len
 *    number of bytes to read
 *  @param buf
 *    buffer to read into
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_module_read (
    unsigned int port, int bank, int page, int offset,
    int len, uint8_t *buf)
{
    int rc = 0;

    if (port > BF_PLAT_MAX_QSFP) {
        return -1;
    }

    bf_sys_mutex_lock (
        &bf_qsfp_info_arr[port].qsfp_mtx);
    // safe override
    if (bank == QSFP_BANKNA) {
        bank = 0;
    }
    if (page == QSFP_PAGE0_LOWER) {
        page = 0;
    }

    if (bf_qsfp_vec.pltfm_read) {
        rc = bf_qsfp_vec.pltfm_read (port, bank, page,
                                     offset, len, buf, 0, NULL);
        bf_sys_mutex_unlock (
            &bf_qsfp_info_arr[port].qsfp_mtx);
        return rc;
    }

    // For Tofino based system compatibility
    if (!bf_qsfp_is_flat_mem (port) &&
        (offset >= MAX_QSFP_PAGE_SIZE)) {
        uint8_t pg = (uint8_t)page;
        rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                         &pg);
    }

    rc |= (bf_pltfm_qsfp_read_module (port, offset,
                                      len, buf));
    bf_sys_mutex_unlock (
        &bf_qsfp_info_arr[port].qsfp_mtx);
    return rc;
}

/** write to a specific location in the memory map
 * It is recommended that bf_qsfp_field_write_onebank be used where possible and
 * bf_qsfp_module_write be used only to write vendor proprietary locations that
 * are not defined by the MSAs
 *
 *  @param port
 *   port
 *  @param bank
 *   bank number, 0 if none
 *  @param page
 *   page number
 *  @param offset
 *    offset into QSFP memory to write into
 *  @param len
 *    number of bytes to write
 *  @param buf
 *    buffer to write
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_module_write (
    unsigned int port, int bank, int page, int offset,
    int len, uint8_t *buf)
{
    int rc = 0;
    if (port > BF_PLAT_MAX_QSFP) {
        return -1;
    }

    bf_sys_mutex_lock (
        &bf_qsfp_info_arr[port].qsfp_mtx);
    // safe override
    if (bank == QSFP_BANKNA) {
        bank = 0;
    }
    if (page == QSFP_PAGE0_LOWER) {
        page = 0;
    }
    if (bf_qsfp_vec.pltfm_write) {
        rc = bf_qsfp_vec.pltfm_write (port, bank, page,
                                      offset, len, buf, 0, NULL);
        bf_sys_mutex_unlock (
            &bf_qsfp_info_arr[port].qsfp_mtx);
        return rc;
    }

    // For Tofino based system compatibility
    if (!bf_qsfp_is_flat_mem (port) &&
        (offset >= MAX_QSFP_PAGE_SIZE) &&
        (!bf_qsfp_info_arr[port].cache_dirty)) {
        uint8_t pg = (uint8_t)page;
        rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                         &pg);
    }

    rc |= (bf_pltfm_qsfp_write_module (port, offset,
                                       len, buf));
    bf_sys_mutex_unlock (
        &bf_qsfp_info_arr[port].qsfp_mtx);
    return rc;
}

/** read the memory of qsfp  transceiver
 *
 *  @param port
 *   port
 *  @param offset
 *    offset into QSFP memory to read from
 *  @param len
 *    number of bytes to read
 *  @param buf
 *    buffer to read into
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_read_transceiver (unsigned int port,
                              int offset,
                              int len,
                              uint8_t *buf)
{
#if 0
    // For Tofino based system compatibility
    if (!bf_qsfp_flat_mem[port]) &&
        (offset >= MAX_QSFP_PAGE_SIZE) &&
        (!bf_qsfp_cache_dirty[port])) {
        uint8_t pg = (uint8_t)page;
        rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                       &pg);
    }
#endif
    return (bf_pltfm_qsfp_read_module (port, offset,
                                       len, buf));
}

/** write into the memory of qsfp  transceiver
 *
 *  @param port
 *   port
 *  @param offset
 *    offset into QSFP memory to write into
 *  @param len
 *    number of bytes to write
 *  @param buf
 *    buffer to write
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_write_transceiver (unsigned int port,
                               int offset,
                               int len,
                               uint8_t *buf)
{
    return (bf_pltfm_qsfp_write_module (port, offset,
                                        len, buf));
}

/* Refresh the cache for all of the per-module and per-lane flags.
 * This routine basically moves the latched flag from the module into our
 * memory, so we can process it when we are ready */
int bf_qsfp_refresh_flags (int port)
{
    int res, chmask, cur_chmask, loopcnt, loopcnt2,
        bitcnt;
    uint8_t new_module_flags[MODULE_FLAG_BYTES];
    uint8_t new_lane_flags[LANE_FLAG_BYTES];

    if (bf_qsfp_info_arr[port].passive_cu) {
        return 0;
    }

    res = bf_qsfp_field_read_onebank (
              port, MODULE_FLAGS, 0, 0, MODULE_FLAG_BYTES,
              new_module_flags);
    if (res != 0) {
        return res;
    }
    for (loopcnt2 = 0; loopcnt2 < MODULE_FLAG_BYTES;
         loopcnt2++) {
        // increment flag counters for all set flags
        for (bitcnt = 0; bitcnt < 8; bitcnt++) {
            if ((new_module_flags[loopcnt2] >> bitcnt) &
                0x1) {
                bf_qsfp_info_arr[port].module_flag_cnt[loopcnt2][bitcnt]++;
            }
        }

        bf_qsfp_info_arr[port].module_flags[loopcnt2] |=
            new_module_flags[loopcnt2];
    }

    if (bf_qsfp_is_cmis (port)) {
        chmask = (1 << bf_qsfp_info_arr[port].num_ch) - 1;
        cur_chmask = 0xFF;
        loopcnt = 0;
        while (chmask & cur_chmask) {
            res = bf_qsfp_field_read_onebank (port,
                                              CMIS_LANE_FLAGS,
                                              (chmask & cur_chmask),
                                              0,
                                              CMIS_LANE_FLAG_BYTES,
                                              new_lane_flags);
            if (res != 0) {
                return res;
            }
            for (loopcnt2 = 0;
                 loopcnt2 < CMIS_LANE_FLAG_BYTES; loopcnt2++) {
                // increment flag counters for all set flags
                for (bitcnt = 0; bitcnt < 8; bitcnt++) {
                    if ((new_lane_flags[loopcnt2] >> bitcnt) & 0x1) {
                        bf_qsfp_info_arr[port].lane_flag_cnt[loopcnt2][bitcnt]++;
                    }
                }

                bf_qsfp_info_arr[port]
                .lane_flags[ (loopcnt * CMIS_LANE_FLAG_BYTES) +
                                                              loopcnt2] |=
                                 new_lane_flags[loopcnt2];
            }
            cur_chmask <<= 8;
            loopcnt++;
        }
    } else {  // SFF-8636
        res = bf_qsfp_field_read_onebank (port,
                                          SFF8636_LANE_STATUS_FLAGS,
                                          0,
                                          0,
                                          SFF8636_LANE_STATUS_FLAG_BYTES,
                                          new_lane_flags);
        if (res != 0) {
            return res;
        }
        for (loopcnt2 = 0;
             loopcnt2 < SFF8636_LANE_STATUS_FLAG_BYTES;
             loopcnt2++) {
            // increment flag counters for all set flags
            for (bitcnt = 0; bitcnt < 8; bitcnt++) {
                if ((new_lane_flags[loopcnt2] >> bitcnt) & 0x1) {
                    bf_qsfp_info_arr[port].lane_flag_cnt[loopcnt2][bitcnt]++;
                }
            }

            bf_qsfp_info_arr[port].lane_flags[loopcnt2] |=
                new_lane_flags[loopcnt2];
        }
        res = bf_qsfp_field_read_onebank (port,
                                          SFF8636_LANE_MONITOR_FLAGS,
                                          0,
                                          0,
                                          SFF8636_LANE_MONITOR_FLAG_BYTES,
                                          new_lane_flags);
        if (res != 0) {
            return res;
        }
        for (loopcnt2 = 0;
             loopcnt2 < SFF8636_LANE_MONITOR_FLAGS;
             loopcnt2++) {
            // increment flag counters for all set flags
            for (bitcnt = 0; bitcnt < 8; bitcnt++) {
                if ((new_lane_flags[loopcnt2] >> bitcnt) & 0x1) {
                    bf_qsfp_info_arr[port].lane_flag_cnt[loopcnt2][bitcnt]++;
                }
            }

            bf_qsfp_info_arr[port]
            .lane_flags[SFF8636_LANE_STATUS_FLAG_BYTES +
                                                       loopcnt2] |=
                            new_lane_flags[loopcnt2];
        }
    }
    return res;
}

/* return sff_flag_map_t for a given field in sff8636_flag_map[] */
static sff_flag_map_t *get_sff_flag_addr (
    const Sff_flag flag)
{
    int i, cnt = sizeof (sff8636_flag_map) / sizeof (
                     sff_flag_map_t);

    for (i = 0; i < cnt; i++) {
        if (sff8636_flag_map[i].flag == flag) {
            return (& (sff8636_flag_map[i]));
        }
    }
    return NULL;
}

/* return sff_flag_map_t for a given field in cmis_flag_map[] */
static sff_flag_map_t *get_cmis_flag_addr (
    const Sff_flag flag)
{
    int i, cnt = sizeof (cmis_flag_map) / sizeof (
                     sff_flag_map_t);

    for (i = 0; i < cnt; i++) {
        if (cmis_flag_map[i].flag == flag) {
            return (& (cmis_flag_map[i]));
        }
    }
    return NULL;
}

/* return the contents of the  sff_flag_map_t for a given flag */
static int get_sff_flag_info (int port,
                              Sff_flag flag, sff_flag_map_t **info)
{
    if (flag >= SFF_FLAG_MAX) {
        return -1;
    }
    if (bf_qsfp_is_cmis (port)) {
        *info = get_cmis_flag_addr (flag);
    } else {
        *info = get_sff_flag_addr (flag);
    }
    if (!*info) {
        return -1;
    }
    return 0;
}

/* Given a pointer to a flag_map_t flag, returns the array index in lane_flags
 * in the array_ind pointer and the bit number in the bit_num pointer.
 * If the flag is per lane, ln_num indicates the requested lane number.
 * For multi-bit flags, the desired bit number should be passed in through
 * bit_num and that same pointer will contain the actual bit number in the cache
 */
static int convert_flag_info_to_array_ind (
    int port,
    sff_flag_map_t *flag_info,
    int ln_num,
    int bit_num,
    int *array_ind,
    int *bit_ind)
{
    int ret = 0, offset;
    sff_field_info_t *flag_field_info;

    if (bf_qsfp_is_cmis (port)) {
        flag_field_info = get_cmis_field_addr (
                              flag_info->field);
    } else {  // sff-8636
        flag_field_info = get_sff_field_addr (
                              flag_info->field);
    }

    if (!flag_field_info) {
        return -1;
    }
    if (!flag_info->perlaneflag) {
        ln_num = 0;
    }

    offset = flag_field_info->offset;
    if (flag_info->field ==
        SFF8636_LANE_MONITOR_FLAGS) {
        offset -= SFF8636_LANE_STATUS_FLAG_BYTES;
    }

    // first get the lane 0 byte location
    *array_ind = flag_info->firstbyte - offset +
                 ((ln_num >> 3) * CMIS_LANE_FLAG_BYTES) +
                 (((ln_num & 0x7) * flag_info->laneincr) >>
                  3);  // add ln offset

    // now calculate the index of the requested lane
    *bit_ind = flag_info->firstbit +
               (((ln_num * flag_info->laneincr) & 0x7) *
                (flag_info->inbyteinv ? -1 : 1)) +
               bit_num;
    return ret;
}

/* clears the local cached copy of the specified flag */
/* ln = -1 is all lanes
   bit = -1 is all bits */
int bf_qsfp_clear_flag (int port, Sff_flag flag,
                        int ln, int bit, bool clrcnt)
{
    sff_flag_map_t *flag_info;
    int array_ind, bit_ind;
    int cur_bit, bitmask, cur_ln, chmask;

    if (get_sff_flag_info (port, flag,
                           &flag_info) < 0) {
        LOG_ERROR ("QSFP    %2d : flag %d not decodable",
                   port, flag);
        return -1;
    }

    if (!flag_info) {
        LOG_ERROR ("QSFP    %2d : flag %d decoded to NULL",
                   port, flag);
        return -1;
    }

    if (bit < 0) {
        cur_bit = 0;
        bitmask = (1 << flag_info->numbits) - 1;
    } else {
        cur_bit = bit;
        bitmask = 1 << bit;
    }
    while (cur_bit < flag_info->numbits) {
        if (bitmask & (1 << cur_bit)) {
            if (flag_info->perlaneflag) {
                if (ln < 0) {
                    cur_ln = 0;
                    chmask = (1 << bf_qsfp_info_arr[port].num_ch) - 1;
                } else {
                    cur_ln = ln;
                    chmask = 1 << ln;
                }
                while (cur_ln < bf_qsfp_info_arr[port].num_ch) {
                    if (chmask & (1 << cur_ln)) {
                        if (convert_flag_info_to_array_ind (
                                port, flag_info, cur_ln, cur_bit, &array_ind,
                                &bit_ind) !=
                            0) {
                            return -1;
                        }
                        // clear the flag bit
                        bf_qsfp_info_arr[port].lane_flags[array_ind] &=
                            ~ (1 << bit_ind);
                        if (clrcnt) {
                            bf_qsfp_info_arr[port].lane_flag_cnt[array_ind][bit_ind]
                                = 0;
                        }
                    }
                    cur_ln++;
                }
            } else {
                if (convert_flag_info_to_array_ind (
                        port, flag_info, 0, cur_bit, &array_ind,
                        &bit_ind) != 0) {
                    return -1;
                }
                bf_qsfp_info_arr[port].module_flags[array_ind] &=
                    ~ (1 << bit_ind);
                if (clrcnt) {
                    bf_qsfp_info_arr[port].module_flag_cnt[array_ind][bit_ind]
                        = 0;
                }
            }
        }
        cur_bit++;
    }
    return 0;
}

/* returns the last read value of the specified flag */
int bf_qsfp_get_flag (int port, Sff_flag flag,
                      int ln, int bit, uint32_t *cnt)
{
    sff_flag_map_t *flag_info;
    int array_ind, bit_ind, result;

    if (get_sff_flag_info (port, flag,
                           &flag_info) < 0) {
        LOG_ERROR ("QSFP    %2d : flag %d not decodable",
                   port, flag);
        return -1;
    }

    if (!flag_info) {
        LOG_ERROR ("QSFP    %2d : flag %d decoded to NULL",
                   port, flag);
        return -1;
    }

    if (convert_flag_info_to_array_ind (
            port, flag_info, ln, bit, &array_ind,
            &bit_ind) != 0) {
        return -1;
    }

    if (flag_info->perlaneflag) {
        result = (bf_qsfp_info_arr[port].lane_flags[array_ind]
                  >> bit_ind) & 0x1;
        if (cnt != NULL) {
            *cnt = bf_qsfp_info_arr[port].lane_flag_cnt[array_ind][bit_ind];
        }
    } else {
        result = (bf_qsfp_info_arr[port].module_flags[array_ind]
                  >> bit_ind) & 0x1;
        if (cnt != NULL) {
            *cnt = bf_qsfp_info_arr[port].module_flag_cnt[array_ind][bit_ind];
        }
    }

    return result;
}

/* return qsfp module sensor flags */
/* note that since the flags are clear on read (by spec), calling
 * this function also has the side-effect of clearing the flags */
static int get_qsfp_module_sensor_flags (int port,
        Sff_field fieldName,
        qsfp_flags_level_t *flags)
{
    uint8_t data;

    if (bf_qsfp_field_read_onebank (port, fieldName,
                                    0, 0, 1, &data) < 0) {
        return -1;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        if (fieldName == TEMPERATURE_ALARMS) {
            bf_qsfp_info_arr[port].idprom[6] = data;
        } else if (fieldName == VCC_ALARMS) {
            bf_qsfp_info_arr[port].idprom[7] = data;
        }
    }
    return extract_qsfp_flags (fieldName, port, &data,
                               0, flags);
}

static double get_qsfp_module_sensor (int port,
                                      Sff_field fieldName,
                                      qsfp_conversion_fn con_fn)
{
    uint8_t data[2];

    if (bf_qsfp_field_read_onebank (port, fieldName,
                                    0, 0, 2, data) < 0) {
        return 0.0;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis(port))  {
        /* CMIS */
        if (fieldName == TEMPERATURE) {
            bf_qsfp_info_arr[port].idprom[14] = data[0];
            bf_qsfp_info_arr[port].idprom[15] = data[1];
        } else if (fieldName == VCC) {
            bf_qsfp_info_arr[port].idprom[16] = data[0];
            bf_qsfp_info_arr[port].idprom[17] = data[1];
        }
    } else {
        /* SFF-8636 */
        if (fieldName == TEMPERATURE) {
            bf_qsfp_info_arr[port].idprom[22] = data[0];
            bf_qsfp_info_arr[port].idprom[23] = data[1];
        } else if (fieldName == VCC) {
            bf_qsfp_info_arr[port].idprom[26] = data[0];
            bf_qsfp_info_arr[port].idprom[27] = data[1];
        }
    }
    return con_fn (data[0] << 8 | data[1]);
}

/* return the string of the sff field */
/* replaces trailing spaces with null terminators */
static void get_qsfp_str (int port,
                          Sff_field field,
                          char *buf,
                          size_t buf_size)
{
    int lastchar;
    if (bf_qsfp_field_read_onebank (
            port, field, 0, 0, buf_size - 1,
            (uint8_t *)buf) < 0) {
        return;
    }

    // replace trailing spaces with null characters. The char array is
    // intentionally over-sized by one, so always make the last char a
    // null terminator
    lastchar = buf_size - 1;
    do {
        buf[lastchar] = '\0';
    } while ((lastchar > 0) &&
             (buf[lastchar] == ' '));
}

/*
 * Cable length is report as a single byte;  each field has a
 * specific multiplier to use to get the true length.  For instance,
 * single mode fiber length is specified in km, so the multiplier
 * is 1000.  In addition, the raw value of 255 indicates that the
 * cable is longer than can be represented.  We use a negative
 * value of the appropriate magnitude to communicate that to thrift
 * clients.
 */
static float get_qsfp_cable_length (int port,
                                    Sff_field fieldName)
{
    uint8_t data;
    float multiplier, length;

    if (bf_qsfp_field_read_onebank (port, fieldName,
                                    0, 0, 1, &data) < 0) {
        return 0.0;
    }

    multiplier = get_qsfp_multiplier (fieldName);
    if ((bf_qsfp_is_cmis (port)) &&
        (fieldName == LENGTH_CBLASSY)) {
        // multiplier is built into upper 2 bits of memory map data
        multiplier = pow (10, (data >> 6) - 1);
        data &= 0x1F;
    } else {
        multiplier = get_qsfp_multiplier (fieldName);
    }

    length = data * multiplier;
    if (data == MAX_CABLE_LEN) {
        length = - (MAX_CABLE_LEN - 1) * multiplier;
    }
    return length;
}

/*
 * Threhold values are stored just once;  they aren't per-channel,
 * so in all cases we simple assemble two-byte values and convert
 * them based on the type of the field.
 */
static int get_threshold_values (int port,
                                 Sff_field field,
                                 qsfp_threshold_level_t *thresh,
                                 qsfp_conversion_fn con_fn)
{
    uint8_t data[8];

    if (bf_qsfp_is_passive_cu (port)) {
        return -1;
    }
    if (bf_qsfp_field_read_onebank (port, field, 0, 0,
                                    8, data) < 0) {
        return -1;
    }

    thresh->alarm.high = con_fn ((data[0] << 8) |
                                 data[1]);
    thresh->alarm.low = con_fn ((data[2] << 8) |
                                data[3]);
    thresh->warn.high = con_fn ((data[4] << 8) |
                                data[5]);
    thresh->warn.low = con_fn ((data[6] << 8) |
                               data[7]);

    return 0;
}

/** return connector type identifier number
 *
 *  @param port
 *   port
 *  @param module_type
 *   returned moudule type (identifier)
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_connector_type (int port,
                             uint8_t *connector_type)
{
    return bf_qsfp_field_read_onebank (port,
                                        CONNECTOR_TYPE, 0, 0, 1, connector_type);
    return 0;
}

/** return module type identifier number
 *
 *  @param port
 *   port
 *  @param module_type
 *   returned moudule type (identifier)
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_module_type (int port,
                             uint8_t *module_type)
{
    return bf_qsfp_field_read_onebank (port,
                                       IDENTIFIER, 0, 0, 1, module_type);
}

/** return module type identifier string
 *
 *  @param port
 *   port
 *  @param module_type_str
 *   returned moudule type string (identifier)
 *  @param incl_hex
 *   when true, parenthetical hex value is appended to the string
 *  @return
 *   true if supported in this module, false if not
 */
bool bf_qsfp_get_module_type_str (int port,
                                  char *module_type_str,
                                  bool incl_hex)
{
    uint8_t module_type;

    if (bf_qsfp_get_module_type (port,
                                 &module_type) >= 0) {
        switch (module_type) {
            case QSFP:
                strcpy (module_type_str, "QSFP");
                break;
            case QSFP_PLUS:
            case QSFP_CMIS:  // QSFP+ can be CMIS
                strcpy (module_type_str, "QSFP+");
                break;
            case QSFP_28:
                strcpy (module_type_str, "QSFP28");
                break;
            case QSFP_DD:
                strcpy (module_type_str, "QSFP-DD");
                break;
            case OSFP:
                strcpy (module_type_str, "OSFP");
                break;
            default:
                strcpy (module_type_str, "UNKNOWN");
                break;
        }
    } else {
        strcpy (module_type_str, "ERROR");
    }

    if (incl_hex) {
        char buf[10];
        sprintf (buf, " (0x%02x)", module_type);
        strcat (module_type_str, buf);
    }
    return true;
}

/** return qsfp vendor information
 *
 *  @param port
 *   port
 *  @param vendor
 *   qsfp vendor information struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_vendor_info (int port,
                              qsfp_vendor_info_t *vendor)
{
    get_qsfp_str (port, VENDOR_NAME, vendor->name,
                  sizeof (vendor->name));
    bf_qsfp_field_read_onebank (port, VENDOR_OUI, 0,
                                0, 3, (uint8_t *)&vendor->oui[0]);
    get_qsfp_str (
        port, PART_NUMBER, vendor->part_number,
        sizeof (vendor->part_number));
    get_qsfp_str (port, REVISION_NUMBER, vendor->rev,
                  sizeof (vendor->rev));
    get_qsfp_str (port,
                  VENDOR_SERIAL_NUMBER,
                  vendor->serial_number,
                  sizeof (vendor->serial_number));
    get_qsfp_str (port, MFG_DATE, vendor->date_code,
                  sizeof (vendor->date_code));
    return true;
}

/** return qsfp alarm threshold information
 *
 *  @param port
 *   port
 *  @param thresh
 *   qsfp alarm threshold struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_threshold_info (int port,
                                 qsfp_alarm_threshold_t *thresh)
{
    int rc;

    if (bf_qsfp_is_passive_cu (port)) {
        return false;
    }
    rc = get_threshold_values (port,
                               TEMPERATURE_THRESH, &thresh->temp, get_temp);
    rc |= get_threshold_values (port, VCC_THRESH,
                                &thresh->vcc, get_vcc);
    rc |= get_threshold_values (port, RX_PWR_THRESH,
                                &thresh->rx_pwr, get_pwr);
    rc |=
        get_threshold_values (port, TX_BIAS_THRESH,
                              &thresh->tx_bias, get_txbias);
    rc |= get_threshold_values (port, TX_PWR_THRESH,
                                &thresh->tx_pwr, get_pwr);
    return (rc ? false : true);
}

int bf_qsfp_get_threshold_temp (int port,
                                qsfp_threshold_level_t *thresh)
{
    return get_threshold_values (port,
                                 TEMPERATURE_THRESH, thresh, get_temp);
}

int bf_qsfp_get_threshold_vcc (int port,
                               qsfp_threshold_level_t *thresh)
{
    return get_threshold_values (port, VCC_THRESH,
                                 thresh, get_vcc);
}

int bf_qsfp_get_threshold_rx_pwr (int port,
                                  qsfp_threshold_level_t *thresh)
{
    return get_threshold_values (port, RX_PWR_THRESH,
                                 thresh, get_pwr);
}

int bf_qsfp_get_threshold_tx_bias (int port,
                                   qsfp_threshold_level_t *thresh)
{
    return get_threshold_values (port, TX_BIAS_THRESH,
                                 thresh, get_txbias);
}

/** return qsfp sensor information
 *
 *  @param port
 *   port
 *  @param info
 *   qsfp global sensor struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_module_sensor_info (int port,
                                     qsfp_global_sensor_t *info)
{
    info->temp.value = get_qsfp_module_sensor (port,
                       TEMPERATURE, get_temp);
    info->temp._isset.value = true;
    get_qsfp_module_sensor_flags (port,
                                  TEMPERATURE_ALARMS, & (info->temp.flags));
    info->temp._isset.flags = true;
    info->vcc.value = get_qsfp_module_sensor (port,
                      VCC, get_vcc);
    info->vcc._isset.value = true;
    get_qsfp_module_sensor_flags (port, VCC_ALARMS,
                                  & (info->vcc.flags));
    info->vcc._isset.flags = true;
    return true;
}

/** return qsfp temp sensor information
 *
 *  @param port
 *   port
 *  @return
 *   qsfp temp in degrees C
 */
double bf_qsfp_get_temp_sensor (int port)
{
    if ((!bf_qsfp_get_reset(port)) &&
            bf_qsfp_is_present((port)) &&
            bf_qsfp_is_optical((port)) && 1/* FSM_ST_DETECTED */) {
        bf_qsfp_info_arr[port].module_temper_cache =
            get_qsfp_module_sensor (port, TEMPERATURE,
                                    get_temp);
        return bf_qsfp_info_arr[port].module_temper_cache;
    }
    return 0;
}

/** return qsfp cable information
 *
 *  @param port
 *   port
 *  @param cable
 *   qsfp cable struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_cable_info (int port,
                             qsfp_cable_t *cable)
{
    cable->_isset.cable_assy = false;
    cable->_isset.xcvr_om1 = false;
    cable->_isset.xcvr_om2 = false;
    cable->_isset.xcvr_om3 = false;
    cable->_isset.xcvr_om4 = false;
    cable->_isset.xcvr_om5 = false;
    cable->_isset.xcvr_single_mode = false;

    cable->is_cable_assy = bf_qsfp_is_cable_assy (
                               port);

    if ((!bf_qsfp_is_passive_cu (port)) &&
        (!cable->is_cable_assy)) {
        cable->xcvr_single_mode =
            (int32_t)get_qsfp_cable_length (port,
                                            LENGTH_SM_KM);
        cable->_isset.xcvr_single_mode =
            (cable->xcvr_single_mode != 0);
        if (bf_qsfp_is_cmis (port)) {
            cable->xcvr_om5 = (int32_t)get_qsfp_cable_length (
                                  port, LENGTH_OM5);
            cable->_isset.xcvr_om5 = (cable->xcvr_om5 != 0);
            cable->xcvr_om4 = (int32_t)get_qsfp_cable_length (
                                  port, LENGTH_OM4);
            cable->_isset.xcvr_om4 = (cable->xcvr_om4 != 0);
        }
        cable->xcvr_om3 = (int32_t)get_qsfp_cable_length (
                              port, LENGTH_OM3);
        cable->_isset.xcvr_om3 = (cable->xcvr_om3 != 0);
        cable->xcvr_om2 = (int32_t)get_qsfp_cable_length (
                              port, LENGTH_OM2);
        cable->_isset.xcvr_om2 = (cable->xcvr_om2 != 0);
        if (!bf_qsfp_is_cmis (port)) {
            cable->xcvr_om1 = (int32_t)get_qsfp_cable_length (
                                  port, LENGTH_OM1);
            cable->_isset.xcvr_om1 = (cable->xcvr_om1 != 0);
        }
    } else {
        cable->cable_assy = get_qsfp_cable_length (port,
                            LENGTH_CBLASSY);
        cable->_isset.cable_assy =
            true;  // 0m means < 1m in some cases
    }
    return (cable->_isset.cable_assy ||
            cable->_isset.xcvr_om1 ||
            cable->_isset.xcvr_om2 ||
            cable->_isset.xcvr_om3 ||
            cable->_isset.xcvr_om4 ||
            cable->_isset.xcvr_om5 ||
            cable->_isset.xcvr_single_mode);
}

/** return cmis module state information
 *
 *  @param port
 *   port
 *  @param module_state_info
 *   qsfp_module_state_info_t struct pointer for module state info
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_module_state_info (
    int port, qsfp_module_state_info_t
    *module_state_info)
{
    uint8_t data;
    if (!bf_qsfp_is_cmis (port)) {
        return false;
    }

    if (bf_qsfp_field_read_onebank (port,
                                    MODULE_STATE, 0, 0, 1, &data) < 0) {
        return false;
    }
    module_state_info->module_state = (data >> 1) &
                                      0x7;
    switch (module_state_info->module_state) {
        case MODULE_ST_LOWPWR:
            strcpy (module_state_info->module_state_str,
                    "ModuleLowPwr");
            break;
        case MODULE_ST_PWRUP:
            strcpy (module_state_info->module_state_str,
                    "ModulePwrUp");
            break;
        case MODULE_ST_READY:
            strcpy (module_state_info->module_state_str,
                    "ModuleReady");
            break;
        case MODULE_ST_PWRDN:
            strcpy (module_state_info->module_state_str,
                    "ModulePwrDn");
            break;
        case MODULE_ST_FAULT:
            strcpy (module_state_info->module_state_str,
                    "Fault");
            break;
        default:
            strcpy (module_state_info->module_state_str,
                    "Unknown");
            break;
    }

    if (bf_qsfp_field_read_onebank (port,
                                    POWER_CONTROL, 0, 0, 1, &data) < 0) {
        return false;
    }
    module_state_info->forceLowPwr = (data >> 4) &
                                     0x1;
    module_state_info->lowPwr = (data >> 6) & 0x1;
    return true;
}

/** return cmis datapath state information
 *
 *  @param port
 *   port
 *  @param dp_state_info
 *   qsfp_datapath_state_info_t struct pointer for data path state info
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_dp_state_info (int port,
                                int num_ch,
                                qsfp_datapath_state_info_t *dp_state_info)
{
    uint8_t dp_state[4];
    uint8_t dp_deinit;
    int cur_ch, sub_ch;
    int chmask = 0xFFFF >> (16 - num_ch);
    int curbank_mask = 0xFF;

    if (!bf_qsfp_is_cmis (port)) {
        return false;
    }

    for (cur_ch = 0; cur_ch < num_ch; cur_ch += 8) {
        if (bf_qsfp_field_read_onebank (
                port, DATAPATH_STATE_ALL, (chmask & curbank_mask),
                0, 4, dp_state) <
            0) {
            return false;
        }
        if (bf_qsfp_field_read_onebank (
                port, DATAPATH_DEINIT, (chmask & curbank_mask), 0,
                1, &dp_deinit) <
            0) {
            return false;
        }
        for (sub_ch = cur_ch; ((sub_ch < num_ch) &&
                               (sub_ch < cur_ch + 8));
             sub_ch++) {
            dp_state_info[sub_ch].data_path_deinit_bit =
                (dp_deinit >> (sub_ch % 8)) & 0x1;
            dp_state_info[sub_ch].datapath_state =
                (dp_state[sub_ch >> 1] >> (4 * (sub_ch & 0x1))) &
                0xF;
            switch (dp_state_info[sub_ch].datapath_state) {
                case DATAPATH_ST_DEACTIVATED:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPDeactivated(or unused)");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "DAct");
                    break;
                case DATAPATH_ST_INIT:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPInit");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "Init");
                    break;
                case DATAPATH_ST_DEINIT:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPDeinit");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "DInt");
                    break;
                case DATAPATH_ST_ACTIVATED:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPActivated");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "Act");
                    break;
                case DATAPATH_ST_TXTURNON:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPTxTurnOn");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "TOn");
                    break;
                case DATAPATH_ST_TXTURNOFF:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPTxTurnOff");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "TOff");
                    break;
                case DATAPATH_ST_INITIALIZED:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "DPInitialized");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "Ized");
                    break;
                default:
                    strcpy (dp_state_info[sub_ch].datapath_state_str,
                            "Unknown");
                    strcpy (dp_state_info[sub_ch].datapath_state_str_short,
                            "Unk");
                    break;
            }
        }
        curbank_mask <<= 8;
    }
    return true;
}

bool bf_qsfp_get_dp_state_config (int port,
                                  int num_ch,
                                  qsfp_datapath_staged_set_info_t *set_cfg,
                                  Control_Sets cur_set)
{
    uint8_t data[33];
    Sff_field field_name = 0;
    int cur_ch, sub_ch;
    int chmask = 0xFFFF >> (16 - num_ch);
    int curbank = 0xFF;

    if (!bf_qsfp_is_cmis (port)) {
        return false;
    }

    switch (cur_set) {
        case ACTIVE_SET:
            field_name = DATAPATH_CFG_ALL_ACTIVE_SET;
            break;
        case STAGED_SET0:
            field_name = DATAPATH_CFG_ALL_STAGED_SET0;
            break;
        case STAGED_SET1:
            field_name = DATAPATH_CFG_ALL_STAGED_SET1;
            break;
    }

    for (cur_ch = 0; cur_ch < num_ch; cur_ch += 8) {
        if (bf_qsfp_field_read_onebank (
                port, field_name, (chmask & curbank), 0,
                sizeof (data), data) < 0) {
            return false;
        }
        for (sub_ch = cur_ch; ((sub_ch < num_ch) &&
                               (sub_ch < cur_ch + 8));
             sub_ch++) {
            set_cfg[sub_ch].selected_ApSel_code = data[sub_ch
                                                  % 8] >> 4;
            set_cfg[sub_ch].data_path_id = (data[sub_ch % 8]
                                            >> 1) & 0x7;
            set_cfg[sub_ch].explicit_control = data[sub_ch %
                                                    8] & 0x1;
        }
        curbank <<= 8;
    }
    return true;
}

/*
 * Iterate through all channels collecting appropriate data
 */
/** return per channel sensor information for all channels in a port,
 *  even if they span multiple banks
 *
 *  @param port
 *   port
 *  @param chn
 *   qsfp channel struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_chan_sensor_data (int port,
                                   qsfp_channel_t *chn)
{
    if (!bf_qsfp_get_chan_tx_bias (port, chn)) {
        LOG_WARNING ("%s: bf_qsfp_get_chan_tx_bias(port=%d) failed",
                   __func__, port);
        return false;
    }

    if (!bf_qsfp_get_chan_tx_bias_alarm (port, chn)) {
        LOG_WARNING (
            "%s: bf_qsfp_get_chan_tx_bias_alarm(port=%d) failed",
            __func__, port);
        return false;
    }

    if (!bf_qsfp_get_chan_tx_pwr (port, chn)) {
        LOG_WARNING ("%s: bf_qsfp_get_chan_tx_pwr(port=%d) failed",
                   __func__, port);
        return false;
    }

    if (!bf_qsfp_get_chan_tx_pwr_alarm (port, chn)) {
        LOG_WARNING (
            "%s: bf_qsfp_get_chan_tx_pwr_alarm(port=%d) failed",
            __func__, port);
        return false;
    }

    if (!bf_qsfp_get_chan_rx_pwr (port, chn)) {
        LOG_WARNING ("%s: bf_qsfp_get_chan_rx_pwr(port=%d) failed",
                   __func__, port);
        return false;
    }

    if (!bf_qsfp_get_chan_rx_pwr_alarm (port, chn)) {
        LOG_WARNING (
            "%s: bf_qsfp_get_chan_rx_pwr_alarm(port=%d) failed",
            __func__, port);
        return false;
    }

    return true;
}

static int bf_qsfp_chmask_get (int port)
{
    if (bf_qsfp_info_arr[port].num_ch == 4) {
        return 0xF;
    } else if (bf_qsfp_info_arr[port].num_ch >= 8) {
        return 0xFF;
    }
    return 0;
}

bool bf_qsfp_get_chan_tx_bias (int port,
                               qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (port,
                                    CHANNEL_TX_BIAS, chmask, 0, 16, data) <
        0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        /* Currently only update the first eight bytes to support SFF-8636.
         * Support for CMIS will be added in the future.
         * by sunzheng, 2023-08-04. */
        for (i=0; i < 8; i++) {
            bf_qsfp_info_arr[port].idprom[i + 42] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            uint16_t val = data[i * 2] << 8 | data[ (i * 2) +
                                                    1];
            chn[i].sensors.tx_bias.value = get_txbias (val);
            chn[i].sensors.tx_bias._isset.value = true;
        }
    }
    return true;
}

bool bf_qsfp_get_chan_tx_pwr (int port,
                              qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (port,
                                    CHANNEL_TX_PWR, chmask, 0, 16, data) <
        0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        for (i=0; i < 8; i++) {
            bf_qsfp_info_arr[port].idprom[i + 50] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            uint16_t val = data[i * 2] << 8 | data[ (i * 2) +
                                                    1];
            /* Convert to dBm. */
            chn[i].sensors.tx_pwr.value = ((val == 0) ? (-40 * 1.0) : (get_pwr (val)));
            chn[i].sensors.tx_pwr._isset.value = true;
        }
    }
    return true;
}

bool bf_qsfp_get_chan_rx_pwr (int port,
                              qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (port,
                                    CHANNEL_RX_PWR, chmask, 0, 16, data) <
        0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        for (i=0; i < 8; i++) {
            bf_qsfp_info_arr[port].idprom[i + 34] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            uint16_t val = data[i * 2] << 8 | data[ (i * 2) +
                                                    1];
            /* Convert to dBm. */
            chn[i].sensors.rx_pwr.value = ((val == 0) ? (-40 * 1.0) : (get_pwr (val)));
            chn[i].sensors.rx_pwr._isset.value = true;
        }
    }
    return true;
}

bool bf_qsfp_get_chan_tx_bias_alarm (int port,
                                     qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (
            port, CHANNEL_TX_BIAS_ALARMS, chmask, 0, 16,
            data) < 0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        for (i = 0; i < 2; i ++) {
            bf_qsfp_info_arr[port].idprom[i + 11] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            extract_qsfp_flags (
                CHANNEL_TX_BIAS_ALARMS, port, data, i,
                &chn[i].sensors.tx_bias.flags);
            chn[i].sensors.tx_bias._isset.flags = true;
        }
    }
    return true;
}

bool bf_qsfp_get_chan_tx_pwr_alarm (int port,
                                    qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (
            port, CHANNEL_TX_PWR_ALARMS, chmask, 0, 16,
            data) < 0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        for (i = 0; i < 2; i++) {
            bf_qsfp_info_arr[port].idprom[i + 13] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            extract_qsfp_flags (
                CHANNEL_TX_PWR_ALARMS, port, data, i,
                &chn[i].sensors.tx_pwr.flags);
            chn[i].sensors.tx_pwr._isset.flags = true;
        }
    }
    return true;
}

bool bf_qsfp_get_chan_rx_pwr_alarm (int port,
                                    qsfp_channel_t *chn)
{
    uint8_t data[16];
    int i;
    int chmask = bf_qsfp_chmask_get (port);

    if (!chmask) {
        LOG_WARNING ("%s(port=%d): not supported channel count(%d)",
                   __func__,
                   port,
                   bf_qsfp_info_arr[port].num_ch);
    }

    if (bf_qsfp_field_read_onebank (
            port, CHANNEL_RX_PWR_ALARMS, chmask, 0, 16,
            data) < 0) {
        LOG_WARNING ("%s: bf_qsfp_field_read_onebank(port=%d,chmask=%02x) failed",
                   __func__,
                   port,
                   chmask);
        return false;
    }

    /* Update cache. */
    if (bf_qsfp_is_cmis (port)) {
        /* TBD */
    } else {
        /* SFF-8636 */
        for (i = 0; i < 2; i++) {
            bf_qsfp_info_arr[port].idprom[i + 9] = data[i];
        }

        for (i = 0; i < bf_qsfp_info_arr[port].num_ch &&
             i < MAX_CHAN_PER_CONNECTOR;
             i++) {
            extract_qsfp_flags (
                CHANNEL_RX_PWR_ALARMS, port, data, i,
                &chn[i].sensors.rx_pwr.flags);
            chn[i].sensors.rx_pwr._isset.flags = true;
        }
    }
    return true;
}

int bf_qsfp_get_flag_for_all_chans (int port,
                                    Sff_flag flag, int *flag_val)
{
    int n, v;

    for (n = 0; n < bf_qsfp_info_arr[port].num_ch &&
         n < MAX_CHAN_PER_CONNECTOR;
         ++n) {
        v = bf_qsfp_get_flag (port, flag, n, 0, NULL);

        if (v < 0) {
            return -1;
        }

        flag_val[n] = v;
    }

    return n;
}

double bf_qsfp_get_voltage (int port)
{
    if ((!bf_qsfp_get_reset(port)) &&
                bf_qsfp_is_present((port)) &&
                bf_qsfp_is_optical((port)) && 1/* FSM_ST_DETECTED */) {
        return get_qsfp_module_sensor (port, VCC,
                                   get_vcc);
    }
    return 0;
}

/* returns the active and inactive firmware versions */
bool bf_qsfp_get_firmware_ver (int port,
                               uint8_t *active_ver_major,
                               uint8_t *active_ver_minor,
                               uint8_t *inactive_ver_major,
                               uint8_t *inactive_ver_minor)
{
    uint8_t active_ver_data[2] = {0},
                                 inactive_ver_data[2] = {0};
    *active_ver_major = 0;
    *active_ver_minor = 0;
    *inactive_ver_major = 0;
    *inactive_ver_minor = 0;

    if (!bf_qsfp_is_cmis (port)) {
        return false;  // sff-8636 does not currently support
    }

    if (bf_qsfp_info_arr[port].memmap_format ==
        MMFORMAT_CMIS3P0) {
        // CMIS 3.0 and earlier did not comprehend inactive and active versions
        // everything was stored in pg1 byte 128-129, which in later CMIS
        // versions became the Inactive Version field
        if (bf_qsfp_field_read_onebank (
                port, FIRMWARE_VER_INACTIVE, 0, 0, 2,
                active_ver_data) < 0) {
            return false;
        }
    } else {  // 4.0 and later
        if (bf_qsfp_field_read_onebank (
                port, FIRMWARE_VER_ACTIVE, 0, 0, 2,
                active_ver_data) < 0) {
            return false;
        }

        if (bf_qsfp_field_read_onebank (
                port, FIRMWARE_VER_INACTIVE, 0, 0, 2,
                inactive_ver_data) < 0) {
            return false;
        }
    }
    *active_ver_major = active_ver_data[0];
    *active_ver_minor = active_ver_data[1];
    *inactive_ver_major = inactive_ver_data[0];
    *inactive_ver_minor = inactive_ver_data[1];
    return true;
}

/* returns the hardware version */
bool bf_qsfp_get_hardware_ver (int port,
                               uint8_t *hw_ver_major,
                               uint8_t *hw_ver_minor)
{
    uint8_t hw_ver_data[2];
    *hw_ver_major = 0;
    *hw_ver_minor = 0;

    if ((!bf_qsfp_is_cmis (port)) ||
        (bf_qsfp_is_flat_mem (port))) {
        return false;  // sff-8636 does not currently support
    }

    if (bf_qsfp_field_read_onebank (port,
                                    HARDWARE_VER, 0, 0, 2, hw_ver_data) <
        0) {
        return false;
    }

    *hw_ver_major = hw_ver_data[0];
    *hw_ver_minor = hw_ver_data[1];
    return true;
}

/** clear the contents of qsfp port idprom cached info
 *
 *  @param port
 *   port
 */
static void bf_qsfp_idprom_clr (int port)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    memset (& (bf_qsfp_info_arr[port].idprom[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page0[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page1[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page2[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page3[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page17[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page18[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page19[0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_info_arr[port].page47[0]), 0,
            MAX_QSFP_PAGE_SIZE);

    bf_qsfp_info_arr[port].cache_dirty = true;
}

/** set number of QSFP ports on the system
 *
 *  @param num_ports
 */
void bf_qsfp_set_num (int num_ports)
{
    int i;
    if (num_ports > BF_PLAT_MAX_QSFP) {
        LOG_ERROR ("%s QSFP init %d max port set failed \n",
                   __func__, num_ports);
        return;
    }
    for (i = 0; i <= BF_PLAT_MAX_QSFP; i++) {
        bf_sys_mutex_init (&bf_qsfp_info_arr[i].qsfp_mtx);
    }
    bf_plt_max_qsfp = num_ports;
}

/** Get number of QSFP ports on the system
 *
 *  @param num_ports
 */
int bf_qsfp_get_max_qsfp_ports (void)
{
    return bf_plt_max_qsfp;
}

/** init qsfp data structure
 *
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_init (void)
{
    int port;

    for (port = 0; port <= BF_PLAT_MAX_QSFP; port++) {
        bf_qsfp_idprom_clr (port);
        bf_qsfp_info_arr[port].present = false;
        bf_qsfp_info_arr[port].reset = false;
        bf_qsfp_info_arr[port].flat_mem = true;
        bf_qsfp_info_arr[port].passive_cu = false;
        bf_qsfp_info_arr[port].fsm_detected = false;
        bf_qsfp_info_arr[port].num_ch = 0;
        bf_qsfp_info_arr[port].memmap_format =
            MMFORMAT_UNKNOWN;
        bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs
            = false;
        bf_qsfp_info_arr[port].rxlos_debounce_cnt =
            QSFP_RXLOS_DEBOUNCE_DFLT;
        bf_qsfp_info_arr[port].checksum = true;

        /* Special case routine. by tsihang, 2023-05-11. */
        bf_qsfp_info_arr[port].special_case_port.qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        bf_qsfp_info_arr[port].special_case_port.is_set = false;
        /* Keep the bf_qsfp_info_arr[port].special_case_port.ctrlmask never cleanup for Stratum during warm init.
         * As it will no chance to load them from /etc/transceiver-cases.conf to bf_pltfm_special_transceiver_case.
         * Added by tsihang, 2023-05-17. */
        //bf_qsfp_info_arr[port].special_case_port.ctrlmask = 0;
        memset (&bf_qsfp_info_arr[port].special_case_port.vendor[0], 0, 16);
        memset (&bf_qsfp_info_arr[port].special_case_port.pn[0], 0, 16);
        //bf_qsfp_info_arr[port].special_case_port.ctrlmask = BF_TRANS_CTRLMASK_LASER_OFF;
    }
    return 0;
}

/** deinit or init qsfp data structure per port
 *
 *  @param port
 *   port
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_port_deinit (int port)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    bf_qsfp_idprom_clr (port);
    bf_qsfp_info_arr[port].present = false;
    bf_qsfp_info_arr[port].reset = false;
    bf_qsfp_info_arr[port].flat_mem = true;
    bf_qsfp_info_arr[port].passive_cu = false;
    bf_qsfp_info_arr[port].num_ch = 0;
    bf_qsfp_info_arr[port].memmap_format =
        MMFORMAT_UNKNOWN;
    bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs
        = false;

#if defined (APP_ALLOC)
    if (bf_qsfp_info_arr[port].app_list) {
        bf_sys_free (bf_qsfp_info_arr[port].app_list);
        bf_qsfp_info_arr[port].app_list = NULL;
    }
#else
    for (int app = 0; app < BF_QSFP_MAX_APPLICATIONS; app ++) {
        memset (&bf_qsfp_info_arr[port].app_list[app], 0, sizeof (bf_pm_qsfp_apps_t));
        bf_qsfp_info_arr[port].num_apps = 0;
    }
#endif
    return 0;
}

/** does QSFP module support pages
 *
 *  @param port
 *   port
 *  @return
 *   true  if the module supports pages, false otherwise
 */
bool bf_qsfp_has_pages (int port)
{
    return !bf_qsfp_info_arr[port].flat_mem;
}

/** disable qsfp transmitter
 *
 *  @param port
 *   port
 *  @param channel
 *   channel within a port (0-3)
 *  @param disable
 *   disable true: to disable a channel on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_tx_disable_single_lane (int port,
                                    int channel, bool disable)
{
    uint8_t data = 0;
    int rc = 0;
    int max_media_ch;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    max_media_ch = bf_qsfp_get_media_ch_cnt (port);

    // Expect channel range check to be done by the caller.
    // Don't add it here.

    LOG_DEBUG ("QSFP    %2d: TX_DISABLE, ch=%d, new value=%d",
               port,
               channel,
               disable ? 1 : 0);

    /* read the current value and change qsfp memory if needed
     * using read/modify/write
     */

    if (disable && max_media_ch) {
        data = 1 << (channel % max_media_ch);
    }
    rc = bf_qsfp_bitfield_rmw_onebank (
             port, CHANNEL_TX_DISABLE, (1 << channel),
             (1 << channel), data);

    bf_sys_usleep (400000);

    return rc;
}

/** disable qsfp transmitter
 *
 *  @param port
 *   port
 *  @param bit-mask of channels (0x01 - 0x0F) within a port
 *  @param disable
 *   disable true: to disable a channel on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_tx_disable (int port,
                        int channel_mask, bool disable)
{
    int new_st = 0;
    int rc = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    // Expect channel_mask range check to be done by the caller.
    // Don't add it here.

    LOG_DEBUG ("QSFP    %2d : TX_DISABLE, mask=0x%x, new value=%d",
               port,
               channel_mask,
               disable ? 1 : 0);

    if (disable) {  // no need to change new_st if we are enabling
        new_st = channel_mask;
    }

    rc = bf_qsfp_bitfield_rmw (
             port, CHANNEL_TX_DISABLE, channel_mask,
             channel_mask, new_st);
    bf_sys_usleep (400000);

    return rc;
}

/** disable qsfp cdr
 *
 *  @param port
 *   port
 *  @param channel
 *   channel within a port (0-3)
 *  @param disable
 *   disable true: to disable cdr for a channel on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_cdr_disable_single_lane (int port,
                                    int channel, bool disable)
{
    uint8_t data = 0xFF;
    int rc = 0;
    int max_media_ch;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    max_media_ch = bf_qsfp_get_media_ch_cnt (port);

    // Expect channel range check to be done by the caller.
    // Don't add it here.

    LOG_DEBUG ("QSFP    %2d: CDR_CONTROL, ch=%d, new value=%d",
               port,
               channel,
               disable ? 0 : 1);

    /* read the current value and change qsfp memory if needed
     * using read/modify/write
     */

    if (disable && max_media_ch) {
        data &= ~(1 << (channel % max_media_ch));
    }
    rc = bf_qsfp_bitfield_rmw_onebank (
             port, CDR_CONTROL, (1 << channel),
             (1 << channel), data);

    bf_sys_usleep (400000);

    return rc;
}

/** disable qsfp cdr
 *
 *  @param port
 *   port
 *  @param bit-mask of channels (0x01 - 0x0F) within a port
 *  @param disable
 *   disable true: to disable cdr for all channels on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_cdr_disable (int port,
                        int channel_mask, bool disable)
{
    int new_st = 0xFF;
    int rc = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    // Expect channel_mask range check to be done by the caller.
    // Don't add it here.

    LOG_DEBUG ("QSFP    %2d : Setting CDR_CONTROL, mask=0x%x, new value=%d",
               port,
               channel_mask,
               disable ? 0 : 1);

    if (disable) {  // no need to change new_st if we are enabling
        new_st = 0x00;
    }

    rc = bf_qsfp_bitfield_rmw (
             port, CDR_CONTROL, channel_mask,
             channel_mask, new_st);
    bf_sys_usleep (400000);

    return rc;
}

/** deactivate all data paths
 *
 *  @param port
 *   port
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_dp_deactivate_all (int port)
{
    int all_chmask, curbank_chmask;
    uint8_t newval;
    if ((port > bf_plt_max_qsfp) ||
        (!bf_qsfp_is_cmis (port))) {
        return -1;
    }

    // CMIS changed the polarity of this register in CMIS 4.0
    if (bf_qsfp_info_arr[port].memmap_format ==
        MMFORMAT_CMIS3P0) {
        newval = 0;
    } else {
        newval = 0xFF;
    }

    all_chmask = (1 << bf_qsfp_info_arr[port].num_ch)
                 - 1;
    curbank_chmask = 0xFF;
    while (curbank_chmask <= all_chmask) {
        if (bf_qsfp_field_write_onebank (port,
                                         DATAPATH_DEINIT,
                                         (all_chmask & curbank_chmask),
                                         0,
                                         1,
                                         &newval)) {
            return -1;
        }
        curbank_chmask <<= 8;
    }

    return 0;
}

/** reset qsfp module
 *
 *  @param port
 *   port
 *  @param reset
 *   reset 1: to reset the qsfp, 0: to unreset the qsfp
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_reset (int port, bool reset)
{
    if (port <= bf_plt_max_qsfp) {
        bf_qsfp_info_arr[port].reset = reset;
        LOG_DEBUG ("QSFP    %2d : %s", port,
                   reset ? "True" : "False");
        return (bf_pltfm_qsfp_module_reset (port, reset));
    } else {
        return -1; /* TBD: handle cpu port QSFP */
    }
}

bool bf_qsfp_get_reset (int port)
{
    if (port <= bf_plt_max_qsfp) {
        return bf_qsfp_info_arr[port].reset;
    }
    return false;
}

// returns the spec revision, CMIS modules only
int bf_cmis_spec_rev_get (int port,
                          uint8_t *major, uint8_t *minor)
{
    uint8_t rev;
    if (bf_qsfp_field_read_onebank (port, SPEC_REV, 0,
                                    0, 1, &rev) < 0) {
        return -1;
    }
    if (major != NULL) {
        *major = rev >> 4;
    }
    if (minor != NULL) {
        *minor = rev & 0xF;
    }
    return 0;
}

static int set_qsfp_idprom (int port)
{
    uint8_t id;
    uint8_t major_rev;
    uint8_t status;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (!bf_qsfp_info_arr[port].present) {
        LOG_ERROR ("QSFP %d IDProm set failed as QSFP is not present\n",
                   port);
        return -1;
    }

    // determine the module type first
    if (bf_qsfp_field_read_onebank (port, IDENTIFIER,
                                    0, 0, 1, &id) < 0) {
        if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
            LOG_ERROR ("Unable to obtain Identifier field in QSFP %d\n",
                       port);
        }
        return -1;
    }
    if ((id == QSFP) || (id == QSFP_PLUS) ||
        (id == QSFP_28)) {
        bf_qsfp_info_arr[port].memmap_format =
            MMFORMAT_SFF8636;
        bf_qsfp_info_arr[port].num_ch = 4;
    } else {  // CMIS
        // now determine if the CMIS memory map is rev 3.0 or 4.0+
        if (bf_cmis_spec_rev_get (port, &major_rev,
                                  NULL) < 0) {
            LOG_ERROR ("Unable to obtain revision field in module %d\n",
                       port);
            return -1;
        }
        if (major_rev < 4) {  // CMIS 3.X
            bf_qsfp_info_arr[port].memmap_format =
                MMFORMAT_CMIS3P0;
        } else if (major_rev <
                   5) {  // CMIS 4.0 or newer. There was a break in
            // backwards compatibility
            bf_qsfp_info_arr[port].memmap_format =
                MMFORMAT_CMIS4P0;
        } else {
            bf_qsfp_info_arr[port].memmap_format =
                MMFORMAT_CMIS5P0;
        }

        // identify the number of channels in the module, based on the module type
        if (id == QSFP_CMIS) {
            bf_qsfp_info_arr[port].num_ch = 4;
        } else if ((id == QSFP_DD) || (id == OSFP)) {
            bf_qsfp_info_arr[port].num_ch = 8;
        }
    }

    // now that we know the memory map format, we can determine if it is flat or
    // paged memory
    if (bf_qsfp_field_read_onebank (port, STATUS, 0,
                                    0, 1, &status) < 0) {
        LOG_ERROR ("Unable to obtain status field in module %d\n",
                   port);
        return -1;
    }
    if (bf_qsfp_info_arr[port].memmap_format ==
        MMFORMAT_SFF8636) {
        bf_qsfp_info_arr[port].flat_mem = (status >> 2) &
                                          1;
    } else {  // CMIS and UNKNOWN
        bf_qsfp_info_arr[port].flat_mem = (status >> 7) &
                                          1;
    }
    return 0;
}

/** return qsfp presence information
 *
 *  @param port
 *   port
 *  @return
 *   true if presence and false if absent
 */
bool bf_qsfp_is_present (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].present;
}

/** return qsfp checksum status
 *
 *  @param port
 *   port
 *  @return
 *   true if presence and false if absent
 */
bool bf_qsfp_is_checksum_passed (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].checksum;
}

/** mark qsfp module presence
 *
 *  @param port
 *   port
 *  @param present
 *   true if present and false if removed
 */
void bf_qsfp_set_present (int port, bool present)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    bf_qsfp_info_arr[port].present = present;
    /* Set the dirty bit as the QSFP was removed and
     * the cached data is no longer valid until next
     * set IDProm is called
     */
    if (bf_qsfp_info_arr[port].present == false) {
        bf_qsfp_port_deinit (port);
    }
}
bool bf_qsfp_get_module_capability (int port,
                                    uint16_t *type)
{
    Ethernet_compliance eth_comp;
    if (bf_qsfp_get_eth_compliance (port,
                                    &eth_comp) != 0) {
        *type = 0;
    }

    if (eth_comp & 0x80) {
        Ethernet_extended_compliance ext_comp;
        if (bf_qsfp_get_eth_ext_compliance (port,
                                            &ext_comp) == 0) {
            *type = (eth_comp << 8) | ext_comp;
        } else {
            *type = 0;
        }
    } else {
        *type = eth_comp << 8;
    }

    return true;
}

/** return qsfp transceiver information
 *
 *  @param port
 *   port
 *  @param info
 *   qsfp transceiver information struct
 */
void bf_qsfp_get_transceiver_info (int port,
                                   qsfp_transciever_info_t *info)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    info->present = bf_qsfp_info_arr[port].present;
    info->transceiver = qsfp;
    info->port = port;
    // if (!cache_is_valid(port)) {
    //  return;
    //}

    /* Default to QSFP_100GBASE_LR4. To be fixed. */
    info->transceiver = QSFP_100GBASE_LR4;

    if (bf_qsfp_get_module_type (port,
                                 &info->module_type)) {
        info->_isset.mtype = true;
    }
    if (bf_qsfp_get_connector_type (port,
                                &info->connector_type)) {
        info->_isset.ctype = true;
    }
    if (bf_qsfp_get_module_capability (port,
                                       &info->module_capability)) {
        info->_isset.mcapa = true;
    }

    if (bf_qsfp_get_vendor_info (port,
                                 &info->vendor)) {
        info->_isset.vendor = true;
    }
    if (bf_qsfp_get_cable_info (port, &info->cable)) {
        info->_isset.cable = true;
    }

    if (!bf_qsfp_is_passive_cu (port)) {
        if (bf_qsfp_get_module_sensor_info (port,
                                            &info->sensor)) {
            info->_isset.sensor = true;
        }
        if (bf_qsfp_get_threshold_info (port,
                                        &info->thresh)) {
            info->_isset.thresh = true;
        }
        for (int i = 0; i < bf_qsfp_info_arr[port].num_ch;
             i++) {
            info->chn[i].chn = i;
        }

        if (bf_qsfp_get_chan_sensor_data (port,
                                          info->chn)) {
            info->_isset.chn = true;
        } else {
            info->_isset.chn = false;
        }
    }
}

/** return cmis module media type
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to media type
 *  @return
 *   0 on success and -1 on error
 */

static int cmis_get_media_type (int port,
                                uint8_t *media_type)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    if (bf_qsfp_field_read_onebank (port,
                                    MODULE_MEDIA_TYPE, 0, 0, 1, media_type) <
        0) {
        return -1;
    }
    return 0;
}

/** return cmis application data for one application
 *
 *  @param port
 *   port
 *  @param apsel_code
 *   apsel code to read (1-based)
 *  @param host_id
 *   pointer to host_id result
 *  @param media_id
 *   pointer to media_id result
 *  @param lane_counts
 *   pointer to host and media lane counts result
 *  @param lane_option_mask
 *   pointer to host lane assignment option mask result
 *  @return
 *   0 on success and -1 on error
 */

static int cmis_get_application_data (int port,
                                      int apsel_code,
                                      uint8_t *host_id,
                                      uint8_t *media_id,
                                      uint8_t *host_lane_count,
                                      uint8_t *media_lane_count,
                                      uint8_t *host_lane_option_mask,
                                      uint8_t *media_lane_option_mask)
{
    uint8_t app_data[BYTES_PER_APP];
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (apsel_code > 15) {
        return -1;
    }

    if (apsel_code <= 8) {
        if (bf_qsfp_field_read_onebank (port,
                                        APSEL1_ALL,
                                        0,
                                        ((apsel_code - 1) * BYTES_PER_APP),
                                        BYTES_PER_APP,
                                        app_data) < 0) {
            return -1;
        }
    } else {
        if (bf_qsfp_field_read_onebank (port,
                                        APSEL9_ALL,
                                        0,
                                        ((apsel_code - 9) * BYTES_PER_APP),
                                        BYTES_PER_APP,
                                        app_data) < 0) {
            return -1;
        }
    }

    *host_id = app_data[0];
    *media_id = app_data[1];
    *host_lane_count = app_data[2] >> 4;
    *media_lane_count = app_data[2] & 0xF;
    *host_lane_option_mask = app_data[3];

    if (!bf_qsfp_is_flat_mem (port)) {
        if (bf_qsfp_field_read_onebank (port,
                                        APSEL1_MEDIA_LANE_OPTION_MASK,
                                        0,
                                        (apsel_code - 1),
                                        1,
                                        media_lane_option_mask) < 0) {
            return -1;
        }
    }
    return 0;
}

// populate bf_qsfp_info_arr with the supported Application details
static int qsfp_cmis_populate_app_list (int port)
{
    int cur_app_num = 0;
    while (cur_app_num <
           bf_qsfp_info_arr[port].num_apps) {
        if (cmis_get_application_data (
                port,
                cur_app_num + 1,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].host_if_id,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].media_if_id,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].host_lane_cnt,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].media_lane_cnt,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].host_lane_assign_mask,
                &bf_qsfp_info_arr[port].app_list[cur_app_num].media_lane_assign_mask) != 0) {
            return -1;
        }
        BF_APP_VALID_SET(bf_qsfp_info_arr[port].app_list[cur_app_num]);
        cur_app_num++;
    }
    return 0;
}

static int qsfp_sff8636_add_app (int port,
                                 uint8_t media_type,
                                 uint8_t host_if_id,
                                 uint8_t media_if_id,
                                 uint8_t host_lane_cnt,
                                 uint8_t media_lane_cnt,
                                 uint8_t host_lane_assign_mask,
                                 uint8_t media_lane_assign_mask)
{
    int cur_app = bf_qsfp_info_arr[port].num_apps;

    bf_qsfp_info_arr[port].num_apps++;
#if defined (APP_ALLOC)
    if (bf_qsfp_info_arr[port].num_apps == 1) {
        bf_qsfp_info_arr[port].app_list =
            (bf_pm_qsfp_apps_t *)bf_sys_malloc (sizeof (
                    bf_pm_qsfp_apps_t));
    } else {
        bf_qsfp_info_arr[port].app_list =
            (bf_pm_qsfp_apps_t *)bf_sys_realloc (
                bf_qsfp_info_arr[port].app_list,
                bf_qsfp_info_arr[port].num_apps * sizeof (
                    bf_pm_qsfp_apps_t));
    }

    if (!bf_qsfp_info_arr[port].app_list) {
        LOG_ERROR (
            "Error in malloc for app_list at "
            "%s:%d\n",
            __func__,
            __LINE__);
        return -1;
    }
#else
    if (cur_app
            == BF_QSFP_MAX_APPLICATIONS) {
         LOG_ERROR (
            "Error in adding app_list at "
            "%s:%d\n",
            __func__,
            __LINE__);
        bf_qsfp_info_arr[port].num_apps --;
        return -1;
    }
    BF_APP_VALID_SET(bf_qsfp_info_arr[port].app_list[cur_app]);
#endif
    bf_qsfp_info_arr[port].media_type = media_type;
    bf_qsfp_info_arr[port].app_list[cur_app].host_if_id
        = host_if_id;
    bf_qsfp_info_arr[port].app_list[cur_app].media_if_id
        = media_if_id;
    bf_qsfp_info_arr[port].app_list[cur_app].host_lane_cnt
        = host_lane_cnt;
    bf_qsfp_info_arr[port].app_list[cur_app].media_lane_cnt
        = media_lane_cnt;
    bf_qsfp_info_arr[port].app_list[cur_app].host_lane_assign_mask
        = host_lane_assign_mask;
    return 0;
}

// This function figures out how many SFF-8636 'applications' there are. Note
// that this is not an Application as defined in SFF-8636 (which has been
// obsoleted), but instead we are converting the old SFF-8636 ethernet
// compliance advertising fields into data structures that look more like
// CMIS Applications, since that is the way the industry is moving in the
// future.
//
// NOTE:
// a) For TF1, app-list is not used except for CLI.
// b) For TF2, app-list is used only for Optics
static int qsfp_sff8636_populate_app_list (
    int port)
{
    Ethernet_compliance eth_comp;
    Ethernet_extended_compliance eth_ext_comp;
    Ethernet_extended_compliance eth_sec_comp;

    bf_qsfp_info_arr[port].num_apps = 0;

    // Get the ethernet compliance type of the cable
    if (bf_qsfp_get_eth_compliance (port,
                                    &eth_comp) != 0) {
        LOG_WARNING (
            "Error : in getting the ethernet compliance type of the QSFP at "
            "%s:%d\n",
            __func__,
            __LINE__);
    }

    // Get the ethernet extended compliance type of the cable
    if (bf_qsfp_get_eth_ext_compliance (port,
                                        &eth_ext_comp) != 0) {
        LOG_WARNING (
            "Error : in getting the ethernet extended compliance type of the "
            "QSFP at "
            "%s:%d\n",
            __func__,
            __LINE__);
    }

    // Get the ethernet extended compliance type of the cable
    if (bf_qsfp_get_eth_secondary_compliance (port,
            &eth_sec_comp) != 0) {
        LOG_WARNING (
            "Error : in getting the ethernet extended compliance type of the "
            "QSFP at "
            "%s:%d\n",
            __func__,
            __LINE__);
    }

    // Populate App structure with Ethernet compliance
    // qsfp_sff8636_add_app(int port,
    //                      uint8_t host_if_id,
    //                      uint8_t media_if_id,
    //                      uint8_t host_lane_cnt,
    //                      uint8_t media_lane_cnt,
    //                      uint8_t host_lane_assign_mask,
    //                      uint8_t media_lane_assign_mask)
    if (eth_comp & 0x1) {  // 40G active cable (XLPPI)
        qsfp_sff8636_add_app (port, 0x4, 7, 1, 4, 4, 0x1,
                              0x1);
    } else if ((eth_comp >> 1) &
               0x1) {  // 40GBASE-LR4
        qsfp_sff8636_add_app (port, 0x2, 7, 9, 4, 4, 0x1,
                              0x1);
    } else if ((eth_comp >> 2) &
               0x1) {  // 40GBASE-SR4
        qsfp_sff8636_add_app (port, 0x1, 7, 4, 4, 4, 0x1,
                              0x1);
    } else if ((eth_comp >> 3) &
               0x1) {  // 40GBASE-CR4
        qsfp_sff8636_add_app (port, 0x3, 7, 1, 4, 4, 0x1,
                              0x1);
    }

    if ((eth_comp >> 4) &
        0x1) {  // 10GBASE-SR - assume breakout x4
        //qsfp_sff8636_add_app (port, 0x1, 7, 2, 1, 1, 0xF, 0xF);
        qsfp_sff8636_add_app (port, 0x1, 4, 2, 1, 1, 0xF,
                              0xF);
    } else if ((eth_comp >> 5) &
               0x1) {  // 10GBASE-LR - assume breakout x4
        //qsfp_sff8636_add_app (port, 0x2, 7, 4, 1, 1, 0xF, 0xF);
        qsfp_sff8636_add_app (port, 0x2, 4, 4, 1, 1, 0xF,
                              0xF);
    }

    switch (eth_ext_comp) {
        case EXT_COMPLIANCE_NONE:
        case EXT_COMPLIANCE_RSVD2:
        case EXT_COMPLIANCE_RSVD3:
            break;
        case AOC_100G_BER_12:  // host-id
        case ACC_100G_BER_12:
        case AOC_100G_BER_5:  // 100G AOC (Active Optical Cable) or 25GAUI C2M AOC.
        // Providing a worst BER of 5 ? 10-5
        case ACC_100G_BER_5:  // 100G ACC (Active Copper Cable) or 25GAUI C2M ACC.
            // Providing a worst BER of 5 ? 10-5
            qsfp_sff8636_add_app (port, 0x4, 11, 2, 4, 4, 0x1,
                                  0x1);

            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            //qsfp_sff8636_add_app (port, 0x4, 5, 2, 1, 1, 0xF, 0xF);
            qsfp_sff8636_add_app (port, 0x4, 4, 2, 1, 1, 0xF,
                                  0xF);
            break;
        case SR4_100GBASE:  // 100GBASE-SR4 or 25GBASE-SR
            qsfp_sff8636_add_app (port, 0x1, 11, 9, 4, 4, 0x1,
                                  0x1);

            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x1, 5, 3, 1, 1, 0xF,
                                  0xF);
            break;
        case LR4_100GBASE:   // 100GBASE-LR4 or 25GBASE-LR
        case PSM4_100G_SMF:  // 100G PSM4
            qsfp_sff8636_add_app (port, 0x2, 11, 13, 4, 4,
                                  0x1, 0x1);

            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x2, 5, 7, 1, 1, 0xF,
                                  0xF);
            break;
        case ER4_100GBASE:  // 100GBASE-ER4 or 25GBASE-ER
            qsfp_sff8636_add_app (port, 0x2, 11, 14, 4, 4,
                                  0x1, 0x1);

            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x2, 5, 8, 1, 1, 0xF,
                                  0xF);
            break;
        case SR10_100GBASE:  // 100GBASE-SR10
            // not in CMIS
            break;
        case CWDM4_100G:  // 100G CWDM4
            qsfp_sff8636_add_app (port, 0x2, 11, 16, 4, 4,
                                  0x1, 0x1);
            break;
        case CR4_100GBASE:  // 100GBASE-CR4 or 25GBASE-CR CA-25G-L
            qsfp_sff8636_add_app (port, 0x3, 26, 1, 4, 4, 0x1,
                                  0x1);

            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x3, 20, 1, 1, 1, 0xF,
                                  0xF);
            break;
        case CR_25GBASE_CA_S:  // 25GBASE-CR CA-25G-S
            qsfp_sff8636_add_app (port, 0x3, 21, 1, 1, 1, 0xF,
                                  0xF);
            break;
        case CR_25GBASE_CA_N:  // 25GBASE-CR CA-25G-N
            qsfp_sff8636_add_app (port, 0x3, 22, 1, 1, 1, 0xF,
                                  0xF);
            break;
        case ACC_200G_BER_6:  // 50GAUI, 100GAUI-2 or 200GAUI-4 C2M
        case AOC_200G_BER_6:
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_200GAUI_4_C2M,
                                  MEDIA_ACTIVE_CBL_BER_6,
                                  4,
                                  4,
                                  0x1,
                                  0x1);
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_100GAUI_2_C2M,
                                  MEDIA_ACTIVE_CBL_BER_6,
                                  2,
                                  2,
                                  0x5,
                                  0x5);
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_50GAUI_1_C2M,
                                  MEDIA_ACTIVE_CBL_BER_6,
                                  1,
                                  1,
                                  0xF,
                                  0xF);
            break;
        case ACC_200G_BER_4:  // 50GAUI, 100GAUI-2 or 200GAUI-4 C2M
        case AOC_200G_BER_4:
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_200GAUI_4_C2M,
                                  MEDIA_ACTIVE_CBL_BER_4,
                                  4,
                                  4,
                                  0x1,
                                  0x1);
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_100GAUI_2_C2M,
                                  MEDIA_ACTIVE_CBL_BER_4,
                                  2,
                                  2,
                                  0x5,
                                  0x5);
            qsfp_sff8636_add_app (port,
                                  MEDIA_TYPE_ACTIVE_CBL,
                                  HOST_TYPE_50GAUI_1_C2M,
                                  MEDIA_ACTIVE_CBL_BER_4,
                                  1,
                                  1,
                                  0xF,
                                  0xF);
            break;
        // the following compliance codes are not currently supported
        // We simply populate it with 100G optics or copper
        /*
        case ER4_40GBASE:
        case SR_10GBASE_4:
        case PSM4_40G_SMF:
        case G959_P1I1_2D1:
        case G959_P1S1_2D2:
        case G959_P1L1_2D2:
        case T_10BASE_SFI:
        case CLR4_100G:
        case DWDM2_100GE:
        */
        default:
            break;
    }

    if (!bf_qsfp_info_arr[port].num_apps) {
        LOG_DEBUG (
            "QSFP    %2d : NOTICE: SFF8636 Ethernet compliance not found. \
         Eth-comp 0x%0x : Eth-ext-comp 0x%0x Eth-sec-comp 0x%0x\n",
            port,
            eth_comp,
            eth_ext_comp,
            eth_sec_comp);
        // Add a dumy
        if (bf_qsfp_is_optical (port)) {
            qsfp_sff8636_add_app (port, 0x1, 11, 9, 4, 4, 0x1,
                                  0x1);
            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x1, 5, 3, 1, 1, 0xF,
                                  0xF);
        } else {
            qsfp_sff8636_add_app (port, 0x3, 26, 1, 4, 4, 0x1,
                                  0x1);  // 100G Copper
            // assume it supports breakout, but this may be a bad assumption. Need to
            // read the Pg 0 Byte 113 to see
            // the cable construction
            qsfp_sff8636_add_app (port, 0x3, 20, 1, 1, 1, 0xF,
                                  0xF);
        }
    }

    return 0;
}

/** calculates and returns the cmis application count
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to number of supported Applications
 *  @return
 *   0 on success and -1 on error
 */
static int cmis_calc_application_count (int port,
                                        int *app_count)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    uint8_t host_id;

    *app_count = 0;
    do {
        bf_qsfp_field_read_onebank (
            port, APSEL1_HOST_ID, 0,
            (*app_count * BYTES_PER_APP), 1, &host_id);
        (*app_count)++;
        LOG_DEBUG("QSFP    %2d : App00 %d : host_id 0x%x", port, (*app_count),
                host_id);
    } while ((host_id != 0xFF) && (*app_count < 8));

    if ((host_id != 0xFF) &&
        (!bf_qsfp_is_flat_mem (port))) {
        // not yet at end of list, list continues on Pg 1
        do {
            bf_qsfp_field_read_onebank (port,
                                        APSEL9_HOST_ID,
                                        0,
                                        ((*app_count - 9) * BYTES_PER_APP),
                                        1,
                                        &host_id);
            (*app_count)++;
            LOG_DEBUG("QSFP    %2d : App01 %d : host_id 0x%x", port, (*app_count),
                    host_id);
        } while ((host_id != 0xFF) && (*app_count < 15));
    }
    (*app_count)--;
    return 0;
}

static int bf_qsfp_chksum_chk (int port,
                               uint8_t *buf, bool is_cmis)
{
    int i = 0;
    uint32_t base_chksum = 0, ext_chksum = 0,
             page_chksum = 0;

    if (is_cmis) {
        for (i = 0; i < 94; i++) {
            page_chksum = page_chksum + buf[i];
        }
        page_chksum = page_chksum & 0xFF;

        if (buf[94] != page_chksum) {
            return -1;
        }

    } else {
        for (i = 0; i < 63; i++) {
            base_chksum = base_chksum + buf[i];
        }

        base_chksum = base_chksum & 0xFF;

        if (buf[63] != base_chksum) {
            return -1;
        }

        for (i = 64; i < 95; i++) {
            ext_chksum = ext_chksum + buf[i];
        }

        ext_chksum = ext_chksum & 0xFF;

        if (buf[95] != ext_chksum) {
            return -1;
        }
    }
    return 0;
}

/** update qsfp cached memory map data
 *
 *  @param port
 *   port
 */
static int bf_qsfp_update_cache (int port)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    bf_qsfp_idprom_clr (port);

    if (!bf_qsfp_info_arr[port].present) {
        return 0;  // not present, exit now that the cache is cleared
    }

    // first, figure out memory map format.
    if (set_qsfp_idprom (port)) {
        if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
            LOG_WARNING ("Error setting idprom for QSFP %2d\n",
                       port);
        }
        return -1;
    }

    // now that we know the memory map format, we can selectively read the
    // locations that are static into our cache. The static locations of CMIS and
    // SFF-8636 are not even close to the same. The _field_map variables indicate
    // if each field is stored in the cache or not

    // page 00h (lower), bytes 0-2
    if (bf_qsfp_module_read (port,
                             0, /* bank */
                             0, /* page */
                             0, /* offset */
                             3, /* length */
                             &bf_qsfp_info_arr[port].idprom[0]) < 0) {
        LOG_WARNING ("Error reading QSFP %2d lower memory\n",
                   port);
        return -1;
    }

    // BOTH module types, CMIS and SFF-8636, flat and paged
    // page 00h (upper), bytes 128-255 - read first because we'll need this
    if (bf_qsfp_module_read (port,
                             0,   /* bank */
                             0,   /* page */
                             128, /* offset */
                             128, /* length */
                             &bf_qsfp_info_arr[port].page0[0]) < 0) {
        LOG_WARNING ("Error reading QSFP %2d page 0\n",
                   port);
        return -1;
    }

    if (bf_qsfp_chksum_chk (
            port, &bf_qsfp_info_arr[port].page0[0],
            bf_qsfp_is_cmis (port))) {
        if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
            LOG_ERROR ("ChkSum not matched for QSFP %2d\n", port);
        }
        bf_qsfp_info_arr[port].checksum = false;
    }

    if (bf_qsfp_is_cmis (port)) {
        // page 00h (lower), bytes 85-117
        if (bf_qsfp_module_read (port,
                                 0,  /* bank */
                                 0,  /* page */
                                 85, /* offset */
                                 33, /* length */
                                 &bf_qsfp_info_arr[port].idprom[85]) < 0) {
            LOG_WARNING ("Error reading QSFP %2d lower memory\n",
                       port);
            return -1;
        }

        if (!bf_qsfp_is_flat_mem (port)) {
            // page 01h, bytes 130-255
            if (bf_qsfp_module_read (port,
                                     0,   /* bank */
                                     1,   /* page */
                                     130, /* offset */
                                     126, /* length */
                                     &bf_qsfp_info_arr[port].page1[2]) < 0) {
                LOG_WARNING ("Error reading QSFP %2d page 1 offset <130-255>\n",
                           port);
                return -1;
            }

            // page 02h, bytes 128-255
            if (bf_qsfp_module_read (port,
                                     0,   /* bank */
                                     2,   /* page */
                                     128, /* offset */
                                     128, /* length */
                                     &bf_qsfp_info_arr[port].page2[0]) < 0) {
                LOG_ERROR ("Error reading QSFP %2d page 2\n",
                           port);
                return -1;
            }

            // page 13h, bytes 128
            if (bf_qsfp_module_read (port,
                                     0,   /* bank */
                                     19,  /* page */
                                     128, /* offset */
                                     1,   /* length */
                                     &bf_qsfp_info_arr[port].page19[0]) < 0) {
                LOG_ERROR ("Error reading QSFP %2d page 13 offset <19-19>\n",
                           port);
                return -1;
            }

            // page 2fh, bytes 128
            if (bf_qsfp_module_read (port,
                                     0,   /* bank */
                                     47,  /* page */
                                     128, /* offset */
                                     1,   /* length */
                                     &bf_qsfp_info_arr[port].page47[0]) < 0) {
                LOG_ERROR ("Error reading QSFP %2d page 2f offset <128-128>\n",
                           port);
                return -1;
            }
        }

        if (cmis_get_media_type (port,
                                 &bf_qsfp_info_arr[port].media_type) != 0) {
            LOG_ERROR (
                "Error : in getting the CMIS media type type of the QSFP at "
                "%s:%d\n",
                __func__,
                __LINE__);
        }

        bf_pltfm_qsfpdd_type_t cmis_type;
        if (bf_cmis_type_get (port, &cmis_type) != 0) {
            LOG_ERROR ("Error: in getting the CMIS type for port %d at %s:%d",
                       port,
                       __func__,
                       __LINE__);
        }

        if (cmis_type == BF_PLTFM_QSFPDD_OPT) {
            bf_qsfp_info_arr[port].passive_cu = false;
        } else {
            bf_qsfp_info_arr[port].passive_cu = true;
        }

        if (cmis_calc_application_count (port,
                                         &bf_qsfp_info_arr[port].num_apps) !=
            0) {
            LOG_ERROR (
                "Error : in getting the number of CMIS Applications at "
                "%s:%d\n",
                __func__,
                __LINE__);
        }
        LOG_DEBUG ("QSFP    %2d : Application count = %d",
                   port,
                   bf_qsfp_info_arr[port].num_apps);

#if defined (APP_ALLOC)
        bf_qsfp_info_arr[port].app_list =
            (bf_pm_qsfp_apps_t *)bf_sys_malloc (
                bf_qsfp_info_arr[port].num_apps * sizeof (
                    bf_pm_qsfp_apps_t));
        if (!bf_qsfp_info_arr[port].app_list) {
            LOG_ERROR (
                "Error in malloc for app_list at "
                "%s:%d\n",
                __func__,
                __LINE__);
            return -1;
        }
#endif
        if (qsfp_cmis_populate_app_list (port) != 0) {
            LOG_ERROR (
                "Error : populating the Application list at "
                "%s:%d\n",
                __func__,
                __LINE__);
        }
    } else {  // SFF-8636
        // page 00h (lower), bytes 86-99, Control Functions.
        if (bf_qsfp_module_read (port,
                                 0,  /* bank */
                                 0,  /* page */
                                 86, /* offset */
                                 14, /* length */
                                 &bf_qsfp_info_arr[port].idprom[86]) < 0) {
            LOG_WARNING ("Error reading QSFP %2d lower memory\n",
                       port);
            return -1;
        }

        // page 00h (lower), bytes 107-116
        if (bf_qsfp_module_read (port,
                                 0,   /* bank */
                                 0,   /* page */
                                 107, /* offset */
                                 10,  /* length */
                                 &bf_qsfp_info_arr[port].idprom[107]) < 0) {
            LOG_WARNING ("Error reading QSFP %2d lower memory\n",
                       port);
            return -1;
        }

        bf_pltfm_qsfp_type_t qsfp_type;
        if (bf_qsfp_type_get (port, &qsfp_type) != 0) {
            LOG_WARNING ("Error: in getting the QSFP type for port %2d at %s:%d",
                       port,
                       __func__,
                       __LINE__);
        }
        if (qsfp_type == BF_PLTFM_QSFP_OPT) {
            bf_qsfp_info_arr[port].passive_cu = false;
        } else {
            bf_qsfp_info_arr[port].passive_cu = true;
        }

        if (!bf_qsfp_is_flat_mem (port)) {
            // page 03h, bytes 128-229
            if (bf_qsfp_module_read (port,
                                     0,   /* bank */
                                     3,   /* page */
                                     128, /* offset */
                                     102, /* length */
                                     &bf_qsfp_info_arr[port].page3[0]) < 0) {
                LOG_WARNING ("Error reading QSFP %2d page 3 offset <128-299>\n",
                           port);
                return -1;
            }
        }
        if (qsfp_sff8636_populate_app_list (port) != 0) {
            LOG_ERROR (
                "Error : populating the Ethernet compliance list at "
                "%s:%d\n",
                __func__,
                __LINE__);
        }
    }

    LOG_DEBUG (
        "QSFP    %2d : Spec %s : %s : %s : lanes %d", port,
        bf_qsfp_is_cmis (port)       ? "CMIS" : "SFF-8636",
        bf_qsfp_is_passive_cu (port) ? "Passive copper" : "Active/Optical",
        bf_qsfp_is_flat_mem (port)   ? "Flat" : "Paged",
        bf_qsfp_info_arr[port].num_ch);

    bf_qsfp_info_arr[port].cache_dirty = false;
    LOG_DEBUG ("QSFP    %2d : Update cache complete",
               port);
    return 0;
}

/** Update module/channel monitor and control functions.
 *
 *  @param port
 *   port
 */
int bf_qsfp_update_cache2 (int port)
{
    int rc;
    bool debug = false;
    uint8_t *idprom;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    idprom = &bf_qsfp_info_arr[port].idprom[0];

    if (!bf_qsfp_is_present(port) ||
        bf_qsfp_get_reset(port)) {
        return 0;
    }

    /* As we have know the kind of current trans, that's much easier to update cache. */

    if (bf_qsfp_is_cmis (port)) {
        /* CMIS */
        // page 11h (upper)
        if ((rc = bf_qsfp_module_read (port,
                                0,
                                17,
                                128,
                                128,
                                &bf_qsfp_info_arr[port].page17[0])) < 0) {
            LOG_WARNING ("Error<%d>: "
                      "Reading QSFP %2d page 17 offset <128-255>\n", rc, port);
            return -1;
        }
        // page 12h (upper)
        if ((rc = bf_qsfp_module_read (port,
                                0,
                                18,
                                128,
                                128,
                                &bf_qsfp_info_arr[port].page18[0])) < 0) {
            LOG_WARNING ("Error<%d>: "
                      "Reading QSFP %2d page 18\n", rc, port);
            return -1;
        }
    } else {
        /* SFF-8636 */
        // page 00h (lower), bytes 3-14, Interrupt Flags.
        if ((rc = bf_qsfp_module_read (port,
                                0,
                                0,
                                3,
                                12,
                                &idprom[3])) < 0) {
            LOG_WARNING ("Error<%d>: "
                      "Reading QSFP %2d page 0 offset <3-14>\n", rc, port);
            return -1;
        }

        // page 00h (lower), bytes 86-99, Control Functions.
        if ((rc = bf_qsfp_module_read (port,
                                 0,  /* bank */
                                 0,  /* page */
                                 86, /* offset */
                                 14, /* length */
                                 &idprom[86])) < 0) {
            LOG_WARNING ("Error<%d>: "
                       "Reading QSFP %2d page 0 offset <86-99>\n", rc, port);
            return -1;
        }

        /* Belowing is done by health monitor. */
#if 0
        // page 00h (lower), bytes 22-33, Module Monitor.
        if ((rc = bf_qsfp_module_read (port,
                                0,
                                0,
                                22,
                                12,
                                &idprom[22])) < 0) {
            LOG_WARNING ("Error<%d>: "
                      "Reading QSFP %2d pg0 offset <22-33>\n", rc, port);
            return -1;
        }

        // page 00h (lower), bytes 34-57, Channel Monitor.
        if ((rc = bf_qsfp_module_read (port,
                                0,
                                0,
                                34,
                                24,
                                &idprom[34])) < 0) {
            LOG_WARNING ("Error<%d>: "
                      "Reading QSFP %2d pg0 offset <34-57>\n", rc, port);
            return -1;
        }

        // page 00h (lower), bytes 107-116
        //if ((rc = bf_qsfp_module_read (port,
        //                         0,   /* bank */
        //                         0,   /* page */
        //                         107, /* offset */
        //                         10,  /* length */
        //                         &idprom[107])) < 0) {
        //    LOG_WARNING ("Error<%d>: "
        //                "Reading QSFP %2d pg0 offset <107-116>\n", rc, port);
        //    return -1;
        //}
#endif
        if (debug) {
            LOG_DEBUG (
                "QSFP    %2d : Module Monitor (Bytes 22-33) :"
                " %02x %02x %02x %02x %02x %02x"
                " %02x %02x %02x %02x %02x %02x",
                port,
                idprom[22], idprom[23], idprom[24], idprom[25],
                idprom[26], idprom[27], idprom[28], idprom[29],
                idprom[30], idprom[31], idprom[32], idprom[33]);

            LOG_DEBUG (
                "QSFP    %2d : Module Channel Monitor (Bytes 34-57) :"
                " %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x"
                " %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                port,
                idprom[34], idprom[35], idprom[36], idprom[37],
                idprom[38], idprom[39], idprom[40], idprom[41],
                idprom[42], idprom[43], idprom[44], idprom[45],
                idprom[46], idprom[47], idprom[48], idprom[49],
                idprom[50], idprom[51], idprom[52], idprom[53],
                idprom[54], idprom[55], idprom[56], idprom[57]);

            LOG_DEBUG (
                "QSFP    %2d : Module Control Functions (Bytes 86-99) :"
                " %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                port,
                idprom[86], idprom[87], idprom[88], idprom[89],
                idprom[90], idprom[91], idprom[92], idprom[93],
                idprom[94], idprom[95], idprom[96], idprom[97],
                idprom[98], idprom[99]);
        }
    }

    return 0;
}

/** detect qsfp module by accessing its memory and update cached information
 * accordingly
 *
 *  @param port
 *   port
 */
int bf_qsfp_detect_transceiver (int port,
                                bool *is_present)
{
    if (port < 0 || port > bf_plt_max_qsfp) {
        return -1;
    }
    bool st = bf_pltfm_detect_qsfp (port);
    // Just overwrite presence bit
    if (bf_qsfp_soft_removal_get (port) && st) {
        st = 0;
    }

    if (st != bf_qsfp_info_arr[port].present ||
        bf_qsfp_info_arr[port].cache_dirty) {
        if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
            LOG_DEBUG ("QSFP    %2d : %s", port,
                       st ? "PRESENT" : "REMOVED");
        }
        bf_qsfp_set_present (port, st);
        *is_present = st;
        if (st) {
            if (bf_qsfp_update_cache (port)) {
                /* NOTE, we do not clear IDPROM data here so that we can read
                 * whatever data it returned.
                 */
                bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs
                    = true;
                return -1;
            }
            bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs
                = false;
            // bf_qsfp_set_pwr_ctrl(port);  don't power up the module here, do it in
            // the module FSM
        }
    } else {
        *is_present = st;
    }
    return 0;
}

/** return qsfp presence information as per modules' presence pins
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0=present, 1=not present
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0=present, 1=not present
 */
int bf_qsfp_get_transceiver_pres (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports)
{
    uint32_t pos, port;
    int rc = 0;

    rc = bf_pltfm_qsfp_get_presence_mask (lower_ports,
                                          upper_ports, cpu_ports);

    // If the qsfp is plugged-in and soft-removal is set, or force present is
    // set, override presence
    for (port = 0; port <= (uint32_t)bf_plt_max_qsfp;
         port++) {
        pos = port % 32;
        if (port < 32) {
            if ((! (*lower_ports & (1 << pos))) &&
                (bf_qsfp_soft_removal_get (port + 1))) {
                *lower_ports |= (1 << pos);
            }
        } else if (port < 64) {
            if ((! (*upper_ports & (1 << pos))) &&
                (bf_qsfp_soft_removal_get (port + 1))) {
                *upper_ports |= (1 << pos);
            }
        }
        // add separate command for cpu-port soft-removal/insertion
    }

    // return actual HW error
    return rc;
}

/** return qsfp interrupt information as per modules' interrupt pins
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0=int present, 1=no interrupt
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0=int present, 1=no interrupt
 */
void bf_qsfp_get_transceiver_int (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports)
{
    bf_pltfm_qsfp_get_int_mask (lower_ports,
                                upper_ports, cpu_ports);
}

/** return qsfp lpmode hardware pin information
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0= no lpmode, 1= lpmode
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0= no lpmode, 1= lpmode
 */
int bf_qsfp_get_transceiver_lpmode (
    uint32_t *lower_ports,
    uint32_t *upper_ports,
    uint32_t *cpu_ports)
{
    return (bf_pltfm_qsfp_get_lpmode_mask (
                lower_ports, upper_ports, cpu_ports));
}

/** set qsfp lpmode by using hardware pin
 *
 *  @param port
 *    front panel port
 *  @param lpmode
 *    true : set lpmode, false: set no-lpmode
 *  @return
 *    0 success, -1 failure;
 */
int bf_qsfp_set_transceiver_lpmode (int port,
                                    bool lpmode)
{
    return (bf_pltfm_qsfp_set_lpmode (port, lpmode));
}

/** get qsfp power control through software register
 *
 *  @param port
 *   port
 */
int bf_qsfp_get_pwr_ctrl(int port) {
    // uint8_t new_pwr_ctrl;
    bf_pltfm_status_t sts = BF_SUCCESS;
    int hw_lp_mode = 0;
    uint32_t lower_ports;
    uint32_t upper_ports;
    uint32_t cpu_ports;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    // get the current pwr control byte from cache, so we can read-modify-write it
    // these function calls take care of the CMIS/SFF-8636 address decode
    uint8_t pwr_ctrl;
    if (bf_qsfp_field_read_onebank(port, POWER_CONTROL, 0, 0, 1, &pwr_ctrl) < 0) {
        LOG_DEBUG("QSFP    %2d : qsfp Power Control read failure", port);
        return -1;
    }

    LOG_DEBUG("QSFP    %2d : value read from qsfp Power Control byte : 0x%x",
                port,
                pwr_ctrl);
    // modify the read value
    if (!bf_qsfp_is_cmis(port)) {  // SFF-8636
        if (pwr_ctrl & POWER_OVERRIDE) {
          if (pwr_ctrl & POWER_SET)  // this bit set to 1 forces LPMode
          {
            return 1;
          } else {
            return 0;
          }
        }
    } else {  // is CMIS
            // CMIS 3.0 and CMIS 4.0+ : get low power is same for both case
        if (pwr_ctrl & FORCELOWPWR)
            return 1;
        else
            return 0;
    }
    sts = bf_qsfp_get_transceiver_lpmode(&lower_ports, &upper_ports, &cpu_ports);

    if (sts == BF_SUCCESS) {
        if (port < 33) {
            hw_lp_mode = ((lower_ports >> (port - 1)) & (0x1));
        } else if (port < 65) {
            hw_lp_mode = ((upper_ports >> (port - 33)) & (0x1));
        } else if (port == 65) {
            hw_lp_mode = cpu_ports;
        } else {
            hw_lp_mode = 0;
        }
        return (hw_lp_mode ? 1 : 0);
    } else {
        return -1;
    }
}

/** set qsfp power control through software register
 *
 *  @param port
 *   port
 */
void bf_qsfp_set_pwr_ctrl (int port, bool lpmode)
{
    uint8_t new_pwr_ctrl;

    if (port > bf_plt_max_qsfp) {
        return;
    }

    // get the current pwr control byte from cache, so we can read-modify-write it
    // these function calls take care of the CMIS/SFF-8636 address decode
    uint8_t pwr_ctrl;
    if (bf_qsfp_field_read_onebank (port,
                                    POWER_CONTROL, 0, 0, 1, &pwr_ctrl) < 0) {
        LOG_DEBUG ("QSFP    %2d : qsfp Power Control read failure",
                   port);
        return;
    }

    LOG_DEBUG ("QSFP    %2d : value read from qsfp Power Control byte : 0x%x",
               port,
               pwr_ctrl);

    // modify the read value
    if (!bf_qsfp_is_cmis (port)) { // SFF-8636
        uint8_t extId;
        if (bf_qsfp_field_read_onebank (port,
                                        PWR_REQUIREMENTS, 0, 0, 1, &extId) <
            0) {
            LOG_DEBUG ("QSFP    %2d : qsfp ext id read failure",
                       port);
            return;
        }

        int high_pwr_level = (extId &
                              EXT_ID_HI_POWER_MASK);
        int power_level = (extId & EXT_ID_MASK) >>
                          EXT_ID_SHIFT;

        // first, figure out if this is a power class 1 module, which means
        // high-power mode is unnecessary for the module to be fully functional
        if (high_pwr_level == 0 && power_level == 0) {
            LOG_DEBUG(
                "QSFP    %2d : qsfp power class is 1. No high-power mode required",
                port);
            return;
        }

        new_pwr_ctrl = POWER_OVERRIDE;
        if (high_pwr_level > 0) {
            new_pwr_ctrl |= HIGH_POWER_OVERRIDE;
        }
        if (lpmode) {
            new_pwr_ctrl |=
                POWER_SET;  // this bit set to 1 forces LPMode
        }
    } else {  // is CMIS
        // CMIS 3.0 and CMIS 4.0+ do this differently
        if (bf_qsfp_info_arr[port].memmap_format ==
            MMFORMAT_CMIS3P0) {
            LOG_DEBUG ("QSFP    %2d : CMIS <= v3 Power Control",
                       port);
            if (lpmode) {
                // setting the ForceLowPwr bits always forces low power mode,
                // regardless of the module state
                // good housekeeping would clear the datapathpwrup bits too, this
                // is a TODO
                new_pwr_ctrl = pwr_ctrl | FORCELOWPWR;
            } else {
                // high power mode occurs when datapathpwrup bits are set, so all we
                // need to do here is clear ForceLowPwr
                new_pwr_ctrl = pwr_ctrl & (0xFF ^ FORCELOWPWR);
            }
        } else {  // CMIS 4.0+
            LOG_DEBUG ("QSFP    %2d : CMIS >= v4 Power Control",
                       port);
            if (lpmode) {
                // setting the ForceLowPwr bits always forces low power mode,
                // regardless of the module state. Set the LowPwr bit too
                // for completeness.
                new_pwr_ctrl = pwr_ctrl | FORCELOWPWR |
                               CMIS4_LOWPWR;

                // Set the DataPathDeinit bits too
                bf_qsfp_dp_deactivate_all (port);
            } else {
                // ensure LowPwr and ForceLowPwr bits are unset
                new_pwr_ctrl = pwr_ctrl & (0xFF ^ FORCELOWPWR) &
                               (0xFF ^ CMIS4_LOWPWR);
            }
        }
    }

    // write the new value
    if (bf_qsfp_field_write_onebank (
            port, POWER_CONTROL, 0, 0, 1, &new_pwr_ctrl)) {
        LOG_ERROR ("QSFP    %2d : Error writing Power Control",
                   port);
        return;
    }

    LOG_DEBUG ("QSFP    %2d : value Written to new Power Control byte : 0x%x",
               port,
               new_pwr_ctrl);
}

/* copies cached x-ver data to user suppled buffer
 * buffer must take MAX_QSFP_PAGE_SIZE  bytes in it
 */
/** return qsfp cached information
 *
 *  @param port
 *   port
 *  @param page
 *   lower page, upper page0 or page3
 *  @param buf
 *   read the data into
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_cached_info (int port, int page,
                             uint8_t *buf)
{
    uint8_t *cptr;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_info_arr[port].cache_dirty == true) {
        return -1;
    }
    if (page == QSFP_PAGE0_LOWER) {
        cptr = &bf_qsfp_info_arr[port].idprom[0];
    } else if (page == QSFP_PAGE0_UPPER) {
        cptr = &bf_qsfp_info_arr[port].page0[0];
    } else if (page == QSFP_PAGE1) {
        cptr = &bf_qsfp_info_arr[port].page1[0];
    } else if (page == QSFP_PAGE2) {
        cptr = &bf_qsfp_info_arr[port].page2[0];
    } else if (page == QSFP_PAGE3) {
        cptr = &bf_qsfp_info_arr[port].page3[0];
    } else if (page == QSFP_PAGE17) {
        cptr = &bf_qsfp_info_arr[port].page17[0];
    } else if (page == QSFP_PAGE18) {
        cptr = &bf_qsfp_info_arr[port].page18[0];
    } else if (page == QSFP_PAGE19) {
        cptr = &bf_qsfp_info_arr[port].page19[0];
    } else if (page == QSFP_PAGE47) {
        cptr = &bf_qsfp_info_arr[port].page47[0];
    } else {
        return -1;
    }
    memcpy (buf, cptr, MAX_QSFP_PAGE_SIZE);
    return 0;
}

/** Reads qsfp and returns its information. Mainly used by Sonic.
 *
 *  @param port
 *   port
 *  @param page
 *   lower page, upper page0 or page3
 *  @param buf
 *   read the data into. Buffer must take MAX_QSFP_PAGE_SIZE bytes in it
 *  @return
 *   0 on success and -1 on error
 *  @note
 *   buffer must take MAX_QSFP_PAGE_SIZE  bytes in it
 */
int bf_qsfp_get_info (int port, int page,
                      uint8_t *buf)
{
    uint8_t *cptr, eeprom_info[MAX_QSFP_PAGE_SIZE] = {0};

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_info_arr[port].cache_dirty == true) {
        return -1;
    }
    if (page == QSFP_PAGE0_LOWER) {
        cptr = &bf_qsfp_info_arr[port].idprom[0];
    } else if (page == QSFP_PAGE0_UPPER) {
        cptr = &bf_qsfp_info_arr[port].page0[0];
    } else if (page == QSFP_PAGE1) {
       cptr = &bf_qsfp_info_arr[port].page1[0];
    } else if (page == QSFP_PAGE2) {
       cptr = &bf_qsfp_info_arr[port].page2[0];
    } else if (page == QSFP_PAGE3) {
       cptr = &bf_qsfp_info_arr[port].page3[0];
    } else if (page == QSFP_PAGE17) {
       cptr = &bf_qsfp_info_arr[port].page17[0];
    } else if (page == QSFP_PAGE18) {
       cptr = &bf_qsfp_info_arr[port].page18[0];
    } else if (page == QSFP_PAGE19) {
       cptr = &bf_qsfp_info_arr[port].page19[0];
    } else if (page == QSFP_PAGE47) {
       cptr = &bf_qsfp_info_arr[port].page47[0];
    } else {
        return -1;
    }

    // For non-optical modules, suffice to read from cache.
    // For optical modules, get current values for DOM purposes.
    if (bf_qsfp_is_optical (port)) {
        if (bf_qsfp_module_read (port,
                                 0,                                   /* bank */
                                 page == QSFP_PAGE0_LOWER ? 0 : page, /* page */
                                 page == QSFP_PAGE0_LOWER ? 0 : 128,  /* offset */
                                 MAX_QSFP_PAGE_SIZE,                  /* length */
                                 eeprom_info) < 0) {
            LOG_ERROR ("Error reading QSFP %d memory bank %d page %d offset %d\n",
                       port,
                       0,
                       page,
                       0);
            return -1;
        }

        if (page == QSFP_PAGE0_UPPER) {
            if (bf_qsfp_chksum_chk (port, eeprom_info,
                                    bf_qsfp_is_cmis (port))) {
                if (!bf_qsfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
                    LOG_ERROR ("Checksum not matched for %d\n", port);
                }
                bf_qsfp_info_arr[port].checksum = false;
            }
        }
        memcpy (cptr, eeprom_info, MAX_QSFP_PAGE_SIZE);
    }

    memcpy (buf, cptr, MAX_QSFP_PAGE_SIZE);
    return 0;
}

typedef struct cache_segment {
    uint8_t start_offset;
    uint8_t length;
} cache_segment_t;

typedef struct cache_segment_list {
    int page;
    int size;
    cache_segment_t *list;
} cache_segment_list_t;

cache_segment_list_t *find_suitable_segment_list (
    unsigned int port, int page)
{
    static cache_segment_t
    sff8636_and_cmis_page0_upper_cache[] = {
        {0, 128},
    };
    int page_count = 0;
    cache_segment_list_t *cache_segment_list = NULL;
    if (bf_qsfp_is_sff8636 (port)) {
        static cache_segment_t sff8636_page0_lower_cache[]
        = {
            {0, 3},
            {107, 10},
        };
        static cache_segment_t sff8636_page3_cache[] = {
            {0, 102},
        };
        static cache_segment_list_t sff8636_cache[] = {
            {
                QSFP_PAGE0_LOWER,
                sizeof (sff8636_page0_lower_cache) / sizeof (cache_segment_t),
                sff8636_page0_lower_cache
            },
            {
                QSFP_PAGE0_UPPER,
                sizeof (sff8636_and_cmis_page0_upper_cache) / sizeof (cache_segment_t),
                sff8636_and_cmis_page0_upper_cache
            },
            {
                QSFP_PAGE3,
                sizeof (sff8636_page3_cache) / sizeof (cache_segment_t),
                sff8636_page3_cache
            },
        };
        cache_segment_list = sff8636_cache;
        page_count = sizeof (sff8636_cache) / sizeof (
                         cache_segment_list_t);
    } else if (bf_qsfp_is_cmis (port)) {
        static cache_segment_t cmis_page0_lower_cache[] =
        {
            {0, 3},
            {85, 33},
        };
        static cache_segment_t cmis_page1_cache[] = {
            {2, 126},
        };
        static cache_segment_t cmis_page2_cache[] = {
            {0, 128},
        };
        static cache_segment_list_t cmis_cache[] = {
            {
                QSFP_PAGE0_LOWER,
                sizeof (cmis_page0_lower_cache) / sizeof (cache_segment_t),
                cmis_page0_lower_cache
            },
            {
                QSFP_PAGE0_UPPER,
                sizeof (sff8636_and_cmis_page0_upper_cache) / sizeof (cache_segment_t),
                sff8636_and_cmis_page0_upper_cache
            },
            {
                QSFP_PAGE1,
                sizeof (cmis_page1_cache) / sizeof (cache_segment_t),
                cmis_page1_cache
            },
            {
                QSFP_PAGE2,
                sizeof (cmis_page2_cache) / sizeof (cache_segment_t),
                cmis_page2_cache
            },
        };
        cache_segment_list = cmis_cache;
        page_count = sizeof (cmis_cache) / sizeof (
                         cache_segment_list_t);
    }
    for (int i = 0; i != page_count; ++i) {
        if (cache_segment_list[i].page == page) {
            return cache_segment_list + i;
        }
    }
    return NULL;
}

cache_segment_t *find_suitable_segment (
    cache_segment_t *cache,
    int cache_size,
    int offset)
{
    if (cache != NULL) {
        for (int i = 0; i != cache_size; ++i) {
            if (offset >= cache[i].start_offset &&
                offset < (cache[i].start_offset +
                          cache[i].length)) {
                return cache + i;
            }
        }
    }
    return NULL;
}

/** Reads qsfp eeprom from cache if available and if not - directly from
 * hardware using bf_qsfp_module_read
 *
 *  @param port
 *   port
 *  @param page
 *   page
 *  @param offset
 *   read from
 *  @param length
 *   read length
 *  @param buf
 *   read the data into. Must take length size
 *  @return
 *   0 on success and -1 on error
 */
bf_pltfm_status_t bf_qsfp_module_cached_read (
    unsigned int port,
    int bank,
    int page,
    int offset,
    int length,
    uint8_t *buf)
{
    if (bf_qsfp_is_optical (port)) {
        return bf_qsfp_module_read (port,
                                    bank,
                                    page == QSFP_PAGE0_LOWER ? 0 : page,
                                    offset + (page == QSFP_PAGE0_LOWER) ? 0 : 128,
                                    length,
                                    buf);
    }
    uint8_t cached_page_buf[MAX_QSFP_PAGE_SIZE];
    cache_segment_t *cache_list = NULL;
    int cache_size = 0;
    cache_segment_list_t *cache_segment_list =
        find_suitable_segment_list (port, page);
    if (cache_segment_list) {
        cache_list = cache_segment_list->list;
        cache_size = cache_segment_list->size;
        bf_pltfm_status_t sts = bf_qsfp_get_info(port, page, cached_page_buf);
        if (sts != BF_SUCCESS) {
            LOG_ERROR("%s(port=%d): error getting page %d, status = %d",
                    __func__,
                    port,
                    page,
                    sts);
            return sts;
        }
    }
    int left_to_read = length;
    int curr_offset = offset;
    int writing_buf_offset = 0;
    /*  While remained length to read counter is not 0, copy from cached page
     *  'cached_page_buf' if available, if not - read directly via
     *  bf_qsfp_module_read and reduce remained length counter
     */
    while (left_to_read) {
        cache_segment_t *is_cached =
            find_suitable_segment (cache_list, cache_size,
                                   curr_offset);
        if (is_cached) {
            int cached_end_offset = is_cached->start_offset +
                                    is_cached->length;
            int left_cached = cached_end_offset - curr_offset;
            int cached_length_to_read = min (left_cached,
                                             left_to_read);
            memcpy (buf + writing_buf_offset,
                    &cached_page_buf[curr_offset],
                    cached_length_to_read);
            left_to_read -= cached_length_to_read;
            curr_offset += cached_length_to_read;
            writing_buf_offset += cached_length_to_read;
        } else {
            int uncached_length_to_read = 1;
            while (!find_suitable_segment (cache_list,
                                           cache_size,
                                           curr_offset + uncached_length_to_read) &&
                   uncached_length_to_read < left_to_read) {
                ++uncached_length_to_read;
            }
            int base_offset = page == QSFP_PAGE0_LOWER ? 0 :
                              128;
            int device_page = page == QSFP_PAGE0_LOWER ? 0 :
                              page;
            bf_pltfm_status_t sts = bf_qsfp_module_read (port,
                                    bank,
                                    device_page,
                                    base_offset + curr_offset,
                                    uncached_length_to_read,
                                    buf + writing_buf_offset);
            if (sts != BF_SUCCESS) {
                LOG_ERROR (
                    "%s(port=%d): error reading page %d, offset %d, length %d, status "
                    "= %d",
                    __func__,
                    port,
                    device_page,
                    base_offset + curr_offset,
                    uncached_length_to_read,
                    sts);
                return sts;
            }
            left_to_read -= uncached_length_to_read;
            curr_offset += uncached_length_to_read;
            writing_buf_offset += uncached_length_to_read;
        }
    }
    return BF_SUCCESS;
}

/** return qsfp "disabled" state cached information
 *
 *  @param port
 *   port
 *  @param channel
 *   channel id
 *  @return
 *   true if channel tx is disabled (or bad param) and false if enabled
 */
bool bf_qsfp_tx_is_disabled (int port,
                             int channel)
{
    uint8_t data;

    if ((port > bf_plt_max_qsfp) ||
        (channel >= bf_qsfp_info_arr[port].num_ch)) {
        return true;
    }
    /* read the current value */
    if (bf_qsfp_field_read_onebank (
            port, CHANNEL_TX_DISABLE, (1 << channel), 0, 1,
            &data) < 0) {
        return true;
    }
    return (data & (1 << channel));
}

/** return qsfp ethernet compliance code information
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to Ethernet_compliance
 *  @return
 *   0 on success and -1 on error
 * only works for sff-8636 modules
 */
int bf_qsfp_get_eth_compliance (int port,
                                Ethernet_compliance *code)
{
    uint8_t eth_compliance;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_field_read_onebank (
            port, ETHERNET_COMPLIANCE, 0, 0, 1,
            &eth_compliance) < 0) {
        *code = COMPLIANCE_NONE;
        return -1;
    }
    *code = eth_compliance;
    return 0;
}

/** return qsfp ethernet extended compliance code information
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to Ethernet_extended_compliance
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_eth_ext_compliance (int port,
                                    Ethernet_extended_compliance *code)
{
    uint8_t eth_ext_comp;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_field_read_onebank (
            port, ETHERNET_EXTENDED_COMPLIANCE, 0, 0, 1,
            &eth_ext_comp) < 0) {
        *code = EXT_COMPLIANCE_NONE;
        return -1;
    }
    *code = eth_ext_comp;
    return 0;
}

/** return qsfp ethernet secondary extended compliance code information
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to Ethernet_secondary_extended_compliance
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_eth_secondary_compliance (
    int port,
    Ethernet_extended_compliance *code)
{
    uint8_t eth_ext_comp;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_field_read_onebank (
            port, ETHERNET_SECONDARY_COMPLIANCE, 0, 0, 1,
            &eth_ext_comp) < 0) {
        *code = EXT_COMPLIANCE_NONE;
        return -1;
    }
    *code = eth_ext_comp;
    return 0;
}

/** return qsfp memory map flat or paged
 *
 *  @param port
 *   port
 *  @return
 *   true if flat memory
 */
bool bf_qsfp_is_flat_mem (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].flat_mem;
}

/** return qsfp passive copper
 *
 *  @param port
 *   port
 *  @return
 *   true if passive copper
 */
bool bf_qsfp_is_passive_cu (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].passive_cu;
}

/** return qsfp is active cable
 *
 *  @param port
 *   port
 *  @return
 *   true if Optical/AOC
 */
bool bf_qsfp_is_active_cbl (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return (bf_qsfp_info_arr[port].media_type ==
            MEDIA_TYPE_ACTIVE_CBL);
}

/** return qsfp is optical/AOC
 *
 *  @param port
 *   port
 *  @return
 *   true if Optical/AOC
 */
bool bf_qsfp_is_optical (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return !bf_qsfp_info_arr[port].passive_cu;
}

/** return the number of actual channels in the module
 *
 *  @param port
 *   port
 *  @return
 *   number of channels, 0 on error
 */
uint8_t bf_qsfp_get_ch_cnt (int port)
{
    if (port > bf_plt_max_qsfp) {
        return 0;
    }
    return bf_qsfp_info_arr[port].num_ch;
}

/** return the number of media-side channels in the module
 *
 *  @param port
 *   port
 *  @return
 *   number of channels, 0 on error
 */
uint8_t bf_qsfp_get_media_ch_cnt (int port)
{
    if (port > bf_plt_max_qsfp) {
        return 0;
    }

    // if we're not CMIS, assume SFF-8636 (QSFP)
    if (!bf_qsfp_is_cmis (port)) {
        return 4;
    }

    // go through all applications and find the max media lane qty
    int cur_app, media_ch_cnt = 0;
    int max_headch, cur_ln_option_mask;
    for (cur_app = 0;
         cur_app < bf_qsfp_info_arr[port].num_apps;
         cur_app++) {
        // first find the max starting lane number
        max_headch = 0;
        cur_ln_option_mask =
            bf_qsfp_info_arr[port].app_list[cur_app].media_lane_assign_mask;
        while (cur_ln_option_mask) {
            cur_ln_option_mask >>= 1;
            if (cur_ln_option_mask) {
                max_headch++;
            }
        }

        // add the number of lanes in the App to the starting lane number, is it
        // the new largest value?
        if (media_ch_cnt <
            max_headch +
            bf_qsfp_info_arr[port].app_list[cur_app].media_lane_cnt) {
            media_ch_cnt =
                max_headch +
                bf_qsfp_info_arr[port].app_list[cur_app].media_lane_cnt;
        }
    }

    if (media_ch_cnt ==
        0) {  // maybe this module doesn't have Apps (e.g. copper)
        media_ch_cnt = bf_qsfp_info_arr[port].num_ch;
    }

    LOG_DEBUG ("QSFP    %2d : Media lane count = %d",
               port, media_ch_cnt);
    return media_ch_cnt;
}

bool bf_qsfp_get_spec_rev_str (int port,
                               char *rev_str)
{
    uint8_t rev;
    bf_qsfp_field_read_onebank (port, SPEC_REV, 0, 0,
                                1, &rev);
    if (bf_qsfp_is_cmis (port)) {
        sprintf (rev_str, "%d.%d", rev >> 4, rev & 0xF);
    } else {  // SFF-8636
        switch (rev) {
            case 0x0:
                strcpy (rev_str, "Not specified");
                break;
            case 0x1:
            case 0x2:
                strcpy (rev_str, "SFF-8436 4.8 or earlier");
                break;
            case 0x3:
                strcpy (rev_str, "1.3 or earlier");
                break;
            case 0x4:
                strcpy (rev_str, "1.4");
                break;
            case 0x5:
                strcpy (rev_str, "1.5");
                break;
            case 0x6:
                strcpy (rev_str, "2.0");
                break;
            case 0x7:
                strcpy (rev_str, "2.5, 2.6, and 2.7");
                break;
            case 0x8:
                strcpy (rev_str, "2.8, 2.9, and 2.10");
                break;
            default:
                strcpy (rev_str, "Unknown");
                break;
        }
    }
    return true;
}

/*
 * Force sets qsfp_type and type_copper
 */
int bf_qsfp_special_case_set (int port,
                              bf_pltfm_qsfp_type_t qsfp_type,
                              bool is_set)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    /* Closed by tsihang, 2023-04-17. */
    //memset (&bf_qsfp_info_arr[port].special_case_port,
    //        0,
    //        sizeof (bf_qsfp_special_info_t));
    if (is_set) {
        bf_qsfp_info_arr[port].special_case_port.qsfp_type
            = qsfp_type;
        bf_qsfp_info_arr[port].special_case_port.is_set =
            true;
    }

    return 0;
}

// returns true if copper type
static bool bf_qsfp_get_is_type_copper (
    bf_pltfm_qsfp_type_t qsfp_type)
{
    if (qsfp_type != BF_PLTFM_QSFP_OPT) {
        return true;
    }
    return false;
}

/* checks and vendor id and vendor P/N against a set of known special
 * cases and force sets qsft_type and type_copper
 * return true: if handled as special case and false, otherwise.
 */
static bool bf_qsfp_special_case_handled (
    int port,
    bf_pltfm_qsfp_type_t *qsfp_type,
    bool *type_copper)
{
    qsfp_vendor_info_t vendor;

    bf_qsfp_get_vendor_info (port, &vendor);
    if (!memcmp (vendor.name, "Ampheno",
                 strlen ("Ampheno")) &&
        !memcmp (vendor.part_number, "62162000",
                 strlen ("62162000"))) {
        *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
        *type_copper = true;
        /* vendor strings are not NULL terminated. So,doit here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to 3m-copper for %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (vendor.name, "ELPEU",
                        strlen ("ELPEU")) &&
               !memcmp (
                   vendor.part_number, "QSFP28-LB-6D",
                   strlen ("QSFP28-LB-6D"))) {
        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
        *type_copper = true;
        /* vendor strings are not NULL terminated. So,doit here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to copper-loopback for %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (vendor.name, "CISCO-FINISAR",
                        strlen ("CISCO-FINISAR")) &&
               !memcmp (vendor.part_number,
                        "FCBN410QE2C01-C2",
                        strlen ("FCBN410QE2C01-C2"))) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        *type_copper = false;
        /* vendor strings are not NULL terminated. So,do it here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to optical %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (
                   vendor.name, "Arista Networks",
                   strlen ("Arista Networks")) &&
               !memcmp (vendor.part_number,
                        "QSFP28-100G-AOC",
                        strlen ("QSFP28-100G-AOC"))) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        *type_copper = false;
        /* vendor strings are not NULL terminated. So,do it here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to optical %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (
        bf_qsfp_info_arr[port].special_case_port.is_set) {
        // Set by cli
        *qsfp_type =
            bf_qsfp_info_arr[port].special_case_port.qsfp_type;
        *type_copper = bf_qsfp_get_is_type_copper (
                           *qsfp_type);
        vendor.name[15] = vendor.part_number[15] = 0;
        return true;
    }
    /* add more such problematic cases here */

    /* its not a special case */
    return false;
}

static bool bf_qsfp_is_eth_ext_compliance_copper (
    uint8_t ext_comp)
{
    // fitler only copper cases; all other cases are optical
    switch (ext_comp) {
        case 0xB:
        case 0xC:
        case 0xD:
        case 0x16:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x40:
            return true;
        default:
            break;
    }
    return false;
}

int bf_qsfp_type_get (int port,
                      bf_pltfm_qsfp_type_t *qsfp_type)
{
    bool type_copper = true;  // default to copper

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    Ethernet_compliance eth_comp;
    if (bf_qsfp_get_eth_compliance (port,
                                    &eth_comp) != 0) {
        // Default to Copper Loop back in case of error

        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
        return -1;
    }

    /* there are some odd module/cable types that have inconsistent information
     * in it. We need to categorise them seperately
     */
    if (bf_qsfp_special_case_handled (port, qsfp_type,
                                      &type_copper)) {
        return 0;
    }

    LOG_DEBUG ("QSFP    %2d : eth_comp : %2d", port,
               eth_comp);

    /* SFF standard is not clear about necessity to look at both compliance
     * code and the extended compliance code. From the cable types known so far,
     * if bit-7 of compliance code is set, then, we look at extended compliance
     * code as well. Otherwise, we characterize the QSFP by regular compliance
     * code only.
     */
    if (eth_comp &
        0x77) {  // all except 40GBASE-CR4 and ext compliance bits
        type_copper = false;
    } else if (eth_comp & 0x80) {
        Ethernet_extended_compliance ext_comp;
        // See SFF-8024 spec rev 4.6.1
        if (bf_qsfp_get_eth_ext_compliance (port,
                                            &ext_comp) == 0) {
            type_copper =
                bf_qsfp_is_eth_ext_compliance_copper (ext_comp);
        } else {
            type_copper = true;
        }
    } else {
        /* Force to QSFP OPT as if ext_compiance and eth ext_compliance not 0x80 or 0x77.
        * This may occur vary error and let us keep tracking.
        * by tsihang, 2022-06-22.
        */
        LOG_WARNING ("QSFP    %2d : Force to OPT as if ext_compiance and eth ext_compliance not 0x80 or 0x77.\
        This override by tsihang.\n", port);
        type_copper = false;
    }

    if (!type_copper) {  // we are done if found optical module
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        return 0;
    }

    uint8_t cable_len;
    if (bf_qsfp_field_read_onebank (port,
                                    LENGTH_CBLASSY, 0, 0, 1, &cable_len) <
        0) {
        return -1;
    }

    LOG_DEBUG ("QSFP    %2d : len_copp : %2d", port,
               cable_len);

    switch (cable_len) {
        case 0:
            *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
            return 0;
        case 1:
            *qsfp_type = BF_PLTFM_QSFP_CU_1_M;
            return 0;
        case 2:
            *qsfp_type = BF_PLTFM_QSFP_CU_2_M;
            return 0;
        default:
            *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
            return 0;
    }
    return 0;
}

void bf_qsfp_debug_clear_all_presence_bits (void)
{
    int port;

    for (port = 0; port < BF_PLAT_MAX_QSFP + 1;
         port++) {
        if (bf_qsfp_info_arr[port].present) {
            bf_qsfp_set_present (port, false);
        }
    }
}

extern bool bf_pm_qsfp_is_luxtera (int port);

bool bf_qsfp_is_luxtera_optic (int port)
{
    return bf_pm_qsfp_is_luxtera (port);
}

bool bf_qsfp_is_cmis (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }

    if (bf_qsfp_info_arr[port].memmap_format >=
        MMFORMAT_CMIS3P0) {
        return true;
    }
    return false;
}

bool bf_qsfp_is_sff8636 (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }

    if (bf_qsfp_info_arr[port].memmap_format ==
        MMFORMAT_SFF8636) {
        return true;
    }
    return false;
}

MemMap_Format bf_qsfp_get_memmap_format (
    int port)
{
    if (port > bf_plt_max_qsfp) {
        return MMFORMAT_UNKNOWN;
    }

    return bf_qsfp_info_arr[port].memmap_format;
}

bool bf_qsfp_is_cable_assy (int port)
{
    uint8_t ct;

    if (port > bf_plt_max_qsfp) {
        return false;
    }

    if (bf_qsfp_field_read_onebank (port, CONN_TYPE,
                                    0, 0, 1, &ct) < 0) {
        return false;
    }

    if (ct == QSFP_CONN_TYPE_NO_SEPARABLE_CAB) {
        return true;
    }
    return false;
}

const char *bf_qsfp_get_conn_type_string (
    int port)
{
    uint8_t ct;

    if (port > bf_plt_max_qsfp) {
        return "Unknown";
    }

    if (bf_qsfp_field_read_onebank (port, CONN_TYPE,
                                    0, 0, 1, &ct) < 0) {
        return "Unknown";
    }

    switch (ct) {
        case QSFP_CONN_TYPE_LC:
            return "LC";
        case QSFP_CONN_TYPE_MPO_1x12:
            return "MPO 1x12";
        case QSFP_CONN_TYPE_MPO_2x16:
            return "MPO 2x16";
        case QSFP_CONN_TYPE_MPO_1x16:
            return "MPO 1x16";
        case QSFP_CONN_TYPE_NO_SEPARABLE_CAB:
            return "Not-separable";
    }
    return "Unknown";
}

static const char *bf_cmis_get_cu_string (
    int port)
{
    bf_pltfm_qsfpdd_type_t qsfpdd_type;

    if (port > bf_plt_max_qsfp) {
        return "Unknown ";
    }

    if (bf_cmis_type_get (port, &qsfpdd_type) != 0) {
        qsfpdd_type = BF_PLTFM_QSFPDD_UNKNOWN;
    }

    switch (qsfpdd_type) {
        case BF_PLTFM_QSFPDD_CU_0_5_M:
            return "0.5 m cu";
            break;
        case BF_PLTFM_QSFPDD_CU_1_M:
            return "1.0 m cu";
            break;
        case BF_PLTFM_QSFPDD_CU_1_5_M:
            return "1.5 m cu";
            break;
        case BF_PLTFM_QSFPDD_CU_2_M:
            return "2.0 m cu";
            break;
        case BF_PLTFM_QSFPDD_CU_2_5_M:
            return "2.5 m cu";
            break;
        case BF_PLTFM_QSFPDD_CU_LOOP:
            return "LpBck cu";
            break;
        case BF_PLTFM_QSFPDD_OPT:
            return "Optical";
            break;
        case BF_PLTFM_QSFPDD_UNKNOWN:
        default:
            break;
    }
    return "Unknown";
}

static const char *bf_cmis_get_active_cbl_string (
    int port)
{
    uint8_t media_tech;

    if (port > bf_plt_max_qsfp) {
        return "Unknown ";
    }

    if (bf_qsfp_field_read_onebank (port,
                                    MEDIA_INT_TECH, 0, 0, 1, &media_tech) <
        0) {
        return "Unknown";
    }

    if ((media_tech >= MEDIA_TECH_CU_UNEQ) &&
        (media_tech <= MEDIA_TECH_CU_LINEAR_ACTIVE_EQ)) {
        return "400G-AEC";
    } else {
        return "400G-AOC";
    }
}

static const char
*bf_cmis_get_custom_media_type_string (int port)
{
    uint8_t media_id, media_type;
    qsfp_vendor_info_t vendor;
    bf_qsfp_get_vendor_info (port, &vendor);

    if (cmis_get_media_type (port,
                             &media_type) != 0) {
        return NULL;
    }

    if (bf_qsfp_field_read_onebank (port,
                                    APSEL1_MEDIA_ID, 0, 0, 1, &media_id) <
        0) {
        return NULL;
    }

    if (!strncmp (vendor.name, "INNOLIGHT",
                  strlen ("INNOLIGHT"))) {
        switch (media_type) {
            case MEDIA_TYPE_SMF:
                switch (media_id) {
                    case QSFPDD_200GBASE_DR2:
                        return "200G-DR2";
                    case QSFPDD_800GBASE_DR8:
                        return "800G-DR8";
                }
                break;
        }
    }

    /* by Hang Tsi, 2024/01/23. */
    if (!strncmp (vendor.name, "Asterfusion",
                  strlen ("Asterfusion"))) {
        /* It's better to check PN here as. */
        if (!strncmp (vendor.part_number, "QFDD-8G400",
                  strlen ("QFDD-8G400"))) {
            switch (media_type) {
                case MEDIA_TYPE_MMF:
                    switch (media_id) {
                        case QSFPDD_400G_SR8:
                            return "400G-SR8";
                    }
                    break;
            }
        }
    }

    return NULL;
}

const char *bf_cmis_get_media_type_string (
    int port)
{
    uint8_t media_id, media_type;
    const char *media_str;

    if (port > bf_plt_max_qsfp) {
        return "Unknown ";
    }

    if (!bf_qsfp_is_cmis (port)) {
        return "Unknown ";
    }

    if (bf_qsfp_is_passive_cu (port)) {
        return bf_cmis_get_cu_string (port);
    }

    if (bf_qsfp_is_active_cbl (port)) {
        return bf_cmis_get_active_cbl_string (port);
    }
    if (cmis_get_media_type (port,
                             &media_type) != 0) {
        return "Unknown ";
    }

    if (bf_qsfp_field_read_onebank (port,
                                    APSEL1_MEDIA_ID, 0, 0, 1, &media_id) <
        0) {
        return "Unknown";
    }
#if 0
    /* by Hang Tsi, 2024/01/23. */
    char media_id_str[64] = {0};
    if (bf_qsfp_decode_media_id (port,
                             media_type,
                             media_id,
                             media_id_str)) {
        return media_id_str;
        //fprintf (stdout, "Hang Tsi : %s\n", media_id_str);
    }
#endif
    switch (media_type) {
        case MEDIA_TYPE_MMF:
            switch (media_id) {
                case QSFPDD_400GBASE_SR8:
                    return "400G-SR8";
                case QSFPDD_400GBASE_SR4:
                    return "400G-SR4";
            }
            break;
        case MEDIA_TYPE_SMF:
            switch (media_id) {
                case QSFPDD_400GBASE_FR8:
                    return "400G-FR8";
                case QSFPDD_400GBASE_LR8:
                    return "400G-LR8";
                case QSFPDD_400GBASE_DR4:
                    return "400G-DR4";
                case QSFPDD_400GBASE_FR4:
                    return "400G-FR4";
                case QSFPDD_400GBASE_LR4:
                    return "400G-LR4";
                case QSFPDD_200GBASE_DR4:
                    return "200G-DR4";
                case QSFPDD_200GBASE_FR4:
                    return "200G-FR4";
                case QSFPDD_200GBASE_LR4:
                    return "200G-LR4";
                case QSFPDD_100GBASE_DR:
                    return "100G-DR";
            }
            break;
        default:
            switch (media_id) {
                case QSFPDD_400GBASE_AOC:
                    return "400G-AOC";
            }
    }

    media_str = bf_cmis_get_custom_media_type_string (
                    port);
    if (media_str) {
        return media_str;
    }

    return "Unknown";
}

int bf_cmis_type_get (int port,
                      bf_pltfm_qsfpdd_type_t *qsfp_type)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    uint8_t media_type;
    if (bf_qsfp_field_read_onebank (
            port, MODULE_MEDIA_TYPE, 0, 0, 1,
            &media_type) < 0) {
        *qsfp_type = BF_PLTFM_QSFPDD_UNKNOWN;
        return 0;
    }

    if ((media_type == MEDIA_TYPE_SMF) ||
        (media_type == MEDIA_TYPE_MMF) ||
        (media_type == MEDIA_TYPE_ACTIVE_CBL)) {
        *qsfp_type = BF_PLTFM_QSFPDD_OPT;
        return 0;
    }

    // if we get here, it's a copper cable. figure out its characteristics
    float cable_len = get_qsfp_cable_length (port,
                      LENGTH_CBLASSY);

    if (cable_len == 0.0) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_LOOP;
        return 0;
    } else if (cable_len <= 0.5f) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_0_5_M;
        return 0;
    } else if (cable_len <= 1.0f) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_1_M;
        return 0;
    } else if (cable_len <= 1.5f) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_1_5_M;
        return 0;
    } else if (cable_len == 2.0f) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_2_M;
        return 0;
    } else if (cable_len == 2.5f) {
        *qsfp_type = BF_PLTFM_QSFPDD_CU_2_5_M;
        return 0;
    } else {  // For all other lengths default to max supported.
        LOG_DEBUG ("QSFPDD length %f unsupported for QSFP %d. Defaulting to 2.5m`n",
                   cable_len,
                   port);
        *qsfp_type = BF_PLTFM_QSFPDD_CU_2_5_M;
    }

    return 0;
}

void bf_qsfp_soft_removal_set (int port,
                               bool removed)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    bf_qsfp_info_arr[port].soft_removed = removed;
}

bool bf_qsfp_soft_removal_get (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].soft_removed;
}

void bf_qsfp_internal_port_set (int port,
                                bool set)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    bf_qsfp_info_arr[port].internal_port = set;
    bf_qsfp_soft_removal_set (port, set);
}

bool bf_qsfp_internal_port_get (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_info_arr[port].internal_port;
}

int bf_cmis_select_Application (int port,
                                int chnum,
                                uint8_t ApSel,
                                uint8_t datapath_ID,
                                bool explicit_control)
{
    uint8_t data = 0;
    data |= ApSel << 4;
    data |= datapath_ID << 1;
    data |= explicit_control;

    return bf_qsfp_field_write_onebank (
               port, APPLICATION_SELECT_LN1, (0x1 << chnum),
               (chnum % 8), 1, &data);
}

int bf_cmis_get_stagedSet (int port,
                           int chnum,
                           uint8_t *ApSel,
                           uint8_t *datapath_ID,
                           bool *explicit_control)
{
    int rc = 0;
    uint8_t data = 0;

    rc = bf_qsfp_field_read_onebank (
             port, APPLICATION_SELECT_LN1, (0x1 << chnum),
             (chnum % 8), 1, &data);
    if (rc) {
        return -1;
    }

    *ApSel = (data >> 4) & 0xF;
    *datapath_ID = (data >> 1) & 0x7;
    *explicit_control = data & 0x1;

    return 0;
}

int bf_cmis_get_activeSet (int port,
                           int chnum,
                           uint8_t *ApSel,
                           uint8_t *datapath_ID,
                           bool *explicit_control)
{
    int rc = 0;
    uint8_t data = 0;

    rc = bf_qsfp_field_read_onebank (
             port, LN1_ACTIVE_SET, (0x1 << chnum), (chnum % 8),
             1, &data);
    if (rc) {
        return -1;
    }

    *ApSel = (data >> 4) & 0xF;
    *datapath_ID = (data >> 1) & 0x7;
    *explicit_control = data & 0x1;

    return 0;
}

int bf_cmis_get_datapath_state (int port,
                                int ch,
                                DataPath_State *datapath_state)
{
    int rc;
    uint8_t readval;

    rc = bf_qsfp_field_read_onebank (port,
                                     DATAPATH_STATE_LN1AND2,
                                     (1 << ch),
                                     (ch >> 1),  // 2 lanes per byte
                                     1,
                                     &readval);
    if (rc == 0) {
        *datapath_state = (readval >> ((ch % 2) * 4)) &
                          0xF;
    }
    return rc;
}

int bf_cmis_get_datapath_config_status (
    int port, int ch,
    DataPath_Config_Status *datapath_config_status)
{
    int rc;
    uint8_t readval;

    rc = bf_qsfp_field_read_onebank (port,
                                     DATAPATH_CFG_STATUS,
                                     (1 << ch),
                                     (ch >> 1),  // 2 lanes per byte
                                     1,
                                     &readval);
    if (rc == 0) {
        *datapath_config_status = (readval >> ((
                ch % 2) * 4)) & 0xF;
    }
    return rc;
}

int bf_cmis_get_module_state (int port,
                              Module_State *module_state)
{
    int rc;
    uint8_t readval;

    rc = bf_qsfp_field_read_onebank (port,
                                     MODULE_STATE, 0, 0, 1, &readval);
    if (rc == 0) {
        *module_state = (readval >> 1) & 0x7;
    }
    return rc;
}

int bf_qsfp_vec_init (bf_qsfp_vec_t *vec)
{
    if (!vec) {
        return -1;
    }

    if (vec->pltfm_write) {
        bf_qsfp_vec.pltfm_write = vec->pltfm_write;
    }
    if (vec->pltfm_read) {
        bf_qsfp_vec.pltfm_read = vec->pltfm_read;
    }

    return 0;
}

void bf_qsfp_get_sff_eth_extended_code_description (
    int port,
    char *ext_code_desc)
{
    sff_ext_code_map_t *cPtr = &sff_ext_code_map[0];
    bool code_found = false;
    size_t asize = sizeof (sff_ext_code_map) /
                   sizeof (sff_ext_code_map[0]);

    Ethernet_compliance eth_comp = COMPLIANCE_NONE;
    Ethernet_extended_compliance ext_comp =
        EXT_COMPLIANCE_NONE;

    bf_qsfp_get_eth_compliance (port, &eth_comp);
    bf_qsfp_get_eth_ext_compliance (port, &ext_comp);

    if (eth_comp &
        0x77) {  // all except 40GBASE-CR4 and ext compliance bits
        sprintf (ext_code_desc,
                 "Other Eth compliance 0x%0x", eth_comp);
        return;
    }

    if (! (eth_comp & COMPLIANCE_RSVD)) { // extended
        sprintf (ext_code_desc,
                 "Non extended Eth compliance 0x%0x", ext_comp);
        return;
    }

    while (asize) {
        if (cPtr->ext_code == ext_comp) {
            strncpy (ext_code_desc, &cPtr->ext_code_desc[0],
                     250);
            code_found = true;
            break;
        }
        cPtr++;
        asize--;
    }

    if (!code_found) {
        sprintf (ext_code_desc, "Reserved (0x%0X)",
                 ext_comp);
    }
}

/** return qsfp cached temperature read via qsfp-fsm
 *
 *  @param port
 *   port
 *  @return
 *   qsfp temp in degrees C
 */
double bf_qsfp_get_cached_module_temper (
    int port)
{
    if (port > bf_plt_max_qsfp) {
        return 0;
    }

    return bf_qsfp_info_arr[port].module_temper_cache;
}
/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_cdr_support (int port,
                         bool *rx_cdr_support, bool *tx_cdr_support)
{
   int rc;
   uint8_t readval;

   if (port > bf_plt_max_qsfp) {
       return 0;
   }

   if (bf_qsfp_is_passive_cu (port)) {
       *rx_cdr_support = false;
       *tx_cdr_support = false;
       return 0;
   }

   if (bf_qsfp_is_sff8636 (port)) {
       rc = bf_qsfp_field_read_onebank (
                port, EXT_IDENTIFIER, 0, 0, sizeof (readval),
                &readval);
       if (rc == 0) {
           *rx_cdr_support = (readval >> 2) & 0x1;
           *tx_cdr_support = (readval >> 3) & 0x1;
       } else {
           return -1;
       }
   } else {
       *rx_cdr_support = false;
       *tx_cdr_support = false;
   }

   return 0;
}

/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_rx_cdr_ctrl_support (int port,
                         bool *rx_cdr_ctrl_support)
{
   int rc;
   uint8_t readval[3];

   if (port > bf_plt_max_qsfp) {
       return 0;
   }

   if (bf_qsfp_is_passive_cu (port)) {
       *rx_cdr_ctrl_support = false;
       return 0;
   }

   if (bf_qsfp_is_sff8636 (port)) {
       rc = bf_qsfp_field_read_onebank (
                port, OPTIONS, 0, 0, sizeof (readval),
                readval);
       if (rc == 0) {
           *rx_cdr_ctrl_support = (readval[1] >> 6) & 0x1;
       } else {
           return -1;
       }
   } else {
       *rx_cdr_ctrl_support = false;
   }

   return 0;
}

/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_tx_cdr_ctrl_support (int port,
                       bool *tx_cdr_ctrl_support)
{
   int rc;
   uint8_t readval[3];

   if (port > bf_plt_max_qsfp) {
       return 0;
   }

   if (bf_qsfp_is_passive_cu (port)) {
       *tx_cdr_ctrl_support = false;
       return 0;
   }

   if (bf_qsfp_is_sff8636 (port)) {
       rc = bf_qsfp_field_read_onebank (
                port, OPTIONS, 0, 0, sizeof (readval),
                readval);
       if (rc == 0) {
           *tx_cdr_ctrl_support = (readval[1] >> 7) & 0x1;
       } else {
           return -1;
       }
   } else {
       *tx_cdr_ctrl_support = false;
   }

   return 0;
}

/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_rxtx_cdr_ctrl_state (int port,
                         uint8_t *cdr_ctrl_state)
{
    int rc;
    uint8_t readval;

    if (port > bf_plt_max_qsfp) {
        return 0;
    }

    if (bf_qsfp_is_passive_cu (port)) {
        *cdr_ctrl_state = 0x00;
        return 0;
    }

    if (bf_qsfp_is_sff8636 (port)) {
       rc = bf_qsfp_field_read_onebank (
                port, CDR_CONTROL, 0, 0, sizeof (readval),
                &readval);
       if (rc == 0) {
           *cdr_ctrl_state = readval;
       } else {
           return -1;
       }
    } else {
        /* The CDR is enabled for most of QSFP28/QSFP+. */
        *cdr_ctrl_state = 0xFF;
    }

    return 0;
}

/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_rx_cdr_lol_support (int port,
                      bool *rx_cdr_lol_support)
{
    int rc;
    uint8_t readval[3];

    if (port > bf_plt_max_qsfp) {
        return 0;
    }

    if (bf_qsfp_is_passive_cu (port)) {
        *rx_cdr_lol_support = false;
        return 0;
    }

    if (bf_qsfp_is_sff8636 (port)) {
      rc = bf_qsfp_field_read_onebank (
               port, OPTIONS, 0, 0, sizeof (readval),
               readval);
      if (rc == 0) {
          *rx_cdr_lol_support = (readval[1] >> 4) & 0x1;
      } else {
          return -1;
      }
    } else {
      *rx_cdr_lol_support = false;
    }
    return 0;
}
/* TF1, by tsihang 2023-05-10. */
int bf_qsfp_get_tx_cdr_lol_support (int port,
                    bool *tx_cdr_lol_support)
{
    int rc;
    uint8_t readval[3];

    if (port > bf_plt_max_qsfp) {
        return 0;
    }

    if (bf_qsfp_is_passive_cu (port)) {
        *tx_cdr_lol_support = false;
        return 0;
    }

    if (bf_qsfp_is_sff8636 (port)) {
      rc = bf_qsfp_field_read_onebank (
               port, OPTIONS, 0, 0, sizeof (readval),
               readval);
      if (rc == 0) {
          *tx_cdr_lol_support = (readval[1] >> 5) & 0x1;
      } else {
          return -1;
      }
    } else {
      *tx_cdr_lol_support = false;
    }
    return 0;
}

int bf_qsfp_is_cdr_required (int port, bool *rx_cdr_required, bool *tx_cdr_required) {
    if (port > bf_plt_max_qsfp) {
        return 0;
    }
    *rx_cdr_required = !(bf_qsfp_info_arr[port].special_case_port.ctrlmask & BF_TRANS_CTRLMASK_RX_CDR_OFF);
    *tx_cdr_required = !(bf_qsfp_info_arr[port].special_case_port.ctrlmask & BF_TRANS_CTRLMASK_TX_CDR_OFF);
    return 0;
}

int bf_qsfp_ctrlmask_set (int port,
                  uint32_t ctrlmask) {
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    bf_qsfp_info_arr[port].special_case_port.ctrlmask = ctrlmask;

    return 0;
}

int bf_qsfp_ctrlmask_get (int port,
                uint32_t *ctrlmask) {
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    *ctrlmask = bf_qsfp_info_arr[port].special_case_port.ctrlmask;

    return 0;
}

bool bf_qsfp_is_force_overwrite (int port)
{
    if (port > bf_plt_max_qsfp) {
        return true;
    }
    return (bf_qsfp_info_arr[port].special_case_port.ctrlmask & BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT);
}

bool bf_qsfp_is_detected (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }

    return bf_qsfp_info_arr[port].fsm_detected;
}

void bf_qsfp_set_detected (int port, bool detected)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    /* Set fsm_detected indicator to notify health monitor thread
     * now it's ready to read the ddm of this port.
     */
    bf_qsfp_info_arr[port].fsm_detected = detected;
}

void bf_qsfp_print_ddm (int conn_id, qsfp_global_sensor_t *trans, qsfp_channel_t *chnl)
{
      LOG_DEBUG ("QSFP DDM Info for port %d",
          conn_id);
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "Port/Ch",
                  "Temp (C)",
                  "Voltage (V)",
                  "Bias(mA)",
                  "Tx Power (dBm)",
                  "Rx Power (dBm)");
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
      for (int i = 0; i < 4; i++) {
       qsfp_channel_t *c = &chnl[i];
       LOG_DEBUG ("      %d/%d %10.1f %15.2f %10.2f %16.2f %16.2f",
                  conn_id,
                  c->chn,
                  trans->temp.value,
                  trans->vcc.value,
                  c->sensors.tx_bias.value,
                  c->sensors.tx_pwr.value,
                  c->sensors.rx_pwr.value);
      }
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
}

/* Added by tsihang, 2023-05-10. */
static bf_qsfp_special_info_t bf_pltfm_special_transceiver_case[BF_PLAT_MAX_QSFP] = {
    {
        BF_PLTFM_QSFP_UNKNOWN,
        false,
        "Unknown",
        "Unknown",
        0x00000000,
    },
};

static inline int
bf_qsfp_tc_entry_cmp (bf_qsfp_special_info_t *tc_entry,
    char *vendor, char *pn, char *option)
{
    (void)option;
    if ((strncmp ((char *)vendor,
            tc_entry->vendor, strlen(&tc_entry->vendor[0])) == 0) &&
         (strncmp ((char *)pn,
             tc_entry->pn, strlen(tc_entry->pn)) == 0)) {
        return 0;
    }
    return -1;
}

int bf_qsfp_tc_entry_add (char *vendor, char *pn, char *option,
    uint32_t ctrlmask)
{
    int i;
    bf_qsfp_special_info_t *tc_entry;

    for (i = 0; i < BF_PLAT_MAX_QSFP; i ++) {
        tc_entry = &bf_pltfm_special_transceiver_case[i];
        if (tc_entry->is_set) continue;
        else {
            /* Find empty entry */
            strncpy (&tc_entry->vendor[0], vendor, 16);
            strncpy (&tc_entry->pn[0], pn, 16);
            //strncpy (&tc_entry->option[0], option, 16);
            tc_entry->ctrlmask = ctrlmask;
            tc_entry->is_set = true;
            LOG_DEBUG ("add entry[%d] : %-16s : %-16s : %-4x\n", i, vendor, pn, ctrlmask);
            return 0;
        }
    }
    return -1;
}

/* ctrlmask. */
int bf_qsfp_tc_entry_find (char *vendor, char *pn, char *option,
    uint32_t *ctrlmask, bool rmv)
{
    int i;
    bf_qsfp_special_info_t *tc_entry;

    for (i = 0; i < BF_PLAT_MAX_QSFP; i ++) {
        tc_entry = &bf_pltfm_special_transceiver_case[i];
        if (!tc_entry->is_set) continue;
        else {
            if (0 == bf_qsfp_tc_entry_cmp (tc_entry, vendor, pn, option)) {
                /* Find the entry and return its ctrlmask. */
                *ctrlmask = tc_entry->ctrlmask;
                LOG_DEBUG ("find entry[%d] : %-16s : %-16s : %-4x\n", i, vendor, pn, tc_entry->ctrlmask);
                if (rmv) {
                    /* Remove it if required. */
                    LOG_DEBUG ("remove entry[%d] : %-16s : %-16s : %-4x\n", i, vendor, pn, tc_entry->ctrlmask);
                    tc_entry->is_set = false;
                }
                return 0;
            }
        }
    }
    return -1;
}

int bf_qsfp_tc_entry_dump (char *vendor, char *pn, char *option)
{
    int i;
    bf_qsfp_special_info_t *tc_entry;

    for (i = 0; i < BF_PLAT_MAX_QSFP; i ++) {
        tc_entry = &bf_pltfm_special_transceiver_case[i];
        if (!tc_entry->is_set) continue;
        else {
            if (vendor && pn /* && option */) {
                if (0 == bf_qsfp_tc_entry_cmp (tc_entry, vendor, pn, option)) {
                    LOG_DEBUG ("find entry[%d] : %-16s : %-16s : %-4x",
                        i, tc_entry->vendor, tc_entry->pn, tc_entry->ctrlmask);
                    return 0;
                }
            } else {
                /* Dump all if vendor=NULL and pn=NULL */
                LOG_DEBUG ("dump entry[%d] : %-16s : %-16s : %-4x",
                        i, tc_entry->vendor, tc_entry->pn, tc_entry->ctrlmask);
            }
        }
    }
    return -1;
}

/* Load /etc/transceiver-cases.conf for specail transceiver case in some critical scenarios.
 * The CTRLMASK (hex value) is defined in $SDE_INSTALL/include/bf_qsfp/bf_qsfp.h
 * An example of the file like below:
 * #VENDOR         PN                 CTRLMASK
 * Teraspek        TSQ885S101T1       1=HEX(BF_TRANS_CTRLMASK_CDR_OFF)
 * Teraspek        TSQ885S101E1       1=HEX(BF_TRANS_CTRLMASK_CDR_OFF)
 * Asterfusion     TSQ813L023CT1      3=HEX(BF_TRANS_CTRLMASK_CDR_OFF|BF_TRANS_CTRLMASK_LASER_OFF)
 */
void bf_pltfm_qsfp_load_conf ()
{
    // Initialize all the sub modules
    FILE *fp = NULL;
    const char *cfg = "/etc/transceiver-cases.conf";
    char entry[256 + 1] = {0};
    char vendor[16];
    char pn[16];
    char option[16] = {0};
    uint32_t ctrlmask;
    int length, num_entries = 0;

    char *desc = "#The CTRLMASK defined in $SDE_INSTALL/include/bf_qsfp/bf_qsfp.h as below:\n" \
        "#  #define BF_TRANS_CTRLMASK_RX_CDR_OFF        (1 << 0)\n"   \
        "#  #define BF_TRANS_CTRLMASK_TX_CDR_OFF        (1 << 1)\n"   \
        "#  #define BF_TRANS_CTRLMASK_LASER_OFF         (1 << 2)\n"   \
        "#  #define BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT (1 << 7)\n";

    int i;
    bf_qsfp_special_info_t *tc_entry;

    for (i = 0; i < BF_PLAT_MAX_QSFP; i ++) {
        tc_entry = &bf_pltfm_special_transceiver_case[i];
        tc_entry->qsfp_type = BF_PLTFM_QSFP_UNKNOWN;
        tc_entry->is_set = false;
        tc_entry->ctrlmask = 0;
        memset (&tc_entry->vendor[0], 0, 16);
        memset (&tc_entry->pn[0], 0, 16);

    }

    fp = fopen (cfg, "r");
    if (!fp) {
        LOG_WARNING (
            "fopen(%s) : "
                 "%s. Trying to create one.\n", cfg, strerror (errno));
        /* Create a new one and write known transceivers to it */
        fp = fopen (cfg, "w+");
        if (!fp) {
            LOG_ERROR (
                "fcreate(%s) : "
                     "%s.\n", cfg, strerror (errno));
            exit(0);
        }

        /* Write desc and examples. */
        fwrite (desc, 1, strlen (desc), fp);
        length = sprintf (entry, "%-16s   %-16s   %-16s\n", "#VENDOR", "#PN", "#CTRLMASK");
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-16s\n", "#WTD", "RTXM420-010", "01=HEX(BF_TRANS_CTRLMASK_RX_CDR_OFF)");
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-16s\n", "#Asterfusion", "TSQ885S101T1", "07=HEX(BF_TRANS_CTRLMASK_RX_CDR_OFF|BF_TRANS_CTRLMASK_TX_CDR_OFF|BF_TRANS_CTRLMASK_LASER_OFF)");
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-16s\n", "#Asterfusion", "TSQ813L103H1", "81=HEX(BF_TRANS_CTRLMASK_RX_CDR_OFF|BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT)");
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-16s\n", "#Asterfusion", "TSQ885S101E1", "83=HEX(BF_TRANS_CTRLMASK_RX_CDR_OFF|BF_TRANS_CTRLMASK_TX_CDR_OFF|BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT)");
        fwrite (entry, 1, length, fp);

        /* Write real ones. */
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "WTD", "RTXM420-010", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "Teraspek", "TSQ885S101T1", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "Asterfusion", "TSQ885S101T1", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "Asterfusion", "TSQ813L103H1", 0x81);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "Teraspek", "TSQ885S101E1", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "Teraspek", "TSO885S030E1", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "FINISAR", "FTLC1157RGPL-TF", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "FINISAR", "FTLC1154RGPL", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "FINISAR", "FCBN425QB2C07", 3);
        fwrite (entry, 1, length, fp);
        length = sprintf (entry, "%-16s   %-16s   %-4x\n", "FINISAR", "FCBN425QB2C05", 3);
        fwrite (entry, 1, length, fp);

        fflush(fp);
    }

    /* Read all entries back. */
    fprintf (stdout,
             "\n\nLoading %s ...", cfg);

    /* To the start-of-file. */
    fseek(fp, 0, SEEK_SET);
    while (fgets (entry, 256, fp)) {
        if (entry[0] == '#') {
            continue;
        }
        sscanf (entry, "%s   %s   %x", &vendor[0], &pn[0], &ctrlmask);
        if (0 == bf_qsfp_tc_entry_add (vendor, pn, option, ctrlmask))
            num_entries ++;
        memset (&entry[0], 0, 256);
    }
    fclose (fp);
    fprintf (stdout,
                "   done(%d entries)\n", num_entries);
}
