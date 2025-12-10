/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_eeprom.c
 * @date
 *
 * EEPROM parsing
 *
 */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"
#include "bf_pltfm_master_i2c.h"
#include "bf_pltfm_uart.h"

#define lqe_valen       256
#define EEPROM_SIZE     256
char eeprom_raw_data[EEPROM_RAW_DATA_SIZE];

bf_pltfm_eeprom_t eeprom;

uint8_t eeprom_data[EEPROM_SIZE] = {
    0x54, 0x6c, 0x76, 0x49, 0x6e, 0x66, 0x6f, 0x00, 0x01, 0x00
};

static bf_pltfm_board_id_t bd_id =
    BF_PLTFM_BD_ID_UNKNOWN;

struct bf_pltfm_board_ctx_t {
    bf_pltfm_board_id_t id;
    const char *desc;
    bf_pltfm_type type;
    bf_pltfm_subtype subtype;
};

/* New method of board_ctx_t decareled.
 * AFN_BD_ID_X532PT_V1P1 means X532P-T subversion 1.1. */
static struct bf_pltfm_board_ctx_t bd_ctx[] = {
    {AFN_BD_ID_X532PT_V1P0, "APNS320T-A1-V1.0", AFN_X532PT, V1P0},  // X532P-T V1.0
    {AFN_BD_ID_X532PT_V1P1, "APNS320T-A1-V1.1", AFN_X532PT, V1P1},  // X532P-T V1.1
    {AFN_BD_ID_X532PT_V2P0, "APNS320T-A1-V2.0", AFN_X532PT, V2P0},  // X532P-T V2.0
    {AFN_BD_ID_X532PT_V3P0, "APNS320T-A1-V3.0", AFN_X532PT, V3P0},  // X532P-T V3.0

    {AFN_BD_ID_X564PT_V1P0, "APNS640T-A1-V1.0", AFN_X564PT, V1P0},  // X564P-T V1.0
    {AFN_BD_ID_X564PT_V1P1, "APNS640T-A1-V1.1", AFN_X564PT, V1P1},  // X564P-T V1.1
    {AFN_BD_ID_X564PT_V1P2, "APNS640T-A1-V1.2", AFN_X564PT, V1P2},  // X564P-T V1.2
    {AFN_BD_ID_X564PT_V2P0, "APNS640T-A1-V2.0", AFN_X564PT, V2P0},  // X564P-T V2.0

    {AFN_BD_ID_X308PT_V1P0, "APNS320T-B1-V1.0", AFN_X308PT, V1P0},  // X308P-T V1.0
    {AFN_BD_ID_X308PT_V1P1, "APNS320T-B1-V1.1", AFN_X308PT, V1P1},  // X308P-T V1.1, 4x 1G.
    {AFN_BD_ID_X308PT_V2P0, "APNS320T-B1-V1.1", AFN_X308PT, V2P0},  // Announced as v2.0.
    {AFN_BD_ID_X308PT_V3P0, "APNS320T-C1-V1.0", AFN_X308PT, V3P0},  // Announced as v3.0 with PTP hwcomp.
    /* More X308P-T board id. */

    {AFN_BD_ID_X732QT_V1P0, "APNS1280T2-A1-V1.0", AFN_X732QT, V1P0},
    {AFN_BD_ID_X732QT_V1P1, "APNS1280T2-A1-V1.1", AFN_X732QT, V1P1},
    {AFN_BD_ID_X732QT_V2P0, "APNS1280T2-A1-V2.0", AFN_X732QT, V2P0},
    /* More AFN_X732QT board id. */
    {AFN_BD_ID_HC36Y24C_V1P0, "Hello this is AFN_HC36Y24C", AFN_HC36Y24C, V1P0},

    {AFN_BD_ID_X312PT_V1P1,   "AFN_X312PT-V1.x",     AFN_X312PT, V1P0},
    {AFN_BD_ID_X312PT_V2P0,   "AFN_X312PT-V2.x",     AFN_X312PT, V2P0},
    {AFN_BD_ID_X312PT_V3P0,   "AFN_X312PT-V3.x",     AFN_X312PT, V3P0},
    {AFN_BD_ID_X312PT_V4P0,   "AFN_X312PT-V4.x",     AFN_X312PT, V4P0},
    {AFN_BD_ID_X312PT_V5P0,   "AFN_X312PT-V5.x",     AFN_X312PT, V5P0},
    /* As I know so far, product name is not a suitable way to distinguish platforms
     * from X-T to CX-T/PX-T, so how to distinguish X312P and HC ???
     * by tsihang, 2021-07-14. */
};

