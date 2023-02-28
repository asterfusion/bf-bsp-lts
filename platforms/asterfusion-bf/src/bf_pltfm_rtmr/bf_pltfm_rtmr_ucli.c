/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rtmr_ucli.c
 * @date
 *
 * Unit testing cli for repeaters
 *
 */

/* Standard includes */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

/* Module includes */
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_ext_phy.h>
#include <bf_pltfm_rtmr.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>

/* Local header includes */
#include "bf_pltfm_rtmr_priv.h"

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_set", 3,
                       " <port_id>, <reg_addr> <value>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    uint32_t reg_value;
    bf_pltfm_rtmr_reg_info_t reg_info;

    port_num = atoi (uc->pargs->args[0]);
    reg_addr = (uint16_t)strtol (uc->pargs->args[1],
                                 NULL, 16);
    reg_value = (uint16_t)strtol (uc->pargs->args[2],
                                  NULL, 16);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    reg_info.offset = (uint16_t)reg_addr;
    reg_info.data = (uint16_t)reg_value;

    bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                             (void *)&reg_info);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_offset_dir_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_reg_set_offset_dir_all",
                       3,
                       " <offset> <value>"
                       "<direction (0=ingress 1=egress)>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    uint32_t reg_value;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i, j, start, end;
    uint8_t direction;

    reg_addr = (uint16_t)strtol (uc->pargs->args[0],
                                 NULL, 16);
    reg_value = (uint16_t)strtol (uc->pargs->args[1],
                                  NULL, 16);
    direction = atoi (uc->pargs->args[2]);

    if (direction == 1) {
        start = 0;
        end = 4;
    } else {
        start = 4;
        end = 8;
    }

    for (i = MIN_MVR_RTMR_PORTS;
         i <= MAX_MVR_RTMR_PORTS; i++) {
        port_num = i;

        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        for (j = start; j < end; j++) {
            reg_info.offset = ((uint16_t)reg_addr |
                               RTMR_REG_BASE | (j << 8));
            reg_info.data = (uint16_t)reg_value;

            bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                                     (void *)&reg_info);
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_offset_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_set_offset_all",
                       2, " <offset> <value>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    uint32_t reg_value;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i, j;

    reg_addr = (uint16_t)strtol (uc->pargs->args[0],
                                 NULL, 16);
    reg_value = (uint16_t)strtol (uc->pargs->args[1],
                                  NULL, 16);
    for (i = MIN_MVR_RTMR_PORTS;
         i <= MAX_MVR_RTMR_PORTS; i++) {
        port_num = i;

        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        for (j = 0; j < 8; j++) {
            reg_info.offset = ((uint16_t)reg_addr |
                               RTMR_REG_BASE | (j << 8));
            reg_info.data = (uint16_t)reg_value;

            bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                                     (void *)&reg_info);
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_set_all", 2,
                       " <address> <value>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    uint32_t reg_value;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i;

    reg_addr = (uint16_t)strtol (uc->pargs->args[0],
                                 NULL, 16);
    reg_value = (uint16_t)strtol (uc->pargs->args[1],
                                  NULL, 16);
    for (i = MIN_MVR_RTMR_PORTS;
         i <= MAX_MVR_RTMR_PORTS; i++) {
        port_num = i;

        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        reg_info.offset = (uint16_t)reg_addr;
        reg_info.data = (uint16_t)reg_value;

        bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                                 (void *)&reg_info);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_show", 2,
                       " <port_id> <reg_addr>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    bf_pltfm_rtmr_reg_info_t reg_info;

    port_num = atoi (uc->pargs->args[0]);
    reg_addr = (uint16_t)strtol (uc->pargs->args[1],
                                 NULL, 16);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    reg_info.offset = (uint16_t)reg_addr;
    reg_info.data = 0xff;

    bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                             (void *)&reg_info);

    aim_printf (&uc->pvs, "Offset: 0x%x : 0x%x\n",
                reg_info.offset, reg_info.data);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_all_reg__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_show_all_reg", 1,
                       " <port_id>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i;
    uint16_t j;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        aim_printf (&uc->pvs,
                    "Port number is out of range. Retimer ports are <33-64>\n");
        return 0;
    }
    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    for (i = 0; i < 0xBF; i++) {
        reg_info.offset = ((i & 0xff) | RTMR_6X_REG_BASE);
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "offset: 0x%4x : 0x%4x\n",
            reg_info.offset, reg_info.data);
    }

    aim_printf (
        &uc->pvs,
        "------------------------------------------------------------------------"
        "--------\n");

    aim_printf (
        &uc->pvs,
        "       | 0x70xx | 0x71xx | 0x72xx | 0x73xx | 0x74xx | 0x75xx | 0x76xx | "
        "0x77xx |\n");
    aim_printf (
        &uc->pvs,
        "------------------------------------------------------------------------"
        "--------\n");
    for (j = 0; j <= 0x2C; j++) {
        aim_printf (&uc->pvs, "0x%4x |",
                    (RTMR_7X_REG_BASE | j));
        for (i = 0; i < 8; i++) {
            reg_info.offset = ((uint8_t)j | RTMR_7X_REG_BASE
                               | (i << 8));
            reg_info.data = 0xff;

            bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                     (void *)&reg_info);

            aim_printf (&uc->pvs, " 0x%4x |", reg_info.data);
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (
        &uc->pvs,
        "------------------------------------------------------------------------"
        "--------\n");

    aim_printf (
        &uc->pvs,
        "       | 0x80xx | 0x81xx | 0x82xx | 0x83xx | 0x84xx | 0x85xx | 0x86xx | "
        "0x87xx |\n");
    aim_printf (
        &uc->pvs,
        "------------------------------------------------------------------------"
        "--------\n");
    for (j = 0; j <= 0xff; j++) {
        aim_printf (&uc->pvs, "0x%4x |",
                    (RTMR_REG_BASE | j));
        for (i = 0; i < 8; i++) {
            reg_info.offset = ((uint8_t)j | RTMR_REG_BASE |
                               (i << 8));
            reg_info.data = 0xff;

            bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                     (void *)&reg_info);

            aim_printf (&uc->pvs, " 0x%4x |", reg_info.data);
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (
        &uc->pvs,
        "------------------------------------------------------------------------"
        "--------\n");

    for (i = 0; i < 0x3C; i++) {
        reg_info.offset = ((i & 0xff) | 0x9800);
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "offset: 0x%4x : 0x%4x\n",
            reg_info.offset, reg_info.data);
    }

    for (i = 0; i < 0x20; i++) {
        reg_info.offset = ((i & 0xff) | 0x9900);
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "offset: 0x%4x : 0x%4x\n",
            reg_info.offset, reg_info.data);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_offset_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_show_offset_all",
                       1, " <offset>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i, j;

    reg_addr = (uint16_t)strtol (uc->pargs->args[0],
                                 NULL, 16);
    for (i = MIN_MVR_RTMR_PORTS;
         i <= MAX_MVR_RTMR_PORTS; i++) {
        port_num = i;

        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        aim_printf (&uc->pvs, "Port %02d ", port_num);
        for (j = 0; j < 8; j++) {
            reg_info.offset = ((uint16_t)reg_addr |
                               RTMR_REG_BASE | (j << 8));
            reg_info.data = 0xff;

            bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                     (void *)&reg_info);

            if (j == 0) {
                aim_printf (&uc->pvs, "eg: |");
            }
            if (j == 4) {
                aim_printf (&uc->pvs, "        Ig: |");
            }
            aim_printf (
                &uc->pvs, "  0x%4x : 0x%4x  |", reg_info.offset,
                reg_info.data);
            if ((j == 3) || (j == 7)) {
                aim_printf (&uc->pvs, "\n");
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reg_show_all", 1,
                       " <address>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint32_t reg_addr;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t i;

    reg_addr = (uint16_t)strtol (uc->pargs->args[0],
                                 NULL, 16);

    for (i = MIN_MVR_RTMR_PORTS;
         i <= MAX_MVR_RTMR_PORTS; i++) {
        port_num = i;

        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        reg_info.offset = (uint16_t)reg_addr;
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (&port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "Port %02d : 0x%4x \n",
                    port_num, reg_info.data);
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_fware_dl__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_fware_dl", 1,
                       " <port_id>");

    FILE *fp;
    uint8_t buffer[BF_RTMR_MAX_FW];
    uint32_t buf_size;
    bf_pltfm_status_t res = UCLI_STATUS_OK;

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    bf_pltfm_rtmr_fw_info_t fw_info;
    char *str = NULL;
    char install_dir[RTMR_INSTALL_DIR_MAX];
    char install_dir_f[RTMR_INSTALL_DIR_FULL_MAX];

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    str = bf_pltfm_rtmr_install_dir_get (install_dir,
                                         sizeof (install_dir));
    if (str == NULL) {
        /* Error.  Unable to get install dir */
        return UCLI_STATUS_E_ERROR;
    }

    snprintf (install_dir_f,
              RTMR_INSTALL_DIR_FULL_MAX,
              "%s/%s",
              install_dir,
              RTMR_FW_FILE_LOC);

    fp = fopen (install_dir_f, "rb");
    if (!fp) {
        LOG_ERROR ("Failed to open firmware file: %s\n",
                   RTMR_FW_FILE_LOC);
        return UCLI_STATUS_E_ERROR;
    }
    {
        uint16_t crc;
        bf_pltfm_rtmr_crc_rd_from_bin_file (&crc);
        printf ("CRC:%x\n", crc);
    }
    fseek (fp, 4096, SEEK_SET);
    buf_size = fread (buffer, 1, BF_RTMR_MAX_FW, fp);
    fclose (fp);

    fw_info.image = buffer;
    fw_info.size = buf_size;

    /* Stop qsfp polling to prevent i2c conflict */
    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_FW_DL, (void *)&fw_info);

    /* Restart qsfp polling */
    bf_pltfm_pm_qsfp_scan_poll_start();

    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_fware_dl_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_fware_dl_all", 0,
                       " ");

    bf_pltfm_rtmr_fw_info_t fw_info;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    FILE *fp;
    uint8_t buffer[BF_RTMR_MAX_FW];
    uint32_t buf_size;
    bool has_rtmr = false;
    uint8_t port_num;
    char *str = NULL;
    char install_dir[RTMR_INSTALL_DIR_MAX];
    char install_dir_f[RTMR_INSTALL_DIR_FULL_MAX];

    str = bf_pltfm_rtmr_install_dir_get (install_dir,
                                         sizeof (install_dir));
    if (str == NULL) {
        /* Error.  Unable to get install dir */
        return UCLI_STATUS_E_ERROR;
    }

    snprintf (install_dir_f,
              RTMR_INSTALL_DIR_FULL_MAX,
              "%s/%s",
              install_dir,
              RTMR_FW_FILE_LOC);

    fp = fopen (install_dir_f, "rb");
    if (!fp) {
        LOG_ERROR ("Failed to open firmware file: %s\n",
                   RTMR_FW_FILE_LOC);
        return UCLI_STATUS_E_ERROR;
    }
    fseek (fp, 4096, SEEK_SET);
    buf_size = fread (buffer, 1, BF_RTMR_MAX_FW, fp);
    fclose (fp);

    fw_info.image = buffer;
    fw_info.size = buf_size;

    /* Stop qsfp polling to prevent i2c conflict */
    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */

    for (port_num = 1; port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        port_info.conn_id = port_num;
        port_info.chnl_id = 0;

        bf_bd_cfg_port_has_rtmr (&port_info, &has_rtmr);

        if (!has_rtmr) {
            /* No retimer on this port */
            continue;
        }

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_FW_DL, (void *)&fw_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to download firmware for port: %d\n",
                       port_num);
            continue;
        }
    }

    /* Restart qsfp polling */
    bf_pltfm_pm_qsfp_scan_poll_start();

    return UCLI_STATUS_OK;
}

