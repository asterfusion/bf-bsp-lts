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
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_rptr.h>
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
#include <bf_pltfm_slave_i2c.h>
#include <bf_pltfm_si5342.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_qsfp.h>
#include <bf_pltfm_sfp.h>
#include <bf_pltfm_rtmr.h>
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

static bf_dev_init_mode_t g_switchd_init_mode = BF_DEV_INIT_COLD;

extern ucli_node_t
*bf_pltfm_cpld_ucli_node_create (ucli_node_t *m);
extern ucli_node_t
*bf_pltfm_spi_ucli_node_create (ucli_node_t *m);

static ucli_node_t *bf_pltfm_ucli_node;
static bf_sys_rmutex_t
mav_i2c_lock;    /* i2c(cp2112) lock for QSFP IO. by tsihang, 2021-07-13. */
extern bf_sys_rmutex_t update_lock;

extern bf_pltfm_status_t bf_pltfm_get_bmc_ver(char *bmc_ver);

bool platform_is_hw (void)
{
    bf_pltfm_status_t sts;
    bf_pltfm_board_id_t bd;
    sts = bf_pltfm_chss_mgmt_bd_type_get (&bd);
    if (sts == BF_PLTFM_SUCCESS &&
        bd != BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU) {
        return true;
    } else {
        return false;
    }
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

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);
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
    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);
}

static bf_pltfm_status_t chss_mgmt_init()
{
    bf_pltfm_status_t sts;
    char fmt[128];
    sts = bf_pltfm_chss_mgmt_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("pltfm_mgr: Chassis Mgmt library initialization failed\n");
        return sts;
    }

    bf_pltfm_chss_mgmt_platform_type_get (
        &bf_pltfm_mgr_ctx()->pltfm_type,
        &bf_pltfm_mgr_ctx()->pltfm_subtype);

    /* Check strictly. */
    if (!platform_type_equal (X532P) &&
        !platform_type_equal (X564P) &&
        !platform_type_equal (X308P) &&
        !platform_type_equal (X312P) &&
        !platform_type_equal (HC)    &&
        1 /* More platform here */) {
        LOG_ERROR ("pltfm_mgr: Unsupported platform, verify EEPROM please.\n");
        exit (0);
    } else {
        if (platform_type_equal (X564P)) {
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
            if (platform_subtype_equal(v2dot0)) {
                bf_pltfm_mgr_ctx()->sensor_count = 8;
            }
        } else if (platform_type_equal (X532P)) {
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
        } else if (platform_type_equal (X308P)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 6;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 10;
            bf_pltfm_mgr_ctx()->cpld_count = 2;
        } else if (platform_type_equal (X312P)) {
            bf_pltfm_mgr_ctx()->flags |= (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 5;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            /* LM75, LM63 and GHC(4) and Tofino(2).
             * This makes an offset to help bf_pltfm_onlp_mntr_tofino_temperature. */
            bf_pltfm_mgr_ctx()->sensor_count = 9;
            if (platform_subtype_equal(v3dot0) ||
                platform_subtype_equal(v4dot0) ||
                platform_subtype_equal(v5dot0)) {
            /* LM75, LM63, LM86 and GHC(4) and Tofino(2).
             * This makes an offset to help bf_pltfm_onlp_mntr_tofino_temperature. */
                bf_pltfm_mgr_ctx()->sensor_count = 9;
            }
            bf_pltfm_mgr_ctx()->cpld_count = 5;
        } else if (platform_type_equal (HC)) {
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
        }
        bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date = time (
                    NULL);
    }

    fprintf (stdout, "Platform : %02x(%02x)\n",
             bf_pltfm_mgr_ctx()->pltfm_type, bf_pltfm_mgr_ctx()->pltfm_subtype);
    fprintf (stdout, "Max FANs : %2d\n",
             bf_pltfm_mgr_ctx()->fan_group_count *
             bf_pltfm_mgr_ctx()->fan_per_group);
    fprintf (stdout, "Max PSUs : %2d\n",
             bf_pltfm_mgr_ctx()->psu_count);
    fprintf (stdout, "Max SNRs : %2d\n",
             bf_pltfm_mgr_ctx()->sensor_count);

    /* To accelerate 2nd launching for stratum. */
    if (g_switchd_init_mode != BF_DEV_INIT_COLD) {
        return BF_PLTFM_SUCCESS;
    }

    bf_pltfm_get_bmc_ver (&fmt[0]);
    fprintf (stdout, "\nBMC version : %s\n", fmt);

    //bf_pltfm_chss_mgmt_fan_init();
    //bf_pltfm_chss_mgmt_pwr_init();
    //bf_pltfm_chss_mgmt_tmp_init();
    //bf_pltfm_chss_mgmt_pwr_rails_init();

    /* Open platfom resource file, if non-exist, create it.
     * For onlp, should run BSP at least once. */
    const char *path = LOG_DIR_PREFIX"/platform";
    char data[1024];
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

        fflush (fp);
        fclose (fp);
    }

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

    if (ctrl) {
        bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_CTRL;
    } else {
        bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_CTRL;
    }
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
        sprintf (str, "C%02d%c", module, bf_qsfp_is_present(module) ? '*' : ' ');
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

    bf_pltfm_pwr_supply_info_t info1, info2;
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

