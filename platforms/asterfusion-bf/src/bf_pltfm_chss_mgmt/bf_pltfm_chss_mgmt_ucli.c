/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_ucli.c
 * @date
 *
 * Unit testing cli for chss_mgmt
 *
 */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

/* Module includes */
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_bmc_tty.h>
#include <bf_pltfm.h>

/* Local includes */
#include "bf_pltfm_bd_eeprom.h"

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_ps_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "ps_show", 1,
        " Show <power supply number (1/2)> status");
    bf_pltfm_pwr_supply_t pwr;
    bf_pltfm_pwr_supply_info_t info;

    pwr = atoi (uc->pargs->args[0]);

    if (pwr != POWER_SUPPLY1 &&
        pwr != POWER_SUPPLY2) {
        aim_printf (&uc->pvs,
                    "Invalid power supply number passed. It should be 1 or 2 \n");
        return -1;
    }

    memset (&info, 0, sizeof (info));
    if (bf_pltfm_chss_mgmt_pwr_supply_get (pwr,
                                           &info) != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in reading power supply status : PWR%d\n",
                    pwr);
        return -1;
    }

    aim_printf (&uc->pvs, "Presence        %s \n",
                (info.presence ? "true" : "false"));
    if (info.presence) {
        aim_printf (&uc->pvs,
                    "Vin             %3d.%2d V\n", info.vin >> 8,
                    (info.vin & 0x00FF));
        aim_printf (&uc->pvs,
                    "Vout            %3d.%2d V\n", info.vout >> 8,
                    (info.vout & 0x00FF));
        aim_printf (&uc->pvs,
                    "Iin             %3d.%2d A\n", info.iin >> 8,
                    (info.iin & 0x00FF));
        aim_printf (&uc->pvs,
                    "Iout            %3d.%2d A\n", info.iout >> 8,
                    (info.iout & 0x00FF));
        aim_printf (&uc->pvs, "Pin             %4d W\n",
                    info.pwr_in);
        aim_printf (&uc->pvs, "Pout            %4d W\n",
                    info.pwr_out);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_vrail_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "vrail_show", 0,
                       " Show all voltage rails reading");

    bf_pltfm_pwr_rails_info_t t;

    if (bf_pltfm_chss_mgmt_pwr_rails_get (
            &t) != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in reading rails voltages\n");
        return -1;
    }

    aim_printf (&uc->pvs,
                "Barefoot_Core       %5d mV\n", t.vrail1);
    aim_printf (&uc->pvs,
                "Barefoot_AVDD_0_9V  %5d mV\n", t.vrail2);
    aim_printf (&uc->pvs,
                "Payload_12V         %5d mV\n", t.vrail3);
    aim_printf (&uc->pvs,
                "Payload_3_3V        %5d mV\n", t.vrail4);
    aim_printf (&uc->pvs,
                "Payload_5V          %5d mV\n", t.vrail5);
    aim_printf (&uc->pvs,
                "Payload_2_5V        %5d mV\n", t.vrail6);
    aim_printf (&uc->pvs,
                "88E6131_1_9V        %5d mV\n", t.vrail7);
    aim_printf (&uc->pvs,
                "88E6131_1_2V        %5d mV\n", t.vrail8);

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_tmp_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "tmp_show", 0,
                       " Show all temperature sensors reading");

    bf_pltfm_temperature_info_t t;

    if (bf_pltfm_chss_mgmt_temperature_get (
            &t) != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in reading temperature \n");
        return -1;
    }

    /* The number of the sensor may vary in different platform.
     * by tsihang, 2021-07-13. */
    if (platform_type_equal (X532P)) {
        aim_printf (&uc->pvs, "tmp1    %.1f C   \"%s\"\n",
                    t.tmp1, "Far left of mainboard");
        aim_printf (&uc->pvs, "tmp2    %.1f C   \"%s\"\n",
                    t.tmp2, "Far right of mainboard");
        aim_printf (&uc->pvs, "tmp3    %.1f C   \"%s\"\n",
                    t.tmp3, "ASIC Ambient of XP70");
        aim_printf (&uc->pvs, "tmp4    %.1f C   \"%s\"\n",
                    t.tmp4, "ASIC Junction of XP70");
        aim_printf (&uc->pvs, "tmp5    %.1f C   \"%s\"\n",
                    t.tmp5, "Fan 1");
        aim_printf (&uc->pvs, "tmp6    %.1f C   \"%s\"\n",
                    t.tmp6, "Fan 2");
        /* Added more sensors. */
    } else if (platform_type_equal (X564P)) {
        aim_printf (&uc->pvs, "tmp1    %.1f C   \"%s\"\n",
                    t.tmp1, "Far left of mainboard");
        aim_printf (&uc->pvs, "tmp2    %.1f C   \"%s\"\n",
                    t.tmp2, "Far right of mainboard");
        aim_printf (&uc->pvs, "tmp3    %.1f C   \"%s\"\n",
                    t.tmp3, "ASIC Ambient of XP70");
        aim_printf (&uc->pvs, "tmp4    %.1f C   \"%s\"\n",
                    t.tmp4, "ASIC Junction of XP70");
        aim_printf (&uc->pvs, "tmp5    %.1f C   \"%s\"\n",
                    t.tmp5, "Fan 1");
        aim_printf (&uc->pvs, "tmp6    %.1f C   \"%s\"\n",
                    t.tmp6, "Fan 2");
        /* Added more sensors. */
    } else if (platform_type_equal (X312P)) {
        aim_printf (&uc->pvs, "tmp1    %.1f C   \"%s\"\n",
                    t.tmp1, "lm75");
        aim_printf (&uc->pvs, "tmp2    %.1f C   \"%s\"\n",
                    t.tmp2, "lm63");
        if (platform_subtype_equal(v1dot3)) {
            aim_printf (&uc->pvs, "tmp3    %.1f C   \"%s\"\n",
                        t.tmp3, "lm86");
        }
        /* Added more sensors. */
    } else if (platform_type_equal (HC)) {
        aim_printf (&uc->pvs, "tmp1    %.1f C   \"%s\"\n",
                    t.tmp1, "Unkonwn");
        aim_printf (&uc->pvs, "tmp2    %.1f C   \"%s\"\n",
                    t.tmp2, "Unkonwn");
        aim_printf (&uc->pvs, "tmp3    %.1f C   \"%s\"\n",
                    t.tmp3, "Unkonwn");
        aim_printf (&uc->pvs, "tmp4    %.1f C   \"%s\"\n",
                    t.tmp4, "Unkonwn");
        /* Added more sensors. */
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_fan_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "fan_show", 0,
                       " Show all fan speed data");
    uint32_t i;
    bf_pltfm_fan_data_t fdata;

    if (bf_pltfm_chss_mgmt_fan_data_get (
            &fdata) != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in reading temperature \n");
        return -1;
    }

    if (!fdata.fantray_present) {
        aim_printf (&uc->pvs, "Fan tray present \n");
    } else {
        aim_printf (&uc->pvs, "Fan tray not present \n");
        return 0;
    }

    aim_printf (&uc->pvs,
                "FAN  FRONT RPM  REAR RPM  MAX SPEED%%  \n");

    for (i = 0;
         i < (bf_pltfm_mgr_ctx()->fan_group_count *
              bf_pltfm_mgr_ctx()->fan_per_group); i++) {
        aim_printf (&uc->pvs,
                    "%2d     %5d     %5d      %3d%% \n",
                    fdata.F[i].fan_num,
                    fdata.F[i].front_speed,
                    fdata.F[i].rear_speed,
                    fdata.F[i].percent);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_fan_speed_set__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "fan_speed_set",
                       2,
                       " Set <Fan Num> speed in <%(0...100)> of max speed");
    bf_pltfm_fan_info_t fdata;

    fdata.fan_num = atoi (uc->pargs->args[0]);
    fdata.percent = atoi (uc->pargs->args[1]);
    fdata.front_speed = 0;
    fdata.rear_speed = 0;

    if (fdata.fan_num >
        (bf_pltfm_mgr_ctx()->fan_group_count *
         bf_pltfm_mgr_ctx()->fan_per_group)) {
        aim_printf (&uc->pvs,
                    "Invalid fan number (max %d) \n",
                    (bf_pltfm_mgr_ctx()->fan_group_count *
                     bf_pltfm_mgr_ctx()->fan_per_group));
        return 0;
    }

    if (bf_pltfm_chss_mgmt_fan_speed_set (
            &fdata) != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in setting fan speed \n");
        return -1;
    }

    aim_printf (&uc->pvs,
                "Fan speed set successfull\n");
    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_sys_mac_get__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "sys_mac_get",
                       0,
                       "System Mac address and number of extended address");

    uint8_t mac_info[6] = {0};
    uint8_t num_sys_port = 0;

    if (bf_pltfm_chss_mgmt_sys_mac_addr_get (mac_info,
            &num_sys_port) !=
        BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in getting system mac addr \n");
        return -1;
    }

    aim_printf (&uc->pvs,
                "System Mac addr: %02x:%02x:%02x:%02x:%02x:%02x \n",
                mac_info[0],
                mac_info[1],
                mac_info[2],
                mac_info[3],
                mac_info[4],
                mac_info[5]);
    aim_printf (&uc->pvs,
                "Number of extended addr available %d\n",
                num_sys_port);

    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_port_mac_get__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "port_mac_get", 2,
        "mac address of <conn_id> <channel>");

    uint8_t mac_info[6] = {0};
    bf_pltfm_port_info_t port;

    port.conn_id = atoi (uc->pargs->args[0]);
    port.chnl_id = atoi (uc->pargs->args[1]);

    if (bf_pltfm_chss_mgmt_port_mac_addr_get (&port,
            mac_info) !=
        BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in getting port mac addr \n");
        return -1;
    }

    aim_printf (&uc->pvs, "Port/channel:%d/%d  ",
                port.conn_id, port.chnl_id);
    aim_printf (&uc->pvs,
                "Port Mac addr: %02x:%02x:%02x:%02x:%02x:%02x \n",
                mac_info[0],
                mac_info[1],
                mac_info[2],
                mac_info[3],
                mac_info[4],
                mac_info[5]);
    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_eeprom_data_get__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "eeprom_data", 0,
                       "EEPROM parsed data");
    char code;
    char desc[256] = {0};
    char content[256] = {0};

    for (int i = 0; ; i ++) {
        bf_pltfm_bd_tlv_get (i, &code, desc, content);
        aim_printf (&uc->pvs, "%24s:\t0x%02x\t%s\n", desc,
                    code & 0xFF, content);
        if ((code & 0xFF) == 0xFE) {
            break;
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_rptr_ucli_ucli__chss_mgmt_tofino_tmp_show__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "tofino_tmp_show", 0,
                       " Show tofino temperature");

    bf_dev_id_t dev_id = 0;
    int sensor = 0;
    bf_pltfm_switch_temperature_info_t temp_mC;
    bf_pltfm_status_t r;

    r = bf_pltfm_chss_mgmt_switch_temperature_get (
            dev_id, sensor, &temp_mC);

    if (r != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "Error in retrieving tofino temperature\n");
        return -1;
    }

    aim_printf (&uc->pvs,
                "TOFINO MAIN TEMP SENSOR: %d C\n",
                (temp_mC.main_sensor / 1000));

    aim_printf (&uc->pvs,
                "TOFINO REMOTE TEMP SENSOR %d C\n",
                (temp_mC.remote_sensor / 1000));

    return 0;
}