static ucli_status_t
bf_pltfm_rtmr_do_chk_fware_dl (ucli_context_t *uc,
                               uint8_t port_num)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t data_hi;
    uint16_t data_lo;

    port_info.conn_id = port_num;
    port_info.chnl_id = 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    /* Write Firmware Checksum read back code to 0x9818 */
    reg_info.offset = RTMR_FW_IND_CTRL_REG;
    reg_info.data = 0xf000;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Read back 0x9818 and 0x9819 */
    reg_info.offset = RTMR_FW_IND_CTRL_REG;
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    data_hi = reg_info.data;

    reg_info.offset = RTMR_FW_IND_OFFSET_REG;
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    data_lo = reg_info.data;

    aim_printf (&uc->pvs, "Hash code: 0x%02x%04x\n",
                (data_hi & 0xff), data_lo);

    return UCLI_STATUS_OK;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chk_fware_dl__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_chk_fware_dl", 1,
                       " <port_id>");

    uint8_t port_num;

    port_num = atoi (uc->pargs->args[0]);

    return bf_pltfm_rtmr_do_chk_fware_dl (uc,
                                          port_num);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chk_fware_dl_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_chk_fware_dl_all", 0,
                       " ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        aim_printf (&uc->pvs, "Port: %d: ", port_num);
        res = bf_pltfm_rtmr_do_chk_fware_dl (uc,
                                             port_num);
        if (res != UCLI_STATUS_OK) {
            /* Log error and continue */
            LOG_ERROR ("Failed to check firmware checksum.\n");
            continue;
        }
    }

    return UCLI_STATUS_OK;
}