static void bf_pltfm_ucli_dump_panel(ucli_context_t
                           *uc) {
    if (platform_type_equal (X564P)) {
        bf_pltfm_ucli_dump_x564p_panel(uc);
    }else if (platform_type_equal (X532P)) {
        bf_pltfm_ucli_dump_x532p_panel(uc);
    } else if (platform_type_equal (X308P)) {
        bf_pltfm_ucli_dump_x308p_panel(uc);
    } else if (platform_type_equal (X312P)) {
        bf_pltfm_ucli_dump_x312p_panel(uc);
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

    aim_printf (&uc->pvs, "Ver    %s\n",
                VERSION_NUMBER);

    aim_printf (&uc->pvs, "Platform  : %s\n",
                platform_type_equal (X532P) ? "X532P-T"  :
                platform_type_equal (X564P) ? "X564P-T"  :
                platform_type_equal (X308P) ? "X308P-T"  :
                platform_type_equal (X312P) ? "X312P-T"  :
                platform_type_equal (HC)    ? "HC36Y24C" :
                "Unknown");
    aim_printf (&uc->pvs, "BD ID     : %s\n",
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
        char ver[8] = {0};
        bf_pltfm_get_cpld_ver((uint8_t)(each_element + 1), &ver[0]);
        aim_printf (&uc->pvs, "    CPLD%d : %s\n",
                    (each_element + 1), ver);
    }

    bf_pltfm_get_bmc_ver (&fmt[0]);
    aim_printf (&uc->pvs, "    BMC   : %s\n",
               fmt);

    uint32_t days, hours, mins, secs;
    time(&g_tm_end);
    diff = difftime(g_tm_end, g_tm_start);
    bf_pltfm_convert_uptime (diff, &days, &hours, &mins, &secs);
    aim_printf (&uc->pvs, "    Uptime: ");
    if (days) aim_printf (&uc->pvs, "%3u days, ", days);
    if (hours || mins) aim_printf (&uc->pvs, "%2u:%2u, ", hours, mins);
    aim_printf (&uc->pvs, "%2u sec(s)\n", secs);

    FILE *fp = fopen ("/proc/uptime", "r");
    if (fp) {
        if (fgets (fmt, 128 - 1, fp)) {
            if (sscanf (fmt, "%lf ", &diff)) {
                bf_pltfm_convert_uptime (diff, &days, &hours, &mins, &secs);
                aim_printf (&uc->pvs, "Sys Uptime: ");
                if (days) aim_printf (&uc->pvs, "%3u days, ", days);
                if (hours || mins) aim_printf (&uc->pvs, "%2u:%2u, ", hours, mins);
                aim_printf (&uc->pvs, "%2u sec(s)\n", secs);
            }
        }
        fclose (fp);
    }
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
    uint8_t buf[2] = {0x00, 0xaa};
    uint8_t data[I2C_SMBUS_BLOCK_MAX] = {0};

    /* Keep the name consistent with the document X-T Programmable Bare Metal. */
    UCLI_COMMAND_INFO (uc, "console", 1, "Redirect console to <bmc/come/dpu1/dpu2>");

    if (platform_type_equal (X312P)) {
        if (memcmp (uc->pargs->args[0], "bmc", 3) == 0){
            aim_printf (&uc->pvs, "Console redirects to BMC\n");
            LOG_WARNING ("Console redirects to BMC\n");
            buf[0] = 0x02;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, data, 10000);
        } else if (memcmp (uc->pargs->args[0], "come", 4) == 0){
            aim_printf (&uc->pvs, "Console redirects to COM-E\n");
            LOG_WARNING ("Console redirects to COM-E\n");
            buf[0] = 0x03;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, data, 10000);
        } else if (memcmp (uc->pargs->args[0], "dpu1", 4) == 0){
            aim_printf (&uc->pvs, "Console redirects to DPU-1\n");
            LOG_WARNING ("Console redirects to DPU-1\n");
            buf[0] = 0x00;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, data, 10000);
        } else if (memcmp (uc->pargs->args[0], "dpu2", 4) == 0){
            aim_printf (&uc->pvs, "Console redirects to DPU-2\n");
            LOG_WARNING ("Console redirects to DPU-2\n");
            buf[0] = 0x01;
            bf_pltfm_bmc_write_read(0x3e, 0x7c, buf, 2, 0xff, data, 10000);
        } else {
            aim_printf (&uc->pvs, "Usage: console <bmc/come/dpu1/dpu2>\n");
        }
    } else {
        aim_printf (&uc->pvs, "Not supported on this platform yet!\n");
    }

    return 0;
}