struct tlv_t tlvs[] = {
    {0x21, "Product Name"},
    {0x22, "Part Number"},
    {0x23, "Serial Number"},
    {0x24, "Base MAC Addre"},
    {0x25, "Manufacture Date"},
    {0x26, "Product Version"},
    {0x27, "Product Subversion"},
    {0x28, "Platform Name"},
    {0x29, "Onie Version"},         // new
    {0x2A, "MAC Addresses"},
    {0x2B, "Manufacturer"},
    {0x2C, "Country Code"},
    {0x2D, "Vendor Name"},
    {0x2E, "Diag Version"},         // new
    {0x2F, "Service Tag"},
    {0x30, "Switch ASIC Vendor"},
    {0x31, "Main Board Version"},   // up
    {0x32, "COME Version"},         // down
    {0x33, "GHC-0 Board Version"},  // DPU-1
    {0x34, "GHC-1 Board Version"},  // DPU-2
    {0xFE, "CRC-32"}
};

static inline uint8_t ctoi (char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return 0xA + c - 'a';
    } else if (c >= 'A' && c <= 'F') {
        return 0xA + c - 'A';
    } else {
        return 0;
    }
}

bf_pltfm_status_t bf_pltfm_bd_type_get (
    bf_pltfm_board_id_t *board_id)
{
    *board_id = bd_id;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_type_set_by_key (uint8_t type, uint8_t subtype)
{
    unsigned int i;

    for (i = 0; i < (int)ARRAY_LENGTH (bd_ctx); i++) {
        if (bd_ctx[i].type == type &&
            bd_ctx[i].subtype == subtype) {
            bd_id = bd_ctx[i].id;
            bf_pltfm_bd_type_set_priv (bd_ctx[i].type, bd_ctx[i].subtype);
            LOG_DEBUG ("Board type : %04x : %s",
                       bd_id,
                       (bf_pltfm_mgr_ctx()->pltfm_type == AFN_X564PT ? "AFN_X564PT" :
                        bf_pltfm_mgr_ctx()->pltfm_type == AFN_X532PT ? "AFN_X532PT" :
                        bf_pltfm_mgr_ctx()->pltfm_type == AFN_X308PT ? "AFN_X308PT" :
                        bf_pltfm_mgr_ctx()->pltfm_type == AFN_X312PT ? "AFN_X312PT" :
                        bf_pltfm_mgr_ctx()->pltfm_type == AFN_X732QT ? "AFN_X732QT" :
                        "AFN_HC36Y24C"));
            return 0;
        }
    }

    return -1;
}

/**
 * Parse product name in EEPROM data.
 * An ASCII string containing the product name.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_name (const char *index)
{
    char str[21];
    uint8_t i = 0;

    while (i < 20) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';
    strncpy (eeprom.bf_pltfm_product_name, str, i + 1);

    //LOG_DEBUG ("Product Name: %s \n",
    //           eeprom.bf_pltfm_product_name);

    return 0;
}

/**
 * Parse product number in EEPROM data.
 * An ASCII string containing the vendor’s part number for the device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_number (const char *index)
{
    size_t len = 0;

    len = sizeof (eeprom.bf_pltfm_product_number) - 1;

    strncpy (eeprom.bf_pltfm_product_number, index,
             len);
    eeprom.bf_pltfm_product_number[len] = '\0';

    /* XXXX-XXXX-PTP-XX */
    if (strstr (eeprom.bf_pltfm_product_number, "PTP")) {
        /* Assume that there is PTP board installed if able to find "PTP" from EEPROM. */
        bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_PTPX_INSTALLED;
    }

    //LOG_DEBUG ("Product Number: %s \n",
    //           eeprom.bf_pltfm_product_number);

    return 0;
}

