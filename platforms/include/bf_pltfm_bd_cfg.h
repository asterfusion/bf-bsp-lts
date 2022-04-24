/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PLTFM_BD_CFG_H
#define _BF_PLTFM_BD_CFG_H

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* returns board map for the given platform in the form of
 * the struct pltfm_bd_map_t
 */
struct pltfm_bd_map_t *platform_pltfm_bd_map_get (
    int *rows);

/* returns the name of the given platform board */
int platform_name_get_str (char *name,
                           size_t name_size);

/* returns the number of ports for the given platform */
int platform_num_ports_get (void);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
