/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pwd.h>
#include <net/if.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_porting.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_ext_phy.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include "platform_priv.h"

#include <bf_pltfm_spi.h>
#include <bf_led/bf_led.h>
#include <bf_pltfm.h>
#include <bf_pltfm_led.h>
#include <bf_mav_led.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_uart.h>
#include <bf_pltfm_syscpld.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_qsfp.h>
#include <bf_pltfm_sfp.h>
#include <bf_pltfm_bd_cfg.h>

#include "version.h"
#include "pltfm_types.h"

static time_t g_tm_start, g_tm_end;

//#define USE_I2C_ACCESS

#ifdef THRIFT_ENABLED
static void *bf_pltfm_rpc_server_cookie;
extern int add_platform_to_rpc_server (
    void *processor);
extern int rmv_platform_from_rpc_server (
    void *processor);
extern int bf_platform_rpc_server_start (
    void **server_cookie);
extern int bf_platform_rpc_server_end (
    void *processor);

int bf_pltfm_agent_rpc_server_thrift_service_add (
    void *processor)
{
    return add_platform_to_rpc_server (processor);
}

int bf_pltfm_agent_rpc_server_thrift_service_rmv (
    void *processor)
{
    return rmv_platform_from_rpc_server (processor);
}

#endif

extern void bf_pltfm_health_monitor_enable (
    bool enable);
static bf_dev_init_mode_t g_switchd_init_mode = BF_DEV_INIT_COLD;

extern ucli_node_t
*bf_pltfm_cpld_ucli_node_create (ucli_node_t *m);
extern ucli_node_t
*bf_pltfm_spi_ucli_node_create (ucli_node_t *m);

static ucli_node_t *bf_pltfm_ucli_node;
static bf_sys_rmutex_t
mav_i2c_lock;    /* i2c(cp2112) lock for QSFP IO. by tsihang, 2021-07-13. */
extern bf_sys_rmutex_t update_lock;

extern bf_pltfm_status_t bf_pltfm_get_bmc_ver(char *bmc_ver, bool forced);

bool platform_is_hw (void)
{
    return true;
}

/*start Health Monitor */
static void pltfm_mgr_start_health_mntr (void)
{
    int ret = 0;
    pthread_attr_t health_mntr_t_attr, onlp_mntr_t_attr;
    pthread_attr_init (&health_mntr_t_attr);
    pthread_attr_init (&onlp_mntr_t_attr);

    if (bf_sys_rmutex_init (&update_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: Update lock init failed\n");
        return;
    }

    if ((ret = pthread_create (
                   &bf_pltfm_mgr_ctx()->health_mntr_t_id,
                   &health_mntr_t_attr,
                   health_mntr_init, NULL)) !=
        0) {
        LOG_ERROR (
            "pltfm_mgr: ERROR: thread creation failed for health monitor, ret=%d\n",
            ret);
    } else {
        pthread_setname_np (
            bf_pltfm_mgr_ctx()->health_mntr_t_id,
            bf_pltfm_mgr_ctx()->np_name);
        LOG_DEBUG ("pltfm_mgr: health monitor initialized(%u : %s)\n",
                   bf_pltfm_mgr_ctx()->health_mntr_t_id,
                   bf_pltfm_mgr_ctx()->np_name);
    }

    if ((ret = pthread_create (
                   &bf_pltfm_mgr_ctx()->onlp_mntr_t_id,
                   &onlp_mntr_t_attr,
                   onlp_mntr_init, NULL)) !=
        0) {
        LOG_ERROR (
            "pltfm_mgr: ERROR: thread creation failed for transceiver monitor, ret=%d\n",
            ret);
    } else {
        pthread_setname_np (
            bf_pltfm_mgr_ctx()->onlp_mntr_t_id,
            bf_pltfm_mgr_ctx()->np_onlp_mntr_name);
        LOG_DEBUG ("pltfm_mgr: transceiver monitor initialized(%u : %s)\n",
                   bf_pltfm_mgr_ctx()->onlp_mntr_t_id,
                   bf_pltfm_mgr_ctx()->np_onlp_mntr_name);
    }
}

/* Stop Health Monitor */
static void pltfm_mgr_stop_health_mntr (void)
{
    int ret;
    pthread_attr_t health_mntr_t_attr, onlp_mntr_t_attr;

    if (bf_pltfm_mgr_ctx()->health_mntr_t_id) {
        int err = pthread_getattr_np (
                      bf_pltfm_mgr_ctx()->health_mntr_t_id,
                      &health_mntr_t_attr);
        ret = pthread_cancel (
                  bf_pltfm_mgr_ctx()->health_mntr_t_id);
        if (ret != 0) {
            LOG_ERROR (
                "pltfm_mgr: ERROR: thread cancelation failed for health monitor, "
                "ret=%d\n", ret);
        } else {
            pthread_join (
                bf_pltfm_mgr_ctx()->health_mntr_t_id, NULL);
        }
        if (!err) {
            pthread_attr_destroy (&health_mntr_t_attr);
        }
        bf_pltfm_mgr_ctx()->health_mntr_t_id = 0;
    }


    if (bf_pltfm_mgr_ctx()->onlp_mntr_t_id) {
        int err = pthread_getattr_np (
                      bf_pltfm_mgr_ctx()->onlp_mntr_t_id,
                      &onlp_mntr_t_attr);
        ret = pthread_cancel (
                  bf_pltfm_mgr_ctx()->onlp_mntr_t_id);
        if (ret != 0) {
            LOG_ERROR (
                "pltfm_mgr: ERROR: thread cancelation failed for health monitor, "
                "ret=%d\n", ret);
        } else {
            pthread_join (
                bf_pltfm_mgr_ctx()->onlp_mntr_t_id, NULL);
        }
        if (!err) {
            pthread_attr_destroy (&onlp_mntr_t_attr);
        }
        bf_pltfm_mgr_ctx()->onlp_mntr_t_id = 0;
    }

    bf_sys_rmutex_del (&update_lock);
}

static bf_pltfm_status_t chss_mgmt_init()
{
    bf_pltfm_status_t sts;
    sts = bf_pltfm_chss_mgmt_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("pltfm_mgr: Chassis Mgmt library initialization failed\n");
        return sts;
    }

    bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_QSFP_REALTIME_DDM;
    bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_SFP_REALTIME_DDM;
    bf_pltfm_mgr_ctx()->dpu_count = 0;

    /* Check strictly. */
    if (!platform_type_equal (AFN_X532PT) &&
        !platform_type_equal (AFN_X564PT) &&
        !platform_type_equal (AFN_X308PT) &&
        !platform_type_equal (AFN_X312PT) &&
        !platform_type_equal (AFN_X732QT) &&
        !platform_type_equal (AFN_HC36Y24C)    &&
        1 /* More platform here */) {
        LOG_ERROR ("pltfm_mgr: Unsupported platform, verify EEPROM please.\n");
        exit (0);
    } else {
        if (platform_type_equal (AFN_X564PT)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 2;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 6;
            bf_pltfm_mgr_ctx()->cpld_count = 3;
            if (platform_subtype_equal(V2P0)) {
                bf_pltfm_mgr_ctx()->sensor_count = 8;
            }
        } else if (platform_type_equal (AFN_X532PT)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 5;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 6;
            bf_pltfm_mgr_ctx()->cpld_count = 2;
        } else if (platform_type_equal (AFN_X308PT)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            if (platform_subtype_equal(V3P0)) {
                bf_pltfm_mgr_ctx()->dpu_count = 2;
                bf_pltfm_mgr_ctx()->fan_group_count = 5;
            } else {
                bf_pltfm_mgr_ctx()->dpu_count = 2;
                bf_pltfm_mgr_ctx()->fan_group_count = 6;
            }
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 10;
            bf_pltfm_mgr_ctx()->cpld_count = 2;
        } else if (platform_type_equal (AFN_X312PT)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 5;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->dpu_count = 2;
            /* LM75, LM63 and GHC(4) and Tofino(2).
             * This makes an offset to help bf_pltfm_onlp_mntr_tofino_temperature. */
            bf_pltfm_mgr_ctx()->sensor_count = 9;
            if (platform_subtype_equal(V3P0) ||
                platform_subtype_equal(V4P0) ||
                platform_subtype_equal(V5P0)) {
            /* LM75, LM63, LM86 and GHC(4) and Tofino(2).
             * This makes an offset to help bf_pltfm_onlp_mntr_tofino_temperature. */
                bf_pltfm_mgr_ctx()->sensor_count = 9;
            }
            bf_pltfm_mgr_ctx()->cpld_count = 5;
        } else if (platform_type_equal (AFN_X732QT)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 6;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 6;
            bf_pltfm_mgr_ctx()->cpld_count = 2;
        } else if (platform_type_equal (AFN_HC36Y24C)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 0;
            bf_pltfm_mgr_ctx()->fan_per_group = 0;
            bf_pltfm_mgr_ctx()->psu_count = 0;
            bf_pltfm_mgr_ctx()->sensor_count = 0;
            bf_pltfm_mgr_ctx()->cpld_count = 3;
            bf_pltfm_mgr_ctx()->dpu_count = 2;
        }
        bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date = time (
                    NULL);
    }


    char pltfmvar[128];
    platform_name_get_str (pltfmvar, sizeof (pltfmvar));
    fprintf (stdout, "########################\n");
    fprintf (stdout, "%s\n", pltfmvar);
    LOG_DEBUG ("%s", pltfmvar);
    fprintf (stdout, "########################\n");

    /* To accelerate 2nd launching for stratum. */
    if (g_switchd_init_mode != BF_DEV_INIT_COLD) {
        return BF_PLTFM_SUCCESS;
    }

    //bf_pltfm_chss_mgmt_fan_init();
    //bf_pltfm_chss_mgmt_pwr_init();
    //bf_pltfm_chss_mgmt_tmp_init();
    //bf_pltfm_chss_mgmt_pwr_rails_init();
    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t cp2112_init()
{
    bf_pltfm_status_t sts;
    sts = bf_pltfm_cp2112_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("pltfm_mgr: CP2112 library initialization failed\n");
        return sts;
    }
    return BF_PLTFM_SUCCESS;
}