static ucli_command_handler_f
bf_pltfm_mgr_ucli_ucli_handlers__[] = {
    pltfm_mgr_ucli_ucli__mntr__, NULL
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
mavericks_pltfm_mgrs_ucli_module__ = {
    "pltfm_mgr_ucli", NULL, bf_pltfm_mgr_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *pltfm_mgrs_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (
        &mavericks_pltfm_mgrs_ucli_module__);
    n = ucli_node_create ("pltfm_mgr", m,
                          &mavericks_pltfm_mgrs_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("pltfm_mgr"));
    return n;
}

static ucli_command_handler_f
bf_pltfm_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__bsp__,
    bf_pltfm_ucli_ucli__console__,
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
    if (platform_type_equal (X564P) ||
        platform_type_equal (X532P) ||
        platform_type_equal (X308P)) {
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

    bf_sys_rmutex_del (&mav_i2c_lock);
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

    /* Initialize the Chassis Management library */
    err = chss_mgmt_init();
    if (err) {
        LOG_ERROR ("pltfm_mgr: chassis mgmt library init failed\n");
        return -1;
    }

    /* Make it very clear for those platforms which need cp2112 init by doing below operations.
     * Because we cann't and shouldn't assume platforms which are NOT X312P-T could init cp2112.
     * by tsihang, 2022-04-27. */
    if (platform_type_equal (X564P) ||
        platform_type_equal (X532P) ||
        platform_type_equal (X308P)) {
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
    bf_pltfm_board_id_t bd_id;
    bf_pltfm_chss_mgmt_bd_type_get (&bd_id);

    if (bd_id != BF_PLTFM_BD_ID_MAVERICKS_P0B_EMU) {
        /* Init led module */
        if (bf_pltfm_led_init (dev_id)) {
            LOG_ERROR ("Error in led init dev_id %d\n",
                       dev_id);
            return BF_PLTFM_COMM_FAILED;
        }
    }
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_platform_ext_phy_config_set (
    bf_dev_id_t dev_id, uint32_t conn,
    bf_pltfm_qsfp_type_t qsfp_type)
{
    bf_status_t sts;

    bf_pltfm_board_id_t bd_id;
    bf_pltfm_chss_mgmt_bd_type_get (&bd_id);
    if ((bd_id == BF_PLTFM_BD_ID_MAVERICKS_P0B) ||
        (bd_id == BF_PLTFM_BD_ID_MAVERICKS_P0A)) {
        sts = bf_pltfm_rptr_config_set (dev_id, conn,
                                        qsfp_type);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error: in setting rptr config for port %d at %s:%d",
                       conn,
                       __func__,
                       __LINE__);
        }
    } else if (bd_id ==
               BF_PLTFM_BD_ID_MAVERICKS_P0C) {
        /* nothing to do for retimers */
        sts = BF_PLTFM_SUCCESS;
    } else {
        sts = BF_PLTFM_OTHER;
    }
    return sts;
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

