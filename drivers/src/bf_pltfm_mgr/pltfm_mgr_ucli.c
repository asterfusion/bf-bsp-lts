/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file pltfm_mgr_ucli.c
 * @date
 *
 * Contains implementation of platform manager ucli
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>  //strlen
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include "bf_pltfm.h"
#include "bf_pltfm_mgr/pltfm_mgr_handlers.h"
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>

#if 0
static ucli_node_t *ucli_node;

static ucli_status_t pltfm_mgr_ucli_ucli__mntr__
(ucli_context_t *uc)
{
    uint8_t cntrl = 0;

    UCLI_COMMAND_INFO (
        uc, "monitoring_control", 1,
        " <0/1> 0: ENABLE , 1: DISABLE");

    cntrl = atoi (uc->pargs->args[0]);

    if (cntrl == MNTR_DISABLE) {
        mntr_cntrl_hndlr = MNTR_DISABLE;
        printf ("Monitoring disabled \n");
    } else {
        mntr_cntrl_hndlr = MNTR_ENABLE;
        printf ("Monitoring enabled \n");
    }

    return 0;
}

/* <auto.ucli.handlers.start> */
/* <auto.ucli.handlers.end> */
static ucli_command_handler_f
bf_pltfm_mgr_ucli_ucli_handlers__[] = {
    pltfm_mgr_ucli_ucli__mntr__, NULL
};

static ucli_module_t
mavericks_pltfm_mgrs_ucli_module__ = {
    "pltfm_mgr_ucli", NULL, bf_pltfm_mgr_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *pltfm_mgrs_ucli_node_create (void)
{
    ucli_node_t *n;
    ucli_module_init (
        &mavericks_pltfm_mgrs_ucli_module__);
    n = ucli_node_create ("pltfm_mgr", NULL,
                          &mavericks_pltfm_mgrs_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("pltfm_mgr"));
    return n;
}

void pltfm_mgr_register_ucli_node()
{
    ucli_node = pltfm_mgrs_ucli_node_create();
    bf_drv_shell_register_ucli (ucli_node);
}

void pltfm_mgr_unregister_ucli_node()
{
    bf_drv_shell_unregister_ucli (ucli_node);
}
#endif