/**
 * Parse Product Serial number in EEPROM data.
 * An ASCII string containing the serial number of the device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_serial_number (const char
                                  *index)
{
    char str[13];
    uint8_t i = 0;

    while (i < 12) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';

    strncpy (eeprom.bf_pltfm_product_serial, str, i + 1);

    //LOG_DEBUG ("Product Serial Number: %s \n",
    //           eeprom.bf_pltfm_product_serial);

    return 0;
}

/**
 * Parse Extended MAC Base address in EEPROM data.
 * The base MAC address for this device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int ext_mac_base (const char *index)
{
    uint8_t i = 0;
    char hex[3];

    for (; i < 6; i++) {
        hex[0] = index[0];
        hex[1] = index[1];
        hex[2] = '\0';
        index = index + 3;
        eeprom.bf_pltfm_mac_base[i] = (uint8_t)strtol (
                                          hex, NULL, 16);
    }

    //LOG_DEBUG ("Extended MAC Base %x:%x:%x:%x:%x:%x\n",
    //           eeprom.bf_pltfm_mac_base[0],
    //           eeprom.bf_pltfm_mac_base[1],
    //           eeprom.bf_pltfm_mac_base[2],
    //           eeprom.bf_pltfm_mac_base[3],
    //           eeprom.bf_pltfm_mac_base[4],
    //           eeprom.bf_pltfm_mac_base[5]);
    return 0;
}

/**
 * Parse system manufacturing date in EEPROM data.
 * An ASCII string that specifies when the device was manufactured.
 * @param ptr pointer to where data exists
 * @return status
 */
static int system_manufacturing_date (
    const char *index)
{
    size_t len = 0;


    len = sizeof (
              eeprom.bf_pltfm_system_manufacturing_date) - 1;

    strncpy (eeprom.bf_pltfm_system_manufacturing_date,
             index, len);
    eeprom.bf_pltfm_system_manufacturing_date[len] =
        '\0';

    //LOG_DEBUG ("System Manufacturing Date: %s \n",
    //           eeprom.bf_pltfm_system_manufacturing_date);

    return 0;
}

