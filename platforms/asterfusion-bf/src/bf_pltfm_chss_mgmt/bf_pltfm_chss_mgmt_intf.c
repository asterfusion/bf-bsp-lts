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

static void bf_pltfm_parse_subversion (const char *subver,
    uint8_t *subtype, bool *find) {
    *find = true;
    *subtype = INVALID_SUBTYPE;

    if (strstr (subver, "1.0")) {
        *subtype = v1dot0;
    } else if (strstr (subver, "1.1")) {
        *subtype = v1dot1;
    } else if (strstr (subver, "1.2")) {
        *subtype = v1dot2;
    } else if (strstr (subver, "2.0")) {
        *subtype = v2dot0;
    } else if (strstr (subver, "2.1")) {
        *subtype = v2dot1;
    } else if (strstr (subver, "2.2")) {
        *subtype = v2dot2;
    } else if (strstr (subver, "3.0")) {
        *subtype = v3dot0;
    } else if (strstr (subver, "3.1")) {
        *subtype = v3dot1;
    } else if (strstr (subver, "3.2")) {
        *subtype = v3dot2;
    } else if (strstr (subver, "4.0")) {
        *subtype = v4dot0;
    } else {
        /* TBD: */
        *find = false;
    }
}

static void bf_pltfm_parse_platorm (const char *str,
                                size_t l)
{
    int i;
    char *c = NULL;
    bool find = true;
    uint8_t type = INVALID_TYPE;
    uint8_t subtype = INVALID_SUBTYPE;

    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (isdigit (str[i]) || isalpha (str[i])) {
            c = (char *)&str[i];
            break;
        }
    }

    if (!c) {
        return;
    }

    bf_pltfm_bd_type_get (
        &type, &subtype);

    if (strstr (c, "532")) {
        type = X532P;
    } else if (strstr (c, "564")) {
        type = X564P;
    } else if (strstr (c, "308")) {
        type = X308P;
    } else if (strstr (c, "312")) {
        type = X312P;
    } else if (strstr (c, "hc")) {
        type = HC;
    } else {
        find = false;
    }

    if (find) {
        bf_pltfm_bd_type_set (
            type, subtype);
    } else {
        fprintf (stdout,
                 "Exit due to the type of current platform is not given\n");
        LOG_ERROR(
                 "Exit due to the type of current platform is not given\n");
        exit (0);
    }
}

static void bf_pltfm_parse_hwversion (const char *str,
                                size_t l)
{
    int i;
    char *c = NULL;
    uint8_t type = INVALID_TYPE;
    uint8_t subtype = INVALID_SUBTYPE;
    bool find = true;

    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (isdigit (str[i]) || isalpha (str[i])) {
            c = (char *)&str[i];
            break;
        }
    }

    if (!c) {
        return;
    }

    bf_pltfm_bd_type_get (
        &type, &subtype);

    bf_pltfm_parse_subversion (c, &subtype, &find);

    /* Overwrite for x312p-t. */
    if (platform_type_equal (X312P)) {
        /* 0x26 and 0x27. */
        if (subtype != v3dot0 &&
            subtype != v3dot1 &&
            subtype != v3dot2 &&
            subtype != v3dot3 &&
            subtype != v4dot0) {
            subtype = v2dot0;
            fprintf (stdout,
                     "WARNNING: Overwrite %02x's subversion to %02x\n", type, subtype);
            LOG_ERROR(
                     "WARNNING: Overwrite %02x's subversion to %02x\n", type, subtype);
        }
    } else if (platform_type_equal (X564P)) {
        /* 0x31. */
        if (subtype != v1dot1 &&
            subtype != v1dot2) {
            find = false;
        }
    } else if (platform_type_equal (X532P)) {
        /* 0x31. */
        if (subtype != v1dot0 &&
            subtype != v1dot1) {
            find = false;
        }
    } else if (platform_type_equal (X308P)) {
        /* 0x31. */
        if (subtype != v1dot0 &&
            subtype != v2dot0) {
            find = false;
        }
    } else if (platform_type_equal (HC)) {
        /* 0x31. */
        /* TBD */
    } else {
        find = false;
    }

    fprintf (stdout,
             "Type : %02x : %02x\n", type, subtype);

    if (find) {
        bf_pltfm_bd_type_set (
            type, subtype);
    } else {
        fprintf (stdout,
                 "Exit due to the subversion of current platform is not given\n");
        LOG_ERROR(
                 "Exit due to the subversion of current platform is not given\n");
        exit (0);
    }
}

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

    if (!c) {
        return;
    }

    /* I2C MUST be disabled or set to 127 for those platforms which are not used it in /etc/platform.conf. */
    bmc_i2c_bus = atoi(c);
    if (bmc_i2c_bus != 0x7F)
    fprintf (stdout,
             "I2C  : %d (CPLD or BMC)\n", bmc_i2c_bus);
}

