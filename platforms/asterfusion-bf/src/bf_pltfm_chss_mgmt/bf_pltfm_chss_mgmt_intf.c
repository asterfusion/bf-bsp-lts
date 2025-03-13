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
#include <bf_pltfm_spi.h>
#include <bf_pltfm_syscpld.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>

// Local header files
#include "bf_pltfm_bd_eeprom.h"

#define lqe_valen  256

extern bf_pltfm_status_t bf_pltfm_bd_type_set_by_key (
    uint8_t type, uint8_t subtype);

COME_type global_come_type = COME_UNKNOWN;
static char g_bmc_version[32] = {0};

static struct x86_carrier_board_t x86_cb[] = {
    {"Unknown",  COME_UNKNOWN},
    {"CME3000",  CME3000},
    {"CME7000",  CME7000},
    {"CG1508",   CG1508},
    {"CG1527",   CG1527},
    {"ADV1508",  ADV1508},
    {"ADV1527",  ADV1527},
    {"ADV1548",  ADV1548},
    {"S021508",  S021508},
    {"S021527",  S021527},
};

#define INVALID_BMC_I2C 0xFF
/* A value 0x7F means a transitional scenarios for platforms with CG15xx using cgosdrv. */
uint8_t bmc_i2c_bus = INVALID_BMC_I2C;
unsigned char bmc_i2c_addr = 0x3e;

static uint32_t bmc_comm_interval_us_for_312 = BMC_COMM_INTERVAL_US;

/* global */
pltfm_mgr_info_t pltfm_mgr_info = {
    .np_name = "pltfm_mgr",
    .health_mntr_t_id = 0,
    .np_onlp_mntr_name = "pltfm_mgr_onlp",
    .onlp_mntr_t_id = 0,
    .flags = 0,

    .pltfm_type = INVALID_TYPE,
    .pltfm_subtype = INVALID_SUBTYPE,

    .psu_count = 0,
    .sensor_count = 0,
    .fan_group_count = 0,
    .fan_per_group = 0,
    .cpld_count = 0,
};

typedef struct pltfm_cpld_path_t {
    bf_pltfm_type platfm;
    bf_pltfm_subtype ver;
    cpld_path_e default_path;
    cpld_path_e forced_path;
} pltfm_cpld_path_t;

/* There is an issue of cgoslx hung on for CG15xx COM-Express
 * when switching installed system from SONiC to ONL or from ONL to SONiC.
 * It is strongly recommended to access CPLD through cp2112 on X532P under all possible COM-Express.
 * Pls upgrade BMC to v1.2.1 or later. */
static pltfm_cpld_path_t pltfm_cpld_path[] = {
    {AFN_X564PT, V1P0, VIA_CGOS,   VIA_CGOS},
    {AFN_X564PT, V1P1, VIA_CGOS,   VIA_CGOS},
    {AFN_X564PT, V1P2, VIA_CGOS,   VIA_CP2112},
    {AFN_X564PT, V2P0, VIA_CP2112, VIA_CP2112},
    {AFN_X532PT, V1P0, VIA_CGOS,   VIA_CP2112},
    {AFN_X532PT, V1P1, VIA_CGOS,   VIA_CP2112},
    {AFN_X532PT, V2P0, VIA_CP2112, VIA_CGOS},
    {AFN_X532PT, V3P0, VIA_CP2112, VIA_CGOS},
    {AFN_X308PT, V1P0, VIA_CP2112, VIA_CGOS},
    {AFN_X308PT, V1P1, VIA_CP2112, VIA_CGOS},
    {AFN_X308PT, V2P0, VIA_CP2112, VIA_CGOS},
    {AFN_X308PT, V3P0, VIA_CP2112, VIA_CGOS},
    {AFN_X732QT, V1P0, VIA_CP2112, VIA_CP2112},
    {AFN_X732QT, V1P1, VIA_CP2112, VIA_CP2112},
};

pltfm_mgr_info_t *bf_pltfm_mgr_ctx()
{
    return &pltfm_mgr_info;
}

