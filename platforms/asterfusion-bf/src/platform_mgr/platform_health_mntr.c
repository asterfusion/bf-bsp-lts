/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdio.h>
#include <unistd.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_types/bf_types.h>
#include <bfsys/bf_sal/bf_sys_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_mgr/pltfm_mgr_handlers.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>

#include <pltfm_types.h>

/* Local header includes */
#include "platform_priv.h"

static bf_pltfm_temperature_info_t peak;

extern bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get__ (
    bf_pltfm_fan_data_t *fdata);
extern bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__ (
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info);
extern bf_pltfm_status_t
__bf_pltfm_chss_mgmt_switch_temperature_get__ (
    bf_dev_id_t dev_id, int sensor,
    bf_pltfm_switch_temperature_info_t *tmp);
extern bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get__ (
    bf_pltfm_temperature_info_t *tmp);
extern bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails);

#define HAVE_ONLP
#if defined(HAVE_ONLP)
#define MAX_LEN 256
#define ONLP_LOG_SFP_PRES_PATH           LOG_DIR_PREFIX"/sfp_%s"
#define ONLP_LOG_QSFP_PRES_PATH          LOG_DIR_PREFIX"/qsfp_%s"
#define ONLP_LOG_SFP_PATH    LOG_DIR_PREFIX"/sfp_%d_%s"
#define ONLP_LOG_QSFP_PATH   LOG_DIR_PREFIX"/qsfp_%d_%s"
#define ONLP_LOG_CHASSIS_FAN_PATH        LOG_DIR_PREFIX"/fan_%d_%s"
#define ONLP_LOG_CHASSIS_PSU_PATH        LOG_DIR_PREFIX"/psu_%d_%s"
#define ONLP_LOG_CHASSIS_TMP_PATH        LOG_DIR_PREFIX"/thermal_%d_temp"
#define ONLP_LOG_CHASSIS_VRAIL_PATH      LOG_DIR_PREFIX"/pwr_rails"

static inline void onlp_save (
    char *path, char *value, size_t valen)
{
    FILE *fp = NULL;
    char path_bak[MAX_LEN + 16] = {0};

    sprintf (path_bak, "%s.bak", path);

    fp = fopen (path_bak, "w+");
    if (fp && (valen == fwrite (value, 1, valen,
                                fp))) {
        fflush (fp);
        fclose (fp);
        /* Rename */
        rename (path_bak, path);
    }
}

static void bf_pltfm_chss_mgmt_onlp_fan (int id,
        bf_pltfm_fan_info_t *fan)
{
    char fonlp[MAX_LEN];
    char value[MAX_LEN];

    sprintf (fonlp, ONLP_LOG_CHASSIS_FAN_PATH, id,
             "presence");
    sprintf (value, "%d", ! (fan->present));
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_FAN_PATH, id,
             "rpm");
    sprintf (value, "%d", fan->front_speed);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_FAN_PATH, id,
             "direction");
    sprintf (value, "%d", fan->direction);
    onlp_save (fonlp, value, strlen (value));
}

static void bf_pltfm_chss_mgmt_onlp_psu (int id,
        bf_pltfm_pwr_supply_info_t *psu)
{
    char fonlp[MAX_LEN];
    char value[MAX_LEN];

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "presence");
    sprintf (value, "%d", ! (psu->presence));
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "vin");
    sprintf (value, "%d", psu->vin);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "vout");
    sprintf (value, "%d", psu->vout);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "iin");
    sprintf (value, "%d", psu->iin);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "iout");
    sprintf (value, "%d", psu->iout);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "pin");
    sprintf (value, "%d", psu->pwr_in);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_PSU_PATH, id,
             "pout");
    sprintf (value, "%d", psu->pwr_out);
    onlp_save (fonlp, value, strlen (value));
}

static void bf_pltfm_chss_mgmt_onlp_temp (int id,
        float temp)
{
    char fonlp[MAX_LEN];
    char value[MAX_LEN];

    sprintf (fonlp, ONLP_LOG_CHASSIS_TMP_PATH, id);
    sprintf (value, "%d", (int) (temp * 1000));
    onlp_save (fonlp, value, strlen (value));
}