static void bf_pltfm_parse_cme(const char *str,
                                  size_t l)
{
    int i;
    char *c = NULL;
    struct x86_carrier_board_t *cb = NULL;

    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (isdigit (str[i]) || isalpha (str[i])) {
            c = (char *)&str[i];
            break;
        }
    }

    if (!c) {
        return;
    }

    foreach_element(0, ARRAY_LENGTH(x86_cb)) {
        cb = &x86_cb[each_element];
        //fprintf(stdout, "%s : %s\n", str, cb->desc);
        if (strstr (c, cb->desc)) {
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
    char *c = NULL;

    BUG_ON (str == NULL);

    for (i = 0; i < (int)(l - 1); i ++) {
        if (isspace (str[i])) {
            continue;
        }
        if (str[i] == '/') {
            c = (char *)&str[i];
            break;
        }
    }

    if (!c) {
        return;
    }

    for (i = 0; i < (int)(l - 1); i ++) {
        uart_ctx.dev[i] = c[i];
    }
    uart_ctx.dev[i] = '\0';

    /* Identify to open UART. */
    uart_ctx.flags |= AF_PLAT_UART_ENABLE;
    fprintf (stdout,
             "UART : %s\n", (uart_ctx.flags & AF_PLAT_UART_ENABLE) ? "enabled (BMC)" : "disabled");
}

static void access_cpld_through_cp2112()
{
    /* Access CPLD through CP2112 */
    uint8_t rd_buf[128] = {0};
    uint8_t cmd = 0x0F;
    uint8_t wr_buf[2] = {0x01, 0xAA};
    fprintf (stdout, "CPLD <- CP2112\n");
    bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                            2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US * 2);
    g_access_cpld_through_cp2112 = true;
}

static void access_cpld_through_superio()
{
    /* Access CPLD through SIO */
    uint8_t rd_buf[128] = {0};
    uint8_t cmd = 0x0F;
    uint8_t wr_buf[2] = {0x02, 0xAA};
    /* An error occured while launching ASIC: bf_pltfm_uart/bf_pltfm_uart.c[260], read(Resource temporarily unavailable).
    * Does the cmd <0x0F> need a return from BMC ? If not so, update func no_return_cmd (called by bf_pltfm_bmc_uart_write_read)
    * to tell the caller there's no need to wait for the return status.
    * Haven't see bad affect so far. Keep tracking.
    * by tsihang, 2022-06-20. */
    fprintf (stdout, "CPLD <- SuperIO\n");
    bf_sys_usleep (100);
    bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                            2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US * 3);
    g_access_cpld_through_cp2112 = false;
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_init()
{
    // Initialize all the sub modules
    FILE *fp = NULL;
    const char *cfg = "/etc/platform.conf";
    char entry[lqe_valen] = {0};
    uint8_t type = INVALID_TYPE;
    uint8_t subtype = INVALID_SUBTYPE;

    bf_pltfm_bd_type_set (type, subtype);

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

            p = strstr (entry, "platform:");
            if (p) {
                /* Find Platform */
                bf_pltfm_parse_platorm (p + 9, strlen (p + 9));
            }
            p = strstr (entry, "hwver:");
            if (p) {
                /* Find HW Version */
                bf_pltfm_parse_hwversion (p + 6, strlen (p + 6));
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

    bf_pltfm_bd_type_get (&type, &subtype);
    if (((type == INVALID_TYPE) || (subtype == INVALID_SUBTYPE)) ||
        bf_pltfm_bd_type_set_by_key(type, subtype)) {
        /* Must never reach here.
         * If so, that means there may be a risk to run bsp to init platform.
         * by tsihang, 2022/08/01. */
        LOG_ERROR ("WARNING: No valid value in EEPROM(0x26/0x27/0x31) to identify current platform.\n");
        LOG_ERROR ("pltfm_mgr: Error in detecting platform.\n");
        return BF_PLTFM_COMM_FAILED;
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

    if (platform_type_equal (X312P)) {
        if (platform_subtype_equal (v2dot0)) {
            fprintf (stdout, "CPLD <- cp2112\n");
        } else if (platform_subtype_equal (v3dot0) ||
                   platform_subtype_equal (v4dot0)) {
            fprintf (stdout, "CPLD <- super io\n");
        }
    } else if (platform_type_equal (X308P)) {
        if (is_HVXXX) {
            access_cpld_through_cp2112();
        }
    } else if (platform_type_equal (X532P)) {
        if (is_CG15XX) {
            fprintf (stdout, "CPLD <- cgolx\n");
        } else if (is_ADV15XX || is_S02XXX) {
            access_cpld_through_cp2112();
        } else if (is_HVXXX) {
            access_cpld_through_superio();
        }
    } else if (platform_type_equal (X564P)) {
        if (is_CG15XX) {
            fprintf (stdout, "CPLD <- cgolx\n");
        } else if (is_ADV15XX || is_S02XXX) {
            access_cpld_through_cp2112();
        } else if (is_HVXXX) {
            access_cpld_through_superio();
        }
    }

    // Other initializations(Fan, etc.) go here

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_bd_type_get (
    bf_pltfm_board_id_t *board_id)
{
    return bf_pltfm_bd_type_get_ext (board_id);
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_platform_type_get (
    uint8_t *type, uint8_t *subtype)
{
    return bf_pltfm_bd_type_get (type, subtype);
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