static ucli_status_t bf_pltfm_rtmr_set_clr_lb (
    uint8_t port_num,
    uint8_t direction,
    uint8_t set_lb)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint8_t chan_num;

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    /* LB_CNTRL_REG.14 need to be set for loopback mode */
    reg_info.offset = LB_CNTRL_REG;
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Set mask based on setting or clearing loopback */
    if (set_lb) {
        reg_info.data |= RTMR_LB_MASK;
    } else {
        reg_info.data &= ~RTMR_LB_MASK;
    }

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Setting loopback for all channels */
    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    for (chan_num = 0; chan_num < MAX_RTMR_CHAN;
         chan_num++) {
        rtmr_lane = chan_num + side_offset;

        /* 8XFF.6=0 for reference clock of the TX lane of the loopback */
        reg_info.offset = PLL_REFCLK_DIV |
                          RTMR_CHAN_ADDR_SET (rtmr_lane);
        reg_info.data = 0xff;

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_READ, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to read current register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set mask based on set_lb */
        if (set_lb) {
            reg_info.data &= ~RTMR_LB_EN_MASK;
        } else {
            reg_info.data |= RTMR_LB_EN_MASK;
        }

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_WRITE, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to write new register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set 8XFE[15:8] to 10h instead of A5h of the TX lane of the loopback. */
        reg_info.offset = PLL_MULTIPLIER |
                          RTMR_CHAN_ADDR_SET (rtmr_lane);
        reg_info.data = 0xff;

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_READ, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to read current register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set mask based on set_lb */
        if (set_lb) {
            reg_info.data &= ~0xff00;
            reg_info.data |= 0x1000;
        } else {
            reg_info.data &= ~0xff00;
            reg_info.data |= 0xa500;
        }

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_WRITE, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to write new register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* clear 0x8000 for registers 0x8x75 */
        reg_info.offset = TIMING_CTRL_REG |
                          RTMR_CHAN_ADDR_SET (rtmr_lane);
        reg_info.data = 0xff;

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_READ, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to read current register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set mask based on set_lb */
        if (set_lb) {
            reg_info.data &= ~RTMR_LB_TIMING_BIT;
        } else {
            reg_info.data |= RTMR_LB_TIMING_BIT;
        }

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_WRITE, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to write new register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set bit 0x100 for registers 0x8x61 */
        reg_info.offset = RX_PRBS_CFG_REG |
                          RTMR_CHAN_ADDR_SET (rtmr_lane);
        reg_info.data = 0xff;

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_READ, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to read current register value.\n");
            return UCLI_STATUS_E_ERROR;
        }

        /* Set mask based on set_lb */
        if (set_lb) {
            reg_info.data |= RTMR_LB_RX_SER_BIT;
        } else {
            reg_info.data &= ~RTMR_LB_RX_SER_BIT;
        }

        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_WRITE, (void *)&reg_info);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to write new register value.\n");
            return UCLI_STATUS_E_ERROR;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_host_lb__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "rtmr_host_lb", 2,
        " <port_id> <set_lb (0=clear 1=set)>");

    uint8_t port_num;
    uint8_t set_lb;

    port_num = atoi (uc->pargs->args[0]);
    set_lb = atoi (uc->pargs->args[0]) ? 1 : 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_set_clr_lb (port_num,
                                     RTMR_DIR_INGRESS, set_lb);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_host_lb_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_host_lb_all", 1,
                       " <set_lb (0=clear 1=set)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t set_lb;

    set_lb = atoi (uc->pargs->args[0]) ? 1 : 0;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        bf_pltfm_rtmr_set_clr_lb (port_num,
                                  RTMR_DIR_INGRESS, set_lb);
    }

    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_line_lb__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc, "rtmr_line_lb", 2,
        " <port_id> <set_lb (0=clear 1=set)>");

    uint8_t port_num;
    uint8_t set_lb;

    port_num = atoi (uc->pargs->args[0]);
    set_lb = atoi (uc->pargs->args[0]) ? 1 : 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_set_clr_lb (port_num,
                                     RTMR_DIR_EGRESS, set_lb);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_line_lb_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_line_lb_all", 1,
                       " <set_lb (0=clear 1=set)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t set_lb;

    set_lb = atoi (uc->pargs->args[0]) ? 1 : 0;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        res |= bf_pltfm_rtmr_set_clr_lb (port_num,
                                         RTMR_DIR_EGRESS, set_lb);
    }

    return res;
}

#if 0
static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_dfe_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_dfe_set", 5,
                       " <port_id> <chan_num> <tap1> <tap2> <tap3>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;

    return 0;
}
#endif

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_dfe_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_dfe_show",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint16_t rtmr_lane;
    uint16_t side_offset;
    uint8_t direction;
    uint16_t tap1_curr;
    uint16_t tap2_3_curr;
    uint16_t tap_minus_1_curr;
    uint16_t dac_sel;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    /* Read Tap 1 value */
    reg_info.offset = RX_DFETAP1_CURR_VAL_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    tap1_curr = reg_info.data;

    /* Read Tap 2 and 3 values */
    reg_info.offset = RX_DFETAP2_3_CURR_VAL_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    tap2_3_curr = reg_info.data;

    /* Read Tap -1 value */
    reg_info.offset =
        RX_DFETAP_MINUS_1_CURR_VAL_REG |
        RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    tap_minus_1_curr = reg_info.data;

    /* Read DAC Sel value */
    reg_info.offset = READ_REF_DAC_CURR_CTL_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    dac_sel = RTMR_GET_DAC_SEL (reg_info.data);

    aim_printf (&uc->pvs, "Port: %d : %d: \n",
                port_num, channel_num);
    aim_printf (&uc->pvs,
                "\tTap 1: %d mV\n",
                RTMR_DFE_TAP_CALC (RTMR_DFE_TAP_1_GET (tap1_curr),
                                   dac_sel));
    aim_printf (&uc->pvs,
                "\tTap 2: %d mV\n",
                RTMR_DFE_TAP_CALC (RTMR_DFE_TAP_2_GET (
                                       tap2_3_curr), dac_sel));
    aim_printf (&uc->pvs,
                "\tTap 3: %d mV\n",
                RTMR_DFE_TAP_CALC (RTMR_DFE_TAP_3_GET (
                                       tap2_3_curr), dac_sel));
    aim_printf (
        &uc->pvs,
        "\tTap -1: %d mV\n",
        RTMR_DFE_TAP_CALC (RTMR_DFE_TAP_MINUS1_GET (
                               tap_minus_1_curr), dac_sel));

    return 0;
}

#if 0
static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_eye_margin_show",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint16_t rtmr_lane;
    uint16_t side_offset;
    uint8_t direction;
    uint16_t eye_margin;
    uint16_t dac_sel;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    /* Read Eye margin value */
    reg_info.offset = RX_READ_EYE_MARGIN_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    eye_margin = RTMR_RX_EYE_MARGIN_GET (
                     reg_info.data);

    /* Read DAC Sel value */
    reg_info.offset = READ_REF_DAC_CURR_CTL_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    dac_sel = RTMR_GET_DAC_SEL (reg_info.data);

    printf ("Port: %d : %d: \n", port_num,
            channel_num);
    printf ("\tEye Margin: 0x%x\n",
            RTMR_EYE_MARGIN_CALC (eye_margin, dac_sel));

    return 0;
}
#endif

static ucli_status_t do_rtmr_eye_margin (
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction,
    int16_t *eye)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t rtmr_lane;
    uint16_t side_offset;
    int16_t eye_margin;
    uint16_t dac_sel;

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    /* Read Eye margin value */
    reg_info.offset = RX_READ_EYE_MARGIN_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    eye_margin = RTMR_RX_EYE_MARGIN_GET (
                     reg_info.data);

    /* Read DAC Sel value */
    reg_info.offset = READ_REF_DAC_CURR_CTL_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    dac_sel = RTMR_GET_DAC_SEL (reg_info.data);

    *eye = RTMR_EYE_MARGIN_CALC (eye_margin, dac_sel);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_eye_margin_show",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    ucli_status_t res;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    int16_t eye;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    res = do_rtmr_eye_margin (port_num, channel_num,
                              direction, &eye);

    if (!res) {
        aim_printf (&uc->pvs, "Port: %d : %d: \n",
                    port_num, channel_num);
        aim_printf (&uc->pvs, "\tEye Margin: %d mV\n",
                    eye);
    }
    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_eye_margin_show_all_chan",
                       2,
                       " <port_id> <direction (0=ingress 1=egress)>");

    ucli_status_t res;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    int16_t eye;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = do_rtmr_eye_margin (port_num, channel_num,
                                  direction, &eye);

        if (!res) {
            aim_printf (&uc->pvs, "Port: %d : %d: \n",
                        port_num, channel_num);
            aim_printf (&uc->pvs, "\tEye Margin: %d mV\n",
                        eye);
        } else {
            return res;
        }
    }
    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_port__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_eye_margin_show_port", 1, " <port_id>");

    ucli_status_t res;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    int16_t eye;

    port_num = atoi (uc->pargs->args[0]);

    aim_printf (&uc->pvs,
                "            |    0   |     1  |    2   |    3   |\n");
    aim_printf (&uc->pvs,
                "-------------------------------------------------\n");
    aim_printf (&uc->pvs, "Port %02d ", port_num);
    for (direction = 0; direction < RTMR_MAX_DIR;
         direction++) {
        if (direction) {
            aim_printf (&uc->pvs, "        eg: |");
        } else {
            aim_printf (&uc->pvs, "Ig: |");
        }
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            res = do_rtmr_eye_margin (port_num, channel_num,
                                      direction, &eye);

            if (!res) {
                aim_printf (&uc->pvs, "  %4d  |", eye);
            } else {
                return res;
            }
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (&uc->pvs, "\n");
    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_eye_margin_show_all",
                       0, " ");

    ucli_status_t res;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    int16_t eye;

    aim_printf (&uc->pvs,
                "            |    0   |     1  |    2   |    3   |\n");
    aim_printf (&uc->pvs,
                "-------------------------------------------------\n");
    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        aim_printf (&uc->pvs, "Port %02d ", port_num);
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            if (direction) {
                aim_printf (&uc->pvs, "        eg: |");
            } else {
                aim_printf (&uc->pvs, "Ig: |");
            }
            for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
                 channel_num++) {
                res = do_rtmr_eye_margin (port_num, channel_num,
                                          direction, &eye);

                if (!res) {
                    aim_printf (&uc->pvs, "  %4d  |", eye);
                } else {
                    return res;
                }
            }
            aim_printf (&uc->pvs, "\n");
        }
        aim_printf (&uc->pvs, "\n");
    }
    aim_printf (&uc->pvs, "\n");
    return res;
}