static ucli_status_t pltfm_mgr_ucli_ucli__mntr__
(ucli_context_t *uc)
{
    extern void health_mntr_dbg_cntrs_get (uint32_t *cntrs);
    uint8_t ctrl = 0;
    uint32_t cntrs[4] = {0};

    UCLI_COMMAND_INFO (
        uc, "pltfm-monitor", 1,
        " <0/1> 0: disable , 1: enable");

    ctrl = atoi (uc->pargs->args[0]);

    bf_pltfm_health_monitor_enable (ctrl == 0 ? false : true);
    bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date = time (
                NULL);

    health_mntr_dbg_cntrs_get((uint32_t *)&cntrs[0]);

    aim_printf (&uc->pvs, "Platform monitor    %s\n",
                (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_CTRL) ?
                "enabled" : "disabled");
    aim_printf (&uc->pvs, "@ %s\n",
                ctime ((time_t *)
                       &bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date));

    aim_printf (&uc->pvs, "pwr %12u : fan %12u : tmp %12u : trans %12u\n",
                cntrs[0], cntrs[1], cntrs[2], cntrs[3]);
    return 0;
}

static ucli_status_t pltfm_mgr_ucli_ucli__set_interval__
(ucli_context_t *uc)
{
    uint32_t us;

    UCLI_COMMAND_INFO (
        uc, "set-bmc-comm-intr", 1,
        " Set the communication interval of BMC in us");

    if (platform_type_equal (AFN_X312PT)) {
        us = atoi (uc->pargs->args[0]);
        if ((us < 1000) || (us > 10000000)) {
            aim_printf (&uc->pvs, "Illegal value, should be 1,000 ~ 10,000,000\n");
        } else {
            bf_pltfm_set_312_bmc_comm_interval(us);
        }
    } else {
        aim_printf (&uc->pvs, "Not supported on this platform yet!\n");
    }

    return 0;
}

static ucli_status_t pltfm_mgr_ucli_ucli__get_interval__
(ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "get-bmc-comm-intr", 0,
        " Get the BMC communication interval in us");

    if (platform_type_equal (AFN_X312PT)) {
        aim_printf (&uc->pvs, "BMC communication interval = %dus\n",
                    bf_pltfm_get_312_bmc_comm_interval());
    } else {
        aim_printf (&uc->pvs, "Not supported on this platform yet!\n");
    }

    return 0;
}

static bool bf_pltfm_ucli_mgt_detected (bool *plugged, bool *linked) {
    int fd = 0, i;
    struct ifreq ifr;
    struct ethtool_value {
            __uint32_t      cmd;
            __uint32_t      data;
    } edata;
    /* ma1   -> ONL */
    /* eth0  -> SONiC or other OS. */
    /* enpXsY-> Debian or other OS. */
    const char *mgts[3] = {"ma1", "eth0", "NULL"};

    fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return false;

    for (i = 0; i < 2; i ++) {
        *plugged = false;
        *linked  = false;
        memset(&ifr, 0, sizeof(ifr));

        strcpy (ifr.ifr_name, mgts[i]);
        if (ioctl (fd, SIOCGIFFLAGS, &ifr) < 0) {
            continue;
        } else {
            if (ifr.ifr_flags & IFF_RUNNING) {
                *plugged = true;
            }

            edata.cmd = 0x0000000a;
            ifr.ifr_data = (caddr_t)&edata;
            if (ioctl (fd, 0x8946, &ifr) < 0) {
                continue;
            } else {
                if (edata.data) {
                    *linked = true;
                }
            }
            /* A valid mgt detected. */
            break;
        }
    }
    close (fd);
    return true;
}

static bool bf_pltfm_ucli_usb_detected () {
    /* In some case USB is not right removed /dev/sdx may be different with /dev/sdb* */
    return ((0 == access ("/dev/sdb", F_OK)) ||
            (0 == access ("/dev/sdc", F_OK)) ||
            (0 == access ("/dev/sdd", F_OK)) ||
            (0 == access ("/dev/sde", F_OK)));
}

static void bf_pltfm_ucli_format_psu_acdc (bf_pltfm_pwr_supply_info_t *psu, char *fmt) {
    sprintf (fmt, "%s      ", "    ");
    if (psu->presence) {
        if (psu->power) {
            sprintf (fmt, "%s      ", (psu->fvalid & PSU_INFO_AC) ? "AC* " : "DC* ");
        } else {
            sprintf (fmt, "%s      ", (psu->fvalid & PSU_INFO_AC) ? "AC  " : "DC  ");
        }
    }
}
static void bf_pltfm_ucli_format_psu_watt (bf_pltfm_pwr_supply_info_t *psu, char *fmt, bool in) {
    sprintf (fmt, "%s      ", "    ");
    if (psu->presence) {
        if (psu->power) {
            sprintf (fmt, "%-5.1f     ", in ? psu->pwr_in / 1000.0 : psu->pwr_out / 1000.0);
        }
    }
}
static void bf_pltfm_ucli_format_psu_ampere (bf_pltfm_pwr_supply_info_t *psu, char *fmt, bool in) {
    sprintf (fmt, "%s      ", "    ");
    if (psu->presence) {
        if (psu->power) {
            sprintf (fmt, "%-5.1f     ", in ? psu->iin / 1000.0 : psu->iout / 1000.0);
        }
    }
}
static void bf_pltfm_ucli_format_psu_volt (bf_pltfm_pwr_supply_info_t *psu, char *fmt, bool in) {
    sprintf (fmt, "%s      ", "    ");
    if (psu->presence) {
        if (psu->power) {
            sprintf (fmt, "%-5.1f     ", in ? psu->vin / 1000.0 : psu->vout / 1000.0);
        }
    }

}

static void bf_pltfm_ucli_format_transceiver (int a1, int d, int n, int o, char *str, bool is_qsfp) {
    int module;
    uint32_t conn_id, chnnl_id = 0;

    /* Arithmetic progression, an = a1 + (n - o) *d */
    module = a1 + (n - o) * d;
    if (is_qsfp) {
        bf_pltfm_qsfp_lookup_by_module(module, &conn_id);
        sprintf (str, "%2d/-", conn_id);
    } else {
        bf_pltfm_sfp_lookup_by_module(module, &conn_id, &chnnl_id);
        sprintf (str, "%2d/%d", conn_id, chnnl_id);
    }
}

static void bf_pltfm_ucli_format_transceiver_alias (int a1, int d, int n, int o, char *str, bool is_qsfp) {
    int module;
    uint32_t conn_id, chnnl_id = 0;

    /* Arithmetic progression, an = a1 + (n - o) *d */
    module = a1 + (n - o) * d;
    if (is_qsfp) {
        bf_pltfm_qsfp_lookup_by_module(module, &conn_id);
        if (platform_type_equal(AFN_X732QT)) {
            sprintf (str, "Q%02d%c", module, bf_qsfp_is_present(module) ? '*' : ' ');
        } else {
            sprintf (str, "C%02d%c", module, bf_qsfp_is_present(module) ? '*' : ' ');
        }
    } else {
        bf_pltfm_sfp_lookup_by_module(module, &conn_id, &chnnl_id);
        sprintf (str, "Y%02d%c", module, bf_sfp_is_present(module) ? '*' : ' ');
    }
}

static void bf_pltfm_ucli_dump_x564p_panel(ucli_context_t
                           *uc) {
    int i;
    const char *boarder = "==== ";
    char str[5] = {0}, fmt[128] = {0};

    /////////////////////////////////////////  Front Panel /////////////////////////////////////////
    // 1st
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 1, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "MGT ");
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 1, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    /*
     * "P*" : Plugged and Linked
     * "P " : Plugged but not Linked
     * "  " : Not Plugged (always means not linked)
     **/
    bool plugged = false, linked = false;
    bf_pltfm_ucli_mgt_detected(&plugged, &linked);
    aim_printf (&uc->pvs, "%s ",  linked ? "P*  " : (plugged ? "P   " : "    "));
    aim_printf (&uc->pvs, "\n");

    // 2nd
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 1, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "USB ");
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 1, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "\n");

    // 3rd
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (33, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            aim_printf (&uc->pvs, "%s ", "    ");
        }
    }
    aim_printf (&uc->pvs, "CON ");
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (33, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            aim_printf (&uc->pvs, "%s ", "    ");
        }
    }
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "\n");

    // 4th
    for (i = 0; i < 18; i ++) {
        if (i == 0) aim_printf (&uc->pvs, "%s", "     ");
        else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (34, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            aim_printf (&uc->pvs, "%s ", "    ");
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (34, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            aim_printf (&uc->pvs, "%s ", "    ");
        }
    }
    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 18; i ++) {
        if (i == 0 || i == 17) aim_printf (&uc->pvs, "%s", "     ");
        else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");


    /////////////////////////////////////////  Rear Panel /////////////////////////////////////////
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /* FAN */
    aim_printf (&uc->pvs, "%s      ", "FAN1");
    aim_printf (&uc->pvs, "%s      ", "FAN2");
    aim_printf (&uc->pvs, "%s      ", "FAN3");
    aim_printf (&uc->pvs, "%s      ", "FAN4");
    /* PSU */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s ^    ", "PSU2");
    aim_printf (&uc->pvs, "%s      ", "PSU1");
    aim_printf (&uc->pvs, "\n");

    /* FAN rota */
    bf_pltfm_fan_data_t fdata;
    int err = bf_pltfm_chss_mgmt_fan_data_get (
                &fdata);
    if (!err && !fdata.fantray_present) {
        /* Pls help this consistency with real hardware. */
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[1].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[3].front_speed);
    } else {
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
    }
    /* PSU present and vin/vout/pin/pout/iin/iout */
    bf_pltfm_pwr_supply_info_t info1, info2;
    memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
    memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));

    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                           &info1);

    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                           &info2);
    /* AC or DC */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");

    bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pin */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pout */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vin */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vout */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iin */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iout */
    for (i = 0; i < 7; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

}