/**
 * Parse Product Version in EEPROM data.
 * A single byte indicating the version, or revision, of the device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_version (const char *index)
{
    char str[4];
    uint8_t i = 0, j = 0;

    while (i < 3) {
        if ((index[i] >= '0') && (index[i] <= '9')) {
            str[j++] = index[i];
        }
        i++;
    }
    str[j] = '\0';
    eeprom.bf_pltfm_product_version = atoi (str);

    //LOG_DEBUG ("Product Version: %d \n",
    //           eeprom.bf_pltfm_product_version);

    return 0;
}

/**
 * Parse Product Sub Version in EEPROM data.
 * An ASCII string containing the label revision.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_subversion (const char *index)
{
    char str[4];
    uint8_t i = 0, j = 0;

    while (i < 3) {
        if ((index[i] >= '0') && (index[i] <= '9')) {
            str[j++] = index[i];
        }
        i++;
    }
    strcpy (eeprom.bf_pltfm_product_subversion,
        str);
    str[j] = '\0';

    //LOG_DEBUG ("Product Sub-Version: %s \n",
    //           eeprom.bf_pltfm_product_subversion);

    return 0;
}

/**
 * Parse Platform Name in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_arch (const char *index)
{
    size_t len = 0;


    len = sizeof (
              eeprom.bf_pltfm_product_platform_arch) - 1;

    strncpy (eeprom.bf_pltfm_product_platform_arch,
             index, len);
    eeprom.bf_pltfm_product_platform_arch[len] = '\0';

    //LOG_DEBUG ("System Platform Arch: %s \n",
    //           eeprom.bf_pltfm_product_platform_arch);

    return 0;
}

/**
 * Parse ONIE version in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int onie_version (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_onie_version) - 1;

    strncpy (eeprom.bf_pltfm_onie_version, index,
             len);
    eeprom.bf_pltfm_onie_version[len] = '\0';

    //LOG_DEBUG ("System ONIE Version: %s \n",
    //           eeprom.bf_pltfm_onie_version);

    return 0;
}

/**
 * Parse Extended Mac addr size in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int ext_mac_size (const char *index)
{
    char str[6];
    uint8_t i = 0, j = 0;

    while (i < 5) {
        if ((index[i] >= '0') && (index[i] <= '9')) {
            str[j++] = index[i];
        }
        i++;
    }
    str[j] = '\0';
    eeprom.bf_pltfm_mac_size = atoi (str);

    //LOG_DEBUG ("Extended MAC Address Size: %d \n",
    //           eeprom.bf_pltfm_mac_size);

    return 0;
}

/**
 * Parse system manufacturer in EEPROM data.
 * An ASCII string containing the name of the entity that manufactured the device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int system_manufacturer (const char
                                *index)
{
    size_t len = 0;

    len = sizeof (eeprom.bf_pltfm_system_manufacturer)
          - 1;

    strncpy (eeprom.bf_pltfm_system_manufacturer,
             index, len);
    eeprom.bf_pltfm_system_manufacturer[len] = '\0';

    //LOG_DEBUG ("System Manufacturer: %s \n",
    //           eeprom.bf_pltfm_system_manufacturer);

    return 0;
}

/**
 * Parse Platform Name in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int country_code (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_cc) - 1;

    strncpy (eeprom.bf_pltfm_cc, index, len);
    eeprom.bf_pltfm_cc[len] = '\0';

    //LOG_DEBUG ("Country Code: %s \n",
    //           eeprom.bf_pltfm_cc);

    return 0;
}

/**
 * Parse Vendor Name in EEPROM data.
 * The name of the vendor who contracted
 * with the manufacturer for the production
 * of this device. This is typically the company name
 * on the outside of the device.
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_vendor (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_product_vendor) - 1;

    strncpy (eeprom.bf_pltfm_product_vendor, index,
             len);
    eeprom.bf_pltfm_product_vendor[len] = '\0';

    //LOG_DEBUG ("Product Vendor: %s \n",
    //           eeprom.bf_pltfm_product_vendor);

    return 0;
}

/**
 * Parse An ASCII string containing the version of the diagnostic software in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int diag_version (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_diag_version) - 1;

    strncpy (eeprom.bf_pltfm_diag_version, index,
             len);
    eeprom.bf_pltfm_diag_version[len] = '\0';

    //LOG_DEBUG ("Diag Version: %s \n",
    //           eeprom.bf_pltfm_diag_version);

    return 0;
}

/**
 * Parse An ASCII string containing a vendor defined service tag in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int serv_tag (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_serv_tag) - 1;

    strncpy (eeprom.bf_pltfm_serv_tag, index, len);
    eeprom.bf_pltfm_serv_tag[len] = '\0';

    //LOG_DEBUG ("Service Tag: %s \n",
    //           eeprom.bf_pltfm_serv_tag);

    return 0;
}

/**
 * Parse ASIC Vendor in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int asic_vendor (const char *index)
{
    size_t len = 0;


    len = sizeof (eeprom.bf_pltfm_asic_vendor) - 1;

    strncpy (eeprom.bf_pltfm_asic_vendor, index, len);
    eeprom.bf_pltfm_asic_vendor[len] = '\0';

    //LOG_DEBUG ("ASIC Vendor: %s \n",
    //           eeprom.bf_pltfm_asic_vendor);

    return 0;
}

/**
 * Parse Product Main Board Version in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int product_main_board_version (
    const char *index)
{
    char str[33];
    uint8_t i = 0;

    while (i < 32) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';

    strncpy (eeprom.bf_pltfm_main_board_version, str,
             i + 1);

    //LOG_DEBUG ("Product Main Board Version: %s \n",
    //           eeprom.bf_pltfm_main_board_version);

    return 0;
}

/**
 * Parse Product Main Board Version in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int comexpress_version (const char *index)
{
    char str[33];
    uint8_t i = 0;

    while (i < 32) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';

    strncpy (eeprom.bf_pltfm_come_version, str, i + 1);

    //LOG_DEBUG ("Product COMe Board Version: %s \n",
    //           eeprom.bf_pltfm_come_version);

    return 0;
}

static int ghc_bd0_version (const char *index)
{
    char str[33];
    uint8_t i = 0;

    while (i < 32) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';

    strncpy (eeprom.bf_pltfm_ghc_bd0_version, str, i + 1);
    /* GHCXXX-YYYYYYYYY */
    if (eeprom.bf_pltfm_ghc_bd0_version[0] == 'G') {
        /* Assume that there is a DPU installed to this slot if able to get version from EEPROM.
         * Later, we will try to get temp to see whether there is a real one.
         * by tsihang, 2023/11/09. */
        bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_DPU1_INSTALLED;
    }
    //LOG_DEBUG ("Product Computing Card0 Version: %s \n",
    //           eeprom.bf_pltfm_ghc_bd0_version);

    return 0;
}

