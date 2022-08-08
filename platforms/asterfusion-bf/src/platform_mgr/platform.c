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
#define __USE_GNU /* See feature_test_macros(7) */
#include <pthread.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <pwd.h>

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

/* global */
static pltfm_mgr_info_t pltfm_mgr_info = {
    .np_name = "pltfm_mgr",
    .health_mntr_t_id = 0,
    .np_onlp_mntr_name = "pltfm_mgr_onlp",
    .onlp_mntr_t_id = 0,
    .flags = 0,

    .pltfm_type = UNKNOWM_PLATFORM,

    .psu_count = 0,
    .sensor_count = 0,
    .fan_group_count = 0,
    .fan_per_group = 0,
};

static ucli_node_t *bf_pltfm_ucli_node;
static bf_sys_rmutex_t
mav_i2c_lock;    /* i2c(cp2112) lock for QSFP IO. by tsihang, 2021-07-13. */
extern bf_sys_rmutex_t update_lock;

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

pltfm_mgr_info_t *bf_pltfm_mgr_ctx()
{
    return &pltfm_mgr_info;
}

static bf_pltfm_status_t chss_mgmt_init()
{
    bf_pltfm_status_t sts;
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
            bf_pltfm_mgr_ctx()->flags = (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 2;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 6;
        } else if (platform_type_equal (X532P)) {
            bf_pltfm_mgr_ctx()->flags = (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 5;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 6;
        } else if (platform_type_equal (X308P)) {
            bf_pltfm_mgr_ctx()->flags = (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 6;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 10;
        } else if (platform_type_equal (X312P)) {
            bf_pltfm_mgr_ctx()->flags = (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 5;
            bf_pltfm_mgr_ctx()->fan_per_group = 2;
            bf_pltfm_mgr_ctx()->psu_count = 2;
            bf_pltfm_mgr_ctx()->sensor_count = 2;
            if (platform_subtype_equal(v1dot3)) {
                bf_pltfm_mgr_ctx()->sensor_count = 3;
            }
        } else if (platform_type_equal (HC)) {
            bf_pltfm_mgr_ctx()->flags = (
                                            AF_PLAT_MNTR_CTRL | AF_PLAT_MNTR_POWER  |
                                            AF_PLAT_MNTR_FAN |
                                            AF_PLAT_MNTR_TMP  | AF_PLAT_MNTR_MODULE
                                        );
            bf_pltfm_mgr_ctx()->fan_group_count = 0;
            bf_pltfm_mgr_ctx()->fan_per_group = 0;
            bf_pltfm_mgr_ctx()->psu_count = 0;
            bf_pltfm_mgr_ctx()->sensor_count = 0;

        }
        bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date = time (
                    NULL);
    }

    fprintf (stdout, "BSP ver  : %s\n",
             VERSION_NUMBER);
    fprintf (stdout, "Max FANs : %2d\n",
             bf_pltfm_mgr_ctx()->fan_group_count *
             bf_pltfm_mgr_ctx()->fan_per_group);
    fprintf (stdout, "Max PSUs : %2d\n",
             bf_pltfm_mgr_ctx()->psu_count);
    fprintf (stdout, "Max SNRs : %2d\n",
             bf_pltfm_mgr_ctx()->sensor_count);

    if (g_switchd_init_mode != BF_DEV_INIT_COLD) {
        return BF_PLTFM_SUCCESS;
    }

    bf_pltfm_chss_mgmt_fan_init();
    bf_pltfm_chss_mgmt_pwr_init();
    bf_pltfm_chss_mgmt_tmp_init();
    bf_pltfm_chss_mgmt_pwr_rails_init();

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

static ucli_status_t
bf_pltfm_ucli_ucli__bsp__ (ucli_context_t
                           *uc)
{
    char bd[128];

    UCLI_COMMAND_INFO (
        uc, "bsp", 0, " Show BSP version.");

    platform_name_get_str (bd, sizeof (bd));

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
                bd);
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

    return 0;
}

static ucli_command_handler_f
bf_pltfm_mgr_ucli_ucli_handlers__[] = {
    pltfm_mgr_ucli_ucli__mntr__, NULL
};

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
    bf_pltfm_ucli_ucli__bsp__, NULL
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
    //bf_pltfm_spi_ucli_node_create(n);
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