static void bf_pltfm_chss_mgmt_onlp_tofino_temp (
    bf_pltfm_switch_temperature_info_t *temp)
{
    char fonlp[MAX_LEN];
    char value[MAX_LEN];

    sprintf (fonlp, ONLP_LOG_CHASSIS_TMP_PATH,
             bf_pltfm_mgr_ctx()->sensor_count + 1);
    sprintf (value, "%d", temp->main_sensor);
    onlp_save (fonlp, value, strlen (value));

    sprintf (fonlp, ONLP_LOG_CHASSIS_TMP_PATH,
             bf_pltfm_mgr_ctx()->sensor_count + 2);
    sprintf (value, "%d", temp->remote_sensor);
    onlp_save (fonlp, value, strlen (value));
}

static void bf_pltfm_chss_mgmt_onlp_pwr_rails (
    uint32_t pwr_rails)
{
    char fonlp[MAX_LEN];
    char value[MAX_LEN];

    sprintf (fonlp, ONLP_LOG_CHASSIS_VRAIL_PATH);
    sprintf (value, "%d", pwr_rails);
    onlp_save (fonlp, value, strlen (value));
}
#endif

static void check_pwr_supply (void)
{
    bf_pltfm_pwr_supply_t pwr;
    bf_pltfm_pwr_supply_info_t info;
    bool presence;

    if (bf_pltfm_mgr_ctx()->psu_count == 0) {
        return;
    }

    /* check presence of power supply 1 */
    pwr = POWER_SUPPLY1;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
        (pwr,
         &presence, &info) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading power supply status : PWR%d\n",
                   pwr);
    }

    if (!presence) {
        LOG_WARNING ("POWER SUPPLY 1 not present \n");
    } else {
#if defined(HAVE_ONLP)
        /* Added ONLP API */
        bf_pltfm_chss_mgmt_onlp_psu ((int)pwr, &info);
#endif
    }

    pwr = POWER_SUPPLY2;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
        (pwr,
         &presence, &info) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading power supply status : PWR%d\n",
                   pwr);
    }

    if (!presence) {
        LOG_WARNING ("POWER SUPPLY 2 not present \n");
    } else {
#if defined(HAVE_ONLP)
        /* Added ONLP API */
        bf_pltfm_chss_mgmt_onlp_psu ((int)pwr, &info);
#endif
    }
    return;
}

static void check_tofino_temperature (void)
{
    bf_dev_id_t dev_id = 0;
    int sensor = 0;
    bf_pltfm_switch_temperature_info_t temp_mC;
    bf_pltfm_status_t r;

    if (bf_pltfm_mgr_ctx()->sensor_count == 0) {
        return;
    }

    r = __bf_pltfm_chss_mgmt_switch_temperature_get__
        (
            dev_id, sensor, &temp_mC);

    if (r != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading switch temperature\n");
    }

    if (temp_mC.main_sensor >=
        TOFINO_TMP_ALARM_RANGE) {
        LOG_ALARM ("TOFINO MAIN TEMP SENOR above threshold value(%d): %d C\n",
                   (TOFINO_TMP_ALARM_RANGE / 1000),
                   (temp_mC.main_sensor / 1000));
        LOG_ALARM ("=========================================\n");
        LOG_ALARM ("        SHUTDOWN THE SYSTEM\n");
        LOG_ALARM ("=========================================\n");
    }

    if (temp_mC.remote_sensor >=
        TOFINO_TMP_ALARM_RANGE) {
        LOG_ALARM ("TOFINO REMOTE TEMP SENOR above threshold value(%d): %d C\n",
                   (TOFINO_TMP_ALARM_RANGE / 1000),
                   (temp_mC.remote_sensor / 1000));
        LOG_ALARM ("=========================================\n");
        LOG_ALARM ("        SHUTDOWN THE SYSTEM\n");
        LOG_ALARM ("=========================================\n");
    }
#if defined(HAVE_ONLP)
    bf_pltfm_chss_mgmt_onlp_tofino_temp (&temp_mC);
#endif
    return;
}

