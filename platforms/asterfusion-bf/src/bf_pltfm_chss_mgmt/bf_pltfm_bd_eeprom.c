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

#include <pltfm_types.h>

#define lqe_valen       256
#define EEPROM_SIZE     256

bf_pltfm_eeprom_t eeprom;

static const unsigned int crc32tab[] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL,
    0x076dc419L, 0x706af48fL, 0xe963a535L, 0x9e6495a3L,
    0x0edb8832L, 0x79dcb8a4L, 0xe0d5e91eL, 0x97d2d988L,
    0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L, 0x90bf1d91L,
    0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L,
    0x136c9856L, 0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL,
    0x14015c4fL, 0x65646cd9L, 0xfa0f3d63L, 0x8d080df5L,
    0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L, 0xa2677172L,
    0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L,
    0x32d86ce3L, 0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L,
    0x26d930acL, 0x51de003aL, 0xc8d75180L, 0xbfd06116L,
    0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L, 0xb8bda50fL,
    0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL,
    0x76dc4190L, 0x01db7106L, 0x98d220bcL, 0xefd5102aL,
    0x71b18589L, 0x06b6b51fL, 0x9fbfe4a5L, 0xe8b8d433L,
    0x7807c9a2L, 0x0f00f934L, 0x9609a88eL, 0xe10e9818L,
    0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL,
    0x6c0695edL, 0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L,
    0x65b0d9c6L, 0x12b7e950L, 0x8bbeb8eaL, 0xfcb9887cL,
    0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L, 0xfbd44c65L,
    0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL,
    0x4369e96aL, 0x346ed9fcL, 0xad678846L, 0xda60b8d0L,
    0x44042d73L, 0x33031de5L, 0xaa0a4c5fL, 0xdd0d7cc9L,
    0x5005713cL, 0x270241aaL, 0xbe0b1010L, 0xc90c2086L,
    0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L,
    0x59b33d17L, 0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL,
    0xedb88320L, 0x9abfb3b6L, 0x03b6e20cL, 0x74b1d29aL,
    0xead54739L, 0x9dd277afL, 0x04db2615L, 0x73dc1683L,
    0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L,
    0xf00f9344L, 0x8708a3d2L, 0x1e01f268L, 0x6906c2feL,
    0xf762575dL, 0x806567cbL, 0x196c3671L, 0x6e6b06e7L,
    0xfed41b76L, 0x89d32be0L, 0x10da7a5aL, 0x67dd4accL,
    0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L,
    0xd1bb67f1L, 0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL,
    0xd80d2bdaL, 0xaf0a1b4cL, 0x36034af6L, 0x41047a60L,
    0xdf60efc3L, 0xa867df55L, 0x316e8eefL, 0x4669be79L,
    0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL,
    0xc5ba3bbeL, 0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L,
    0xc2d7ffa7L, 0xb5d0cf31L, 0x2cd99e8bL, 0x5bdeae1dL,
    0x9b64c2b0L, 0xec63f226L, 0x756aa39cL, 0x026d930aL,
    0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L,
    0x92d28e9bL, 0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L,
    0x86d3d2d4L, 0xf1d4e242L, 0x68ddb3f8L, 0x1fda836eL,
    0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L, 0x18b74777L,
    0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L,
    0xa00ae278L, 0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L,
    0xa7672661L, 0xd06016f7L, 0x4969474dL, 0x3e6e77dbL,
    0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L, 0x37d83bf0L,
    0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L,
    0xbad03605L, 0xcdd70693L, 0x54de5729L, 0x23d967bfL,
    0xb3667a2eL, 0xc4614ab8L, 0x5d681b02L, 0x2a6f2b94L,
    0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL, 0x2d02ef8dL
};

uint8_t eeprom_data[EEPROM_SIZE] = {
    0x54, 0x6c, 0x76, 0x49, 0x6e, 0x66, 0x6f, 0x00, 0x01, 0x00
};

/*
 * BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU -> BF_PLTFM_BD_ID_UNKNOWN
 * edit by tsihang, 2022/06/20
 */
static bf_pltfm_board_id_t bd_id =
    BF_PLTFM_BD_ID_UNKNOWN;

static bf_pltfm_type bf_cur_pltfm_type = INVALID_TYPE;
static bf_pltfm_subtype bf_cur_pltfm_subtype = INVALID_SUBTYPE;

/* New method of board_ctx_t decareled.
 * BF_PLTFM_BD_ID_X532PT_V1DOT1 means X532P-T subversion 1.1. */