static void bf_pltfm_parse_subversion (const char *subver,
    uint8_t *subtype, bool *find) {
    *find = true;
    *subtype = INVALID_SUBTYPE;

    if (strstr (subver, "1.0")) {
        *subtype = V1P0;
    } else if (strstr (subver, "1.1")) {
        *subtype = V1P1;
    } else if (strstr (subver, "1.2")) {
        *subtype = V1P2;
    } else if (strstr (subver, "2.0")) {
        *subtype = V2P0;
    } else if (strstr (subver, "2.1")) {
        *subtype = V2P1;
    } else if (strstr (subver, "2.2")) {
        *subtype = V2P2;
    } else if (strstr (subver, "3.0")) {
        *subtype = V3P0;
    } else if (strstr (subver, "3.1")) {
        *subtype = V3P1;
    } else if (strstr (subver, "3.2")) {
        *subtype = V3P2;
    } else if (strstr (subver, "4.0")) {
        *subtype = V4P0;
    } else if (strstr (subver, "5.0")) {
        *subtype = V5P0;
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

    bf_pltfm_bd_type_get_priv (
        &type, &subtype);

    if (strstr (c, "532")) {
        type = AFN_X532PT;
    } else if (strstr (c, "564")) {
        type = AFN_X564PT;
    } else if (strstr (c, "308")) {
        type = AFN_X308PT;
    } else if (strstr (c, "312")) {
        type = AFN_X312PT;
    } else if (strstr (c, "732")) {
        type = AFN_X732QT;
    } else if (strstr (c, "hc")) {
        type = AFN_HC36Y24C;
    } else if (strstr (c, "3056")) {
        type = AFN_X308PT;
    } else {
        find = false;
    }

    if (find) {
        bf_pltfm_bd_type_set_priv (
            type, subtype);
    } else {
        fprintf (stdout,
                 "Exiting due to the type of current platform is unrecognized\n");
        LOG_ERROR(
                 "Exiting due to the type of current platform is unrecognized\n");
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

    bf_pltfm_bd_type_get_priv (
        &type, &subtype);

    bf_pltfm_parse_subversion (c, &subtype, &find);

    /* Overwrite subversion to 3.x when not given in eeprom.
     * Overwrite subversion to 3.x as it is widely shipped for x312p-t. */
    if (platform_type_equal (AFN_X312PT)) {
        /* 0x26 and 0x27. */
        if (subtype != V2P0 && subtype != V2P1 && subtype != V2P2 && subtype != V2P3 &&
            subtype != V3P0 && subtype != V3P1 && subtype != V3P2 && subtype != V3P3 &&
            subtype != V4P0 && subtype != V5P0) {
            subtype = V3P0;
            fprintf (stdout,
                     "WARNNING: Overwrite %02x's subversion to %02x\n", type, subtype);
            LOG_WARNING(
                     "WARNNING: Overwrite %02x's subversion to %02x\n", type, subtype);
        }
    } else if (platform_type_equal (AFN_X564PT)) {
        /* 0x31. */
        if (subtype != V1P1 &&
            subtype != V1P2 &&
            subtype != V2P0) {
            find = false;
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        /* 0x31. */
        if (subtype != V1P0 &&
            subtype != V1P1 &&
            subtype != V2P0 &&
            subtype != V3P0) {
            find = false;
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        /* 0x31. */
        if (subtype != V1P0 &&
            subtype != V1P1 &&
            subtype != V2P0 &&
            subtype != V3P0) {
            find = false;
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        /* 0x31. */
        if (subtype != V1P0 &&
            subtype != V1P1) {
            find = false;
        }
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        /* 0x31. */
        /* TBD */
    } else {
        find = false;
    }

    if (find) {
        bf_pltfm_bd_type_set_priv (
            type, subtype);
    } else {
        fprintf (stdout,
                 "Exiting due to the sub version of current platform is unrecognized\n");
        LOG_ERROR(
                 "Exiting due to the sub version of current platform is unrecognized\n");
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
}

static void access_cpld_through_cp2112()
{
    /* Access CPLD through CP2112 */
    uint8_t rd_buf[128] = {0};
    uint8_t cmd = 0x0F;
    uint8_t wr_buf[2] = {0x01, 0xAA};
    LOG_DEBUG ("CPLD <- CP2112\n");
    bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                            2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US * 2);
    bf_pltfm_mgr_ctx()->flags |= AF_PLAT_CTRL_CPLD_CP2112;
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
    LOG_DEBUG ("CPLD <- SuperIO\n");
    bf_sys_usleep (100);
    bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                            2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US * 3);
    bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_CTRL_CPLD_CP2112;
}

bf_pltfm_status_t bf_pltfm_get_bmc_ver(char *bmc_ver, bool forced) {
    uint8_t rd_buf[128] = {0};
    uint8_t cmd = 0x0D;
    uint8_t wr_buf[5] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    int ret;

    /* Read cached BMC version to caller. */
    if ((!forced) && (g_bmc_version[0] != 0)) {
        memcpy (bmc_ver, g_bmc_version, 32);
        return 0;
    }

    if (platform_type_equal (AFN_X312PT)) {
        cmd = 0x11;
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       cmd, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       bmc_comm_interval_us_for_312);
        if (ret == 5) {
            /* Write to cache. */
            sprintf (g_bmc_version, "v%x.%x.%x-%s",
                rd_buf[1], rd_buf[2], rd_buf[3], rd_buf[4] == 0x4F ? "offical" : "beta");
        } else {
            sprintf (g_bmc_version, "%s", "N/A");
        }
    } else {
        if (platform_type_equal (AFN_X532PT) ||
            platform_type_equal (AFN_X564PT) ||
            platform_type_equal (AFN_X308PT) ||
            platform_type_equal (AFN_X732QT)) {
            cmd = 0x0D;
            wr_buf[0] = 0xAA;
            wr_buf[1] = 0xAA;
        } else {
            /* Not Defined. */
        }
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
            ret = bf_pltfm_bmc_uart_write_read (
                                        cmd, wr_buf, 2, rd_buf, 128 - 1,
                                        BMC_COMM_INTERVAL_US * 3);
            if (ret == 4) {
                /* Write to cache. */
                sprintf (g_bmc_version, "v%x.%x.%x", rd_buf[1], rd_buf[2], rd_buf[3]);
            } else {
                sprintf (g_bmc_version, "%s", "N/A");
            }
        } else {
            /* Early Hardware with I2C interface. */
        }
    }
    memcpy (bmc_ver, g_bmc_version, 32);

    return 0;
}

bf_pltfm_status_t bf_pltfm_compare_bmc_ver(char *cmp_ver_str) {
    int cmp_ver, cmp_ver_digit[3] = {0};
    int bmc_ver, bmc_ver_digit[3] = {0};
    char cmp_ver_char = '\0';
    char bmc_ver_char = '\0';

    if (g_bmc_version[0] == 'N') {
        return -1;
    }

    if (platform_type_equal (AFN_X312PT)) {
        sscanf (cmp_ver_str, "v%d.%d.%d-%c", &cmp_ver_digit[0], &cmp_ver_digit[1], &cmp_ver_digit[2], &cmp_ver_char);
        sscanf (g_bmc_version, "v%d.%d.%d-%c", &bmc_ver_digit[0], &bmc_ver_digit[1], &bmc_ver_digit[2], &bmc_ver_char);
    } else if (platform_type_equal (AFN_X532PT) ||
               platform_type_equal (AFN_X564PT) ||
               platform_type_equal (AFN_X308PT) ||
               platform_type_equal (AFN_X732QT)) {
        sscanf (cmp_ver_str, "v%d.%d.%d", &cmp_ver_digit[0], &cmp_ver_digit[1], &cmp_ver_digit[2]);
        sscanf (g_bmc_version, "v%d.%d.%d", &bmc_ver_digit[0], &bmc_ver_digit[1], &bmc_ver_digit[2]);
    }

    cmp_ver = (cmp_ver_digit[0] << 24) + (cmp_ver_digit[1] << 16) + (cmp_ver_digit[2] << 8) + cmp_ver_char;
    bmc_ver = (bmc_ver_digit[0] << 24) + (bmc_ver_digit[1] << 16) + (bmc_ver_digit[2] << 8) + bmc_ver_char;

    if (bmc_ver == cmp_ver) {
        return 0;
    } else if (bmc_ver > cmp_ver) {
        return 1;
    } else {
        return -1;
    }
}

void bf_pltfm_start_312_i2c_wdt(void) {
    uint8_t cmd = 0xd;
    uint8_t wr_buf = 0x01;
    bf_pltfm_bmc_write_read (bmc_i2c_addr,
                             cmd, &wr_buf, 1, 0xFF, NULL, 0,
                             bmc_comm_interval_us_for_312);
}

void bf_pltfm_set_312_bmc_comm_interval(uint32_t us) {
    bmc_comm_interval_us_for_312 = us;
}

uint32_t bf_pltfm_get_312_bmc_comm_interval(void) {
    return bmc_comm_interval_us_for_312;
}

/* create /etc/platform.conf by running xt-cfgen.sh */
void bf_pltfm_load_conf () {
    // Initialize all the sub modules
    FILE *fp = NULL;
    char *cfg = "/etc/platform.conf";
    char entry[lqe_valen] = {0};
    uint8_t type = INVALID_TYPE;
    uint8_t subtype = INVALID_SUBTYPE;

    bf_pltfm_bd_type_set_priv (type, subtype);

    fp = fopen (cfg, "r");
    if (!fp) {
        /* For SONiC, use path below as fallback. by sunzheng, 2024.01.03 */
        cfg = "/usr/share/sonic/platform/platform.conf";
        fp = fopen(cfg, "r");
    }
    if (!fp) {
        LOG_ERROR (
                 "Exiting due to invalid platform.conf : "
                 "%s : %s.\n", strerror (errno),
                 (errno == ENOENT) ? "Try to create one by running xt-cfgen.sh" : "......");
        exit (0);
    } else {
        fprintf (stdout,
                 "\n\nLoading %s ...\n", cfg);
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

    bf_pltfm_bd_type_get_priv (&type, &subtype);
    if (((type == INVALID_TYPE) || (subtype == INVALID_SUBTYPE)) ||
        bf_pltfm_bd_type_set_by_key(type, subtype)) {
        /* Must never reach here.
         * If so, that means there may be a risk to run bsp to init platform.
         * by tsihang, 2022/08/01. */
        LOG_ERROR ("Exiting due to invalid value in EEPROM(0x26/0x27/0x31).\n");
        LOG_ERROR ("pltfm_mgr: Error in detecting platform.\n");
        exit (0);
    }
}

cpld_path_e bf_pltfm_find_path_to_cpld()
{
    int i;
    cpld_path_e path = VIA_NULL;

    for (i = 0; i < (int)ARRAY_LENGTH (pltfm_cpld_path); i++) {
        if (platform_type_equal (pltfm_cpld_path[i].platfm) &&
            platform_subtype_equal (pltfm_cpld_path[i].ver)) {
            if (bmc_i2c_bus == 0x7F) {
                path = pltfm_cpld_path[i].forced_path;
            } else {
                path = pltfm_cpld_path[i].default_path;
            }
        }
    }

    return path;
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_init()
{
    char bmc_ver[128] = {0};

    bf_pltfm_load_conf ();

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

    bf_pltfm_get_bmc_ver (&bmc_ver[0], true);
    fprintf (stdout, "########################\n");
    fprintf (stdout, "BMC %s\n", bmc_ver);
    LOG_DEBUG ("BMC %s", bmc_ver);
    fprintf (stdout, "########################\n");

    if (platform_type_equal (AFN_X312PT)) {
        uint8_t rd_buf[128] = {0};
        uint8_t cmd = 0x0D;
        uint8_t wr_buf[5] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA};

        if (bf_pltfm_compare_bmc_ver("v1.0.7-o") >= 0) {
            bmc_comm_interval_us_for_312 = BMC_COMM_INTERVAL_US / 10;
        } else {
            bmc_comm_interval_us_for_312 = BMC_COMM_INTERVAL_US;
        }

        /* If BMC version >= 1.00.02O, then set all fan LED to GREEN */
        if (bf_pltfm_compare_bmc_ver("v1.0.2-o") >= 0) {
            cmd = 0x31;
            wr_buf[0] = 0x03;
            wr_buf[1] = 0x32;
            wr_buf[2] = 0x04;
            wr_buf[3] = 0x01;
            wr_buf[4] = 0xFF;
            bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           cmd, wr_buf, 5, 0xFF, NULL, 0,
                                           bmc_comm_interval_us_for_312);
        }
        /* If BMC version >= 1.00.07O, then start I2C watchdog */
        if (bf_pltfm_compare_bmc_ver("v1.0.7-o") >= 0) {
            cmd = 0xd;
            wr_buf[0] = 0x03;
            wr_buf[1] = 0x2D;
            bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           cmd, wr_buf, 2, 0xFF, NULL, 0,
                                           bmc_comm_interval_us_for_312);

            cmd = 0xd;
            wr_buf[0] = 0x01;
            bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           cmd, wr_buf, 1, 0xFF, NULL, 0,
                                           bmc_comm_interval_us_for_312);
            cmd = 0xd;
            wr_buf[0] = 0x04;
            bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           cmd, wr_buf, 1, 0xFF, rd_buf, sizeof(rd_buf),
                                           bmc_comm_interval_us_for_312);
            LOG_DEBUG ("AFN_X312PT i2c watchdog %s, interval = %ds\n", rd_buf[1] ? "enabled" : "disabled", rd_buf[2]);
        }
        if (platform_subtype_equal (V2P0)) {
            LOG_DEBUG ("CPLD <- cp2112\n");
        } else if (platform_subtype_equal (V3P0) ||
                   platform_subtype_equal (V4P0) ||
                   platform_subtype_equal (V5P0)) {
            LOG_DEBUG ("CPLD <- super io\n");
        }
    } else if (platform_type_equal (AFN_X308PT) ||
               platform_type_equal (AFN_X532PT) ||
               platform_type_equal (AFN_X564PT)) {
        if (is_HVXXX || is_ADV15XX || is_S02XXX) {
            access_cpld_through_cp2112();
        } else if (is_CG15XX){
            cpld_path_e path = bf_pltfm_find_path_to_cpld();

            if (path == VIA_CGOS) {
                LOG_DEBUG ("CPLD <- cgoslx\n");
            } else if (path == VIA_CP2112) {
                access_cpld_through_cp2112();
            } else if (path == VIA_SIO) {
                access_cpld_through_superio();
            }
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        access_cpld_through_cp2112();
    }
    // Other initializations(Fan, etc.) go here

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_chss_mgmt_bd_type_get (
    bf_pltfm_board_id_t *board_id)
{
    return bf_pltfm_bd_type_get (board_id);
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

bf_pltfm_status_t bf_pltfm_get_suboard_status (bool *dpu1_installed,
    bool *dpu2_installed, bool *ptpx_installed) {
    if (!dpu1_installed || !dpu2_installed || !ptpx_installed) return BF_PLTFM_COMM_FAILED;

    *dpu1_installed = *dpu2_installed = *ptpx_installed = false;
    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_DPU1_INSTALLED) {
        *dpu1_installed = true;
    }
    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_DPU2_INSTALLED) {
        *dpu2_installed = true;
    }
    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED) {
        *ptpx_installed = true;
    }

    return BF_PLTFM_SUCCESS;
}