static void check_chassis_temperature (void)
{
    bf_pltfm_temperature_info_t t;

    if (bf_pltfm_mgr_ctx()->sensor_count == 0) {
        return;
    }

    if (__bf_pltfm_chss_mgmt_temperature_get__ (
            &t) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading chassis temperature \n");
        return;
    } else {
#if defined(HAVE_ONLP)
        if (platform_type_equal (X532P)) {
            bf_pltfm_chss_mgmt_onlp_temp (1, (float)t.tmp1);
            bf_pltfm_chss_mgmt_onlp_temp (2, (float)t.tmp2);
            bf_pltfm_chss_mgmt_onlp_temp (3, (float)t.tmp3);
            bf_pltfm_chss_mgmt_onlp_temp (4, (float)t.tmp4);
            bf_pltfm_chss_mgmt_onlp_temp (5, (float)t.tmp5);
            bf_pltfm_chss_mgmt_onlp_temp (6, (float)t.tmp6);
        } else if (platform_type_equal (X564P)) {
            bf_pltfm_chss_mgmt_onlp_temp (1, (float)t.tmp1);
            bf_pltfm_chss_mgmt_onlp_temp (2, (float)t.tmp2);
            bf_pltfm_chss_mgmt_onlp_temp (3, (float)t.tmp3);
            bf_pltfm_chss_mgmt_onlp_temp (4, (float)t.tmp4);
            bf_pltfm_chss_mgmt_onlp_temp (5, (float)t.tmp5);
            bf_pltfm_chss_mgmt_onlp_temp (6, (float)t.tmp6);
        } else if (platform_type_equal (X312P)) {
            bf_pltfm_chss_mgmt_onlp_temp (1, (float)t.tmp1);
            bf_pltfm_chss_mgmt_onlp_temp (2, (float)t.tmp2);
            if (platform_subtype_equal(v1dot3)) {
                bf_pltfm_chss_mgmt_onlp_temp (3, (float)t.tmp3);
            }
        }
#endif
    }

    if (t.tmp1 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP1 in above alarm range :%f\n",
                   t.tmp1);
    }

    if (t.tmp2 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP2 in above alarm range :%f\n",
                   t.tmp2);
    }

    if (t.tmp3 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP3 in above alarm range :%f\n",
                   t.tmp3);
    }

    if (t.tmp4 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP4 in above alarm range :%f\n",
                   t.tmp4);
    }

    if (t.tmp5 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP5 in above alarm range :%f\n",
                   t.tmp5);
    }

    if (t.tmp6 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP6 in above alarm range :%f\n",
                   t.tmp6);
    }

    if (t.tmp7 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP7 in above alarm range :%f\n",
                   t.tmp7);
    }

    if (t.tmp8 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP8 in above alarm range :%f\n",
                   t.tmp8);
    }

    if (t.tmp9 > CHASSIS_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP9 in above alarm range :%f\n",
                   t.tmp9);
    }

    /* TMP10 is assumed to be the ASIC temperature */
    if (t.tmp10 > ASIC_TMP_ALARM_RANGE) {
        LOG_ALARM ("TMP10 in above alarm range :%f\n",
                   t.tmp10);
    }

    if (t.tmp1 > peak.tmp1) {
        peak.tmp1 = t.tmp1;
    }

    if (t.tmp2 > peak.tmp2) {
        peak.tmp2 = t.tmp2;
    }

    if (t.tmp3 > peak.tmp3) {
        peak.tmp3 = t.tmp3;
    }

    if (t.tmp4 > peak.tmp4) {
        peak.tmp4 = t.tmp4;
    }

    if (t.tmp5 > peak.tmp5) {
        peak.tmp5 = t.tmp5;
    }

    if (t.tmp6 > peak.tmp6) {
        peak.tmp6 = t.tmp6;
    }

    if (t.tmp7 > peak.tmp7) {
        peak.tmp7 = t.tmp7;
    }

    if (t.tmp8 > peak.tmp8) {
        peak.tmp8 = t.tmp8;
    }

    if (t.tmp9 > peak.tmp9) {
        peak.tmp9 = t.tmp9;
    }

    if (t.tmp10 > peak.tmp10) {
        peak.tmp10 = t.tmp10;
    }
    return;
}