#if 0
static ucli_status_t
bf_pltfm_rtmr_read_modify_write_reg (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_rtmr_reg_info_t *reg_info,
    uint16_t value, uint16_t mask)
{

    bf_pltfm_status_t res = UCLI_STATUS_OK;

    /* Read current value */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_READ, (void *)reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear data based on mask */
    reg_info-> &= ~ (mask);

    /* Write in new settings */
    reg_info->data |= value;

    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_WRITE, (void *)reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}
#endif

bf_pltfm_status_t
bf_pltfm_platform_ext_phy_prbs_set (
    bf_pltfm_port_info_t *port_info,
    uint8_t direction, uint16_t prbs_mode)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val;

    /* Set PRBS fields */
    val = RTMR_PRBS_GEN_SET;
    val |= RTMR_PRBS_GEN_MODE_SET (prbs_mode);

    /* Read current value */
    if (!port_info) {
        return UCLI_STATUS_E_PARAM;
    }
    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = port_info->chnl_id + side_offset;

    reg_info.offset = TX_TEST_PATTERN_CFG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return res;
    }

    /* Clear fields being written to */
    reg_info.data &= ~ (RTMR_PRBS_GEN_SET |
                        RTMR_PRBS_GEN_MASK);

    /* Write in new settings */
    reg_info.data = reg_info.data | val;

    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return res;
    }

    return UCLI_STATUS_OK;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_prbs_set",
                       4,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)> "
                       "<prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    prbs_mode = atoi (uc->pargs->args[3]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;
    return bf_pltfm_platform_ext_phy_prbs_set (
               &port_info, direction, prbs_mode);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_prbs_set_all_chan",
                       3,
                       " <port_id> <direction (0=ingress 1=egress)> "
                       "<prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);
    prbs_mode = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        port_info.chnl_id = channel_num;
        res = bf_pltfm_platform_ext_phy_prbs_set (
                  &port_info, direction, prbs_mode);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to set prbs port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_port__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_prbs_set_port",
        2,
        " <port_id> <prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    prbs_mode = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        port_info.chnl_id = channel_num;
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res =
                bf_pltfm_platform_ext_phy_prbs_set (&port_info,
                                                    direction, prbs_mode);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to set prbs port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_prbs_set_all",
                       1,
                       " <prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    prbs_mode = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        port_info.conn_id = port_num;
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            port_info.chnl_id = channel_num;
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res = bf_pltfm_platform_ext_phy_prbs_set (
                          &port_info, direction, prbs_mode);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to set prbs port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t bf_pltfm_rtmr_prbs_clr (
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = TX_TEST_PATTERN_CFG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear PRBS fields */
    reg_info.data &= ~ (RTMR_PRBS_GEN_SET |
                        RTMR_PRBS_GEN_MASK);

    /* Write in new settings */
    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_prbs_clr",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_prbs_clr (port_num,
                                   channel_num, direction);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_prbs_clr_all_chan",
                       2,
                       " <port_id> <direction (0=ingress 1=egress)> ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = bf_pltfm_rtmr_prbs_clr (port_num,
                                      channel_num, direction);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to clr prbs port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_port__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_prbs_clr_port", 1,
                       " <port_id>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res = bf_pltfm_rtmr_prbs_clr (port_num,
                                          channel_num, direction);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to clr prbs port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_prbs_clr_all", 0,
                       " ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res = bf_pltfm_rtmr_prbs_clr (port_num,
                                              channel_num, direction);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to clr prbs port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t bf_pltfm_rtmr_chkr_set (
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction,
    uint16_t prbs_mode)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val;

    /* Set PRBS Chkr fields */
    val = RTMR_PRBS_CKR_SET;
    val |= RTMR_PRBS_CKR_MODE_SET (prbs_mode);

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = RX_PRBS_CFG_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~ (RTMR_PRBS_CKR_MASK |
                        RTMR_PRBS_CKR_SET);

    /* Write in new settings */
    reg_info.data |= val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_set",
                       4,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)> "
                       "<prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    prbs_mode = atoi (uc->pargs->args[3]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_chkr_set (port_num,
                                   channel_num, direction, prbs_mode);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_set_all_chan",
                       3,
                       " <port_id> <direction (0=ingress 1=egress)> "
                       "<prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);
    prbs_mode = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = bf_pltfm_rtmr_chkr_set (port_num,
                                      channel_num, direction, prbs_mode);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to clr chkr: port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_port__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_chkr_set_port",
        2,
        " <port_id> <prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    port_num = atoi (uc->pargs->args[0]);
    prbs_mode = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res = bf_pltfm_rtmr_chkr_set (port_num,
                                          channel_num, direction, prbs_mode);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to set chkr: port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_set_all",
                       1,
                       " <prbs_mode (0=prbs9 1=prbs15 2=prbs23 3=prbs31)>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t prbs_mode;

    prbs_mode = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if (prbs_mode > MAX_PRBS_MODE) {
        LOG_ERROR ("Unsupported PRBS mode. <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res =
                    bf_pltfm_rtmr_chkr_set (port_num, channel_num,
                                            direction, prbs_mode);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to set chkr: port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t bf_pltfm_rtmr_chkr_clr (
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = RX_PRBS_CFG_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear PRBS chkr fields */
    reg_info.data &= ~ (RTMR_PRBS_CKR_SET |
                        RTMR_PRBS_CKR_MASK);

    /* Write in new settings */
    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_clr",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_chkr_clr (port_num,
                                   channel_num, direction);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_clr_all_chan",
                       2,
                       " <port_id> <direction (0=ingress 1=egress)> ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = bf_pltfm_rtmr_chkr_clr (port_num,
                                      channel_num, direction);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to clr chkr: port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_port__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_chkr_clr_port", 1,
                       " <port_id>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res = bf_pltfm_rtmr_chkr_clr (port_num,
                                          channel_num, direction);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to clr chkr: port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_chkr_clr_all", 0,
                       " ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res = bf_pltfm_rtmr_chkr_clr (port_num,
                                              channel_num, direction);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to clr chkr: port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_chkr_err_cnt_show (ucli_context_t
                                 *uc,
                                 uint8_t port_num,
                                 uint8_t channel_num,
                                 uint8_t direction)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint32_t hi;
    uint32_t lo;
    uint32_t count;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = RX_PRBS_ERROR_CNT_HI_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    hi = reg_info.data;

    reg_info.offset = RX_PRBS_ERROR_CNT_LO_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    lo = reg_info.data;

    count = (hi << 16) | (lo & 0xffff);
    if (count == 0) {
        /* Skip printing out if no errors seen */
        return 0;
    }
    if (direction) {
        aim_printf (
            &uc->pvs, "Port: %d : Channel %d egress\n",
            port_num, channel_num);
    } else {
        aim_printf (
            &uc->pvs, "Port: %d : Channel %d ingress\n",
            port_num, channel_num);
    }
    aim_printf (&uc->pvs, "\tError count 0x%x.\n",
                count);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_show",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_chkr_err_cnt_show (uc,
                                            port_num, channel_num, direction);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_show_all_chan",
                       2,
                       " <port_id> <direction (0=ingress 1=egress)> ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = bf_pltfm_rtmr_chkr_err_cnt_show (uc,
                                               port_num, channel_num, direction);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to get error count: port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_port__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_show_port", 1, " <port_id>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res =
                bf_pltfm_rtmr_chkr_err_cnt_show (uc, port_num,
                                                 channel_num, direction);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to get error count: port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_show_all", 0, " ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res = bf_pltfm_rtmr_chkr_err_cnt_show (
                          uc, port_num, channel_num, direction);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to get error count: port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_chkr_err_cnt_clear (uint8_t
                                  port_num,
                                  uint8_t channel_num,
                                  uint8_t direction)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val;

    /* Set PRBS Chkr fields */
    val = RTMR_PRBS_CKR_RST_CNT_MASK;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = RX_PRBS_CFG_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~RTMR_PRBS_CKR_RST_CNT_MASK;

    /* Set bit to clear checker counter */
    reg_info.data |= val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Unset bit to clear checker counter */
    reg_info.data &= ~val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_clear",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_chkr_err_cnt_clear (port_num,
            channel_num, direction);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_clear_all_chan",
                       2,
                       " <port_id> <direction (0=ingress 1=egress)> ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    direction = atoi (uc->pargs->args[1]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        res = bf_pltfm_rtmr_chkr_err_cnt_clear (port_num,
                                                channel_num, direction);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Failed to clear error count: port: %d chan: %d dir: %d.\n",
                       port_num,
                       channel_num,
                       direction);
            /* continue and try the other channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_port__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_clear_port", 1, " <port_id>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
         channel_num++) {
        for (direction = 0; direction < RTMR_MAX_DIR;
             direction++) {
            res = bf_pltfm_rtmr_chkr_err_cnt_clear (port_num,
                                                    channel_num, direction);
            if (res != UCLI_STATUS_OK) {
                LOG_ERROR ("Failed to clear error count: port: %d chan: %d dir: %d.\n",
                           port_num,
                           channel_num,
                           direction);
                /* continue and try the other channels */
            }
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_all__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_chkr_err_cnt_clear_all", 0, " ");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (channel_num = 0; channel_num < MAX_RTMR_CHAN;
             channel_num++) {
            for (direction = 0; direction < RTMR_MAX_DIR;
                 direction++) {
                res =
                    bf_pltfm_rtmr_chkr_err_cnt_clear (port_num,
                                                      channel_num, direction);
                if (res != UCLI_STATUS_OK) {
                    LOG_ERROR ("Failed to clear error count: port: %d chan: %d dir: %d.\n",
                               port_num,
                               channel_num,
                               direction);
                    /* continue and try the other channels */
                }
            }
        }
    }

    return 0;
}

