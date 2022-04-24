/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_BD_CFG_H
#define _BF_BD_CFG_H

#include <bf_pltfm_types/bf_pltfm_types.h>

void pltfm_port_info_to_str (bf_pltfm_port_info_t
                             *port_info,
                             char *str_hdl,
                             uint32_t str_len);

bf_pltfm_status_t pltfm_port_str_to_info (
    const char *str,
    bf_pltfm_port_info_t *port_info);

bf_pltfm_status_t pltfm_port_qsfp_type_get (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_qsfp_type_t *qsfp_type);

#endif
