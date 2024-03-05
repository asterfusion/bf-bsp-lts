/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_ps.c
 * @date
 *
 * API's for reading temperature from BMC
 *
 */

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"
#include "bf_pltfm_master_i2c.h"
#include "bf_pltfm_uart.h"
#include "chss_mgmt.h"

#define BMC_CMD_PSU_GET 0x0B
#define BMC_SUB1_STATUS 0x00
#define BMC_SUB1_INFO   0x01
#define BMC_SUB1_MODEL  0x02
#define BMC_SUB1_SN     0x03
#define BMC_SUB1_TEMP   0x04
#define BMC_SUB1_FAN    0x05
#define MAX_PSU_COUNT	   2

static bf_pltfm_pwr_supply_info_t
bmc_psu_data[MAX_PSU_COUNT];

static inline void clr_psu_data(bf_pltfm_pwr_supply_info_t *info)
{
    info->presence = false;
    /* clr psu data. */
    info->vin      = 0;
    info->vout     = 0;
    info->iin      = 0;
    info->iout     = 0;
    info->fspeed   = 0;
    info->pwr_in   = 0;
    info->pwr_out  = 0;
    info->power    = false;
    info->fvalid   = 0;
    info->ffault   = 0;
    info->temp     = 0;
    info->load_sharing  = 0;
    memset ((char *)&info->model[0], 0, 32);
    memset ((char *)&info->serial[0], 0, 32);
    memset ((char *)&info->rev[0], 0, 32);
}

static inline void cpy_psu_data(bf_pltfm_pwr_supply_info_t *dst, bf_pltfm_pwr_supply_info_t *src)
{
    dst->presence = src->presence;
    dst->vin      = src->vin;
    dst->vout     = src->vout;
    dst->iin      = src->iin;
    dst->iout     = src->iout;
    dst->fspeed   = src->fspeed;
    dst->pwr_in   = src->pwr_in;
    dst->pwr_out  = src->pwr_out;
    dst->power    = src->power;
    dst->fvalid   = src->fvalid;
    dst->ffault   = src->ffault;
    dst->temp     = src->temp;
    dst->load_sharing  = src->load_sharing;
    memcpy ((char *)&dst->model[0], (char *)&src->model[0], 32 - 1);
    memcpy ((char *)&dst->serial[0], (char *)&src->serial[0], 32 - 1);
    memcpy ((char *)&dst->rev[0], (char *)&src->rev[0], 32 - 1);
}