static struct bf_pltfm_board_ctx_t bd_ctx[] = {
    {BF_PLTFM_BD_ID_X532PT_V1DOT0, "APNS320T-A1-V1.0", X532P, v1dot0},  // X532P-T V1.0
    {BF_PLTFM_BD_ID_X532PT_V1DOT1, "APNS320T-A1-V1.1", X532P, v1dot1},  // X532P-T V1.1
    {BF_PLTFM_BD_ID_X532PT_V2DOT0, "APNS320T-A1-V2.0", X532P, v2dot0},  // X532P-T V2.0

    {BF_PLTFM_BD_ID_X564PT_V1DOT0, "APNS640T-A1-V1.0", X564P, v1dot0},  // X564P-T V1.0
    {BF_PLTFM_BD_ID_X564PT_V1DOT1, "APNS640T-A1-V1.1", X564P, v1dot1},  // X564P-T V1.1
    {BF_PLTFM_BD_ID_X564PT_V1DOT2, "APNS640T-A1-V1.2", X564P, v1dot2},  // X564P-T V1.2

    {BF_PLTFM_BD_ID_X308PT_V1DOT0, "APNS320T-B1-V1.0", X308P, v1dot0},  // X308P-T V1.0
    /* More X308P-T board id. */

    {BF_PLTFM_BD_ID_HC36Y24C_V1DOT0, "Hello this is HC", HC, v1dot0},

    {BF_PLTFM_BD_ID_X312PT_V1DOT1,   "X312P-V1.x",     X312P, v1dot0},
    {BF_PLTFM_BD_ID_X312PT_V2DOT0,   "X312P-V2.x",     X312P, v2dot0},
    {BF_PLTFM_BD_ID_X312PT_V3DOT0,   "X312P-V3.x",     X312P, v3dot0},
    {BF_PLTFM_BD_ID_X312PT_V4DOT0,   "X312P-V4.x",     X312P, v4dot0},
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
    {0x33, "GHC-0 Board Version"},  // new
    {0x34, "GHC-1 Board Version"},  // new
    {0xFE, "CRC-32"}
};