#if 0
static ucli_status_t
bf_pltfm_ucli_ucli__chss_mgmt_bmc_cmd_ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "bmc_cmd", 2,
        "Execute BMC command <timeout in sec> <\"bmc cmd\">");

    char cmd_out[1024];
    char cmd_in[128];
    int ret;
    unsigned long timeout;

    timeout = atoi (uc->pargs->args[0]);
    snprintf (cmd_in, sizeof (cmd_in), "%s\r\n",
              uc->pargs->args[1]);
    cmd_in[sizeof (cmd_in) - 1] = '\0';
    ret = bmc_send_command_with_output (timeout,
                                        cmd_in, cmd_out, sizeof (cmd_out));

    if (ret != 0) {
        aim_printf (
            &uc->pvs,
            "Error in executing command \"%s\", displaying output data anyway\n",
            uc->pargs->args[0]);
    }

    aim_printf (&uc->pvs, "\n%s\n", cmd_out);

    return ret;
}
#endif

static ucli_command_handler_f
bf_pltfm_chss_mgmt_ucli_ucli_handlers__[] = {
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_sys_mac_get__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_port_mac_get__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_eeprom_data_get__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_tmp_show__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_tofino_tmp_show__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_vrail_show__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_ps_show__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_fan_show__,
    bf_pltfm_rptr_ucli_ucli__chss_mgmt_fan_speed_set__,
    //bf_pltfm_ucli_ucli__chss_mgmt_bmc_cmd_,
    NULL
};

static ucli_module_t
bf_pltfm_chss_mgmt_ucli_module__ = {
    "bf_pltfm_chss_mgmt_ucli",
    NULL,
    bf_pltfm_chss_mgmt_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_pltfm_chss_mgmt_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (
        &bf_pltfm_chss_mgmt_ucli_module__);
    n = ucli_node_create ("chss_mgmt", m,
                          &bf_pltfm_chss_mgmt_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("chss_mgmt"));
    return n;
}