static ucli_status_t bf_pltfm_rtmr_set_tx_coeff (
    ucli_context_t *uc,
    uint16_t reg_addr)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint32_t value;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    value = (uint16_t)strtol (uc->pargs->args[3],
                              NULL, 16);

    /* check port_num is valid */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        /* Port number is not valid */
        LOG_ERROR ("Port number is out of range <32-64> : %d\n",
                   port_num);
        return UCLI_STATUS_E_PARAM;
    }

    /* check channel_num is valid */
    if (channel_num >= MAX_RTMR_CHAN) {
        /* Channel number is not valid */
        LOG_ERROR ("Channel number is out of range <0-3> : %d\n",
                   channel_num);
        return UCLI_STATUS_E_PARAM;
    }

    /* Check whether A or B side */
    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = reg_addr | RTMR_CHAN_ADDR_SET (
                          rtmr_lane);
    reg_info.data = 0xff;

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* read current setting */
    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear current contents */
    reg_info.data &= ~ (TX_COEFF_DATA_SET (0x3f));

    /* Modify current data with new setting */
    reg_info.data |= TX_COEFF_DATA_SET (value);

    /* Write back modified field */
    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_3__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_set_tx_post_3",
        4,
        " <port_id> <chan_num> <direction (0=ingress 1=egress)> <value>");

    return bf_pltfm_rtmr_set_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP3_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_2__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_set_tx_post_2",
        4,
        " <port_id> <chan_num> <direction (0=ingress 1=egress)> <value>");

    return bf_pltfm_rtmr_set_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP2_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_1__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_set_tx_post_1",
        4,
        " <port_id> <chan_num> <direction (0=ingress 1=egress)> <value>");

    return bf_pltfm_rtmr_set_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP1_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_main__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_set_tx_main",
        4,
        " <port_id> <chan_num> <direction (0=ingress 1=egress)> <value>");

    return bf_pltfm_rtmr_set_tx_coeff (uc,
                                       TX_MAIN_TAP_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_precursor__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (
        uc,
        "rtmr_set_tx_precursor",
        4,
        " <port_id> <chan_num> <direction (0=ingress 1=egress)> <value>");

    return bf_pltfm_rtmr_set_tx_coeff (uc,
                                       TX_MAIN_PRE_CURSOR_COEFF);
}

static ucli_status_t bf_pltfm_rtmr_get_tx_coeff (
    ucli_context_t *uc,
    uint16_t reg_addr)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    /* check port_num is valid */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        /* Port number is not valid */
        LOG_ERROR ("Port number is out of range <33-64> : %d\n",
                   port_num);
        return UCLI_STATUS_E_PARAM;
    }

    /* check channel_num is valid */
    if (channel_num >= MAX_RTMR_CHAN) {
        /* Channel number is not valid */
        LOG_ERROR ("Channel number is out of range <0-3> : %d\n",
                   channel_num);
        return UCLI_STATUS_E_PARAM;
    }

    /* Check whether A or B side */
    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = reg_addr | RTMR_CHAN_ADDR_SET (
                          rtmr_lane);
    reg_info.data = 0xff;

    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* read current setting */
    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    aim_printf (&uc->pvs,
                "Register: 0x%x : 0x%x\n",
                reg_info.offset,
                TX_COEFF_DATA_GET (reg_info.data));

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_3__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_show_tx_post_3",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    return bf_pltfm_rtmr_get_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP3_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_2__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_show_tx_post_2",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    return bf_pltfm_rtmr_get_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP2_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_1__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_show_tx_post_1",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    return bf_pltfm_rtmr_get_tx_coeff (uc,
                                       TX_POST_CURSOR_TAP1_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_main__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_show_tx_main",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    return bf_pltfm_rtmr_get_tx_coeff (uc,
                                       TX_MAIN_TAP_COEFF);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_precursor__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_show_tx_precursor",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    return bf_pltfm_rtmr_get_tx_coeff (uc,
                                       TX_MAIN_PRE_CURSOR_COEFF);
}