static void bf_pltfm_ucli_dump_x532p_panel(ucli_context_t
                           *uc) {
    int i;
    const char *boarder = "==== ";
    char str[5] = {0}, fmt[128] = {0};

    /////////////////////////////////////////  Front Panel /////////////////////////////////////////
    // 1st
    for (i = 0; i < 19; i ++) {
        if (i == 0) aim_printf (&uc->pvs, "%s", "     ");
        else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "CON ");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "    ");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    // 2nd
    for (i = 0; i < 19; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "USB ");
    aim_printf (&uc->pvs, "%s ", "MGT ");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    aim_printf (&uc->pvs, "%s ", bf_pltfm_ucli_usb_detected() ? "*   " : "    ");
    /*
     * "P*" : Plugged and Linked
     * "P " : Plugged but not Linked
     * "  " : Not Plugged (always means not linked)
     **/
    bool plugged = false, linked = false;
    bf_pltfm_ucli_mgt_detected(&plugged, &linked);
    aim_printf (&uc->pvs, "%s ",  linked ? "P*  " : (plugged ? "P   " : "    "));
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 19; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /////////////////////////////////////////  Rear Panel /////////////////////////////////////////
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 19; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /* FAN */
    aim_printf (&uc->pvs, "%s      ", "FAN1");
    aim_printf (&uc->pvs, "%s      ", "FAN2");
    aim_printf (&uc->pvs, "%s      ", "FAN3");
    aim_printf (&uc->pvs, "%s      ", "FAN4");
    aim_printf (&uc->pvs, "%s      ", "FAN5");
    /* PSU */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "PSU2");
    aim_printf (&uc->pvs, "%s      ", "PSU1");
    aim_printf (&uc->pvs, "\n");

    /* FAN rota */
    bf_pltfm_fan_data_t fdata;
    int err = bf_pltfm_chss_mgmt_fan_data_get (
                &fdata);
    if (!err && !fdata.fantray_present) {
        /* Pls help this consistency with real hardware. */
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[4].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[6].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[8].front_speed);
    } else {
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
    }
    /* PSU present and vin/vout/pin/pout/iin/iout */
    bf_pltfm_pwr_supply_info_t info1, info2;
    memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
    memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));

    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                           &info1);
    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                           &info2);
    /* AC or DC */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 19; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_ucli_dump_x308p_panel(ucli_context_t
                           *uc) {
    int i;
    const char *boarder = "==== ";
    char str[5] = {0}, fmt[128] = {0};

    /////////////////////////////////////////  Front Panel /////////////////////////////////////////
    // 1st
    for (i = 0; i < 28; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 28; i ++) {
        if (i > 23) {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 24, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 28; i ++) {
        if (i > 23) {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 24, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    // 2nd
    for (i = 0; i < 28; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 28; i ++) {
        if (i > 23) {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 24, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    for (i = 0; i < 28; i ++) {
        if (i > 23) {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 24, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 28; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /////////////////////////////////////////  Rear Panel /////////////////////////////////////////
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 28; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    bf_pltfm_pwr_supply_info_t info1, info2;

    if (platform_subtype_equal(V1P0) ||
        platform_subtype_equal(V1P1) ||
        platform_subtype_equal(V2P0)) {
        /* 1st : CON */
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "CON ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");

        aim_printf (&uc->pvs, "\n");

        /* 2nd */
        aim_printf (&uc->pvs, "%s      ", "PSU2");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "FAN5");
        aim_printf (&uc->pvs, "%s      ", "FAN1");
        aim_printf (&uc->pvs, "%s      ", "FAN2");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "USB ");
        aim_printf (&uc->pvs, "%s      ", "MGT ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "FAN3");
        aim_printf (&uc->pvs, "%s      ", "FAN4");
        aim_printf (&uc->pvs, "%s      ", "FAN6");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "PSU1");
        aim_printf (&uc->pvs, "\n");

        /* 3rd */
        bf_pltfm_fan_data_t fdata;
        int err = bf_pltfm_chss_mgmt_fan_data_get (
                    &fdata);

        memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
        memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));
        err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                            &info1);
        err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                            &info2);
        /* AC or DC */
        bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
        aim_printf (&uc->pvs, "%s", fmt);

        aim_printf (&uc->pvs, "%s      ", "    ");

        if (!err && !fdata.fantray_present) {
            /* Pls help this consistency with real hardware. */
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[8].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        } else {
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
        }

        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", bf_pltfm_ucli_usb_detected() ? "*   " : "    ");

        /*
        * "P*" : Plugged and Linked
        * "P " : Plugged but not Linked
        * "  " : Not Plugged (always means not linked)
        **/
        bool plugged = false, linked = false;
        bf_pltfm_ucli_mgt_detected(&plugged, &linked);
        aim_printf (&uc->pvs, "%s      ",  linked ? "P*  " : (plugged ? "P   " : "    "));

        aim_printf (&uc->pvs, "%s      ", "    ");

        if (!err && !fdata.fantray_present) {
            /* Pls help this consistency with real hardware. */
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[4].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[6].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[10].front_speed);
        } else {
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
        }

        aim_printf (&uc->pvs, "%s      ", "    ");

        /* AC or DC */
        bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
        aim_printf (&uc->pvs, "%s", fmt);

        aim_printf (&uc->pvs, "\n");
    } else if (platform_subtype_equal(V3P0)) {
        /* 1st : CON */
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "CON ");
        aim_printf (&uc->pvs, "%s      ", "PTP ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");

        aim_printf (&uc->pvs, "\n");

        /* 2nd */
        aim_printf (&uc->pvs, "%s      ", "PSU2");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "FAN4");
        aim_printf (&uc->pvs, "%s      ", "FAN1");
        aim_printf (&uc->pvs, "%s      ", "FAN2");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "USB ");
        aim_printf (&uc->pvs, "%s      ", "MGT1");
        aim_printf (&uc->pvs, "%s      ", "MGT2");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "FAN3");
        aim_printf (&uc->pvs, "%s      ", "FAN5");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "PSU1");
        aim_printf (&uc->pvs, "\n");

        /* 3rd */
        bf_pltfm_fan_data_t fdata;
        int err = bf_pltfm_chss_mgmt_fan_data_get (
                    &fdata);

        memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
        memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));
        err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                            &info1);
        err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                            &info2);
        /* AC or DC */
        bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
        aim_printf (&uc->pvs, "%s", fmt);

        aim_printf (&uc->pvs, "%s      ", "    ");

        if (!err && !fdata.fantray_present) {
            /* Pls help this consistency with real hardware. */
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[6].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        } else {
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
        }

        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", bf_pltfm_ucli_usb_detected() ? "*   " : "    ");

        /*
        * "P*" : Plugged and Linked
        * "P " : Plugged but not Linked
        * "  " : Not Plugged (always means not linked)
        **/
        bool mgmt1_plugged = false, mgmt1_linked = false;
        bool mgmt2_plugged = false, mgmt2_linked = false;
        aim_printf (&uc->pvs, "%s      ",  mgmt1_linked ? "P*  " : (mgmt1_plugged ? "P   " : "    "));
        aim_printf (&uc->pvs, "%s      ",  mgmt2_linked ? "P*  " : (mgmt2_plugged ? "P   " : "    "));

        aim_printf (&uc->pvs, "%s      ", "    ");

        if (!err && !fdata.fantray_present) {
            /* Pls help this consistency with real hardware. */
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[4].front_speed);
            aim_printf (&uc->pvs, "%-5u     ", fdata.F[8].front_speed);
        } else {
            aim_printf (&uc->pvs, "%s      ", "    ");
            aim_printf (&uc->pvs, "%s      ", "    ");
        }

        aim_printf (&uc->pvs, "%s      ", "    ");

        /* AC or DC */
        bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
        aim_printf (&uc->pvs, "%s", fmt);

        aim_printf (&uc->pvs, "\n");
    }

    /* 4th, pin */
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* 5th, pout */
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* 6th, vin */
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* 7th, vout */
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* 8th, iin */
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* 9th, iout */
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 12; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 28; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

}

