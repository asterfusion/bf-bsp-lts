/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rtmr.h
 * @date
 *
 * API's for reading and writing to retimers for Mavericks
 *
 */

#ifndef _BF_PLTFM_RTMR_H
#define _BF_PLTFM_RTMR_H

/* Standard includes */
#include <stdint.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_porting.h>
/* Local header includes */

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct bf_pltfm_rtmr_reg_info_t {
    uint16_t offset; /* Retimer register offset */
    uint16_t data;   /* Retimer register data */
} bf_pltfm_rtmr_reg_info_t;

typedef struct bf_pltfm_rtmr_fw_info_t {
    uint8_t *image; /* Retimer firmware image */
    uint32_t size;  /* Retimer firmware image size */
} bf_pltfm_rtmr_fw_info_t;

/**
 * Initialize retimer library
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_pre_init (
    const char *, bool);

bf_pltfm_status_t bf_pltfm_rtmr_reg_read (
    bf_pltfm_port_info_t *port_info,
    uint16_t offset,
    uint16_t *rd_buf);

bf_pltfm_status_t bf_pltfm_rtmr_reg_write (
    bf_pltfm_port_info_t *port_info,
    uint16_t offset,
    uint16_t data);

bf_pltfm_status_t bf_pltfm_rtmr_fw_dl (
    bf_pltfm_port_info_t *port_info,
    uint8_t *buffer,
    uint32_t buf_size);

bf_pltfm_status_t
bf_pltfm_rtmr_crc_rd_from_bin_file (
    uint16_t *crc);

bf_pltfm_status_t bf_pltfm_rtmr_crc_rd (
    bf_pltfm_port_info_t *port_num,
    uint16_t *crc);

bf_pltfm_status_t bf_pltfm_rtmr_operation (
    bf_pltfm_port_info_t *port_info,
    uint8_t operation,
    void *data);

bf_pltfm_status_t bf_pltfm_rtmr_set_pol (
    uint8_t conn_id, uint8_t chnl_id);

bf_pltfm_status_t bf_pltfm_rtmr_set_pol_all (
    void);

bf_pltfm_status_t bf_pltfm_rtmr_reset (
    bf_pltfm_port_info_t *port_info);

bf_pltfm_status_t bf_pltfm_rtmr_reset_all (void);

bf_pltfm_status_t bf_pltfm_rtmr_recover (
    bf_pltfm_port_info_t *port_info);

char *bf_pltfm_rtmr_install_dir_get (
    char *install_dir, uint16_t buf_size);

bf_pltfm_board_id_t bf_pltfm_rtmr_bd_id_get (
    void);

ucli_node_t *bf_pltfm_rtmr_ucli_node_create (
    ucli_node_t *m);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /*_BF_PLTFM_RTMR_H */