static unsigned int crc32 ( const unsigned char
                            *buf, unsigned int size)
{
    unsigned int i, crc;
    crc = 0xFFFFFFFF;


    for (i = 0; i < size; i++) {
        crc = crc32tab[ (crc ^ buf[i]) & 0xff] ^
              (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

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

const char *dump_pltfm()
{
    return (bf_cur_pltfm_type == X564P ? "X564P-T" :
            bf_cur_pltfm_type == X532P ? "X532P-T" :
            bf_cur_pltfm_type == X308P ? "X308P-T" :
            bf_cur_pltfm_type == X312P ? "X312P-T" :
            "HC");
}

bf_pltfm_status_t bf_pltfm_bd_type_set_by_keystring (char *ptr)
{
    unsigned int i;

    if (ptr == NULL) {
        LOG_ERROR ("EEPROM ERROR: Unable to read the board id EEPROM\n");
        return -1;
    }

    for (i = 0; i < (int)ARRAY_LENGTH (bd_ctx); i++) {
        if (strcmp (bd_ctx[i].desc, (char *)ptr) == 0) {
            bd_id = bd_ctx[i].id;
            /* As we know, product name is not a suitable way to distinguish X-T platform.
             * So how to distinguish X312P and HC ???
             * by tsihang, 2021-06-29. */
            bf_pltfm_bd_type_set (bd_ctx[i].type, bd_ctx[i].subtype);
            LOG_DEBUG ("Board type : %04x : %s : %s",
                       bd_id,
                       dump_pltfm(),
                       ptr);
            fprintf (stdout, "Board type : %04x : %s : %s\n",
                     bd_id,
                     dump_pltfm(),
                     ptr);

            return 0;
        }
    }

    LOG_ERROR ("WARNING: No value in EEPROM(0x31) to identify current platform.\n");
    return -1;
}

bf_pltfm_status_t bf_pltfm_bd_type_set_by_key (uint8_t type, uint8_t subtype)
{
    unsigned int i;

    for (i = 0; i < (int)ARRAY_LENGTH (bd_ctx); i++) {
        if (bd_ctx[i].type == type &&
            bd_ctx[i].subtype == subtype) {
            bd_id = bd_ctx[i].id;
            bf_pltfm_bd_type_set (bd_ctx[i].type, bd_ctx[i].subtype);
            LOG_DEBUG ("Board type : %04x : %s",
                       bd_id,
                       dump_pltfm());
            fprintf (stdout, "Board type : %04x : %s\n",
                     bd_id,
                     dump_pltfm());

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
    strncpy (eeprom.bf_pltfm_product_name, str, i + 1);

    LOG_DEBUG ("Product Name: %s \n",
               eeprom.bf_pltfm_product_name);

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

    LOG_DEBUG ("Product Number: %s \n",
               eeprom.bf_pltfm_product_number);

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

    LOG_DEBUG ("Product Serial Number: %s \n",
               eeprom.bf_pltfm_product_serial);

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

    LOG_DEBUG ("Extended MAC Base %x:%x:%x:%x:%x:%x\n",
               eeprom.bf_pltfm_mac_base[0],
               eeprom.bf_pltfm_mac_base[1],
               eeprom.bf_pltfm_mac_base[2],
               eeprom.bf_pltfm_mac_base[3],
               eeprom.bf_pltfm_mac_base[4],
               eeprom.bf_pltfm_mac_base[5]);
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

    LOG_DEBUG ("System Manufacturing Date: %s \n",
               eeprom.bf_pltfm_system_manufacturing_date);

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

    LOG_DEBUG ("Product Version: %d \n",
               eeprom.bf_pltfm_product_version);

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
    str[j] = '\0';
    strncpy (eeprom.bf_pltfm_product_subversion,
        str, j);

    LOG_DEBUG ("Product Sub-Version: %s \n",
               eeprom.bf_pltfm_product_subversion);

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

    LOG_DEBUG ("System Platform Arch: %s \n",
               eeprom.bf_pltfm_product_platform_arch);

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

    LOG_DEBUG ("System ONIE Version: %s \n",
               eeprom.bf_pltfm_onie_version);

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

    LOG_DEBUG ("Extended MAC Address Size: %d \n",
               eeprom.bf_pltfm_mac_size);

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

    LOG_DEBUG ("System Manufacturer: %s \n",
               eeprom.bf_pltfm_system_manufacturer);

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

    LOG_DEBUG ("Country Code: %s \n",
               eeprom.bf_pltfm_cc);

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

    LOG_DEBUG ("Product Vendor: %s \n",
               eeprom.bf_pltfm_product_vendor);

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

    LOG_DEBUG ("Diag Version: %s \n",
               eeprom.bf_pltfm_diag_version);

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

    LOG_DEBUG ("Service Tag: %s \n",
               eeprom.bf_pltfm_serv_tag);

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

    LOG_DEBUG ("ASIC Vendor: %s \n",
               eeprom.bf_pltfm_asic_vendor);

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

    LOG_DEBUG ("Product Main Board Version: %s \n",
               eeprom.bf_pltfm_main_board_version);

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

    LOG_DEBUG ("Product COMe Board Version: %s \n",
               eeprom.bf_pltfm_come_version);

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

    LOG_DEBUG ("Product Computing Card0 Version: %s \n",
               eeprom.bf_pltfm_ghc_bd0_version);

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

    LOG_DEBUG ("Product Computing Card1 Version: %s \n",
               eeprom.bf_pltfm_ghc_bd1_version);

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

    LOG_DEBUG ("CRC32: %d \n", eeprom.bf_pltfm_crc32);

    return 0;
}

static void further_decode ()
{
    int i;
    char temp[32] = {0};
    struct tlv_t *tlv;

    if (platform_type_equal (X312P)) {
        for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
            tlv = &tlvs[i];
            memset (temp, 0, 32);
            if (tlv->code == 0x24) {
                sprintf(temp, "%02X:%02X:%02X:%02X:%02X:%02X", tlv->content[0],
                    tlv->content[1], tlv->content[2], tlv->content[3], tlv->content[4], tlv->content[5]);
                memcpy(tlv->content, (uint8_t *)temp, 32);
            } else if(tlv->code == 0x2a) {
                sprintf(temp, "%d", tlv->content[1]);
                memcpy(tlv->content, (uint8_t *)temp, 1);
            } else if(tlv->code == 0xfe) {
                sprintf(temp, "0x%02X%02X%02X%02X", tlv->content[0],
                    tlv->content[1], tlv->content[2], tlv->content[3]);
                memcpy(tlv->content, (uint8_t *)temp, 32);
            } else if(tlv->code == 0x26) {
                if (!strstr (tlv->content, "4") &&
                    !strstr (tlv->content, "3") &&
                    !strstr (tlv->content, "2")) {
                    fprintf (stdout,
                             "** Invalid data in eeprom sections (0x%02x), This may occur errors while running.\n",
                             tlv->code);
                }
                if (strstr (tlv->content, "4")) {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V4.x", eeprom.bf_pltfm_product_name);
                } else if (strstr (tlv->content, "3")) {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V3.x", eeprom.bf_pltfm_product_name);
                } else {
                    sprintf(eeprom.bf_pltfm_main_board_version, "%s V2.x", eeprom.bf_pltfm_product_name);
                }
                fprintf (stdout, "*** %s\n", eeprom.bf_pltfm_main_board_version);
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
 *   TLV Name             Code Len Value
 *   -------------------- ---- --- -----
 *   Product Name         0x21   7 X532P-T
 *   Part Number          0x22  14 ONBP1U-2Y32C-E
 *   Serial Number        0x23   9 201000001
 *   BASE MAC Address     0x24   6 60:EB:5A:00:7D:04
 *   Manufacture Date     0x25  19 17/07/2019 23:59:59
 *   Device Version       0x26   1 1
 *   Label Revision       0x27   1 0
 *   Platform Name        0x28  32 x86_64-asterfusion_congad1519-r0
 *   Onie Version         0x29  25 master-202008111723-dirty
 *   MAC Addresses        0x2A   2 1
 *   Manufacturer         0x2B  11 Asterfusion
 *   Country Code         0x2C   2 CN
 *   Vendor Name          0x2D  11 Asterfusion
 *   Diag Version         0x2E   3 1.0
 *   Service Tag          0x2F   1 X
 *   Switch Vendor        0x30   5 Intel
 *   Main Board Version   0x31  16 APNS320T-A1-V1.0
 *   COME Version         0x32   0
 *   GHC-0 Board Version  0x33   0
 *   GHC-1 Board Version  0x34   0
 *   CRC-32               0xFE   4 0xA5FE50A4
 *   Checksum is valid.
 *   msh />
 */
bf_pltfm_status_t bf_pltfm_bd_type_init()
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    char cmd = 0x01;
    int i;
    int err;
    int usec_delay;
    size_t l = 0, offset = 0;
    struct tlv_t *tlv;

    fprintf (stdout,
             "\n\n================== EEPROM ==================\n");

    for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i ++) {
        /* Read EEPROM. */
        tlv = &tlvs[i];
        wr_buf[0] = tlv->code;
        wr_buf[1] = 0xAA;

        memset (rd_buf, 0, 128);

        // Must give bmc more time to prepare data
        usec_delay = BMC_COMM_INTERVAL_US;
        if (g_access_bmc_through_uart) {
            err = bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                                                2, rd_buf, 128 - 1, usec_delay);
        } else {
            //usec_delay = ((tlvs[i].code == 0x21) || (tlvs[i].code == 0x22) || (tlvs[i].code == 0x23) || (tlvs[i].code == 0x24)) ?
            //       BMC_COMM_INTERVAL_US * 5 : BMC_COMM_INTERVAL_US;
            /* There're some old device such as x564p-t gets eeprom data through i2c (cgosdrv). */
            usec_delay *= 3;
            err = bf_pltfm_bmc_write_read (bmc_i2c_addr, cmd,
                                           wr_buf, 2, 0xFF, rd_buf, usec_delay);
        }

        if (err != -1) {
            if (g_access_bmc_through_uart) {
                l = strlen ((char *)&rd_buf[0]);
                offset = 0;
            } else {
                l = rd_buf[0];
                offset = 1;
            }

            memcpy (tlv->content, &rd_buf[offset], l);
            fprintf (stdout, "0x%02x %20s: %32s\n",
                     tlv->code, tlv->desc, tlv->content);
            LOG_DEBUG ("0x%02x %20s: %32s", tlv->code,
                       tlv->desc, tlv->content);
        }
    }
    fprintf (stdout, "\n\n");

    /**
     * Parse EEPROM data received from OPENBMC.
     * @return status
     */
    char data[1024];
    int length;
    FILE *fp = NULL;

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
                         "** Unsupported eeprom sections (0x%02x), exit right now.\n",
                         tlv->code);
                exit (0);
        }
    }

    /* Further decode when platform detected.
     * by tsihang, 2022-05-12. */
    further_decode ();

    /* Open eeprom data file, if non-exist, create it.
     * For onlp, should run BSP at least once. */
    const char *path = LOG_DIR_PREFIX"/eeprom";
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

bf_pltfm_status_t bf_pltfm_bd_type_get_ext (
    bf_pltfm_board_id_t *board_id)
{
    *board_id = bd_id;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_type_set (
    uint8_t type, uint8_t subtype)
{
    bf_cur_pltfm_type = type;
    bf_cur_pltfm_subtype = subtype;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_type_get (
    uint8_t *type, uint8_t *subtype)
{
    *type = bf_cur_pltfm_type;
    *subtype = bf_cur_pltfm_subtype;
    return BF_PLTFM_SUCCESS;
}

bool platform_type_equal (uint8_t type)
{
    return (bf_cur_pltfm_type == type);
}

bool platform_subtype_equal (uint8_t subtype)
{
    return (bf_cur_pltfm_subtype == subtype);
}

bf_pltfm_status_t bf_pltfm_bd_eeprom_get (
    bf_pltfm_eeprom_t *ee_data)
{
    memcpy (ee_data, &eeprom,
            sizeof (bf_pltfm_eeprom_t));
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_bd_eeprom_get_sonic (
    uint8_t *buf)
{
    //memcpy(ee_data, &eeprom, sizeof(bf_pltfm_eeprom_t));
    struct tlv_t *tlv;
    int i, j, k, len;
    eeprom_data[10] = 0x0;
    for (i = 0; i < (int)ARRAY_LENGTH (tlvs); i++) {
        tlv = &tlvs[i];
        if ((tlv->code & 0xFF) == 0x24) {
            len = strlen (tlv->content);
            eeprom_data[eeprom_data[10] + 11] =
                (tlv->code & 0xFF);
            eeprom_data[eeprom_data[10] + 12] = 6;
            for (j = 1, k = 1; j <= len; j++) {
                if (((uint8_t)tlv->content[j - 1]) == 0x3a) {
                    continue;
                }
                eeprom_data[eeprom_data[10] + k + 12] = ctoi (
                        tlv->content[j]) | (uint8_t) (ctoi (
                                tlv->content[j - 1]) << 4);
                j++;
                k++;
            }
            eeprom_data[10] += 6 + 2;
        }  else if ((tlv->code & 0xFF) == 0x2a) {
            len = strlen (tlv->content);
            eeprom_data[eeprom_data[10] + 11] =
                (tlv->code & 0xFF);
            eeprom_data[eeprom_data[10] + 12] = 2;
            if (len != 2) {
                eeprom_data[eeprom_data[10] + 12 + 1] = 0x01;
                eeprom_data[eeprom_data[10] + 12 + 2] = 0x04;
                eeprom_data[10] += (2 + 2);
            } else {
                eeprom_data[eeprom_data[10] + 12 + 1] =
                    (uint8_t)tlv->content[0];
                eeprom_data[eeprom_data[10] + 12 + 2] =
                    (uint8_t)tlv->content[1];
                eeprom_data[10] += (len + 2);
            }
        } else if ((tlv->code & 0xFF) > 0x30) {
          //else if ((tlv->code & 0xFF) > 0x2f) {
            continue;
        } else {
            len = strlen (tlv->content);
            eeprom_data[eeprom_data[10] + 11] =
                (tlv->code & 0xFF);
            eeprom_data[eeprom_data[10] + 12] = len;
            for (j = 1; j <= len; j++) {
                eeprom_data[eeprom_data[10] + j + 12] =
                    (uint8_t)tlv->content[j - 1];
            }
            eeprom_data[10] += (len + 2);
        }
    }

    unsigned int crc_check;
    unsigned int crc_check_mask[4] = {
        0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff
    };

    unsigned int crc_check_byte;
    int left_move;
    int eeprom_data_position = eeprom_data[10] + 11;
    eeprom_data[eeprom_data_position] =
        0xfe; // tlv crc_32 checksum
    eeprom_data[eeprom_data_position + 1] =
        0x04; // tlv crc checksum length
    eeprom_data[10] += (4 + 2);
    eeprom_data_position = eeprom_data_position + 2;
    crc_check = crc32 (eeprom_data,
                       eeprom_data_position);
    LOG_DEBUG ("crc_check: %d \n", crc_check);


    for (i = 0; i < 4; i++) {
        crc_check_byte = crc_check & crc_check_mask[i];
        left_move = (3 - i) * 8;
        eeprom_data[eeprom_data_position + i] =
            crc_check_byte >> left_move;
    }
    eeprom_data[EEPROM_SIZE - 1] = '\n';
    for (i = 0; i < EEPROM_SIZE; i++) {
        buf[i] = eeprom_data[i];
    }
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_bd_version_get (char *v)
{
    sprintf(v,
        "%s-%s(%04x)", dump_pltfm(), (char *)&eeprom.bf_pltfm_main_board_version[0], bd_id);

    return 0;
}

bf_pltfm_status_t bf_pltfm_bd_tlv_get (int idx,
                                       char *code, char *desc, char *content)
{
    *code = tlvs[idx].code;
    strcpy (desc,    tlvs[idx].desc);
    strcpy (content, tlvs[idx].content);
    return 0;
}
