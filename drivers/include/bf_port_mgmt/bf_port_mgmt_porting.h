/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _BF_PORT_PLTFM_PORTING_H
#define _BF_PORT_PLTFM_PORTING_H

#include <tofino/bf_pal/bf_pal_pltfm_porting.h>

/**
 * @brief Configure essential pltfm modules so that a port can be successfully
 * added
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_pre_port_add_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Configure essential pltfm modules after a port has been added to the
 * system
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_post_port_add_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Cleanup essential pltfm modules so that a port can be successfully
 * deleted
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_pre_port_delete_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Cleanup essential pltfm modules after a port has been deleted from
 * the system
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_post_port_delete_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Configure essential pltfm modules so that a port can be successfully
 * enabled
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_pre_port_enable_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Configure essential pltfm modules after a port has been enabled so
 * that it can be successfully link up
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_post_port_enable_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Cleanup essential pltfm modules so that a port can be successfully
 * disabled
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_pre_port_disable_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Cleanup essential pltfm modules after a port has been disabled
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_post_port_disable_cfg_set (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Configure necessary pltfm modules after a port has successfully
 * linked up
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_port_link_up_actions (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Configure necessary pltfm modules after a port goes down
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_port_link_down_actions (
    bf_pal_front_port_handle_t *port_hdl,
    bf_pal_front_port_cb_cfg_t *port_cfg);

/**
 * @brief Return the media type of the cable connected to a port
 *
 * @param port_hdl Identifier of the port on the system
 * @param media_type Type of the cable connected to the port
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_pltfm_port_media_type_get (
    bf_pal_front_port_handle_t *port_hdl,
    bf_media_type_t *media_type);

/**
 * @brief Read necessary hardware information at init time (Required
 * especially during warm init)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_cold_init();

/**
 * @brief Start making calls into the SDE only after being notified by the SDE
 * that it is safe to do so
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_safe_to_call_in_notify();

/**
 * @brief Do any needed configuration for prbs
 *
 * @param port_hdl Identifier of the port on the system
 * @param port_cfg Information configured for the port (passed in by bf_pm)
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_prbs_cfg_set();

/**
 * @brief Set ha mode flag to indicate that warm init is in progress
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_ha_mode_enable();

/**
 * @brief Clear ha mode flag to indicate that warm init is in complete
 *
 * @return Status of the API call
 */
bf_status_t bf_pm_ha_mode_disable();
#endif