static void check_fantray (void)
{
    bf_pltfm_fan_data_t fdata;

    if (bf_pltfm_mgr_ctx()->fan_group_count == 0) {
        return;
    }

    if (__bf_pltfm_chss_mgmt_fan_data_get__ (
            &fdata) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading fan data\n");
    } else {
#if defined(HAVE_ONLP)
        int i;
        for (i = 0;
             i < (int) (bf_pltfm_mgr_ctx()->fan_per_group *
                        bf_pltfm_mgr_ctx()->fan_group_count); i ++) {
            bf_pltfm_chss_mgmt_onlp_fan (fdata.F[i].fan_num,
                                         & (fdata.F[i]));
        }
#endif
    }

    if (fdata.fantray_present) {
        LOG_ALARM ("Fan tray not present \n");
    }
    return;
}

static void check_pwr_rails (void)
{
    bf_pltfm_pwr_rails_info_t pwr_rails;

    if (bf_pltfm_mgr_ctx()->psu_count == 0) {
        return;
    }

    if (__bf_pltfm_chss_mgmt_pwr_rails_get__ (
            &pwr_rails) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading RAILs pwr\n");
    } else {
#if defined(HAVE_ONLP)
        // save pwr_rails for onlp
        bf_pltfm_chss_mgmt_onlp_pwr_rails (
            pwr_rails.vrail1);
#endif
    }
    return;
}

extern int bf_pltfm_get_qsfp_ctx (struct
                                  qsfp_ctx_t
                                  **qsfp_ctx);

