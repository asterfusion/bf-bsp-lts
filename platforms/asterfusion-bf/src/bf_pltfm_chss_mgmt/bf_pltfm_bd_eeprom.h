/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_eeprom.h
 * @date
 *
 * EEPROM parsing
 *
 */

/* Standard includes */
#include <stdint.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>

/* Local header includes */

#define URL_SIZE 100
#define EEPROM_RAW_DATA_SIZE 4096

extern char eeprom_raw_data[];

extern bf_pltfm_eeprom_t eeprom;

struct tlv_t {
    unsigned char code;
    const char   *desc;
    char          content[1024];
};

bf_pltfm_status_t bf_pltfm_bd_type_init();
bf_pltfm_status_t bf_pltfm_bd_type_get_ext (
    bf_pltfm_board_id_t *board_id);
bf_pltfm_status_t bf_pltfm_bd_type_get (
    uint8_t *type, uint8_t *subtype);
bf_pltfm_status_t bf_pltfm_bd_type_set (
    uint8_t type, uint8_t subtype);
bf_pltfm_status_t bf_pltfm_bd_type_set_by_key (
    uint8_t type, uint8_t subtype);
bf_pltfm_status_t bf_pltfm_bd_type_set_by_keystring (char *ptr);
bf_pltfm_status_t bf_pltfm_bd_tlv_get (int idx,
                                       char *code, char *desc, char *content);


