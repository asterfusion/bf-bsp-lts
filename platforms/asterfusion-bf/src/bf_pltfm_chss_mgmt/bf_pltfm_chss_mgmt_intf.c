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
bool g_access_cpld_through_cp2112 = false;

static struct x86_carrier_board_t x86_cb[] = {
    {"Unknown",  COME_UNKNOWN},
    {"CME3000",  CME3000},
    {"CME7000",  CME7000},
    {"CG1508",   CG1508},
    {"CG1527",   CG1527},
    {"ADV1508",  ADV1508},
    {"ADV1527",  ADV1527},
    {"S021508",  S021508},
    {"S021527",  S021527},
};

char bmc_i2c_bus = 0x7F;
unsigned char bmc_i2c_addr = 0x3e;

static void bf_pltfm_parse_i2c (const char *str,
                                size_t l)
{
    int i = 0;
    char *c = NULL;

    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (isdigit (str[i])) {
            c = (char *)&str[i];
            break;
        }
    }
    if (c) {
        /* I2C MUST be disabled or set to 127 for those platforms which are not used it in /etc/platform.conf. */
        bmc_i2c_bus = atoi(c);
        if (bmc_i2c_bus != 0x7F)
        fprintf (stdout,
                 "I2C  : %d (CPLD or BMC)\n", bmc_i2c_bus);
    }

}

static void bf_pltfm_parse_cme(const char *str,
                                  size_t l)
{
    struct x86_carrier_board_t *cb = NULL;

    BUG_ON (str == NULL);

    foreach_element(0, ARRAY_LENGTH(x86_cb)) {
        cb = &x86_cb[each_element];
        //fprintf(stdout, "%s : %s\n", str, cb->desc);
        if (strstr (str, cb->desc)) {
            global_come_type = cb->type;
            break;
        }
    }
    BUG_ON (cb == NULL);
    fprintf (stdout,
             "COME : %s \n", cb->desc);
}

static void bf_pltfm_parse_uart(const char *str,
                                  size_t l)
{
    int i = 0;
    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        uart_ctx.dev[i] = str[i];
    }
    uart_ctx.dev[i] = '\0';

    /* Identify to open UART. */
    uart_ctx.flags |= AF_PLAT_UART_ENABLE;
    fprintf (stdout,
             "UART : %s\n", (uart_ctx.flags & AF_PLAT_UART_ENABLE) ? "enabled (BMC)" : "disabled");
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_init()
{
    // Initialize all the sub modules
    FILE *fp = NULL;
    const char *cfg = "/etc/platform.conf";
    char entry[lqe_valen] = {0};

    bf_pltfm_bd_type_set (UNKNOWM_PLATFORM, v1dot0);

    fp = fopen (cfg, "r");
    if (!fp) {
        fprintf (stdout,
                 "fopen(%s) : "
                 "%s\n", cfg, strerror (errno));
        return -1;
    } else {
        fprintf (stdout,
                 "\n\n================== %s ==================\n", cfg);
        while (fgets (entry, lqe_valen, fp)) {
            char *p;
            if (entry[0] == '#') {
                continue;
            }

            p = strstr (entry, "i2c:");
            if (p) {
                /* Find Master I2C */
                bf_pltfm_parse_i2c (p + 4, strlen (p + 4));
            }
            p = strstr (entry, "com-e:");
            if (p) {
                /* Find X86 */
                bf_pltfm_parse_cme (p + 6, strlen (p + 6));
            }
            p = strstr (entry, "uart:");
            if (p) {
                /* Make sure that the UART configuration is disabled in /etc/platform.conf
                 * when current platform is X312P-T.
                 * by tsihang, 2022-04-27. */
                bf_pltfm_parse_uart (p + 5, strlen (p + 5));
            }
            memset (&entry[0], 0, lqe_valen);
        }
        fclose (fp);
    }

    if (bf_pltfm_master_i2c_init ()) {
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

    /* As we have known the platform, do more i2c init case by case.
     * by tsihang, 2022-06-24. */
    if (bf_pltfm_master_i2c_init ()) {
        LOG_ERROR ("pltfm_mgr: Error in master-i2c reinit \n");
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