static int ghc_bd1_version (const char *index)
{
    char str[33];
    uint8_t i = 0;

    while (i < 32) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';

    strncpy (eeprom.bf_pltfm_ghc_bd1_version, str, i + 1);
    /* GHCXXX-YYYYYYYYY */
    if (eeprom.bf_pltfm_ghc_bd1_version[0] == 'G') {
        /* Assume that there is a DPU installed to this slot if able to get version from EEPROM.
         * Later, we will try to get temp to see whether there is a real one.
         * by tsihang, 2023/11/09. */
        bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_DPU2_INSTALLED;
    }
    //LOG_DEBUG ("Product Computing Card1 Version: %s \n",
    //           eeprom.bf_pltfm_ghc_bd1_version);

    return 0;
}

/**
 * Parse CRC32 in EEPROM data
 * @param ptr pointer to where data exists
 * @return status
 */
static int eecrc32 (const char *index)
{
    char str[11];
    uint8_t i = 0;

    while (i < 10) {
        if ((index[i] == '\'') || (index[i] == '\"')) {
            break;
        }

        str[i] = index[i];
        i++;
    }
    str[i] = '\0';
    eeprom.bf_pltfm_crc32 = (uint32_t)strtol (str,
                            NULL, 0);

    //LOG_DEBUG ("CRC32: %d \n", eeprom.bf_pltfm_crc32);

    return 0;
}

static void further_decode ()
{
    int i;
    char temp[32] = {0};
    struct tlv_t *tlv;

    if (platform_type_equal (AFN_X312PT)) {
        for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
            tlv = &tlvs[i];
            memset (temp, 0, 32);
            if (tlv->code == 0x24) {
                sprintf(temp, "%02X:%02X:%02X:%02X:%02X:%02X", (uint8_t)tlv->content[0],
                    (uint8_t)tlv->content[1], (uint8_t)tlv->content[2], (uint8_t)tlv->content[3],
                    (uint8_t)tlv->content[4], (uint8_t)tlv->content[5]);
                memcpy(tlv->content, (uint8_t *)temp, 32);
            } else if(tlv->code == 0x2a) {
                sprintf(temp, "%d", tlv->content[1]);
                memcpy(tlv->content, (uint8_t *)temp, 1);
            } else if(tlv->code == 0xfe) {
                sprintf(temp, "0x%02X%02X%02X%02X", (uint8_t)tlv->content[0],
                    (uint8_t)tlv->content[1], (uint8_t)tlv->content[2], (uint8_t)tlv->content[3]);
                memcpy(tlv->content, (uint8_t *)temp, 32);
            } else if(tlv->code == 0x26) {
                if (!strstr (tlv->content, "5") &&
                    !strstr (tlv->content, "4") &&
                    !strstr (tlv->content, "3") &&
                    !strstr (tlv->content, "2")) {
                    fprintf (stdout,
                             "** Invalid data in eeprom sections (0x%02x), This may occur errors while running.\n",
                             tlv->code);
                }
                if (strstr (tlv->content, "5")) {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V5.x", eeprom.bf_pltfm_product_name);
                } else if (strstr (tlv->content, "4")) {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V4.x", eeprom.bf_pltfm_product_name);
                } else if (strstr (tlv->content, "2")) {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V2.x", eeprom.bf_pltfm_product_name);
                } else {
				    /* default to V3.x */
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V3.x", eeprom.bf_pltfm_product_name);
                }
            }
        }
    }
    /* Further decode for other platform. */
}