static ucli_status_t bf_pltfm_rtmr_ctle_set (
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction,
    uint16_t ctle_val)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    if (ctle_val > MAX_CTLE_VAL) {
        LOG_ERROR ("Unsupported ctle value. <0-7>\n");
        return UCLI_STATUS_E_PARAM;
    }

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    /* Set CTLE Override bit */
    reg_info.offset = RX_CTLE_OVERRIDE |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~
                     (RTMR_CTLE_OVERRIDE_ENABLE_BIT);

    /* Write in new settings */
    reg_info.data = reg_info.data |
                    RTMR_CTLE_OVERRIDE_ENABLE_BIT;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Set CTLE Override value */
    reg_info.offset = RX_CTLE_OVERRIDE_VAL |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    /* Set CTLE value */
    val = RTMR_CTLE_VAL_SET (ctle_val);

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~ (RTMR_CTLE_VAL_SET (0x7));

    /* Write in new settings */
    reg_info.data = reg_info.data | val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_set__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_ctle_set",
                       4,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)> "
                       "<ctle_val>");
    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t ctle_val;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    ctle_val = atoi (uc->pargs->args[3]);

    return bf_pltfm_rtmr_ctle_set (port_num,
                                   channel_num, direction, ctle_val);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_set_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_ctle_set_all", 1,
                       " <ctle_val>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t ctle_val;

    ctle_val = atoi (uc->pargs->args[0]);

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (direction = 0; direction < 2; direction++) {
            for (channel_num = 0; channel_num < 4;
                 channel_num++) {
                bf_pltfm_rtmr_ctle_set (port_num, channel_num,
                                        direction, ctle_val);
            }
        }
    }

    return 0;
}

static ucli_status_t bf_pltfm_rtmr_ctle_show (
    ucli_context_t *uc,
    uint8_t port_num,
    uint8_t channel_num,
    uint8_t direction)
{
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    /* Set CTLE Override bit */
    reg_info.offset = RX_CTLE_OVERRIDE_VAL |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    aim_printf (&uc->pvs,
                "Port: %d Chan: %d direction: %d\n",
                port_num,
                channel_num,
                direction);
    aim_printf (&uc->pvs, "\t CTLE val: %d\n",
                RTMR_CTLE_VAL_GET (reg_info.data));

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_ctle_show",
                       3,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);

    return bf_pltfm_rtmr_ctle_show (uc, port_num,
                                    channel_num, direction);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_show_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_ctle_show_all", 0,
                       " ");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        for (direction = 0; direction < 2; direction++) {
            for (channel_num = 0; channel_num < 4;
                 channel_num++) {
                bf_pltfm_rtmr_ctle_show (uc, port_num,
                                         channel_num, direction);
            }
        }
    }
    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_set_swap", 2,
                       " <port_id> <chan_num>");
    uint8_t port_num;
    uint8_t chan_num;

    port_num = atoi (uc->pargs->args[0]);
    chan_num = atoi (uc->pargs->args[1]);

    /* Check passed in values */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (chan_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number is out of range. Retimer channel are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_set_pol (port_num, chan_num);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap_all_chan__
(
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_set_swap_all_chan",
                       1, " <port_id>");

    bf_pltfm_status_t res = UCLI_STATUS_OK;
    uint8_t port_num;
    uint8_t chan_num;

    port_num = atoi (uc->pargs->args[0]);

    /* Check passed in values */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    for (chan_num = 0; chan_num < MAX_RTMR_CHAN;
         chan_num++) {
        res = bf_pltfm_rtmr_set_pol (port_num, chan_num);

        if (res != UCLI_STATUS_OK) {
            LOG_ERROR ("Error setting polarity for port: %d channel %d.\n",
                       port_num,
                       chan_num);
            /* Continue and try the rest of the channels */
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_set_swap_all", 0,
                       " ");

    return bf_pltfm_rtmr_set_pol_all();
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_rx_swap__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_set_rx_swap",
                       4,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)> "
                       "<swap (0=clear 1=set)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t swap;
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val = 0;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    swap = atoi (uc->pargs->args[3]) ? 1 : 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = RX_PRBS_CFG_REG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~ (RTMR_RX_SWAP_SET (1));

    /* Write in new settings */
    val = RTMR_RX_SWAP_SET (swap);
    reg_info.data = reg_info.data | val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_swap__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_set_tx_swap",
                       4,
                       " <port_id> <chan_num> <direction (0=ingress 1=egress)> "
                       "<swap (0=clear 1=set)>");

    uint8_t port_num;
    uint8_t channel_num;
    uint8_t direction;
    uint16_t swap;
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint16_t side_offset;
    uint16_t rtmr_lane;
    uint16_t val = 0;

    port_num = atoi (uc->pargs->args[0]);
    channel_num = atoi (uc->pargs->args[1]);
    direction = atoi (uc->pargs->args[2]);
    swap = atoi (uc->pargs->args[3]) ? 1 : 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if (channel_num >= MAX_RTMR_CHAN) {
        LOG_ERROR ("Channel number out of range. Retimer channels are <0-3>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((direction != RTMR_DIR_INGRESS) &&
        (direction != RTMR_DIR_EGRESS)) {
        LOG_ERROR ("Invalid value for direction. \n");
        return UCLI_STATUS_E_PARAM;
    }

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = channel_num;

    side_offset = direction ? 0 : MAX_RTMR_CHAN;

    rtmr_lane = channel_num + side_offset;

    reg_info.offset = TX_TEST_PATTERN_CFG |
                      RTMR_CHAN_ADDR_SET (rtmr_lane);
    reg_info.data = 0xff;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear fields being written to */
    reg_info.data &= ~ (RTMR_TX_SWAP_SET (1));

    /* Write in new settings */
    val = RTMR_TX_SWAP_SET (swap);
    reg_info.data = reg_info.data | val;

    res = bf_pltfm_rtmr_operation (&port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_set_port_mode__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc,
                       "rtmr_set_port_mode",
                       3,
                       " <port_id> "
                       "<speed (0=10G 1=25G 2=40G 3=40G_NB 4=50G 5=100G)> "
                       "<AN (0=disable 1=enable)>");

    uint8_t port_num;
    uint8_t speed;
    uint16_t an_enable;
    bf_pltfm_status_t res = UCLI_STATUS_OK;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_ext_phy_cfg_t port_cfg;

    port_num = atoi (uc->pargs->args[0]);
    speed = atoi (uc->pargs->args[1]) + BF_SPEED_10G;
    an_enable = atoi (uc->pargs->args[2]) ? 1 : 0;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    if ((speed < BF_SPEED_10G) ||
        (speed > BF_SPEED_100G)) {
        LOG_ERROR ("Invalid value for port speed. \n");
        return UCLI_STATUS_E_PARAM;
    }

    /* Read current value */
    port_info.conn_id = port_num;
    port_info.chnl_id = 0;

    port_cfg.speed_cfg = speed;
    port_cfg.is_an_on = an_enable;

    res = bf_pltfm_ext_phy_set_mode (&port_info,
                                     &port_cfg);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Failed to set mode for port: %d.\n",
                   port_num);
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reset__
(ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reset", 1,
                       " <port_id>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;

    port_num = atoi (uc->pargs->args[0]);

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    return bf_pltfm_rtmr_reset (&port_info);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_reset_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_reset_all", 0, " ");

    return bf_pltfm_rtmr_reset_all();
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_recover__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_recover", 1,
                       " <port_id>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;

    port_num = atoi (uc->pargs->args[0]);

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    /* Stop qsfp polling to prevent i2c conflict */
    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */

    bf_pltfm_rtmr_recover (&port_info);
    bf_pltfm_pm_qsfp_scan_poll_start();
    return 0;
}

