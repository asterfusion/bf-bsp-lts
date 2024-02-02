/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file platform_priv.h
 * @date
 *
 * Contains definitions of platform manager
 *
 */
#ifndef _PLATFORM_PRIV_H
#define _PLATFORM_PRIV_H

#define LOG_ALARM LOG_ERROR

/*temp in celsius*/
#define CHASSIS_TMP_ALARM_RANGE 70
#define ASIC_TMP_ALARM_RANGE 95

/*temp in milli celsius*/
#define TOFINO_TMP_ALARM_RANGE (105 * 1000)


void *health_mntr_init (void *arg);
void *onlp_mntr_init (void *arg);


void pltfm_mgr_register_ucli_node();

void pltfm_mgr_unregister_ucli_node();

bf_pltfm_status_t platform_bd_deinit(void);
bf_pltfm_status_t platform_bd_init(void);

#endif