static void format_psu_string(uint8_t* str_in)
{
    for (int i = 1; i <= str_in[0]; i ++) {
        if ((!isprint(str_in[i])) ||
            (str_in[i] >= 128)) {
            str_in[i] = ' ';
        }
    }
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x532p__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = BMC_SUB1_STATUS;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        info[0].power = (rd_buf[1] == 1) ?
                           true : false;
        info[1].power = (rd_buf[4] == 1) ?
                           true : false;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        clr_psu_data(&info[pwr - 1]);
        return BF_PLTFM_SUCCESS;
    }

    wr_buf[0] = BMC_SUB1_INFO;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 27) && (rd_buf[0] == 26)) {
        info[0].vin        = rd_buf[1] * 1000 + rd_buf[2] * 100;
        info[0].vout       = rd_buf[3] * 1000 + rd_buf[4] * 100;
        info[0].iin        = rd_buf[5] * 1000 + rd_buf[6] * 100;
        info[0].iout       = rd_buf[7] * 1000 + rd_buf[8] * 100;
        info[0].pwr_out    = (rd_buf[9] << 8 | rd_buf[10]) * 1000;
        info[0].pwr_in     = (rd_buf[11] << 8 | rd_buf[12])* 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[0].fvalid |= PSU_INFO_AC;

        info[1].vin        = rd_buf[14] * 1000 + rd_buf[15] * 100;
        info[1].vout       = rd_buf[16] * 1000 + rd_buf[17] * 100;
        info[1].iin        = rd_buf[18] * 1000 + rd_buf[19] * 100;
        info[1].iout       = rd_buf[20] * 1000 + rd_buf[21] * 100;
        info[1].pwr_out    = (rd_buf[22] << 8 | rd_buf[23]) * 1000;
        info[1].pwr_in     = (rd_buf[24] << 8 | rd_buf[25]) * 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[1].fvalid |= PSU_INFO_AC;

        err = BF_PLTFM_SUCCESS;
    }

    /* If BMC version is earlier than 1.2.3(hwver == 1.x), do not read MODEL/SN/REV...
     * If BMC version is earlier than 1.0.5(hwver == 2.0), do not read MODEL/SN/REV... */
    if (platform_subtype_equal(V1P0) ||
        platform_subtype_equal(V1P1)) {
        if (bf_pltfm_compare_bmc_ver("v1.2.3") < 0) {
            return BF_PLTFM_SUCCESS;
        }
    } else if (platform_subtype_equal(V2P0)) {
        if (bf_pltfm_compare_bmc_ver("v1.0.5") < 0) {
            return BF_PLTFM_SUCCESS;
        }
    } else {
        return BF_PLTFM_SUCCESS;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = BMC_SUB1_TEMP;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);
        if ((ret == 3) && (rd_buf[0] == 2)) {
            info[0].temp  = rd_buf[1];
            if (info[0].temp != 0) {
                info[0].fvalid |= PSU_INFO_VALID_TEMP;
            }

            info[1].temp  = rd_buf[2];
            if (info[1].temp != 0) {
                info[1].fvalid |= PSU_INFO_VALID_TEMP;
            }
        }

        wr_buf[0] = BMC_SUB1_FAN;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 5) && (rd_buf[0] == 4)) {
            info[0].fspeed  = (rd_buf[1] << 8) + rd_buf[2];
            if (info[0].fspeed != 0) {
                info[0].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }

            info[1].fspeed  = (rd_buf[3] << 8) + rd_buf[4];
            if (info[1].fspeed != 0) {
                info[1].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }
        }

        for (uint32_t i = 1; i <= MAX_PSU_COUNT; i++) {
            wr_buf[0] = BMC_SUB1_SN;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                if ((strlen (bmc_psu_data[i - 1].serial) == rd_buf[0]) &&
                    (memcmp (bmc_psu_data[i - 1].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (info[i - 1].serial, bmc_psu_data[i - 1].serial, sizeof(info[i - 1].serial));
                    memcpy (info[i - 1].model,  bmc_psu_data[i - 1].model,  sizeof(info[i - 1].model));
                    info[i - 1].fvalid = bmc_psu_data[i - 1].fvalid;
                    continue;
                }

                memset (info[i - 1].serial, 0x00, sizeof(info[i - 1].serial));
                memcpy (info[i - 1].serial, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on PSU module, then no need to read following info */
                continue;
            }

            wr_buf[0] = BMC_SUB1_MODEL;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                memset (info[i - 1].model, 0x00, sizeof(info[i - 1].model));
                memcpy (info[i - 1].model, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_MODEL;
            }
        }

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x564p__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = BMC_SUB1_STATUS;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        info[0].power = (rd_buf[1] == 1) ?
                           true : false;
        info[1].power = (rd_buf[4] == 1) ?
                           true : false;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        clr_psu_data(&info[pwr - 1]);
        return BF_PLTFM_SUCCESS;
    }

    wr_buf[0] = BMC_SUB1_INFO;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 27) && (rd_buf[0] == 26)) {
        info[0].vin        = rd_buf[1] * 1000 + rd_buf[2] * 100;
        info[0].vout       = rd_buf[3] * 1000 + rd_buf[4] * 100;
        info[0].iin        = rd_buf[5] * 1000 + rd_buf[6] * 100;
        info[0].iout       = rd_buf[7] * 1000 + rd_buf[8] * 100;
        info[0].pwr_out    = (rd_buf[9] << 8 | rd_buf[10]) * 1000;
        info[0].pwr_in     = (rd_buf[11] << 8 | rd_buf[12])* 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[0].fvalid |= PSU_INFO_AC;

        info[1].vin        = rd_buf[14] * 1000 + rd_buf[15] * 100;
        info[1].vout       = rd_buf[16] * 1000 + rd_buf[17] * 100;
        info[1].iin        = rd_buf[18] * 1000 + rd_buf[19] * 100;
        info[1].iout       = rd_buf[20] * 1000 + rd_buf[21] * 100;
        info[1].pwr_out    = (rd_buf[22] << 8 | rd_buf[23]) * 1000;
        info[1].pwr_in     = (rd_buf[24] << 8 | rd_buf[25]) * 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[1].fvalid |= PSU_INFO_AC;

        err = BF_PLTFM_SUCCESS;
    }

    /* If BMC version is earlier than 2.0.4(hwver == 1.x), do not read MODEL/SN/REV...
     * If BMC version is earlier than 1.0.2(hwver == 2.0), do not read MODEL/SN/REV... */
    if (platform_subtype_equal(V1P0) ||
        platform_subtype_equal(V1P1) ||
        platform_subtype_equal(V1P2)) {
        if (bf_pltfm_compare_bmc_ver("v2.0.4") < 0) {
            return BF_PLTFM_SUCCESS;
        }
    } else if (platform_subtype_equal(V2P0)) {
        if (bf_pltfm_compare_bmc_ver("v1.0.2") < 0) {
            return BF_PLTFM_SUCCESS;
        }
    } else {
        return BF_PLTFM_SUCCESS;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = BMC_SUB1_TEMP;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 3) && (rd_buf[0] == 2)) {
            info[0].temp  = rd_buf[1];
            if (info[0].temp != 0) {
                info[0].fvalid |= PSU_INFO_VALID_TEMP;
            }

            info[1].temp  = rd_buf[2];
            if (info[1].temp != 0) {
                info[1].fvalid |= PSU_INFO_VALID_TEMP;
            }
        }

        wr_buf[0] = BMC_SUB1_FAN;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 5) && (rd_buf[0] == 4)) {
            info[0].fspeed  = (rd_buf[1] << 8) + rd_buf[2];
            if (info[0].fspeed != 0) {
                info[0].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }

            info[1].fspeed  = (rd_buf[3] << 8) + rd_buf[4];
            if (info[1].fspeed != 0) {
                info[1].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }
        }

        for (uint32_t i = 1; i <= MAX_PSU_COUNT; i++) {
            wr_buf[0] = BMC_SUB1_SN;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                if ((strlen (bmc_psu_data[i - 1].serial) == rd_buf[0]) &&
                    (memcmp (bmc_psu_data[i - 1].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (info[i - 1].serial, bmc_psu_data[i - 1].serial, sizeof(info[i - 1].serial));
                    memcpy (info[i - 1].model,  bmc_psu_data[i - 1].model,  sizeof(info[i - 1].model));
                    info[i - 1].fvalid = bmc_psu_data[i - 1].fvalid;
                    continue;
                }

                memset (info[i - 1].serial, 0x00, sizeof(info[i - 1].serial));
                memcpy (info[i - 1].serial, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on PSU module, then no need to read following info */
                continue;
            }

            wr_buf[0] = BMC_SUB1_MODEL;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                memset (info[i - 1].model, 0x00, sizeof(info[i - 1].model));
                memcpy (info[i - 1].model, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_MODEL;
            }
        }

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x308p__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret = 0;

    wr_buf[0] = BMC_SUB1_STATUS;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        info[0].power = (rd_buf[1] == 1) ?
                           true : false;
        info[1].power = (rd_buf[4] == 1) ?
                           true : false;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        clr_psu_data(&info[pwr - 1]);
        return BF_PLTFM_SUCCESS;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {

        wr_buf[0] = BMC_SUB1_INFO;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);

        if ((ret == 27) && (rd_buf[0] == 26)) {
            info[0].vin        = rd_buf[1] * 1000 + rd_buf[2] * 100;
            info[0].vout       = rd_buf[3] * 1000 + rd_buf[4] * 100;
            info[0].iin        = rd_buf[5] * 1000 + rd_buf[6] * 100;
            info[0].iout       = rd_buf[7] * 1000 + rd_buf[8] * 100;
            info[0].pwr_out    = (rd_buf[9] << 8 | rd_buf[10]) * 1000;
            info[0].pwr_in     = (rd_buf[11] << 8 | rd_buf[12])* 1000;
            /* Default to AC as we do not have a way to detect at this moment.
            * by tsihang, 2022-07-08. */
            info[0].fvalid |= PSU_INFO_AC;

            info[1].vin        = rd_buf[14] * 1000 + rd_buf[15] * 100;
            info[1].vout       = rd_buf[16] * 1000 + rd_buf[17] * 100;
            info[1].iin        = rd_buf[18] * 1000 + rd_buf[19] * 100;
            info[1].iout       = rd_buf[20] * 1000 + rd_buf[21] * 100;
            info[1].pwr_out    = (rd_buf[22] << 8 | rd_buf[23]) * 1000;
            info[1].pwr_in     = (rd_buf[24] << 8 | rd_buf[25]) * 1000;
            /* Default to AC as we do not have a way to detect at this moment.
            * by tsihang, 2022-07-08. */
            info[1].fvalid |= PSU_INFO_AC;
        }

        /* Check BMC version first.
         * If the version is earlier than 1.0.5, do not read MODEL/SN/REV... */
        if (bf_pltfm_compare_bmc_ver("v1.0.5") < 0) {
            return BF_PLTFM_SUCCESS;
        }

        wr_buf[0] = BMC_SUB1_TEMP;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 3) && (rd_buf[0] == 2)) {
            info[0].temp  = rd_buf[1];
            if (info[0].temp != 0) {
                info[0].fvalid |= PSU_INFO_VALID_TEMP;
            }

            info[1].temp  = rd_buf[2];
            if (info[1].temp != 0) {
                info[1].fvalid |= PSU_INFO_VALID_TEMP;
            }
        }

        wr_buf[0] = BMC_SUB1_FAN;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 5) && (rd_buf[0] == 4)) {
            info[0].fspeed  = (rd_buf[1] << 8) + rd_buf[2];
            if (info[0].fspeed != 0) {
                info[0].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }

            info[1].fspeed  = (rd_buf[3] << 8) + rd_buf[4];
            if (info[1].fspeed != 0) {
                info[1].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }
        }

        for (uint32_t i = 1; i <= MAX_PSU_COUNT; i++) {
            wr_buf[0] = BMC_SUB1_SN;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                if ((strlen (bmc_psu_data[i - 1].serial) == rd_buf[0]) &&
                    (memcmp (bmc_psu_data[i - 1].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (info[i - 1].serial, bmc_psu_data[i - 1].serial, sizeof(info[i - 1].serial));
                    memcpy (info[i - 1].model,  bmc_psu_data[i - 1].model,  sizeof(info[i - 1].model));
                    info[i - 1].fvalid = bmc_psu_data[i - 1].fvalid;
                    continue;
                }

                memset (info[i - 1].serial, 0x00, sizeof(info[i - 1].serial));
                memcpy (info[i - 1].serial, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on PSU module, then no need to read following info */
                continue;
            }

            wr_buf[0] = BMC_SUB1_MODEL;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);
            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                memset (info[i - 1].model, 0x00, sizeof(info[i - 1].model));
                memcpy (info[i - 1].model, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_MODEL;
            }
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x732q__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = BMC_SUB1_STATUS;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        info[0].power = (rd_buf[1] == 1) ?
                           true : false;
        info[1].power = (rd_buf[4] == 1) ?
                           true : false;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        clr_psu_data(&info[pwr - 1]);
        return BF_PLTFM_SUCCESS;
    }

    wr_buf[0] = BMC_SUB1_INFO;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 27) && (rd_buf[0] == 26)) {
        info[0].vin        = rd_buf[1] * 1000 + rd_buf[2] * 100;
        info[0].vout       = rd_buf[3] * 1000 + rd_buf[4] * 100;
        info[0].iin        = rd_buf[5] * 1000 + rd_buf[6] * 100;
        info[0].iout       = rd_buf[7] * 1000 + rd_buf[8] * 100;
        info[0].pwr_out    = (rd_buf[9] << 8 | rd_buf[10]) * 1000;
        info[0].pwr_in     = (rd_buf[11] << 8 | rd_buf[12])* 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[0].fvalid |= PSU_INFO_AC;

        info[1].vin        = rd_buf[14] * 1000 + rd_buf[15] * 100;
        info[1].vout       = rd_buf[16] * 1000 + rd_buf[17] * 100;
        info[1].iin        = rd_buf[18] * 1000 + rd_buf[19] * 100;
        info[1].iout       = rd_buf[20] * 1000 + rd_buf[21] * 100;
        info[1].pwr_out    = (rd_buf[22] << 8 | rd_buf[23]) * 1000;
        info[1].pwr_in     = (rd_buf[24] << 8 | rd_buf[25]) * 1000;
        /* Default to AC as we do not have a way to detect at this moment.
         * by tsihang, 2022-07-08. */
        info[1].fvalid |= PSU_INFO_AC;

        err = BF_PLTFM_SUCCESS;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = BMC_SUB1_TEMP;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);
        if ((ret == 3) && (rd_buf[0] == 2)) {
            info[0].temp  = rd_buf[1];
            if (info[0].temp != 0) {
                info[0].fvalid |= PSU_INFO_VALID_TEMP;
            }

            info[1].temp  = rd_buf[2];
            if (info[1].temp != 0) {
                info[1].fvalid |= PSU_INFO_VALID_TEMP;
            }
        }

        wr_buf[0] = BMC_SUB1_FAN;
        wr_buf[1] = 0xAA;
        ret = bf_pltfm_bmc_uart_write_read (
                BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                BMC_COMM_INTERVAL_US);

        if ((ret == 5) && (rd_buf[0] == 4)) {
            info[0].fspeed  = (rd_buf[1] << 8) + rd_buf[2];
            if (info[0].fspeed != 0) {
                info[0].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }

            info[1].fspeed  = (rd_buf[3] << 8) + rd_buf[4];
            if (info[1].fspeed != 0) {
                info[1].fvalid |= PSU_INFO_VALID_FAN_ROTA;
            }
        }

        for (uint32_t i = 1; i <= MAX_PSU_COUNT; i++) {
            wr_buf[0] = BMC_SUB1_SN;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                if ((strlen (bmc_psu_data[i - 1].serial) == rd_buf[0]) &&
                    (memcmp (bmc_psu_data[i - 1].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (info[i - 1].serial, bmc_psu_data[i - 1].serial, sizeof(info[i - 1].serial));
                    memcpy (info[i - 1].model,  bmc_psu_data[i - 1].model,  sizeof(info[i - 1].model));
                    info[i - 1].fvalid = bmc_psu_data[i - 1].fvalid;
                    continue;
                }

                memset (info[i - 1].serial, 0x00, sizeof(info[i - 1].serial));
                memcpy (info[i - 1].serial, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on PSU module, then no need to read following info */
                continue;
            }

            wr_buf[0] = BMC_SUB1_MODEL;
            wr_buf[1] = i;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                format_psu_string(rd_buf);
                memset (info[i - 1].model, 0x00, sizeof(info[i - 1].model));
                memcpy (info[i - 1].model, &rd_buf[1], rd_buf[0]);
                info[i - 1].fvalid |= PSU_INFO_VALID_MODEL;
            }
        }

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

const uint8_t CRC8[] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static bool psu_check_crc(uint8_t* p_out, uint8_t* p_in)
{
    int i, len;
    uint8_t buf[32];
    uint8_t crc_temp = 0;
    uint8_t crc_index;

    buf[0] = p_out[1] << 1;
    buf[1] = p_out[2];
    buf[2] = buf[0] + 1;
    memcpy (&buf[3], &p_in[1], p_out[3]);
    len = p_out[3] + 2;

    for (i = 0; i < len; i ++) {
        crc_index = crc_temp ^ buf[i];
        crc_temp = CRC8[crc_index];
    }

    return (crc_temp == p_in[p_out[3]]) ? true : false;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x312p__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    uint32_t value;
    int32_t n;
    float y;
    bool debug_print = false;
    int usec_delay = bf_pltfm_get_312_bmc_comm_interval();

    /* Default to AC as we do not have a way to detect at this moment.
     * by tsihang, 2022-07-08. */
    info->fvalid |= PSU_INFO_AC;

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(V2P0)) {
        // def
        uint8_t buf[10] = {0};
        uint8_t res[I2C_SMBUS_BLOCK_MAX + 2] = {0};
        int rdlen;

        // check precense using psu fan status
        buf[0] = 0xaa;
        buf[1] = 0xaa;
        rdlen = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                        0xf, buf, 2, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 7) {
            if (pwr == POWER_SUPPLY1) {
                *present = res[1] ? true : false;
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "psu1 is %s\n",
                            *present ? "present" : "absent");
                }
            } else if (pwr == POWER_SUPPLY2) {
                *present = res[4] ? true : false;
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "psu2 is %s\n",
                            *present ? "present" : "absent");
                }
            } else {
                return BF_PLTFM_INVALID_ARG;
            }
        } else {
            fprintf (stdout, "rdlen is %d\n", rdlen);
            return BF_PLTFM_COMM_FAILED;
        }

        // if not present return
        if (! (*present)) {
            clr_psu_data(info);
            return BF_PLTFM_SUCCESS;
        }

        // pin
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x97;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_in = y * 1000;
            if (debug_print) {
                fprintf (stdout, "pwr_in is %d\n", info->pwr_in);
            }
        }

        /* Set power good as true if input power > 5W,
           otherwiase, skip reading rest of the info */
        if (info->pwr_in > 5000) {
            info->power = true;
        } else {
            info->pwr_in = 0;
            return BF_PLTFM_SUCCESS;
        }

        /* read values, if read error, just not recored and continue */
        // vin
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x88;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->vin = y * 1000;
            if (debug_print) {
                fprintf (stdout, "vin is %d\n", info->vin);
            }
        }
        // vout mode
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x20;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 4) {
            n = res[1] & 0x1f;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            // vout
            buf[0] = 0x05;
            buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
            buf[2] = 0x8b;
            buf[3] = 0x2;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                            4, 0xff, res, sizeof(res), usec_delay);
            if (rdlen == 4) {
                value = res[1] + (res[2] << 8);
                y = value;
                y = y * pow (2, (double)n) + 0.05;
                info->vout = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "vout is %d\n", info->vout);
                }
            }
        }
        // iin
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x89;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iin = y * 1000;
            if (debug_print) {
                fprintf (stdout, "iin is %d\n", info->iin);
            }
        }
        // iout
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x8c;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iout = y * 1000;
            if (debug_print) {
                fprintf (stdout, "iout is %d\n", info->iout);
            }
        }
        // pout
        buf[0] = 0xaa;
        buf[1] = 0xaa;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0xb, buf,
                                        2, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 5) {
            value = res[pwr * 2 - 1] + (res[pwr * 2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_out = y * 1000;
            if (debug_print) {
                fprintf (stdout, "pwr_out is %d\n",
                        info->pwr_out);
            }
        }
        // sn
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x9e;
        buf[3] = 0xa;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == 12) {
            memcpy (info->serial, &res[1], 11);
            if (debug_print) {
                fprintf (stdout, "sn is %s\n", info->serial);
            }
            info->fvalid |= PSU_INFO_VALID_SERIAL;
        }
    }

    if (platform_subtype_equal(V3P0) ||
        platform_subtype_equal(V4P0) ||
        platform_subtype_equal(V5P0)) {
        uint8_t pre_buf[2] = {0};
        uint8_t buf[10] = {0};
        int ret = -1;
        uint8_t psu_pout_data[4] = {0};
        uint8_t psu_pin_data[4] = {0};
        uint8_t psu_vout_data[4] = {0};
        uint8_t psu_vin_data[4] = {0};
        uint8_t psu_iout_data[4] = {0};
        uint8_t psu_iin_data[4] = {0};
        uint8_t psu_vout_mode[3] = {0};
        uint8_t psu_sn_data[12] = {0};
//        uint8_t psu_warn_data[3] = {0};
        uint8_t psu_model_data[15] = {0};
        uint8_t psu_present_data[3] = {0};
        uint8_t psu_fan_rota_data[4] = {0};
        uint8_t psu_fan_rev[15] = {0};
//        char *psu_status[2]= {"Psu ok", "Psu waring"};
//        char *psu_fan_rot[2]= {"Fan ok", "Fan warning"};
//        char *psu_ac_dc[2]= {"AC", "DC"};
        int retry;
        static bool psu_pres_buf[2][3];

        buf[0] = 0x5;
        buf[3] = 0x3;
        if (pwr == POWER_SUPPLY1) {
            buf[1] = 0x58;

            /*psu present 00: 01 <pin high/low>*/
            pre_buf[0] = 0xc;
            pre_buf[1] = 0xc;
            ret = bf_pltfm_bmc_write_read(0x3e, 0x20, pre_buf, 2, 0xff, psu_present_data, sizeof(psu_present_data), usec_delay);
            if (ret == -1) {
                LOG_ERROR("Read psu present error\n");
                return BF_PLTFM_COMM_FAILED;
            } else {
                psu_pres_buf[0][0] = psu_pres_buf[0][1];
                psu_pres_buf[0][1] = psu_pres_buf[0][2];
                psu_pres_buf[0][2] = psu_present_data[1] ? false : true;
                *present = psu_pres_buf[0][2] || psu_pres_buf[0][1];
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "PSU1  : %s\n",
                            *present ? "present" : "absent");
                }
            }
        } else if (pwr == POWER_SUPPLY2) {
            buf[1] = 0x59;

            /*psu present 00: 01 <pin high/low>*/
            pre_buf[0] = 0xd;
            pre_buf[1] = 0xc;
            ret = bf_pltfm_bmc_write_read(0x3e, 0x20, pre_buf, 2, 0xff, psu_present_data, sizeof(psu_present_data), usec_delay);
            if (ret == -1) {
                LOG_ERROR("Read psu present error\n");
                return BF_PLTFM_COMM_FAILED;
            } else {
                psu_pres_buf[1][0] = psu_pres_buf[1][1];
                psu_pres_buf[1][1] = psu_pres_buf[1][2];
                psu_pres_buf[1][2] = psu_present_data[1] ? false : true;
                *present = psu_pres_buf[1][2] || psu_pres_buf[1][1];
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "PSU2  : %s\n",
                            *present ? "present" : "absent");
                }
            }
        } else {
            return BF_PLTFM_COMM_FAILED;
        }

        // if not present return
        if (! (*present)) {
            clr_psu_data(info);
            return BF_PLTFM_SUCCESS;
        }

        // When the PSU is not present for the first time, copy the data from previous one
        if (psu_pres_buf[pwr - 1][2] == false) {
            cpy_psu_data(info, &bmc_psu_data[pwr - 1]);
            return BF_PLTFM_SUCCESS;
        }

        /* PSU Vin : 0x3e 0x30 0x5 0x58/0x59 0x88 0x3 */
        /* PSU Vin : 02 <psu low> <psu high> CRC8*/
        buf[2] = 0x88;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vin_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_vin_data)) {
                value = (psu_vin_data[2] << 8) | psu_vin_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n) + 0.05;
                info->vin = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "VIN   : %d\n",
                            info->vin);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu vin error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* Set power good as true if input voltage > 30V,
           otherwiase, skip reading rest of the info */
        if (info->vin > 30 * 1000) {
            info->power = true;
        } else {
            info->vin = 0;
            return BF_PLTFM_SUCCESS;
        }

        /* PSU Pin : 0x3e 0x30 0x5 0x58/0x59 0x97 0x3 */
        /* PSU Pin : 02 <psu low> <psu high> CRC8 */
        buf[2] = 0x97;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_pin_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_pin_data)) {
                value = (psu_pin_data[2] << 8) | psu_pin_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n) + 0.5;
                info->pwr_in = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "PIN   : %d\n",
                            info->pwr_in);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu pin error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU Pout : 0x3e 0x30 0x5 0x58/0x59 0x96 0x3 */
        /* PSU Pout : 02 <psu low> <psu high> CRC8 */
        buf[2] = 0x96;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_pout_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_pout_data)) {
                value = (psu_pout_data[2] << 8) | psu_pout_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n) + 0.5;
                info->pwr_out = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "POUT  : %d\n",
                            info->pwr_out);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu pout error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU Vout_mode : 0x3e 0x30 0x5 0x58/0x59 0x20 0x2 */
        /* PSU Vout_mode : 02 <Voutmode> CRC8 */
        buf[2] = 0x20;
        buf[3] = 0x2;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vout_mode, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_vout_mode)) {
                value = psu_vout_mode[1];
                n = value & 0x1F;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu vout_mode error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU Vout : 0x3e 0x30 0x5 0x58/0x59 0x8b 0x3 */
        /* PSU Vout : 02 <psu low> <psu high> CRC8*/
        buf[2] = 0x8b;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vout_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_vout_data)) {
                value = (psu_vout_data[2] << 8) | psu_vout_data[1];
                y = value;
                y = y * pow (2, (double)n) + 0.05;
                info->vout = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "VOUT  : %d\n",
                            info->vout);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu vout error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU iout : 0x3e 0x30 0x5 0x58/0x59 0x8c 0x3 */
        /* PSU iout : 02 <psu low> <psu high> CRC8*/
        buf[2] = 0x8c;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_iout_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_iout_data)){
                value = (psu_iout_data[2] << 8) | psu_iout_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n) + 0.05;
                info->iout = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "IOUT  : %d\n",
                            info->iout);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu iout error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU iin : 0x3e 0x30 0x5 0x58/0x59 0x89 0x3 */
        /* PSU iin : 02 <psu low> <psu high> CRC8*/
        buf[2] = 0x89;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_iin_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_iin_data)) {
                value = (psu_iin_data[2] << 8) | psu_iin_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n) + 0.05;
                info->iin = y * 1000;
                if (debug_print) {
                    fprintf (stdout, "IIN   : %d\n",
                            info->iin);
                }
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu iin error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU Fan Rota : 0x3e 0x30 0x5 0x58/0x59 0x90 0x3 */
        /* PSU Fan Rota : 02 1e 23 CRC8 */
        buf[2] = 0x90;
        buf[3] = 0x3;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_fan_rota_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_fan_rota_data)) {
                value = (psu_fan_rota_data[2] << 8) + psu_fan_rota_data[1];
                n = (value & 0xF800) >> 11;
                n = (n & 0x10) ? (n - 0x1F) - 1 : n;
                y = (value & 0x07FF);
                y = y * pow (2, (double)n);
                info->fspeed = y;
                if (debug_print) {
                    fprintf (stdout, "FAN RO: %d\n",
                            info->fspeed);
                }
                info->fvalid |= PSU_INFO_VALID_FAN_ROTA;
                break;
            } else if (retry >= 2) {
                LOG_ERROR("Read psu fan speed error\n");
                return BF_PLTFM_COMM_FAILED;
            }
        }

        /* PSU sn : 0x3e 0x30 0x5 0x58/0x59 0x9e 0xb */
        /* PSU sn : 0B 09 <1> <2> <3> <4> <5> <6> <7> <8> <9> CRC8 */
        buf[2] = 0x9e;
        buf[3] = 0xb;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_sn_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_sn_data)) {
                if (memcmp (bmc_psu_data[pwr - 1].serial, &psu_sn_data[2], 9) == 0) {
                    memcpy (info->serial, bmc_psu_data[pwr - 1].serial, sizeof(info->serial));
                    memcpy (info->model,  bmc_psu_data[pwr - 1].model,  sizeof(info->model));
                    memcpy (info->rev,    bmc_psu_data[pwr - 1].rev,    sizeof(info->rev));
                    info->fvalid = bmc_psu_data[pwr - 1].fvalid;
                    return BF_PLTFM_SUCCESS;
                } else {
                    format_psu_string(&psu_sn_data[1]);
                    memcpy (info->serial, &psu_sn_data[2], 9);
                    if (debug_print) {
                        fprintf (stdout, "SN    : %s\n",
                                info->serial);
                    }
                    info->fvalid |= PSU_INFO_VALID_SERIAL;
                    break;
                }
            } else if (retry >= 2) {
                LOG_WARNING("Read psu sn error\n");
            }
        }

        /* PSU Model : 0x3e 0x30 0x5 0x58/0x59 0x9A 0xe */
        /* PSU Model : 0e 0c 45 52 41 38 35 2d 43 50 41 2d 42 46 CRC8         ??ERA85-CPA-BF */
        buf[2] = 0x9A;
        buf[3] = 0xe;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_model_data, buf[3] + 1, usec_delay);
            if ((ret == buf[3] + 1) && psu_check_crc(buf, psu_model_data)) {
                format_psu_string(&psu_model_data[1]);
                memcpy (info->model, &psu_model_data[2], 12);
                if (debug_print) {
                    fprintf (stdout, "Model : %s\n",
                            info->model);
                }
                info->fvalid |= PSU_INFO_VALID_MODEL;
                info->fvalid |= PSU_INFO_AC;
                if (info->model[0] != 'E') {
                    info->fvalid &= ~PSU_INFO_AC;
                }
                break;
            } else if (retry >= 2) {
                LOG_WARNING("Read psu model error\n");
            }
        }

        /* PSU Rev : 0x3e 0x30 0x5 0x58/0x59 0xAD 0xe */
        /* PSU Rev : 0e 19 46 41 30 30 30 32 39 31 33 30 30 31 CRC8         ??FA0002913001 */
        buf[2] = 0xAD;
        buf[3] = 0xe;
        for (retry = 0; ; retry ++) {
            ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_fan_rev, buf[3] + 1, usec_delay);
            if (ret == buf[3] + 1) {
                for (int i = 0; i <= buf[3]; i ++) {
                    if (psu_fan_rev[i] == 0xff) {
                        if (retry >= 2) {
                            LOG_ERROR("Read psu rev error\n");
                            return BF_PLTFM_COMM_FAILED;
                        } else {
                            continue;
						}
                    }
                }
                psu_fan_rev[1] = psu_fan_rev[0] - 2;
                format_psu_string(&psu_fan_rev[1]);
                memcpy (info->rev, &psu_fan_rev[2], 12);
                if (debug_print) {
                    fprintf (stdout, "Rev   : %s\n",
                            info->rev);
                }
                info->fvalid |= PSU_INFO_VALID_REV;
                break;
            } else if (retry >= 2) {
                LOG_WARNING("Read psu rev error\n");
            }
        }