static ucli_status_t bf_pltfm_rtmr_do_fw_write (
    uint8_t port_num,
    uint32_t index,
    uint32_t value)
{
    bf_pltfm_rtmr_reg_info_t reg_info;
    bf_pltfm_port_info_t port_info;

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    reg_info.offset = RTMR_FW_IND_OFFSET_REG;
    reg_info.data = (uint16_t)index;

    bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                             (void *)&reg_info);

    reg_info.offset = RTMR_FW_IND_VALUE_REG;
    reg_info.data = (uint16_t)value;

    bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                             (void *)&reg_info);

    reg_info.offset = RTMR_FW_IND_CTRL_REG;
    reg_info.data = (0xe << 12) | (0x20 & 0xff);

    bf_pltfm_rtmr_operation (&port_info, RTMR_WRITE,
                             (void *)&reg_info);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_fw_write__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_fw_write", 3,
                       " <port_id> <index> <value>");

    uint8_t port_num;
    uint32_t index;
    uint32_t value;

    port_num = atoi (uc->pargs->args[0]);
    index = (uint16_t)strtol (uc->pargs->args[1],
                              NULL, 16);
    value = (uint16_t)strtol (uc->pargs->args[2],
                              NULL, 16);

    return bf_pltfm_rtmr_do_fw_write (port_num, index,
                                      value);
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_fw_write_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_fw_write_all", 2,
                       " <index> <value>");

    ucli_status_t res;
    uint8_t port_num;
    uint32_t index;
    uint32_t value;

    index = (uint16_t)strtol (uc->pargs->args[0],
                              NULL, 16);
    value = (uint16_t)strtol (uc->pargs->args[1],
                              NULL, 16);

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        res = bf_pltfm_rtmr_do_fw_write (port_num, index,
                                         value);
        if (res) {
            /* Log error and continue */
            LOG_ERROR ("Error writing to fw register on port: %d\n",
                       port_num);
            continue;
        }
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_crc_show__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_crc_show", 1,
                       " <port_id>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint16_t crc;
    bf_pltfm_status_t res = 0;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    res = bf_pltfm_rtmr_crc_rd (&port_info, &crc);

    if (res != UCLI_STATUS_OK) {
        LOG_ERROR ("Error (%d) Failed to read crc from retimer port: %d.\n",
                   res,
                   port_num);
        return res;
    }

    aim_printf (&uc->pvs, "Port: %d : CRC: 0x%x\n",
                port_num, crc);
    return res;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_crc_show_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_crc_show_all", 0,
                       " ");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;
    uint16_t crc;
    bf_pltfm_status_t res = 0;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        res = bf_pltfm_rtmr_crc_rd (&port_info, &crc);
        if (res != UCLI_STATUS_OK) {
            LOG_ERROR (
                "Error (%d) Failed to read crc from port: %d.\n",
                res, port_num);
        }

        aim_printf (&uc->pvs, "Port: %d : CRC: 0x%x\n",
                    port_num, crc);
    }

    return res;
}

static char rtmr_dump_brdr[] =
    "-----------------------------------------------------------------"
    "-----------------------------------------------------------------"
    "--------------------------------------------------------------\n";
static char rtmr_dump_hdr[] =
    "|Chip,Port,Lane|  TX5, TX4, TX3, TX2, TX1 |  SD, RDY,  St  "
    "|CTLE,ExRchEn,ExRch,BIAS0,DAC,EYE | Dlta,  F1,   F2,   F3  | "
    "Cal1, Cal2, "
    "Cal3,FreqErr|CtleD|FIFOPtr|AN,AnFiH,AnFiLn|RSpd,TSpd|TSp,RSp|\n";