/*
 * Example format of EEPROM returned by onie_syseeprom
 *
 *   msh />onie_syseeprom -a
 *   TlvInfo Header:
 *      Id String:    TlvInfo
 *      Version:      1
 *      Total Length: 199
 *   TLV Name                   Code Len Value
 *   --------------------       ---- --- -----
 *   Product Name               0x21   7 X532P-T
 *   Part Number                0x22  14 ONBP1U-2Y32C-E
 *   Serial Number              0x23   9 201000001
 *   BASE MAC Address           0x24   6 60:EB:5A:00:7D:04
 *   Manufacture Date           0x25  19 17/07/2019 23:59:59
 *   Device Version             0x26   1 1
 *   Label Revision             0x27   1 0
 *   Platform Name              0x28  32 x86_64-asterfusion_congad1519-r0
 *   Onie Version               0x29  25 master-202008111723-dirty
 *   MAC Addresses              0x2A   2 1
 *   Manufacturer               0x2B  11 Asterfusion
 *   Country Code               0x2C   2 CN
 *   Vendor Name                0x2D  11 Asterfusion
 *   Diag Version               0x2E   3 1.0
 *   Service Tag                0x2F   1 X
 *   Switch Vendor              0x30   5 Intel
 *   Main Board Version         0x31  16 APNS320T-A1-V1.0
 *   COME Version               0x32   0
 *   GHC-0/DPU-1 Board Version  0x33   0
 *   GHC-1/DPU-2 Board Version  0x34   0
 *   CRC-32                     0xFE   4 0xA5FE50A4
 *   Checksum is valid.
 *   msh />
 */