static void bf_pltfm_ucli_dump_x312p_panel(ucli_context_t
                           *uc) {
    int i;
    const char *boarder = "==== ";
    char str[5] = {0}, fmt[128] = {0};

    /////////////////////////////////////////  Front Panel /////////////////////////////////////////
    // 1st
    for (i = 0; i < 24; i ++) {
        if (i >= 18) aim_printf (&uc->pvs, "%s", "     ");
        else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "MGT ");
    for (i = 0; i < 23; i ++) {
        if (i > 16) aim_printf (&uc->pvs, "%s", "");
        else {
            if (i != 0) {
                bf_pltfm_ucli_format_transceiver (1, 3, i, 1, str, false);
            } else {
                bf_pltfm_ucli_format_transceiver (49, 3, i, 0, str, false);
            }
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    /*
     * "P*" : Plugged and Linked
     * "P " : Plugged but not Linked
     * "  " : Not Plugged (always means not linked)
     **/
    bool plugged = false, linked = false;
    bf_pltfm_ucli_mgt_detected(&plugged, &linked);
    aim_printf (&uc->pvs, "%s ",  linked ? "P*  " : (plugged ? "P   " : "    "));
    for (i = 0; i < 23; i ++) {
        if (i > 16) aim_printf (&uc->pvs, "%s", "");
        else {
            if (i != 0) {
                bf_pltfm_ucli_format_transceiver_alias (1, 3, i, 1, str, false);
            } else {
                bf_pltfm_ucli_format_transceiver_alias (49, 3, i, 0, str, false);
            }
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    // 2nd
    for (i = 0; i < 24; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "CON ");
    for (i = 0; i < 23; i ++) {
        if (i > 16) {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 17, str, true);
        } else {
            if (i != 0) {
                bf_pltfm_ucli_format_transceiver (2, 3, i, 1, str, false);
            } else {
                bf_pltfm_ucli_format_transceiver (50, 3, i, 0, str, false);
            }
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    aim_printf (&uc->pvs, "%s ", "    ");
    for (i = 0; i < 23; i ++) {
        if (i > 16) {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 17, str, true);
        } else {
            if (i != 0) {
                bf_pltfm_ucli_format_transceiver_alias (2, 3, i, 1, str, false);
            } else {
                bf_pltfm_ucli_format_transceiver_alias (50, 3, i, 0, str, false);
            }
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "\n");

    // 3rd
    for (i = 0; i < 24; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "USB ");
    for (i = 0; i < 22; i ++) {
        if (i > 15) {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 16, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver (3, 3, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", bf_pltfm_ucli_usb_detected() ? "*   " : "    ");
    for (i = 0; i < 22; i ++) {
        if (i > 15) {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 16, str, true);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (3, 3, i, 0, str, false);
        }
        aim_printf (&uc->pvs, "%s ", str);
    }
    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 24; i ++) {
            if (i == 0) aim_printf (&uc->pvs, "%s", "     ");
            else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");

    /////////////////////////////////////////  Rear Panel /////////////////////////////////////////
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 24; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /* FAN */
    aim_printf (&uc->pvs, "%s      ", "FAN1");
    aim_printf (&uc->pvs, "%s      ", "FAN2");
    aim_printf (&uc->pvs, "%s      ", "FAN3");
    aim_printf (&uc->pvs, "%s      ", "FAN4");
    aim_printf (&uc->pvs, "%s      ", "FAN5");
    /* PSU */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "PSU2");
    aim_printf (&uc->pvs, "%s      ", "PSU1");
    aim_printf (&uc->pvs, "\n");

    /* FAN rota */
    bf_pltfm_fan_data_t fdata;
    int err = bf_pltfm_chss_mgmt_fan_data_get (
                &fdata);
    if (!err && !fdata.fantray_present) {
        /* Pls help this consistency with real hardware. */
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[4].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[6].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[8].front_speed);
    } else {
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
    }
    /* PSU present and vin/vout/pin/pout/iin/iout */
    bf_pltfm_pwr_supply_info_t info1, info2;
    memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
    memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));

    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                           &info1);

    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                           &info2);
    /* AC or DC */
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iin */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iout */
    for (i = 0; i < 8; i ++) aim_printf (&uc->pvs, "%s      ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 24; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_ucli_dump_x732q_panel(ucli_context_t
                           *uc) {
    int i;
    const char *boarder = "==== ";
    char str[5] = {0}, fmt[128] = {0};

    /////////////////////////////////////////  Front Panel /////////////////////////////////////////
    // 1st
    for (i = 0; i < 18; i ++) {
        if (i == 0) aim_printf (&uc->pvs, "%s", "     ");
        else aim_printf (&uc->pvs, "%s", boarder);
    }
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "MGT1");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    /*
     * "P*" : Plugged and Linked
     * "P " : Plugged but not Linked
     * "  " : Not Plugged (always means not linked)
     **/
    bool plugged = false, linked = false;
    bf_pltfm_ucli_mgt_detected(&plugged, &linked);
    aim_printf (&uc->pvs, "%s ",  linked ? "P*  " : (plugged ? "P   " : "    "));
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (1, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    // 2nd
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "%s ", "MGT2");
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    /* Alias and present. */
    /*
     * "P*" : Plugged and Linked
     * "P " : Plugged but not Linked
     * "  " : Not Plugged (always means not linked)
     **/
    bf_pltfm_ucli_mgt_detected(&plugged, &linked);
    aim_printf (&uc->pvs, "%s ",  linked ? "P*  " : (plugged ? "P   " : "    "));
    for (i = 0; i < 17; i ++) {
        if (i > 0) {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 1, str, true);
            aim_printf (&uc->pvs, "%s ", str);
        } else {
            bf_pltfm_ucli_format_transceiver_alias (2, 2, i, 0, str, false);
            aim_printf (&uc->pvs, "%s ", str);
        }
    }
    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    /////////////////////////////////////////  Rear Panel /////////////////////////////////////////
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");

    aim_printf (&uc->pvs, "%s ", "PSU2");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN5");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN1");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN2");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "USB ");
    aim_printf (&uc->pvs, "%s ", "CON ");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN3");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN4");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "FAN6");
    aim_printf (&uc->pvs, "%s ", "    ");
    aim_printf (&uc->pvs, "%s ", "PSU1");
    aim_printf (&uc->pvs, "\n");

    /* PSU info */
    bf_pltfm_pwr_supply_info_t info1, info2;
    memset (&info1, 0, sizeof (bf_pltfm_pwr_supply_info_t));
    memset (&info2, 0, sizeof (bf_pltfm_pwr_supply_info_t));
    int err = bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY1,
                                           &info1);
    err |= bf_pltfm_chss_mgmt_pwr_supply_get (POWER_SUPPLY2,
                                           &info2);

    /* PSU 2 AC DC */
    bf_pltfm_ucli_format_psu_acdc (&info2, fmt);
    aim_printf (&uc->pvs, "%s", fmt);

    /* FAN rota */
    bf_pltfm_fan_data_t fdata;
    err |= bf_pltfm_chss_mgmt_fan_data_get (
                &fdata);
    if (!err && !fdata.fantray_present) {
        /* Pls help this consistency with real hardware. */
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[0].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[2].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[4].front_speed);
        aim_printf (&uc->pvs, "%s ", bf_pltfm_ucli_usb_detected() ? "*   " : "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[6].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[8].front_speed);
        aim_printf (&uc->pvs, "%-5u     ", fdata.F[10].front_speed);
    } else {
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
        aim_printf (&uc->pvs, "%s      ", "    ");
    }

    /* PSU 1 AC DC */
    bf_pltfm_ucli_format_psu_acdc (&info1, fmt);
    aim_printf (&uc->pvs, "%s", fmt);
    aim_printf (&uc->pvs, "\n");

    /* Pin */
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Pout */
    bf_pltfm_ucli_format_psu_watt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_watt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vin */
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Vout */
    bf_pltfm_ucli_format_psu_volt (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_volt (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iin */
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, true);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    /* Iout */
    bf_pltfm_ucli_format_psu_ampere (&info2, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);
    for (i = 0; i < 15; i ++) aim_printf (&uc->pvs, "%s ", "    ");
    bf_pltfm_ucli_format_psu_ampere (&info1, fmt, false);
    aim_printf (&uc->pvs, "%s", fmt);

    aim_printf (&uc->pvs, "\n");

    for (i = 0; i < 18; i ++) aim_printf (&uc->pvs, "%s", boarder);
    aim_printf (&uc->pvs, "\n");
}

static void bf_pltfm_ucli_dump_panel(ucli_context_t
                           *uc) {
    if (platform_type_equal (AFN_X564PT)) {
        bf_pltfm_ucli_dump_x564p_panel(uc);
    } else if (platform_type_equal (AFN_X532PT)) {
        bf_pltfm_ucli_dump_x532p_panel(uc);
    } else if (platform_type_equal (AFN_X308PT)) {
        bf_pltfm_ucli_dump_x308p_panel(uc);
    } else if (platform_type_equal (AFN_X312PT)) {
        bf_pltfm_ucli_dump_x312p_panel(uc);
    } else if (platform_type_equal (AFN_X732QT)) {
        bf_pltfm_ucli_dump_x732q_panel(uc);
    }
}

static void
bf_pltfm_convert_uptime(double diffsec,
    uint32_t *days, uint32_t *hours, uint32_t *mins, uint32_t *secs) {
    *days = *hours = *mins = *secs = 0;
    *days  = (uint32_t)diffsec / (3600 * 24);
    *hours = ((uint32_t)diffsec % (3600 * 24)) / 3600;
    *mins  = (((uint32_t)diffsec % (3600 * 24)) % 3600) / 60;
    *secs  = (uint32_t)diffsec % 60;
}

