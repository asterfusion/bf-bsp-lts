/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _PLTFM_MGR_INTF_H
#define _PLTFM_MGR_INTF_H

#ifdef STATIC_LINK_LIB
#include <bf_switchd/bf_switchd.h>

int bf_pltfm_init_handlers_register (
    bf_switchd_context_t *ctx);

#endif  // STATIC_LINK_LIB

#endif  // _PLTFM_MGR_INTF_H