bf_pltfm_status_t bf_pltfm_bd_type_init()
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    char cmd = 0x01;
    int i;
    int err, err_total = 0;
    int usec_delay;
    size_t l = 0, offset = 0;
    struct tlv_t *tlv;
    FILE *fp = NULL;
    const char *path = LOG_DIR_PREFIX"/eeprom";

    fprintf (stdout,
             "\n\nReading EEPROM ...\n");

    for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
        /* Read EEPROM. */
        tlv = &tlvs[i];
        wr_buf[0] = tlv->code;
        wr_buf[1] = 0xAA;

        memset (rd_buf, 0, 128);

        // Must give bmc more time to prepare data
        usec_delay = BMC_COMM_INTERVAL_US;
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
            err = bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                                                2, rd_buf, 128 - 1, usec_delay);
        } else {
            //usec_delay = ((tlvs[i].code == 0x21) || (tlvs[i].code == 0x22) || (tlvs[i].code == 0x23) || (tlvs[i].code == 0x24)) ?
            //       BMC_COMM_INTERVAL_US * 5 : BMC_COMM_INTERVAL_US;
            /* There're some old device such as x564p-t gets eeprom data through i2c (cgosdrv). */
            usec_delay = bf_pltfm_get_312_bmc_comm_interval();
            err = bf_pltfm_bmc_write_read (bmc_i2c_addr, cmd,
                                           wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf), usec_delay);
        }

        if (err > 0) {
            if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
                l = strlen ((char *)&rd_buf[0]);
                offset = 0;
            } else {
                l = rd_buf[0];
                offset = 1;
            }

            memcpy (tlv->content, &rd_buf[offset], l);
            LOG_DEBUG ("0x%02x %20s: %32s", tlv->code,
                       tlv->desc, tlv->content);
        } else {
            err_total ++;
        }
    }
    fprintf (stdout, "\n\n");

    /* A high risk from this condition. by tsihang, 2023-05-26. */
    /* If could not get eeprom data from BMC,
     *  try to get it from file. by huachen, 2023-05-23. */
    if (err_total >= (int)ARRAY_LENGTH (tlvs)) {
        fprintf (stdout, "\n\nWarn : Trying to get eeprom data from %s\n\n",
            path);
        LOG_WARNING ("\n\nWarn : Trying to get eeprom data from %s\n\n",
            path);
        fp = fopen(path, "r");
        if (!fp) {
            LOG_ERROR ("\n\nError : Launch failed\n\n");
            /* First boot error, no chance to rescure, exit forcely. */
            exit (0);
        } else {
            char tlv_str[lqe_valen + 1];
            char tlv_header[128];
            char *p;
            while (fgets (tlv_str, lqe_valen, fp)) {
                p = strchr(tlv_str, 0x0A);
                *p = '\0';
                for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
                    tlv = &tlvs[i];
                    sprintf (tlv_header, "%s 0x%02x ", tlv->desc, tlv->code);
                    p = strstr (tlv_str, tlv_header);
                    if(p) {
                        strcpy (tlv->content, p + strlen(tlv_header));
                        LOG_DEBUG ("0x%02x %20s: %32s", tlv->code,
                                tlv->desc, tlv->content);
                        break;
                    }
                }
            }
            fclose(fp);
        }
        /* An error detected from uart/sio, so here disable health montor in case of risk. */
        bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_CTRL;
    }

    /**
     * Parse EEPROM data received from OPENBMC.
     * @return status
     */
    char data[2048];
    int length;

    for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
        tlv = &tlvs[i];
        if (!tlv->content) {
            continue;
        }
        switch (tlv->code) {
            case 0x21:
                product_name (tlv->content);
                break;
            case 0x22:
                product_number (tlv->content);
                break;
            case 0x23:
                product_serial_number (tlv->content);
                break;
            case 0x24:
                ext_mac_base (tlv->content);
                break;
            case 0x25:
                system_manufacturing_date (tlv->content);
                break;
            case 0x26:
                product_version (tlv->content);
                break;
            case 0x27:
                product_subversion (tlv->content);
                break;
            case 0x28:
                product_arch (tlv->content);
                break;
            case 0x29:
                onie_version (tlv->content);
                break;
            case 0x2A:
                ext_mac_size (tlv->content);
                break;
            case 0x2B:
                system_manufacturer (tlv->content);
                break;
            case 0x2C:
                country_code (tlv->content);
                break;
            case 0x2D:
                product_vendor (tlv->content);
                break;
            case 0x2E:
                diag_version (tlv->content);
                break;
            case 0x2F:
                serv_tag (tlv->content);
                break;
            case 0x30:
                asic_vendor (tlv->content);
                break;
            case 0x31:
                product_main_board_version (tlv->content);
                break;
            case 0x32:
                comexpress_version (tlv->content);
                break;
            case 0x33:
                ghc_bd0_version (tlv->content);
                break;
            case 0x34:
                ghc_bd1_version (tlv->content);
                break;
            case 0xFD:
                break;
            case 0xFE:
                eecrc32 (tlv->content);
                break;
            default:
                fprintf (stdout,
                         "Exiting due to the sections (0x%02x) is out of scope.\n", tlv->code);
                LOG_ERROR(
                         "Exiting due to the sections (0x%02x) is out of scope.\n", tlv->code);
                exit (0);
        }
    }

    /* Further decode when platform detected.
     * by tsihang, 2022-05-12. */
    further_decode ();

    /* Open eeprom data file, if non-exist, create it.
     * For onlp, should run BSP at least once. */
    if (unlikely (!access (path, F_OK))) {
        if (likely (remove (path))) {
            fprintf (stdout,
                     "%s[%d], "
                     "access(%s)"
                     "\n",
                     __FILE__, __LINE__, oryx_safe_strerror (errno));
        }
    }

    fp = fopen (path, "w+");
    if (fp) {
        for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
            tlv = &tlvs[i];
            length = sprintf (data, "%s 0x%02x %s\n",
                              tlv->desc, tlv->code & 0xFF, tlv->content);
            fwrite (data, 1, length, fp);
        }
        fflush (fp);
        fclose (fp);
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_eeprom_get (
    bf_pltfm_eeprom_t *ee_data)
{
    memcpy (ee_data, &eeprom,
            sizeof (bf_pltfm_eeprom_t));
    return BF_PLTFM_SUCCESS;
}

const char *bf_pltfm_bd_vendor_get (void)
{
    return (char *)&eeprom.bf_pltfm_product_vendor[0];
}

const char *bf_pltfm_bd_product_name_get (void)
{
    return (char *)&eeprom.bf_pltfm_product_name[0];
}

bf_pltfm_status_t bf_pltfm_bd_tlv_get (int idx,
                                       char *code, char *desc, char *content)
{
    *code = tlvs[idx].code;
    strcpy (desc,    tlvs[idx].desc);
    strcpy (content, tlvs[idx].content);
    return 0;
}