//        /*PSU warn 00: 02 <psu low> <psu high>*/
//        buf[2] = 0x79;
//        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_warn_data, sizeof(psu_warn_data), usec_delay);
//        if (ret == -1) {
//            LOG_ERROR("Read psu warning error\n");
//            return BF_PLTFM_COMM_FAILED;
//        }
//
//        /*PSU product name 00: 02 <psu low> <psu high>*/
//        buf[2] = 0x9a;
//        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_prod_name_data, sizeof(psu_prod_name_data), usec_delay);
//        if (ret == -1) {
//            LOG_ERROR("Read psu product name error\n");
//            return BF_PLTFM_COMM_FAILED;
//        }

//        /*0x0 is normal, 1 is warning*/
//        if (psu_warn_data[1] < 2) {
//            memcpy(info->status, psu_status[psu_warn_data[1]], strlen(psu_status[psu_warn_data[1]]));
//        } else {
//            memcpy(info->status, psu_status[1], strlen(psu_status[1]));
//        }
//
//        /*0x1 is normal and ac ,2 is warning and dc*/
//        if (psu_prod_name_data[2] == 69) {
//            memcpy(info->fan_rot, psu_fan_rot[0], strlen(psu_fan_rot[0]));
//            memcpy(info->ac_dc, psu_ac_dc[0], strlen(psu_ac_dc[0]));
//        } else {
//            memcpy(info->fan_rot, psu_fan_rot[1], strlen(psu_fan_rot[1]));
//            memcpy(info->ac_dc, psu_ac_dc[1], strlen(psu_ac_dc[1]));
//        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_hc36y24c__
(
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    pwr = pwr;
    present = present;
    info = info;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__ (
    bf_pltfm_pwr_supply_t pwr, bool *present,
    bf_pltfm_pwr_supply_info_t *info)
{
    int err = BF_PLTFM_COMM_FAILED;
    bf_pltfm_pwr_supply_info_t
    tmp_psu_data[MAX_PSU_COUNT], *p = NULL;

    if ((pwr != POWER_SUPPLY1 &&
         pwr != POWER_SUPPLY2) || info == NULL || present == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    clr_psu_data (info);
    clr_psu_data (&tmp_psu_data[POWER_SUPPLY1 - 1]);
    clr_psu_data (&tmp_psu_data[POWER_SUPPLY2 - 1]);
    *present = false;

    if (platform_type_equal (AFN_X532PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x532p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[pwr - 1];
    } else if (platform_type_equal (AFN_X564PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x564p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[pwr - 1];
    } else if (platform_type_equal (AFN_X308PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x308p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[pwr - 1];
    } else if (platform_type_equal (AFN_X312PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x312p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[0];
    } else if (platform_type_equal (AFN_X732QT)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x732q__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[pwr - 1];
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_hc36y24c__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
        p = &tmp_psu_data[pwr - 1];
    }

    if (err) {
    } else {
        if (p) {
            /* Write to cache. */
            cpy_psu_data (&bmc_psu_data[pwr - 1], p);
            /* Read from cache. */
            cpy_psu_data (info, &bmc_psu_data[pwr - 1]);
        }
    }

    return err;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_supply_prsnc_get (
    bf_pltfm_pwr_supply_t pwr, bool *present)
{
    if ((pwr != POWER_SUPPLY1 &&
         pwr != POWER_SUPPLY2) || present == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    *present = bmc_psu_data[pwr - 1].presence;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_supply_get (
    bf_pltfm_pwr_supply_t pwr,
    bf_pltfm_pwr_supply_info_t *info)
{

    if ((pwr != POWER_SUPPLY1 &&
         pwr != POWER_SUPPLY2) || info == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    cpy_psu_data (info, &bmc_psu_data[pwr - 1]);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_init()
{
    bf_pltfm_pwr_supply_t pwr;
    bf_pltfm_pwr_supply_info_t infos, *info = NULL;
    bool presence = false;

    clr_psu_data (&bmc_psu_data[POWER_SUPPLY1 - 1]);
    clr_psu_data (&bmc_psu_data[POWER_SUPPLY2 - 1]);

    pwr = POWER_SUPPLY1;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
        (pwr, &presence, &infos) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading power supply status : PWR%d\n",
                 pwr);
    } else {
        info = (bf_pltfm_pwr_supply_info_t *)&infos;
        if (presence) {
            fprintf (stdout, "PWR%d : %s\n",
                     pwr, (info->fvalid & PSU_INFO_AC) ? "AC" : "DC");

            fprintf (stdout, "  Presence        %s \n",
                     (info->presence ? "true" : "false"));
            fprintf (stdout, "  Power ok        %s \n",
                     (info->power ? "true" : "false"));
            fprintf (stdout,
                     "  Vin             %5.1f V\n", info->vin / 1000.0);
            fprintf (stdout,
                     "  Vout            %5.1f V\n", info->vout / 1000.0);
            fprintf (stdout,
                     "  Iin             %5.1f A\n", info->iin / 1000.0);
            fprintf (stdout,
                     "  Iout            %5.1f A\n", info->iout / 1000.0);
            fprintf (stdout,
                     "  Pin            %6.1f W\n", info->pwr_in / 1000.0);
            fprintf (stdout,
                     "  Pout           %6.1f W\n", info->pwr_out / 1000.0);
            if (info->fvalid & PSU_INFO_VALID_TEMP) {
                fprintf (stdout,
                         "  Temp            %5d C\n",
                         info->temp);
            }
            if (info->fvalid & PSU_INFO_VALID_SERIAL) {
                fprintf (stdout,
                         "  SN              %s\n",
                         info->serial);
            }
            if (info->fvalid & PSU_INFO_VALID_MODEL) {
                fprintf (stdout,
                         "  Model           %s\n",
                         info->model);
            }
            if (info->fvalid & PSU_INFO_VALID_REV) {
                fprintf (stdout,
                         "  Rev             %s\n",
                         info->rev);
            }
            if (info->fvalid & PSU_INFO_VALID_FAN_ROTA) {
                fprintf (stdout,
                         "  FAN Rota        %5d rpm\n",
                         info->fspeed);
            }
        }
    }

    pwr = POWER_SUPPLY2;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
         (pwr, &presence, &infos) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
              "Error in reading power supply status : PWR%d\n",
              pwr);
    } else {
        info = (bf_pltfm_pwr_supply_info_t *)&infos;
        if (presence) {
            fprintf (stdout, "PWR%d : %s\n",
                     pwr, (info->fvalid & PSU_INFO_AC) ? "AC" : "DC");

            fprintf (stdout, "  Presence        %s \n",
                     (info->presence ? "true" : "false"));
            fprintf (stdout, "  Power ok        %s \n",
                     (info->power ? "true" : "false"));
            fprintf (stdout,
                     "  Vin             %5.1f V\n", info->vin / 1000.0);
            fprintf (stdout,
                     "  Vout            %5.1f V\n", info->vout / 1000.0);
            fprintf (stdout,
                     "  Iin             %5.1f A\n", info->iin / 1000.0);
            fprintf (stdout,
                     "  Iout            %5.1f A\n", info->iout / 1000.0);
            fprintf (stdout,
                     "  Pin            %6.1f W\n", info->pwr_in / 1000.0);
            fprintf (stdout,
                     "  Pout           %6.1f W\n", info->pwr_out / 1000.0);
            if (info->fvalid & PSU_INFO_VALID_TEMP) {
                fprintf (stdout,
                         "  Temp            %5d C\n",
                         info->temp);
            }
            if (info->fvalid & PSU_INFO_VALID_SERIAL) {
                fprintf (stdout,
                         "  Serial          %s\n",
                         info->serial);
            }
            if (info->fvalid & PSU_INFO_VALID_MODEL) {
                fprintf (stdout,
                         "  Model           %s\n",
                         info->model);
            }
            if (info->fvalid & PSU_INFO_VALID_REV) {
                fprintf (stdout,
                         "  Rev             %s\n",
                         info->rev);
            }
            if (info->fvalid & PSU_INFO_VALID_FAN_ROTA) {
                fprintf (stdout,
                         "  FAN Rota        %5d rpm\n",
                         info->fspeed);
            }
        }
    }

    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_bmc_data_psu_decode__ (uint8_t* p_src)
{
    uint8_t len  = p_src[0];
    uint8_t type = p_src[1];
    uint8_t num  = p_src[2];
    bf_pltfm_pwr_supply_info_t temp_psu_data[2];

    if ((type != 3) || (len != num * 17 + 2))  {
        return BF_PLTFM_INVALID_ARG;
    }

    memset(&temp_psu_data[0], 0, sizeof (bf_pltfm_pwr_supply_info_t));
    memset(&temp_psu_data[1], 0, sizeof (bf_pltfm_pwr_supply_info_t));

    for (int i = 0, j = 3; i < num; i ++, j += 17) {
        temp_psu_data[i].presence = (p_src[j] & 0x01) ? false : true;
        temp_psu_data[i].power    = (p_src[j] & 0x02) ? true : false;
        temp_psu_data[i].vin      = p_src[j + 1] * 1000 + p_src[j + 2] * 100;
        temp_psu_data[i].vout     = p_src[j + 3] * 1000 + p_src[j + 4] * 100;
        temp_psu_data[i].iin      = p_src[j + 5] * 1000 + p_src[j + 6] * 100;
        temp_psu_data[i].iout     = p_src[j + 7] * 1000 + p_src[j + 8] * 100;
        temp_psu_data[i].pwr_out  = (p_src[j + 9]  * 256 + p_src[j + 10]) * 1000;
        temp_psu_data[i].pwr_in   = (p_src[j + 11] * 256 + p_src[j + 12]) * 1000;
        temp_psu_data[i].temp     = p_src[j + 14];
        temp_psu_data[i].fspeed   = p_src[j + 16] * 256 + p_src[j + 15];
        temp_psu_data[i].fvalid   = PSU_INFO_AC + PSU_INFO_VALID_TEMP + PSU_INFO_VALID_FAN_ROTA;

        if (!temp_psu_data[i].presence) {
            memset(&temp_psu_data[i], 0, sizeof (bf_pltfm_pwr_supply_info_t));
            temp_psu_data[i].fvalid   = PSU_INFO_AC;
        } else if ((temp_psu_data[i].power == true) &&
            ((bmc_psu_data[i].fvalid & PSU_INFO_VALID_SERIAL) == false)) {
            uint8_t wr_buf[2];
            uint8_t rd_buf[128];
            int ret = BF_PLTFM_COMM_FAILED;

            wr_buf[1] = i + 1;
            wr_buf[0] = BMC_SUB1_SN;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                memcpy (temp_psu_data[i].serial, &rd_buf[1], rd_buf[0]);
                temp_psu_data[i].fvalid |= PSU_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on PSU module, then no need to read following info */
                continue;
            }

            wr_buf[0] = BMC_SUB1_MODEL;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                memcpy (temp_psu_data[i].model, &rd_buf[1], rd_buf[0]);
                temp_psu_data[i].fvalid |= PSU_INFO_VALID_MODEL;
            }
        } else if (temp_psu_data[i].power == true) {
            memcpy (temp_psu_data[i].serial, bmc_psu_data[i].serial, 32);
            memcpy (temp_psu_data[i].model,  bmc_psu_data[i].model,  32);
            temp_psu_data[i].fvalid = bmc_psu_data[i].fvalid;
        } else {
            memset(&temp_psu_data[i], 0, sizeof (bf_pltfm_pwr_supply_info_t));
            temp_psu_data[i].presence = true;
            temp_psu_data[i].fvalid   = PSU_INFO_AC;
        }
    }

    cpy_psu_data (&bmc_psu_data[0], &temp_psu_data[0]);
    cpy_psu_data (&bmc_psu_data[1], &temp_psu_data[1]);

    return BF_PLTFM_SUCCESS;
}