static ucli_status_t
bf_pltfm_ucli_ucli__bsp__ (ucli_context_t
                           *uc)
{
    char fmt[128];
    double diff;

    UCLI_COMMAND_INFO (
        uc, "bsp", 0, "Show BSP version.");

    platform_name_get_str (fmt, sizeof (fmt));

    aim_printf (&uc->pvs, "Ver    %s, %s\n",
                VERSION_NUMBER, "9.13.x-lts");

    aim_printf (&uc->pvs, "Platform  : %s\n",
                bf_pltfm_bd_product_name_get());

    aim_printf (&uc->pvs, " BD ID    : %s\n",
                fmt);
    aim_printf (&uc->pvs, "Max FANs  : %2d\n",
                bf_pltfm_mgr_ctx()->fan_group_count *
                bf_pltfm_mgr_ctx()->fan_per_group);
    aim_printf (&uc->pvs, "Max PSUs  : %2d\n",
                bf_pltfm_mgr_ctx()->psu_count);
    aim_printf (&uc->pvs, "Max SNRs  : %2d\n",
                bf_pltfm_mgr_ctx()->sensor_count);
    aim_printf (&uc->pvs, "Max QSFPs : %2d\n",
                bf_qsfp_get_max_qsfp_ports());
    aim_printf (&uc->pvs, "Max SFPs  : %2d\n",
                bf_sfp_get_max_sfp_ports());
    aim_printf (&uc->pvs, "Max CPLDs : %2d\n",
                bf_pltfm_mgr_ctx()->cpld_count);
    foreach_element (0, bf_pltfm_mgr_ctx()->cpld_count) {
        bf_pltfm_get_cpld_ver((uint8_t)(each_element + 1), &fmt[0], false);
        aim_printf (&uc->pvs, "    CPLD%d : %s\n",
                    (each_element + 1), fmt);
    }
    aim_printf (&uc->pvs, "Max DPUs  : ");
    if (platform_type_equal(AFN_X532PT) ||
        platform_type_equal(AFN_X564PT) ||
        platform_type_equal(AFN_X732QT)) {
        aim_printf (&uc->pvs, "%s\n",
                "Not Supported");
    } else {
        aim_printf (&uc->pvs, "%2d\n",
                bf_pltfm_mgr_ctx()->dpu_count);
        bool dpu1_status, dpu2_status, ptpx_status;
        bf_pltfm_get_suboard_status (&dpu1_status, &dpu2_status, &ptpx_status);
        aim_printf (&uc->pvs, "    DPU1  : %s\n",
                    dpu1_status ? "Present" : "Absent");
        aim_printf (&uc->pvs, "    DPU2  : %s\n",
                    dpu2_status ? "Present" : "Absent");
        if (platform_type_equal(AFN_X308PT) &&
            platform_subtype_equal(V3P0)) {
            aim_printf (&uc->pvs, "     PTP  : %s\n",
                        ptpx_status ? "Present" : "Absent");
        }
        aim_printf (&uc->pvs, "\n");
    }

    bf_pltfm_get_bmc_ver (&fmt[0], false);
    aim_printf (&uc->pvs, "    BMC   : %s\n\n",
               fmt);

    aim_printf (&uc->pvs, "Plat Mon  : %s %2X @ %s\n",
                (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_CTRL) ?
                "enabled" : "disabled", bf_pltfm_mgr_ctx()->flags,
                ctime ((time_t *) &bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date));

    uint32_t days, hours, mins, secs;
    time(&g_tm_end);
    diff = difftime(g_tm_end, g_tm_start);
    bf_pltfm_convert_uptime (diff, &days, &hours, &mins, &secs);
    aim_printf (&uc->pvs, "    Uptime: ");
    if (days) aim_printf (&uc->pvs, "%3u days ", days);
    if (hours || mins) aim_printf (&uc->pvs, "%2u:%2u.", hours, mins);
    aim_printf (&uc->pvs, "%2u sec(s)\n", secs);

    FILE *fp = fopen ("/proc/uptime", "r");
    if (fp) {
        if (fgets (fmt, 128 - 1, fp)) {
            if (sscanf (fmt, "%lf ", &diff)) {
                bf_pltfm_convert_uptime (diff, &days, &hours, &mins, &secs);
                aim_printf (&uc->pvs, "Sys Uptime: ");
                if (days) aim_printf (&uc->pvs, "%3u days ", days);
                if (hours || mins) aim_printf (&uc->pvs, "%2u:%2u.", hours, mins);
                aim_printf (&uc->pvs, "%2u sec(s)\n", secs);
            }
        }
        fclose (fp);
    }
    aim_printf (&uc->pvs, "    LogExt: %s\n", BF_DRIVERS_LOG_EXT);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    bf_pltfm_ucli_dump_panel(uc);

    aim_printf (&uc->pvs, "\n");
    aim_printf (&uc->pvs, "\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__console__ (ucli_context_t
                           *uc)
{
    bool has_dpu = false;
    bool has_ptp = false;
    bool redirect_bmc  = false;
    bool redirect_dpu1 = false;
    bool redirect_dpu2 = false;
    bool redirect_cme  = false;
    bool redirect_ptp  = false;
    uint8_t buf[2] = {0x00, 0xaa};
    int usec_delay = bf_pltfm_get_312_bmc_comm_interval();

    /* Keep the name consistent with the document X-T Programmable Bare Metal. */
    UCLI_COMMAND_INFO (uc, "console", 1, "Redirect console to <bmc/come/ptp/dpu1/dpu2>");

    if (memcmp (uc->pargs->args[0], "bmc", 3) == 0) {
        redirect_bmc = true;
    } else if (memcmp (uc->pargs->args[0], "come", 4) == 0){
        redirect_cme = true;
    } else if (memcmp (uc->pargs->args[0], "dpu1", 4) == 0){
        redirect_dpu1 = true;
    } else if (memcmp (uc->pargs->args[0], "dpu2", 4) == 0){
        redirect_dpu2 = true;
    } else if (memcmp (uc->pargs->args[0], "ptp", 3) == 0){
        redirect_ptp = true;
    } else {
        aim_printf (&uc->pvs, "Usage: console <bmc/come/ptp/dpu1/dpu2>\n");
        return 0;
    }

    aim_printf (&uc->pvs, "Console redirects to %s\n",
        redirect_bmc  ? "BMC  " : \
        redirect_cme  ? "COM-E" : \
        redirect_dpu1 ? "DPU-1" : \
        redirect_dpu2 ? "DPU-2" : \
        redirect_ptp  ? "PTP  " : \
        "Unknown");
    LOG_WARNING ("Console redirects to %s\n",
        redirect_bmc  ? "BMC  " : \
        redirect_cme  ? "COM-E" : \
        redirect_dpu1 ? "DPU-1" : \
        redirect_dpu2 ? "DPU-2" : \
        redirect_ptp  ? "PTP  " : \
        "Unknown");

    if (platform_type_equal (AFN_X312PT) ||
        platform_type_equal (AFN_X308PT)) {
        has_dpu = true;
    }

    if (platform_type_equal (AFN_X312PT)) {
        if (redirect_bmc){
            buf[0] = 0x02;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, NULL, 0, usec_delay);
        } else if (redirect_cme){
            buf[0] = 0x03;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, NULL, 0, usec_delay);
        } else if (redirect_dpu1){
            buf[0] = 0x00;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, NULL, 0, usec_delay);
        } else if (redirect_dpu2){
            buf[0] = 0x01;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, NULL, 0, usec_delay);
        } else {
            return 0;
        }
    } else if (platform_type_equal (AFN_X308PT) ||
               platform_type_equal (AFN_X532PT) ||
               platform_type_equal (AFN_X564PT)) {
        if (bf_pltfm_compare_bmc_ver("v3.1.0") < 0) {
            aim_printf (&uc->pvs, "Not supported on this BMC version yet! "
                                  "Please upgrade to v3.1.0 and above.\n");
            return 0;
        }

        if (platform_type_equal (AFN_X532PT) || platform_type_equal (AFN_X564PT)) {
            has_dpu = false;
            has_ptp = false;
        } else {
            /* X308P-T V3. */
            if (platform_type_equal (AFN_X308PT) && platform_subtype_equal (V3P0)) {
                has_ptp = true;
            }
        }

        if (redirect_bmc){
            buf[0] = 0x00;
            bf_pltfm_bmc_uart_write_read (0x21, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
        } else if (redirect_cme){
            buf[0] = 0x01;
            bf_pltfm_bmc_uart_write_read (0x21, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
        } else if (has_dpu && redirect_dpu1){
            buf[0] = 0x02;
            bf_pltfm_bmc_uart_write_read (0x21, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
        } else if (has_dpu && redirect_dpu2){
            buf[0] = 0x03;
            bf_pltfm_bmc_uart_write_read (0x21, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
        } else if (has_ptp && redirect_ptp){
            buf[0] = 0x04;
            bf_pltfm_bmc_uart_write_read (0x21, buf, 2, NULL, 0, BMC_COMM_INTERVAL_US);
        } else {
            return 0;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__bmc_ota__ (ucli_context_t
                               *uc)
{
    int sec = 30;
    char fname[256], bmc_ver[32] = {0};
    char c = 'N';
    bool forced = false;

    UCLI_COMMAND_INFO (uc, "bmc-ota", -1, "bmc-ota <file name> <-f>, upgrade BMC firmware");

    if ((uc->pargs->count < 1) || (uc->pargs->count > 2)) {
        aim_printf (&uc->pvs, "Usage: bmc-ota <file name> <-f>\n");
        return 0;
    }

    strncpy (fname, uc->pargs->args[0],
             sizeof (fname) - 1);
    fname[sizeof (fname) - 1] = '\0';

    if (strstr(fname, ".rbl") == NULL) {
        aim_printf (&uc->pvs, "Only support .rbl file!\n");
        return 0;
    }

    if (uc->pargs->count == 2) {
        if (memcmp (uc->pargs->args[1], "-f", 2) == 0) {
            forced = true;
        } else {
            aim_printf (&uc->pvs, "Usage: bmc-ota <file name> <-f>\n");
            return 0;
        }
    }

    /* Get real version to make a right decision and update the cache.
     * Otherwise requires a re-launch on app even through the BMC has been upgrade via YMode.
     * by tsihang, 2023-07-12. */
    bf_pltfm_get_bmc_ver (&bmc_ver[0], true);
    aim_printf (&uc->pvs, "\nCurrent BMC version: %s\n\n", bmc_ver);

    if ((platform_type_equal (AFN_X308PT) ||
         platform_type_equal (AFN_X532PT) ||
         platform_type_equal (AFN_X564PT)) &&
        (bf_pltfm_compare_bmc_ver("v3.1.0") >= 0)) {
        aim_printf (&uc->pvs, "\nThere could be port link risk when upgrade BMC online.\n");

        if (!forced) {
            aim_printf (
                    &uc->pvs,"Enter Y/N: ");
            int x = scanf("%c", &c); x = x;
            aim_printf (
                    &uc->pvs,"%c\n", c);
            if ((c != 'Y') && (c != 'y')) {
                aim_printf (
                        &uc->pvs,"Abort\n");
                return 0;
            }
        }

        sleep (1);

        bf_pltfm_health_monitor_enable (false);

        /* Make sure no BMC access in health monitor. */
        do {
            sleep (1);
        } while (!(bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_IDLE));

        aim_printf (&uc->pvs, "Upload BMC firmware ... \n");

		if (bf_pltfm_bmc_uart_ymodem_send_file(fname) > 0) {
            aim_printf (&uc->pvs, "Done\n");
            aim_printf (&uc->pvs, "Performing upgrade, %d sec(s) ...\n", sec);
            /* Not very sure, is it required to wait BMC here. */
            sleep(sec);
            bf_pltfm_get_bmc_ver (&bmc_ver[0], true);
            aim_printf (&uc->pvs, "\nNew BMC version: %s\n", bmc_ver);
        } else {
            aim_printf (&uc->pvs, "Failed.\n");
        }
        bf_pltfm_health_monitor_enable (true);
    } else {
        if (platform_type_equal (AFN_X312PT)) {
            aim_printf (&uc->pvs, "Not supported on this platform yet!\n");
        } else {
            aim_printf (&uc->pvs, "Not supported on this BMC version yet!\n"
                                  "Required BMC version in 3.1.x and later.\n"
                                  "Please upgrade BMC via Ymodem.\n"
                                  "Upgrade BMC online could occure link risk for a port.\n");
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__reset_hwcomp__ (ucli_context_t
                           *uc)
{
    bool reset_bmc  = false;
    bool reset_dpu1 = false;
    bool reset_dpu2 = false;
    bool reset_tof  = false;
    bool reset_ptp  = false;
    bool do_reset_by_val_1 = false; /* perform reset by value 1 or value 0. */
    bool perform = false;
    bool forced  = false;
    bool dbg = false;

    /* Keep the name consistent with the document X-T Programmable Bare Metal. */
    UCLI_COMMAND_INFO (uc, "reset", -1, "Reset <tof/bmc/ptp/dpu1/dpu2> <-f>");

    if ((uc->pargs->count < 1) || (uc->pargs->count > 2)) {
        aim_printf (&uc->pvs, "Usage: reset <tof/bmc/ptp/dpu1/dpu2> <-f>\n");
        return 0;
    }

    if (memcmp (uc->pargs->args[0], "tof", 3) == 0){
        reset_tof  = true;
        perform = false;    /* Never try to reset tof as it will make the bf_switchd quit. */
    } else if (memcmp (uc->pargs->args[0], "bmc", 3) == 0){
        reset_bmc  = true;
    } else if (memcmp (uc->pargs->args[0], "dpu1", 4) == 0){
        reset_dpu1 = true;
    } else if (memcmp (uc->pargs->args[0], "dpu2", 4) == 0){
        reset_dpu2 = true;
    } else if (memcmp (uc->pargs->args[0], "ptp", 3) == 0){
        reset_ptp  = true;
    } else {
        aim_printf (&uc->pvs, "Usage: reset <tof/bmc/ptp/dpu1/dpu2> <-f>\n");
        return 0;
    }

    if (uc->pargs->count == 2) {
        if (memcmp (uc->pargs->args[1], "-f", 2) == 0) {
            forced = true;
        } else {
            aim_printf (&uc->pvs, "Usage: reset <tof/bmc/ptp/dpu1/dpu2> <-f>\n");
            return 0;
        }
    }

    if (dbg) {
        aim_printf (
            &uc->pvs,
            "reset: <tof=%s> <bmc=%s> <dpu1=%s> <dpu2=%s> <ptp=%s>\n",
            reset_tof  ? "T" : "F",
            reset_bmc  ? "T" : "F",
            reset_dpu1 ? "T" : "F",
            reset_dpu2 ? "T" : "F",
            reset_ptp  ? "T" : "F");
    }

    /* Perform a reset via cpld for X532P/X564P/X308P.
     * Do we need check cpld version here ? */
    uint8_t cpld_index = 0; /* CPLD index from CME view. */
    uint8_t aoff = 0;       /* Address offset of this control. */
    uint8_t boff = 0;       /* Bit offset. */
    uint8_t val = 0, val_rd = 0;

    if (platform_type_equal (AFN_X312PT)) {
        cpld_index = 1;
        /* Implemented in address offset 13. */
        aoff = 13;
        /* Reset assert by written 0, reset de-assert by written 1. */
        do_reset_by_val_1 = false;

        if (reset_tof) {
            /* Not ready so far. */
        } else if (reset_bmc) {
            /* Not ready so far. */
        } else if (reset_dpu1) {
            /* Implemented in bit offset 2. */
            boff = 2;
            /* Prepared, do it right now. */
            perform = true;
        } else if (reset_dpu2) {
            /* Implemented in bit offset 3. */
            boff = 3;
            /* Prepared, do it right now. */
            perform = true;
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        /* X308P-T equiped with only 1 cpld */
        cpld_index = 1;
        /* Implemented in address offset 2. */
        aoff = 2;
        /* Reset assert by written 1, reset de-assert by written 0. */
        do_reset_by_val_1 = true;

        if (reset_tof) {
            /* Implemented in bit offset 5. */
            boff = 5;
            /* Prepared, do it right now. */
            perform = true;
        } else if (reset_bmc) {
            /* Implemented in bit offset 0. */
            boff = 0;
            /* Prepared, do it right now. */
            perform = true;
        } else if (reset_dpu1) {
            /* Implemented in bit offset 6. */
            boff = 6;
            /* Prepared, do it right now. */
            perform = true;
        } else if (reset_dpu2) {
            /* Implemented in bit offset 7. */
            boff = 7;
            /* Prepared, do it right now. */
            perform = true;
        } else if ((reset_ptp) && platform_subtype_equal (V3P0)) {
            /* Implemented in address offset 2. */
            aoff = 3;
            /* Implemented in bit offset 7. */
            boff = 7;
            /* Prepared, do it right now. */
            perform = true;
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        cpld_index = 1;
        aoff = 2;
        do_reset_by_val_1 = true;
        if (reset_tof) {
            boff = 5;
            perform = true;
        } else if (reset_bmc) {
            boff = 4;
            perform = true;
            do_reset_by_val_1 = false;
        } else {
            /* No DPU. */
            perform = false;
        }

        /* Not support. */
        if ((reset_bmc || reset_tof) &&
            (platform_subtype_equal(V1P0) || platform_subtype_equal(V1P1))) {
            perform = false;
        }

    } else if (platform_type_equal (AFN_X564PT)) {
        /* Not ready so far. */
        cpld_index = 1;
        aoff = 2;
        do_reset_by_val_1 = true;
        if (reset_tof) {
            boff = 3;
            perform = true;
        } else if (reset_bmc) {
            boff = 7;
            perform = true;
            do_reset_by_val_1 = false;
        } else {
            /* No DPU. */
            perform = false;
        }
        /* Not support. */
        if ((reset_bmc || reset_tof) &&
            (platform_subtype_equal(V1P0) || platform_subtype_equal(V1P1) || platform_subtype_equal(V1P2))) {
            perform = false;
        }
    }

    if (dbg) {
        aim_printf (
            &uc->pvs,
            "<perform=%s> <do_reset_by_val_1=%s> <cpld=%d> <aoff=%d> <boff=%d>\n",
            perform  ? "T" : "F",
            do_reset_by_val_1 ? "T" : "F",
            cpld_index,
            aoff,
            boff);
    }

    if (!perform) {
        aim_printf (&uc->pvs, "Not supported.\n");
        return 0;
    }

    if (reset_tof) {
        aim_printf (
            &uc->pvs,
            "bf_switchd could quit during tof resetting and a cold boot required.\n");
        aim_printf (&uc->pvs, "Reset tof ...\n");
        LOG_WARNING ("Reset tof ...\n");
    } else if (reset_bmc) {
        aim_printf (
            &uc->pvs,
            "Ports could down during BMC resetting.\n");
        aim_printf (&uc->pvs, "Reset BMC ...\n");
        LOG_WARNING ("Reset BMC ...\n");
    } else if (reset_dpu1) {
        aim_printf (&uc->pvs, "Reset DPU1 ...\n");
        LOG_WARNING ("Reset DPU1 ...\n");
    } else if (reset_dpu2) {
        aim_printf (&uc->pvs, "Reset DPU2 ...\n");
        LOG_WARNING ("Reset DPU2 ...\n");
    }

    if (!forced) {
        char c = 0;
        aim_printf (
                &uc->pvs,"Enter Y/N: ");
        int x = scanf("%c", &c); x = x;
        aim_printf (
                &uc->pvs,"%c\n", c);
        if (reset_tof) c = 'N'; /* Force abort when perform tof reset. */
        if ((c != 'Y') && (c != 'y')) {
            aim_printf (
                    &uc->pvs,"Abort\n");
            return 0;
        }
    }

    sleep (1);

    /* Read current. */
    if (bf_pltfm_cpld_read_byte (cpld_index, aoff,
                                 &val_rd)) {
        return -1;
    } else {
        val = val_rd;

        /* Prepare reset assert control value. */
        if (do_reset_by_val_1) {
            val |= (1 << boff);
        } else {
            val &= ~(1 << boff);
        }

        if (dbg) {
            aim_printf (
                &uc->pvs,
                "<val_rd=%x> <val_wr=%x>\n",
                val_rd,
                val);
        }
        /* Reset assert. */
        if (!bf_pltfm_cpld_write_byte (cpld_index, aoff,
                                      val)) {
            if (!bf_pltfm_cpld_read_byte (cpld_index, aoff,
                                                     &val_rd)) {
                /* Check ? */
                if (val == val_rd) {
                    aim_printf (&uc->pvs,
                                "Resetting ... \n");

                    /* Prepare reset de-assert control value. */
                    if (do_reset_by_val_1) {
                        val &= ~(1 << boff);
                    } else {
                        val |= (1 << boff);
                    }

                    if (dbg) {
                        aim_printf (
                            &uc->pvs,
                            "<val_rd=%x> <val_wr=%x>\n",
                            val_rd,
                            val);
                    }
                    sleep (3);
                    /* Reset de-assert. */
                    if (!bf_pltfm_cpld_write_byte (cpld_index, aoff,
                                                  val)) {
                        if (!bf_pltfm_cpld_read_byte (cpld_index, aoff,
                                                      &val_rd)) {
                            if (val == val_rd) {
                                aim_printf (&uc->pvs,
                                            "Reset successfully\n");
                                return 0;
                            }
                        }

                    }
                } else {
                    aim_printf (&uc->pvs,
                                "Reset failed.\n");
                }
            }
        }
    }

    return 0;
}

static ucli_command_handler_f
bf_pltfm_mgr_ucli_ucli_handlers__[] = {
    pltfm_mgr_ucli_ucli__mntr__,
    pltfm_mgr_ucli_ucli__set_interval__,
    pltfm_mgr_ucli_ucli__get_interval__,
    NULL
};

static ucli_status_t bf_pltfm_ucli_ucli__spi_wr_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    char fname[64];
    int offset;
    int size;

    UCLI_COMMAND_INFO (
        uc, "spi-wr", 4,
        "spi-wr <dev> <file name> <offset> <size>");

    dev = atoi (uc->pargs->args[0]);
    strncpy (fname, uc->pargs->args[1],
             sizeof (fname) - 1);
    fname[sizeof (fname) - 1] = '\0';
    offset = atoi (uc->pargs->args[2]);
    size = atoi (uc->pargs->args[3]);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_spi: spi-wr <dev=%d> <file name=%s> <offset=%d> <size=%d>\n",
        dev,
        fname,
        offset,
        size);

    if (bf_pltfm_spi_wr (dev, fname, offset, size)) {
        aim_printf (&uc->pvs,
                    "error writing to SPI eeprom\n");
    } else {
        aim_printf (&uc->pvs, "SPI eeprom write OK\n");
    }
    return 0;
}

static ucli_status_t bf_pltfm_ucli_ucli__spi_rd_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    char fname[64];
    int offset;
    int size;

    UCLI_COMMAND_INFO (
        uc, "spi-rd", 4,
        "spi-rd <dev> <file name> <offset> <size>");

    dev = atoi (uc->pargs->args[0]);
    strncpy (fname, uc->pargs->args[1],
             sizeof (fname) - 1);
    fname[sizeof (fname) - 1] = '\0';
    offset = atoi (uc->pargs->args[2]);
    size = atoi (uc->pargs->args[3]);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_spi: spi-rd <dev=%d> <file name=%s> <offset=%d> <size=%d>\n",
        dev,
        fname,
        offset,
        size);

    if (bf_pltfm_spi_rd (dev, fname, offset, size)) {
        aim_printf (&uc->pvs,
                    "error reading from SPI eeprom\n");
    } else {
        aim_printf (&uc->pvs, "SPI eeprom read OK\n");
    }
    return 0;
}

/* Update PCIe firmware. by tsihang, 2022-11-07. */
static ucli_status_t bf_pltfm_ucli_ucli__spi_update_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    char fname0[512] = {0}, fname1[1024] = {0};
    int offset;
    int size;
    FILE *fd;

    UCLI_COMMAND_INFO (
        uc, "spi-update", 2,
        "spi-update <dev> <file name>");

    dev = atoi (uc->pargs->args[0]);
    strncpy (fname0, uc->pargs->args[1],
             sizeof (fname0) - 1);
    fname0[sizeof (fname0) - 1] = '\0';
    offset = 0;

    fd = fopen (fname0, "rb");
    if (!fd) {
        /* no such file, then return fail. */
        aim_printf (
            &uc->pvs,
            "Target PCIe firmware is invalid : %s\n", fname0);
        return -1;
    }

    fseek (fd, 0, SEEK_END);
    size = (size_t)ftell (fd);
    fseek (fd, 0, SEEK_SET);
    fclose (fd);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_spi: spi-update <dev=%d> <file name=%s> <offset=%d> <size=%d>\n",
        dev,
        fname0,
        offset,
        size);

    aim_printf (
        &uc->pvs,
        "\nDO NOT interrupt during the upgrade. Wating for 30+ secs ...\n\n");

    if (bf_pltfm_spi_wr (dev, fname0, offset, size)) {
        aim_printf (&uc->pvs,
                    "error writing to SPI eeprom\n");
    } else {
        aim_printf (&uc->pvs, "SPI eeprom write OK\n");
        sprintf (fname1, "%s.bak", fname0);
        if (bf_pltfm_spi_rd (dev, fname1, offset, size)) {
            aim_printf (&uc->pvs,
                        "error reading from SPI eeprom\n");
        } else {
            aim_printf (&uc->pvs, "SPI eeprom read OK\n");
            aim_printf (&uc->pvs, "Compare ... \n");
            if (bf_pltfm_spi_cmp (fname0, fname1)) {
                aim_printf (&uc->pvs,
                            "error updating to SPI eeprom\n");
            } else {
                aim_printf (&uc->pvs, "SPI eeprom update OK\n");
            }
        }
        /* Remove firmware file comes from reading SPI EEPROM back. */
        if ((0 == access (fname1, F_OK))) {
            remove (fname1);
        }
    }
    return 0;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_spi_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__spi_wr_, bf_pltfm_ucli_ucli__spi_rd_, bf_pltfm_ucli_ucli__spi_update_, NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_spi_ucli_module__
= {
    "spi_ucli", NULL, bf_pltfm_spi_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_spi_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_spi_ucli_module__);
    n = ucli_node_create ("spi", m,
                          &bf_pltfm_spi_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("spi"));
    return n;
}

static ucli_module_t
afn_pltfm_mgrs_ucli_module__ = {
    "pltfm_mgr_ucli", NULL, bf_pltfm_mgr_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *pltfm_mgrs_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (
        &afn_pltfm_mgrs_ucli_module__);
    n = ucli_node_create ("pltfm_mgr", m,
                          &afn_pltfm_mgrs_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("pltfm_mgr"));
    return n;
}

static ucli_command_handler_f
bf_pltfm_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__bsp__,
    bf_pltfm_ucli_ucli__console__,
    bf_pltfm_ucli_ucli__bmc_ota__,
    bf_pltfm_ucli_ucli__reset_hwcomp__,
    NULL
};

static ucli_module_t bf_pltfm_ucli_module__ = {
    "bf_pltfm_ucli", NULL, bf_pltfm_ucli_ucli_handlers__, NULL, NULL
};

ucli_node_t *bf_pltfm_ucli_node_create (void)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_ucli_module__);
    n = ucli_node_create ("bf_pltfm", NULL,
                          &bf_pltfm_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("bf_pltfm"));
    bf_bd_cfg_ucli_node_create (n);
    bf_pltfm_chss_mgmt_ucli_node_create (n);
    bf_pltfm_cp2112_ucli_node_create (n);
    bf_pltfm_led_ucli_node_create (n);
    bf_qsfp_ucli_node_create (n);
    bf_sfp_ucli_node_create (n);
    //bf_pltfm_rptr_ucli_node_create(n);
    //bf_pltfm_rtmr_ucli_node_create(n);
    //bf_pltfm_si5342_ucli_node_create(n);
    //bf_pltfm_slave_i2c_ucli_node_create(n);
    bf_pltfm_spi_ucli_node_create(n);
    pltfm_mgrs_ucli_node_create (n);
    bf_pltfm_cpld_ucli_node_create (n);

    return n;
}

/* platform-mgr thread exit callback API */
void bf_pltfm_platform_exit (void *arg)
{
    (void)arg;

    if (!platform_is_hw ()) {
        return;
    }

    /* Avoid segment fault.
     * by tsihang, 2021-07-08. */
    pltfm_mgr_stop_health_mntr();

    /* Fixed a bug while push pipeline by p4runtime.
     * by tsihang, 2022-04-25. */
    if (bf_pltfm_led_de_init()) {
        LOG_ERROR ("pltfm_mgr: Error while de-initializing pltfm mgr LED.");
    }

    /* Make it very clear for those platforms which need cp2112 de-init by doing below operations.
     * Because we cann't and shouldn't assume platforms which are NOT X312P-T could de-init cp2112.
     * by tsihang, 2022-04-27. */
    if (platform_type_equal (AFN_X564PT) ||
        platform_type_equal (AFN_X532PT) ||
        platform_type_equal (AFN_X308PT)) {
        if (bf_pltfm_cp2112_de_init()) {
            LOG_ERROR ("pltfm_mgr: Error while de-initializing pltfm mgr CP2112.");
        }
    }
    if (bf_pltfm_master_i2c_de_init()) {
        LOG_ERROR ("pltfm_mgr: Error while de-initializing pltfm mgr Master I2C.");
    }
    if (bf_pltfm_uart_de_init()) {
        LOG_ERROR ("pltfm_mgr: Error while de-initializing pltfm mgr UART.");
    }

    if (bf_pltfm_ucli_node) {
        /* To avoid two 'bf_pltfm' nodes in bfshell.
         * by tsihang, 2022-04-21. */
        bf_drv_shell_unregister_ucli (bf_pltfm_ucli_node);
        //ucli_node_destroy (bf_pltfm_ucli_node);
    }

    platform_bd_deinit();

    bf_sys_rmutex_del (&mav_i2c_lock);

    // Make sure extended_log_hdl is closed at last anyway.
    if (bf_pltfm_mgr_ctx()->extended_log_hdl) {
        fclose (bf_pltfm_mgr_ctx()->extended_log_hdl);
        bf_pltfm_mgr_ctx()->extended_log_hdl = NULL;
    }
}

static void hostinfo()
{
    struct utsname uts;
    char *un = "Unknown";
    if ((uname (&uts) < 0) ||
        (getpwuid (getuid()) == NULL)) {
        fprintf (stdout,
                 "%s[%d], "
                 "uname/getpwuid(%s)"
                 "\n",
                 __FILE__, __LINE__, oryx_safe_strerror (errno));
    } else {
        un = getpwuid (getuid())->pw_name;
    }

    fprintf (stdout, "\r\nBSP ver  : %s\n",
             VERSION_NUMBER);
    fprintf (stdout, "\r\nSystem Preview\n");
    fprintf (stdout, "%30s:%60s\n", "Login User",
             getlogin());
    fprintf (stdout, "%30s:%60s\n", "Runtime User",
             un);
    fprintf (stdout, "%30s:%60s\n", "Host",
             uts.nodename);
    fprintf (stdout, "%30s:%60s\n", "Arch",
             uts.machine);
    fprintf (stdout, "%30s:%60d\n", "Bits/LONG",
             __BITS_PER_LONG);
    fprintf (stdout, "%30s:%60s\n", "Platform",
             uts.sysname);
    fprintf (stdout, "%30s:%60s\n", "Kernel",
             uts.release);
    fprintf (stdout, "%30s:%60s\n", "OS",
             uts.version);
    fprintf (stdout, "%30s:%60d\n", "CPU",
             (int)sysconf (_SC_NPROCESSORS_ONLN));
}

/* Initialize and start pltfm_mgrs server */
bf_status_t bf_pltfm_platform_init (
    void *arg)
{
    int err = BF_PLTFM_SUCCESS;
    /* To avoid dependency error, by tsihang, 2021-07-13. */
    bf_switchd_context_t *switchd_ctx =
        (bf_switchd_context_t *)arg;

    g_switchd_init_mode = switchd_ctx->init_mode;
    time(&g_tm_start);

    mkdir (LOG_DIR_PREFIX,
           S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    if (bf_sys_rmutex_init (&mav_i2c_lock) != 0) {
        LOG_ERROR ("pltfm_mgr: i2c lock init failed\n");
        return -1;
    }

    hostinfo();
    AF_LOG_EXT ("Started");

    /* Initialize the Chassis Management library */
    err = chss_mgmt_init();
    if (err) {
        LOG_ERROR ("pltfm_mgr: chassis mgmt library init failed\n");
        return -1;
    }

    err = platform_bd_init();
    if (err) {
        LOG_ERROR ("pltfm_mgr: chassis mgmt library init failed\n");
        return -1;
    }

    /* Make it very clear for those platforms which need cp2112 init by doing below operations.
     * Because we cann't and shouldn't assume platforms which are NOT X312P-T could init cp2112.
     * by tsihang, 2022-04-27. */
    if (platform_type_equal (AFN_X564PT) ||
        platform_type_equal (AFN_X532PT) ||
        platform_type_equal (AFN_X308PT) ||
        platform_type_equal (AFN_X732QT)) {
        /* Initialize the CP2112 devices for the platform */
        err = cp2112_init();
        if (err) {
            LOG_ERROR ("pltfm_mgr: Error in cp2112 init \n");
            return BF_PLTFM_COMM_FAILED;
        }
    }

    err = bf_pltfm_syscpld_init();
    if (err) {
        LOG_ERROR ("pltfm_mgr: Error in syscpld init \n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Register qsfp module */
    err = bf_pltfm_qsfp_init (NULL);
    if (err) {
        LOG_ERROR ("pltfm_mgr: Error in qsfp init \n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* Register sfp module */
    err = bf_pltfm_sfp_init (NULL);
    if (err) {
        LOG_ERROR ("pltfm_mgr: Error in sfp init \n");
        return BF_PLTFM_COMM_FAILED;
    }

#if 0
    /* Initialize repeater library */
    if (bd == BF_PLTFM_BD_ID_MAVERICKS_P0C) {
        bool is_in_ha =
            switchd_ctx->init_mode ==
            BF_DEV_WARM_INIT_FAST_RECFG
            ? true
            : (switchd_ctx->init_mode ==
               BF_DEV_WARM_INIT_HITLESS ? true
               : false);
        if (bf_pltfm_rtmr_pre_init (
                switchd_ctx->install_dir, is_in_ha)) {
            LOG_ERROR ("Error while retimer library init \n");
            err |= BF_PLTFM_COMM_FAILED;
        }
    }
#endif

    /* Open platfom resource file, if non-exist, create it.
     * For onlp, should run BSP at least once. */
    const char *path = LOG_DIR_PREFIX"/platform";
    char data[1024], ver[128] = {0};
    int length;
    FILE *fp = NULL;

    if (unlikely (!access (path, F_OK))) {
        if (likely (remove (path))) {
            fprintf (stdout,
                     "%s[%d], "
                     "access(%s)"
                     "\n",
                     __FILE__, __LINE__, oryx_safe_strerror (errno));
        }
    }

    fp = fopen (path, "w+");
    if (fp) {
        length = sprintf (data, "%s: %d\n",
                          "PSU", bf_pltfm_mgr_ctx()->psu_count);
        fwrite (data, 1, length, fp);
        length = sprintf (data, "%s: %d\n",
                          "FAN", bf_pltfm_mgr_ctx()->fan_group_count *
                                 bf_pltfm_mgr_ctx()->fan_per_group);
        fwrite (data, 1, length, fp);
        length = sprintf (data, "%s: %d\n",
                          "THERMAL", bf_pltfm_mgr_ctx()->sensor_count);
        fwrite (data, 1, length, fp);
        length = sprintf (data, "%s: %d\n",
                          "CPLD", bf_pltfm_mgr_ctx()->cpld_count);
        fwrite (data, 1, length, fp);
        foreach_element (0, bf_pltfm_mgr_ctx()->cpld_count) {
            bf_pltfm_get_cpld_ver((uint8_t)(each_element + 1), &ver[0], false);
            length = sprintf (data, "CPLD%d: %s\n",
                              (each_element + 1), ver);
            fwrite (data, 1, length, fp);
        }
        bf_pltfm_get_bmc_ver (&ver[0], false);
        length = sprintf (data, "BMC: %s\n",
                          ver);
        fwrite (data, 1, length, fp);

        fflush (fp);
        fclose (fp);
    }

    /* Start Health Monitor */
    pltfm_mgr_start_health_mntr();

    bf_pltfm_ucli_node = bf_pltfm_ucli_node_create();
    bf_drv_shell_register_ucli (bf_pltfm_ucli_node);

    /* Launch RPC server (thrift-plugins). */
#ifdef THRIFT_ENABLED
    bf_platform_rpc_server_start (
        &bf_pltfm_rpc_server_cookie);
#endif

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_platform_port_init (
    bf_dev_id_t dev_id,
    bool warm_init)
{
    (void)warm_init;

    /* Init led module */
    if (bf_pltfm_led_init (dev_id)) {
        LOG_ERROR ("Error in led init dev_id %d\n",
                       dev_id);
        return BF_PLTFM_COMM_FAILED;
    }
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_platform_ext_phy_config_set (
    bf_dev_id_t dev_id, uint32_t conn,
    bf_pltfm_qsfp_type_t qsfp_type)
{
    return BF_PLTFM_SUCCESS;
}

/* acquire lock to perform an endpoint i2c operation on a cp2112 topology */
void bf_pltfm_i2c_lock()
{
    bf_sys_rmutex_lock (&mav_i2c_lock);
}

/* release lock after performing an endpoint i2c operation on a cp2112 topology */
void bf_pltfm_i2c_unlock()
{
    bf_sys_rmutex_unlock (&mav_i2c_lock);
}
