/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdio.h>
#include <unistd.h>  //write
#define __USE_GNU    /* See feature_test_macros(7) */
#include <pthread.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_porting.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include "bf_pltfm.h"
#include <bf_port_mgmt/bf_port_mgmt_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_porting.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_qsfp/bf_sfp.h>
#include <tofino/bf_pal/bf_pal_port_intf.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>

/* global */
static bool pltfm_mgr_init_done = false;


/* platform-mgr thread exit callback API */
static void pltfm_mgr_exit_cleanup (void *arg)
{
    (void)arg;

    if (bf_pltfm_pm_deinit()) {
        LOG_ERROR ("pltfm_mgr: Error while de-initializing pltfm mgr");
    }

    bf_pltfm_platform_exit (
        arg); /* platform specific cleanup */
}

// static void pltfm_mgr_exit() { bf_pltfm_platform_exit(NULL); }

/* Initialize and start pltfm_mgrs server */
bf_status_t pltfm_mgr_init (bf_switchd_context_t
                            *switchd_ctx)
{
    bf_status_t sts;

    /* initialize qsfp data structures */
    if (bf_qsfp_init()) {
        LOG_ERROR ("pltfm_mgr: qsfp soft init failed\n");
        return -1;
    }

    if (bf_sfp_init()) {
        LOG_ERROR ("pltfm_mgr: sfp soft init failed\n");
        return -1;
    }

    /* platform specific init here */
    if (bf_pltfm_platform_init (switchd_ctx) !=
        BF_PLTFM_SUCCESS) {
        LOG_ERROR ("pltfm_mgr: platform init failed\n");
        return -1;
    }

    /* init board config data structures */
    if (bf_bd_cfg_init() != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("pltfm_mgr: Board Config library initialization failed\n");
        return -1;
    }

    /* Register pltfm port manager */
    if (!switchd_ctx->skip_hld.port_mgr) {
        if (bf_pltfm_pm_init (switchd_ctx->init_mode) !=
            BF_PLTFM_SUCCESS) {
            LOG_ERROR (
                "pltfm_mgr: Error while initializing the platforms port manager\n");
        }
    }

    if (platform_is_hw()) {
        // Register the interfaces for bf_pm
        bf_pal_pltfm_reg_interface_t bf_pm_interface = {0};
        bf_pm_interface.pltfm_safe_to_call_in_notify =
            &bf_pm_safe_to_call_in_notify;
        bf_pm_interface.pltfm_front_panel_port_get_first =
            &bf_bd_cfg_front_panel_port_get_first;
        bf_pm_interface.pltfm_front_panel_port_get_next =
            &bf_bd_cfg_front_panel_port_get_next;
        bf_pm_interface.pltfm_mac_to_serdes_map_get =
            &bf_bd_cfg_mac_to_serdes_map_get;
        bf_pm_interface.pltfm_serdes_info_get =
            &bf_bd_cfg_serdes_info_get;
        bf_pm_interface.pltfm_pre_port_add_cfg_set =
            &bf_pm_pre_port_add_cfg_set;
        bf_pm_interface.pltfm_post_port_add_cfg_set =
            &bf_pm_post_port_add_cfg_set;
        bf_pm_interface.pltfm_pre_port_delete_cfg_set =
            &bf_pm_pre_port_delete_cfg_set;
        bf_pm_interface.pltfm_post_port_delete_cfg_set =
            &bf_pm_post_port_delete_cfg_set;
        bf_pm_interface.pltfm_pre_port_enable_cfg_set =
            &bf_pm_pre_port_enable_cfg_set;
        bf_pm_interface.pltfm_post_port_enable_cfg_set =
            &bf_pm_post_port_enable_cfg_set;
        bf_pm_interface.pltfm_pre_port_disable_cfg_set =
            &bf_pm_pre_port_disable_cfg_set;
        bf_pm_interface.pltfm_post_port_disable_cfg_set =
            &bf_pm_post_port_disable_cfg_set;
        bf_pm_interface.pltfm_port_link_up_actions =
            &bf_pm_port_link_up_actions;
        bf_pm_interface.pltfm_port_link_down_actions =
            &bf_pm_port_link_down_actions;
        bf_pm_interface.pltfm_port_media_type_get =
            &bf_pm_pltfm_port_media_type_get;
        bf_pm_interface.pltfm_port_str_to_hdl_get =
            &bf_bd_cfg_port_str_to_handle;
        bf_pm_interface.pltfm_init = &bf_pm_cold_init;
        bf_pm_interface.pltfm_port_prbs_cfg_set =
            &bf_pm_prbs_cfg_set;
        bf_pm_interface.pltfm_ha_mode_set =
            &bf_pm_ha_mode_enable;
        bf_pm_interface.pltfm_ha_mode_clear =
            &bf_pm_ha_mode_disable;
        bf_pm_interface.pltfm_deinit = 0;

        // atexit(pltfm_mgr_exit); // not tested

        sts = bf_pal_pltfm_all_interface_set (
                  &bf_pm_interface);
        if (sts != BF_SUCCESS) {
            LOG_ERROR ("Unable to register pltfm interfaces for bf_pm");
        }
    }
    pltfm_mgr_init_done = true;
    /* listen to client commands and execute them */
    while (1) {
        usleep (10000); /* TBD : probably not required */
    }

    return 0;
}

/* Init function  */
void *bf_switchd_agent_init (void *arg)
{
    if (!arg) {
        return NULL;
    }
    bf_switchd_context_t *switchd_ctx =
        (bf_switchd_context_t *)arg;
    LOG_DEBUG ("Platform-mgr started \n");
    /* Register cleanup handler */
    pthread_cleanup_push (pltfm_mgr_exit_cleanup,
                          "Platform-Mgr Cleanup handler");
    pltfm_mgr_init (switchd_ctx);
    pthread_cleanup_pop (0);
    return NULL;
}

void *bf_switchd_agent_init_done (void *arg)
{
    if (!arg) {
        return NULL;
    }
    int *ret = (int *)arg;
    *ret = pltfm_mgr_init_done;
    return NULL;
}

#ifdef THRIFT_ENABLED
int agent_rpc_server_thrift_service_add (
    void *processor)
{
    return bf_pltfm_agent_rpc_server_thrift_service_add (
               processor);
}

int agent_rpc_server_thrift_service_rmv (
    void *processor)
{
    return bf_pltfm_agent_rpc_server_thrift_service_rmv (
               processor);
}
#endif  // THRIFT_ENABLED

#ifdef STATIC_LINK_LIB
int bf_pltfm_init_handlers_register (
    bf_switchd_context_t *ctx)
{
    if (ctx == NULL) {
        LOG_ERROR ("%s: invalid input", __func__);
    }

    ctx->bf_switchd_agent_init_fn =
        bf_switchd_agent_init;
    ctx->bf_switchd_agent_init_done_fn =
        bf_switchd_agent_init_done;
    ctx->bf_pltfm_device_type_get_fn =
        bf_pltfm_device_type_get;
#ifdef THRIFT_ENABLED
    ctx->agent_rpc_server_thrift_service_add_fn =
        agent_rpc_server_thrift_service_add;
    ctx->agent_rpc_server_thrift_service_rmv_fn =
        agent_rpc_server_thrift_service_rmv;
#endif  // THRIFT_ENABLED

    return 0;
}
#endif  // STATIC_LINK_LIB