static ucli_status_t bf_pltfm_rtmr_serdes_dump (
    ucli_context_t *uc,
    bf_pltfm_port_info_t *port_info)
{
    bf_pltfm_rtmr_reg_info_t reg_info;
    uint8_t chan_num;
    uint8_t port_num;
    uint8_t chip_num;
    int16_t eye_margin;
    int16_t eye;
    uint16_t dac_sel;
    uint16_t c_offset;

    port_num = port_info->conn_id;
    chip_num = (port_num - 32);

    aim_printf (&uc->pvs, "%s", rtmr_dump_brdr);

    for (chan_num = 0; chan_num < 8; chan_num++) {
        aim_printf (&uc->pvs, "| %2d,  %2d,  ", chip_num,
                    port_num);
        aim_printf (&uc->pvs, "%2d | ", chan_num);

        /* TX5:8XA4[13:8] */
        reg_info.offset = ((uint16_t)0x00A4 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "%3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x3f),
                              6));

        /* TX4:8XA5[13:8] */
        reg_info.offset = ((uint16_t)0x00A5 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "%3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x3f),
                              6));

        /* TX3:8XA6[13:8] */
        reg_info.offset = ((uint16_t)0x00A6 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "%3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x3f),
                              6));

        /* TX2:8XA7[13:8] */
        reg_info.offset = ((uint16_t)0x00A7 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "%3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x3f),
                              6));

        /* TX1:8XA8[13:8] */
        reg_info.offset = ((uint16_t)0x00A8 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs,
                    "%3d  | ",
                    RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x3f),
                                      6));

        /* SD:8X2E[2] */
        reg_info.offset = ((uint16_t)0x002E |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 2) & 0x1));

        /* RDY:8X2E[3] */
        reg_info.offset = ((uint16_t)0x002E |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 3) & 0x1));

        /* ST: 8X0D[12:8] */
        reg_info.offset = ((uint16_t)0x000D |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %2d | ",
                    ((reg_info.data >> 8) & 0x1f));

        /* CTLE: 8X4E[13:11] */
        reg_info.offset = ((uint16_t)0x004E |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 11) & 0x7));

        /* ExRchEn: 8XDF[1] */
        reg_info.offset = ((uint16_t)0x00DF |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 1) & 0x1));

        /* ExRchVal: 8XDF[7:5] */
        reg_info.offset = ((uint16_t)0x00DF |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 5) & 0x7));

        /* BIAS0: 8XF2[6:4] */
        reg_info.offset = ((uint16_t)0x00F2 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    ((reg_info.data >> 4) & 0x7));

        /* DAC: 8X7F[15:12] */
        reg_info.offset = ((uint16_t)0x007F |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        dac_sel = RTMR_GET_DAC_SEL (reg_info.data);
        aim_printf (&uc->pvs, "0x%1x, ",
                    ((reg_info.data >> 12) & 0xf));

        /* EYE: 8X2A[11:0] */
        reg_info.offset = ((uint16_t)0x002A |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        eye_margin = RTMR_RX_EYE_MARGIN_GET (
                         reg_info.data);
        eye = RTMR_EYE_MARGIN_CALC (eye_margin, dac_sel);

        // aim_printf(&uc->pvs, "  %4d | ", ((reg_info.data) & 0x7fff));
        aim_printf (&uc->pvs, "  %4d | ", eye);

        /* DELTA: 8X0D[6:0] */
        reg_info.offset = ((uint16_t)0x000D |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data) & 0x7f), 7));

        /* F1: 8X2B[6:0] */
        reg_info.offset = ((uint16_t)0x002B |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data) & 0x7f), 7));

        /* F2: 8X2C[14:8] */
        reg_info.offset = ((uint16_t)0x002C |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x7f),
                              7));

        /* F3: 8X2C[6:0] */
        reg_info.offset = ((uint16_t)0x002C |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d | ",
            RTMR_TWO_CMP_BIT (((reg_info.data) & 0x7f), 7));

        /* Cal1: 8X71[14:8] */
        reg_info.offset = ((uint16_t)0x0071 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x7f),
                              7));

        /* Cal2: 8X71[6:0] */
        reg_info.offset = ((uint16_t)0x0071 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data) & 0x7f), 7));

        /* Cal3: 8X72[14:8] */
        reg_info.offset = ((uint16_t)0x0072 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, " %3d, ",
            RTMR_TWO_CMP_BIT (((reg_info.data >> 8) & 0x7f),
                              7));

        /* FreqErr: 8X73[10:0] */
        reg_info.offset = ((uint16_t)0x0073 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (
            &uc->pvs, "%5d |",
            RTMR_TWO_CMP_BIT (((reg_info.data) & 0x7ff), 11));

        /* CTLE Done: 0x9811 */
        reg_info.offset = ((uint16_t)0x9811);
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %d  |",
                    (((reg_info.data) >> chan_num) & 0x1));

        if (chan_num < 4) {
            /* Read 9805 */
            reg_info.offset = (uint16_t)0x9805;
            reg_info.data = 0xff;

            bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                     (void *)&reg_info);

            /* FIFO Pointers 9805[15:13] */
            aim_printf (&uc->pvs,
                        " 0x8%1x%02x|",
                        chan_num,
                        (((reg_info.data) >> (13 - (3 * chan_num))) &
                         0x7));

        } else {
            /* Read 9806 */
            reg_info.offset = (uint16_t)0x9806;
            reg_info.data = 0xff;

            bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                     (void *)&reg_info);

            /* FIFO Pointers 9806[15:13] */
            aim_printf (&uc->pvs,
                        " 0x8%1x%02x|",
                        chan_num,
                        (((reg_info.data) >> (13 - (3 * (chan_num - 4))))
                         & 0x7));
        }

        /* AN mode */

        c_offset = 0x2;
        c_offset |= ((chan_num % 4) << 4);
        reg_info.offset = (uint16_t)0x6000 + c_offset;
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, " %1d, ",
                    (! (((reg_info.data) >> 15) & 0x1)));

        /* AN Finish host */

        c_offset = 0x9;
        c_offset |= ((chan_num % 4) << 4);
        reg_info.offset = (uint16_t)0x6000 + c_offset;
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "  %1d, ",
                    (((reg_info.data) >> 14) & 0x1));

        /* AN Finish line */

        aim_printf (&uc->pvs, "  %1d,  |",
                    (((reg_info.data) >> 13) & 0x1));

        /* link speed 10G or 25G */

        reg_info.offset = (uint16_t)0x9813;
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        /* Rx link speed */
        aim_printf (&uc->pvs,
                    "  %2d,",
                    (((reg_info.data >> (15 - chan_num)) & 0x1) ? 10 :
                     25));

        /* Tx link speed */
        aim_printf (&uc->pvs,
                    " %2d ",
                    (((reg_info.data >> (7 - chan_num)) & 0x1) ? 10 :
                     25));

        /* TX Pol Swap */
        reg_info.offset = ((uint16_t)0x00A0 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, "|  %1d,",
                    ((reg_info.data >> 7) & 0x1));

        /* RX Pol Swap */
        reg_info.offset = ((uint16_t)0x0061 |
                           RTMR_REG_BASE | (chan_num << 8));
        reg_info.data = 0xff;

        bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                                 (void *)&reg_info);

        aim_printf (&uc->pvs, " %1d",
                    ((reg_info.data >> 14) & 0x1));

        aim_printf (&uc->pvs, " |\n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_serdes_dump__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_serdes_dump", 1,
                       " <port_id>");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;

    port_num = atoi (uc->pargs->args[0]);

    /* Check values passed in */
    if ((port_num < MIN_MVR_RTMR_PORTS) ||
        (port_num > MAX_MVR_RTMR_PORTS)) {
        LOG_ERROR ("Port number is out of range. Retimer ports are <33-64>\n");
        return UCLI_STATUS_E_PARAM;
    }

    aim_printf (&uc->pvs, "%s", rtmr_dump_brdr);
    aim_printf (&uc->pvs, "%s", rtmr_dump_hdr);

    port_info.conn_id = port_num;
    port_info.chnl_id = 0xff;

    bf_pltfm_rtmr_serdes_dump (uc, &port_info);

    aim_printf (&uc->pvs, "%s", rtmr_dump_brdr);

    return 0;
}

static ucli_status_t
bf_pltfm_rtmr_ucli_ucli__rtmr_serdes_dump_all__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "rtmr_serdes_dump_all", 0,
                       " ");

    bf_pltfm_port_info_t port_info;
    uint8_t port_num;

    for (port_num = MIN_MVR_RTMR_PORTS;
         port_num <= MAX_MVR_RTMR_PORTS;
         port_num++) {
        if (((port_num - MIN_MVR_RTMR_PORTS) % 5) == 0) {
            aim_printf (&uc->pvs, "%s", rtmr_dump_brdr);
            aim_printf (&uc->pvs, "%s", rtmr_dump_hdr);
        }
        port_info.conn_id = port_num;
        port_info.chnl_id = 0xff;

        bf_pltfm_rtmr_serdes_dump (uc, &port_info);
    }

    aim_printf (&uc->pvs, "%s", rtmr_dump_brdr);

    return 0;
}

static ucli_command_handler_f
bf_pltfm_rtmr_ucli_ucli_handlers__[] = {
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_fware_dl__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_fware_dl_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chk_fware_dl__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chk_fware_dl_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_fw_write__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_fw_write_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_host_lb__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_host_lb_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_line_lb__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_line_lb_all__,

    //    bf_pltfm_rtmr_ucli_ucli__rtmr_dfe_set__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_dfe_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_eye_margin_show_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_set_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_prbs_clr_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_set_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_clr_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_show_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_port__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_chkr_err_cnt_clear_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_3__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_2__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_post_1__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_main__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_precursor__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_3__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_2__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_post_1__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_main__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_show_tx_precursor__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_set__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_set_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_recover__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_ctle_show_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_offset_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_offset_dir_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_offset_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_all_reg__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_show_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reg_set_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap_all_chan__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_swap_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_tx_swap__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_rx_swap__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_set_port_mode__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reset__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_reset_all__,

    bf_pltfm_rtmr_ucli_ucli__rtmr_crc_show__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_crc_show_all__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_serdes_dump__,
    bf_pltfm_rtmr_ucli_ucli__rtmr_serdes_dump_all__,
    NULL
};

static ucli_module_t bf_pltfm_rtmr_ucli_module__
= {
    "bf_pltfm_rtmr_ucli", NULL, bf_pltfm_rtmr_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_rtmr_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    bf_pltfm_board_id_t rtmr_bd_id;

    rtmr_bd_id = bf_pltfm_rtmr_bd_id_get();

    /* Retimer is only present on P0C boards */
    if (rtmr_bd_id != BF_PLTFM_BD_ID_MAVERICKS_P0C) {
        return NULL;
    }
    ucli_module_init (&bf_pltfm_rtmr_ucli_module__);
    n = ucli_node_create ("rtmr", m,
                          &bf_pltfm_rtmr_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("rtmr"));
    return n;
}
