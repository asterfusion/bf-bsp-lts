/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rptr.h
 * @date
 *
 * API's for reading and writing to serdes repeaters for Mavericks
 *
 */

#ifndef _BL_PLTFM_RPTR_H
#define _BL_PLTFM_RPTR_H

/* Standard includes */
#include <stdint.h>

/* Module includes */
#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bfutils/uCli/ucli.h>
/* Local header includes */

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize repeater library
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_init (void);

/**
 * Set particular port's egress or ingress repeater channel
 * @param port_info info struct of the port on the board
 * @param mode Egress or Ingress mode
 * @param rptr_info Repeater info struct
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_ch_eq_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rptr_mode_t mode,
    bf_pltfm_rptr_info_t *rptr_info);

/**
 * Set all 4 repeater channels of a port in ingress and egress mode
 * @param port_num port number for which eq values need to be set
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rptr_conn_eq_set (
    uint32_t port_num);

/**
 * configures repeater based on board config data
 * @param dev_id
 * @param port number for which eq values need to be set
 * @return Status of the API call.
 *
 */
bf_pltfm_status_t bf_pltfm_rptr_config_set (
    bf_dev_id_t dev_id,
    uint32_t port,
    bf_pltfm_qsfp_type_t qsfp_type);

ucli_node_t *bf_pltfm_rptr_ucli_node_create (
    ucli_node_t *m);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /*_BL_PLTFM_RPTR_H */