static void check_module()
{
    int i;
    int module;
    int max_sfp_modules;
    int max_qsfp_modules;
    uint8_t buf[MAX_QSFP_PAGE_SIZE * 2] = {0};

    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    bf_pltfm_get_qsfp_ctx (&qsfp_ctx);

    max_sfp_modules  = bf_sfp_get_max_sfp_ports();
    max_qsfp_modules = bf_qsfp_get_max_qsfp_ports();

#if defined(HAVE_ONLP)
    static uint32_t sfp_pres_mask_h  = 0xFFFFFFFF;
    static uint32_t sfp_pres_mask_l  = 0xFFFFFFFF;
    static uint32_t qsfp_pres_mask_h = 0xFFFFFFFF;
    static uint32_t qsfp_pres_mask_l = 0xFFFFFFFF;
    uint32_t bit_mask;
    uint32_t *p_pres_mask;
    char path[MAX_LEN];
    char value[MAX_LEN];

    for (i = 1; i <= max_qsfp_modules; i ++) {
        qsfp = &qsfp_ctx[i - 1];
        if (strcmp (qsfp->desc, "EMPTY") == 0) {
            continue;
        }
        module = atoi (&qsfp->desc[1]);

        if (module >= 33) {
            bit_mask = 1 << (module - 33);
            p_pres_mask = &qsfp_pres_mask_h;
        } else {
            bit_mask = 1 << (module - 1);
            p_pres_mask = &qsfp_pres_mask_l;
        }

        sprintf (path, ONLP_LOG_QSFP_PATH,
                 module, "eeprom");

        if (!bf_qsfp_is_present (i)) {
            if (*p_pres_mask & bit_mask) {
                remove (path);
            }
            *p_pres_mask &= ~bit_mask;
            continue;
        } else {
            *p_pres_mask |= bit_mask;
        }
        if (bf_qsfp_get_cached_info (i,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                i, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }
#if 0
        if (bf_qsfp_is_cmis (i)) {
            continue;
        }
#endif
        onlp_save (path, (char *)buf,
                   MAX_QSFP_PAGE_SIZE * 2);
    }
    sprintf (path, ONLP_LOG_QSFP_PRES_PATH,
             "presence");
    sprintf (value, "0x%08x%08x", ~qsfp_pres_mask_h,
             ~qsfp_pres_mask_l);
    onlp_save (path, (char *)value, strlen (value));

    for (i = 1; i <= max_sfp_modules; i ++) {
        if (bf_sfp_is_present (i)) {
            if (i >= 33) {
                sfp_pres_mask_h |= 1 << (i - 33);
            } else {
                sfp_pres_mask_l |= 1 << (i - 1);
            }
        } else {
            if (i >= 33) {
                sfp_pres_mask_h &= ~ (1 << (i - 33));
            } else {
                sfp_pres_mask_l &= ~ (1 << (i - 1));
            }
            continue;
        }

        bf_sfp_update_cache (i);
    }
    sprintf (path, ONLP_LOG_SFP_PRES_PATH,
             "presence");
    sprintf (value, "0x%08x%08x", ~sfp_pres_mask_h,
             ~sfp_pres_mask_l);
    onlp_save (path, (char *)value, strlen (value));
#else
    for (i = 1; i <= max_qsfp_modules; i ++) {
        if (!bf_qsfp_is_present (i)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (i,
                                     QSFP_PAGE0_LOWER, buf)) {
            continue;
        }
        if (bf_qsfp_get_cached_info (
                i, QSFP_PAGE0_UPPER,
                buf + MAX_QSFP_PAGE_SIZE)) {
            continue;
        }
#if 0
        if (bf_qsfp_is_cmis (i)) {
            continue;
        }
#endif
    }
    for (i = 1; i <= max_sfp_modules; i ++) {

    }
#endif
    return;
}

bf_pltfm_status_t bf_pltfm_temperature_get (
    bf_pltfm_temperature_info_t *tmp,
    bf_pltfm_temperature_info_t *peak_tmp)
{
    if ((tmp == NULL) || (peak_tmp == tmp)) {
        LOG_ERROR ("Invalid arguments passed \n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (bf_pltfm_chss_mgmt_temperature_get (
            tmp) != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in reading temperature \n");
        return BF_PLTFM_COMM_FAILED;
    }

    peak_tmp->tmp1 = peak.tmp1;
    peak_tmp->tmp2 = peak.tmp2;
    peak_tmp->tmp3 = peak.tmp3;
    peak_tmp->tmp4 = peak.tmp4;
    peak_tmp->tmp5 = peak.tmp5;
    peak_tmp->tmp6 = peak.tmp6;
    peak_tmp->tmp7 = peak.tmp7;
    peak_tmp->tmp8 = peak.tmp8;
    peak_tmp->tmp9 = peak.tmp9;
    peak_tmp->tmp10 = peak.tmp10;

    return BF_PLTFM_SUCCESS;
}

void bf_pltfm_temperature_monitor_enable (
    bool enable)
{
    if (enable) {
        bf_pltfm_mgr_ctx()->flags |= AF_PLAT_MNTR_CTRL;
    } else {
        bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_MNTR_CTRL;
    }
    return;
}

/* Init function of thread */
void *health_mntr_init (void *arg)
{
    (void)arg;
    uint32_t flags = 0;
    int sec = 15;
    static bool first_startup = true;

    printf ("Health monitor started \n");

    /* The more ports (reset circle), the more time to wait until ASIC ready. */
    if (platform_type_equal (X564P)) {
        sec = 30;
    }

    FOREVER {
        flags = bf_pltfm_mgr_ctx()->flags;

        /* Chassis temprature is quite important.
         * Monitor this unconditionally even though Monitor Ctrl is disabled.
         * by tsihang, 2021-07-06. */
        if (likely (flags & AF_PLAT_MNTR_TMP))
        {
            check_chassis_temperature();
            sleep (3);

            // Tofino temperature monitoring still needs to add FSM
            if (1) { // this info available thru check_chassis_temperature()
                /* May occure error if no enough time to wait ASIC ready.
                 * by tsihang, 2021-07-06 */
                if (first_startup) {
                    /* be carefull when you want to change the sleep interval. */
                    sleep (sec);
                    first_startup = false;
                }
                sleep (3);
                check_tofino_temperature();
            }
        }

        if (unlikely (! (flags & AF_PLAT_MNTR_CTRL)))
        {
            fprintf (stdout, "Chassis Monitor is disabled\n");
            fprintf (stdout, "@ %s\n",
                     ctime ((time_t *)
                            &bf_pltfm_mgr_ctx()->ull_mntr_ctrl_date));
            sleep (15);
        } else
        {
            if (likely (flags & AF_PLAT_MNTR_FAN)) {
                sleep (3);
                check_fantray();
            }

            if (likely (flags & AF_PLAT_MNTR_POWER)) {
                sleep (3);
                check_pwr_supply();

                sleep (3);
                check_pwr_rails();
            }

            if (likely (flags & AF_PLAT_MNTR_MODULE)) {
                check_module();
                sleep (5);
            }
        }
    }

    return NULL;
}
