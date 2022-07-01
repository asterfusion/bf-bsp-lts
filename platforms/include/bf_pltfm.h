/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#ifndef _PLTFM_MGR_INIT_H
#define _PLTFM_MGR_INIT_H

#include <stdint.h>
#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pltfm_mgr_info_s {
    const char
    *np_name;    /* The name of health monitor thread. */
    pthread_t health_mntr_t_id;
    const char
    *np_onlp_mntr_name;
    pthread_t onlp_mntr_t_id;

#define AF_PLAT_MNTR_POWER  (1 << 0)
#define AF_PLAT_MNTR_FAN    (1 << 1)
#define AF_PLAT_MNTR_TMP    (1 << 2)
#define AF_PLAT_MNTR_MODULE (1 << 3)
#define AF_PLAT_MNTR_CTRL   (1 << 31)
    uint32_t flags;
    uint64_t ull_mntr_ctrl_date;

    uint8_t pltfm_type;
    uint8_t pltfm_subtype;

    /* Vary data based on real hardware which identified by this.pltfm_type. */
    uint32_t psu_count;
    uint32_t sensor_count;
    uint32_t fan_group_count;
    uint32_t fan_per_group;
} pltfm_mgr_info_t;

extern pltfm_mgr_info_t *bf_pltfm_mgr_ctx();
static inline bool bf_pltfm_equal (uint8_t pltfm)
{
    return (bf_pltfm_mgr_ctx()->pltfm_type == pltfm);
}


/* platform specific initialization for the given platform
 * this function is called before Barefoot switch Asic intiailization
 * @return
 *   BF_PLTFM_SUCCESS on success and other values on error
 *   **This is a good place for driving "Test_core_tap_l" pin of Tofino-B0
 *   * low and hold low forever. However, this could be done anytime before this
 *   * as well (e.g. in some platform startup script).
 */
bf_status_t bf_pltfm_platform_init (
    void *switchd_ctx);

/* platform specific cleanup for the given platform
 * this function is called after the Barefoot switch Asic decommissiing
 * param: arg can be suitably interpreted by platforms implementation in future
          (currently, NULL)
 */
void bf_pltfm_platform_exit (void *arg);

/* platform specific port subsystem initialization
 * this function i scalled after the Barefoot switch ASIC is initialized
 * and before the ports are initialized.
 * e.g., this function can implement any platforms specific operation
 * that may involve Barefoot switch Asic's support.
 */
bf_pltfm_status_t bf_pltfm_platform_port_init (
    bf_dev_id_t dev_id,
    bool warm_init);

/* platform specific query
 * all real hardware platform should stub this to return true
 * @return
 *   BF_PLTFM_SUCCESS on success and other values on error
 */
bool platform_is_hw (void);

#ifdef THRIFT_ENABLED
/* add platform thrift service
 * @return
 *   0 is success and other values on error
 */
int bf_pltfm_agent_rpc_server_thrift_service_add (
    void *processor);

/* remove platform thrift service
 * @return
 *   0 is success and other values on error
 */
int bf_pltfm_agent_rpc_server_thrift_service_rmv (
    void *processor);
#endif

#ifdef STATIC_LINK_LIB
bf_pltfm_status_t bf_pltfm_device_type_get (
    bf_dev_id_t dev_id,
    bool *is_sw_model);

#endif  // STATIC_LINK_LIB

bf_pltfm_status_t platform_port_mac_addr_get (
    bf_pltfm_port_info_t *port_info,
    uint8_t *mac_addr);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
