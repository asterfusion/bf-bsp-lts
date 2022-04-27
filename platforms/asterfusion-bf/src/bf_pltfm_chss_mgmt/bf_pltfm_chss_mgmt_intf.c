/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_intf.c
 * @date
 *
 * API's for reading temperature from BMC
 *
 */

#include <stdio.h>
#include "sys/types.h"
#include "ifaddrs.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_uart.h>
#include <bf_pltfm_syscpld.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm.h>

#include "pltfm_types.h"

// Local header files
#include "bf_pltfm_bd_eeprom.h"

#define lqe_valen  256

COME_type global_come_type = COME_UNKNOWN;

/* COME_type as the index. */
const char *cme_desc[] = {
    "Unknown",
    "CME3000",
    "CME7000",
    "CGT1508",
    "CGT1527"
};

char bmc_i2c_bus = 0;
unsigned char bmc_i2c_addr = 0x3e;

static void bf_pltfm_parse_i2c (const char *str,
                                int l, char *c)
{
    int i = 0;

    for (i = 0; i < l; i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (isdigit (str[i])) {
            *c = str[i];
            break;
        }
    }
}

static int my_strcmp (char *p, char *standard)
{
    char *start, *end;
    start = p;
    end = p + strlen (p) - 1;
    while (*start == ' ') {
        start ++;
    }
    if (*end == '\n') {
        end --;
    }
    while (*end == ' ') {
        end --;
    }
    * (end + 1) = '\0';
    return strcasecmp (start, standard);
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_init()
{
    // Initialize all the sub modules
    FILE *fp = NULL;
    const char *cfg = "/etc/platform.conf";
    char entry[lqe_valen] = {0};
    int i2c = 0;

    bmc_i2c_bus = 0;

    fp = fopen (cfg, "r");
    if (!fp) {
        fprintf (stdout,
                 "fopen(%s) : "
                 "%s\n", cfg, strerror (errno));
        return -1;
    } else {
        while (fgets (entry, lqe_valen, fp)) {
            char *p;
            if (entry[0] == '#') {
                continue;
            }
            /* Find Master I2C */
            p = strstr (entry, "i2c:");
            if (p) {
                p += 4;
                char c = '0';
                bf_pltfm_parse_i2c (p, strlen (p), &c);
                i2c = c - '0';
            }

            /* Find other ... */
            p = strstr (entry, "com-e:");
            if (p) {
                p += 6;
                if (!my_strcmp (p, "CME3000")) {
                    global_come_type = CME3000;
                }
                if (!my_strcmp (p, "CME7000")) {
                    global_come_type = CME7000;
                }
                if (!my_strcmp (p, "CG1508")) {
                    global_come_type = CG1508;
                }
                if (!my_strcmp (p, "CG1527")) {
                    global_come_type = CG1527;
                }
            }

            /* Make sure that the UART configuration is disabled in /etc/platform.conf
             * when current platform is X312P-T.
             * by tsihang, 2022-04-27. */
            p = strstr (entry, "uart:");
            if (p) {
                p += 5;
                for (int i = 0; i < (int)strlen (p) - 1; i ++) {
                    uart_ctx.dev[i] = p[i];
                }
                /* Identify to open UART. */
                uart_ctx.flags |= AF_PLAT_UART_ENABLE;
            }

            memset (&entry[0], 0, lqe_valen);
        }
        fclose (fp);
    }

    bmc_i2c_bus = i2c;
    fprintf (stdout,
             "COME : %s : I2C : %2d (%s)\n",
             cme_desc[global_come_type], bmc_i2c_bus,
             is_CG15XX() ? "out of duty" : "on duty");

    if (bf_pltfm_master_i2c_init()) {
        LOG_ERROR ("pltfm_mgr: Error in master-i2c init \n");
        return BF_PLTFM_COMM_FAILED;
    }

    if (bf_pltfm_uart_init ()) {
        LOG_ERROR ("pltfm_mgr: Error in UART init \n");
        return BF_PLTFM_COMM_FAILED;
    }

    /* How to do a self-detect to get eeprom automatically ?
     * Getting EEPROM data through i2c channel or through uart
     * which is not very clear when a platform given at this moment.
     * As the situation so far, showing us a i2c channel in /etc/platform.conf
     * is still an appropriate way, while which even though is not a best one,
     * to cover many scenario of deployment. And it may reduce lots of dirty code.
     *
     * by tsihang, 2022-04-20.
     */
    if (bf_pltfm_bd_type_init()) {
        LOG_ERROR ("pltfm_mgr: Failed to initialize EEPROM library\n");
        return BF_PLTFM_COMM_FAILED;
    }

    // Other initializations(Fan, etc.) go here


    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_bd_type_get (
    bf_pltfm_board_id_t *board_id)
{
    return bf_pltfm_bd_type_get (board_id);
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_platform_type_get (
    uint8_t *type, uint8_t *subtype)
{
    return bf_pltfm_platform_type_get (type, subtype);
}

bf_pltfm_status_t bf_pltfm_device_type_get (
    bf_dev_id_t dev_id,
    bool *is_sw_model)
{
    /* This func returns the device type based on compile time flags */
#ifdef DEVICE_IS_SW_MODEL
    *is_sw_model = true;
#else
    *is_sw_model = false;
#endif

    return BF_PLTFM_SUCCESS;
}

