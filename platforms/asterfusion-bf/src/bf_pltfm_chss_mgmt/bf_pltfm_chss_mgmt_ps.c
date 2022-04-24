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
#include <curl/curl.h>
#include <linux/i2c-dev.h>
#include <math.h>

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
#define MAX_PSU_COUNT		2

static bf_pltfm_pwr_supply_info_t
bmc_psu_data[MAX_PSU_COUNT] = {0};


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

    if (g_access_bmc_through_uart) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US *
                  3);    /* the usec may be too long. */
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        return BF_PLTFM_SUCCESS;
    }

    wr_buf[0] = BMC_SUB1_INFO;
    wr_buf[1] = 0xAA;

    if (g_access_bmc_through_uart) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US *
                  3);    /* the usec may be too long. */
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 27) && (rd_buf[0] == 26)) {
        info[0].vin        = rd_buf[1] << 8 | rd_buf[2];
        info[0].vout       = rd_buf[3] << 8 | rd_buf[4];
        info[0].iin        = rd_buf[5] << 8 | rd_buf[6];
        info[0].iout       = rd_buf[7] << 8 | rd_buf[8];
        info[0].pwr_out    = rd_buf[9] << 8 | rd_buf[10];
        info[0].pwr_in     = rd_buf[11] << 8 | rd_buf[12];

        info[1].vin        = rd_buf[14] << 8 | rd_buf[15];
        info[1].vout       = rd_buf[16] << 8 | rd_buf[17];
        info[1].iin        = rd_buf[18] << 8 | rd_buf[19];
        info[1].iout       = rd_buf[20] << 8 | rd_buf[21];
        info[1].pwr_out    = rd_buf[22] << 8 | rd_buf[23];
        info[1].pwr_in     = rd_buf[24] << 8 | rd_buf[25];

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

    if (g_access_bmc_through_uart) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        info[0].presence = (rd_buf[3] == 1) ?
                           false : true;
        info[1].presence = (rd_buf[6] == 1) ?
                           false : true;
        err = BF_PLTFM_SUCCESS;
    }

    if (!err) {
        *present = info[pwr - 1].presence;
    }

    /* There's no PSU info if the psu is absent.
     * Here return success.
     * by tsihang, 2021-07-13. */
    if (! (*present)) {
        return BF_PLTFM_SUCCESS;
    }

    wr_buf[0] = BMC_SUB1_INFO;
    wr_buf[1] = 0xAA;

    if (g_access_bmc_through_uart) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_PSU_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US * 1.5);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_PSU_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);
    }

    if ((ret == 27) && (rd_buf[0] == 26)) {
        info[0].vin        = rd_buf[1] << 8 | rd_buf[2];
        info[0].vout       = rd_buf[3] << 8 | rd_buf[4];
        info[0].iin        = rd_buf[5] << 8 | rd_buf[6];
        info[0].iout       = rd_buf[7] << 8 | rd_buf[8];
        info[0].pwr_out    = rd_buf[9] << 8 | rd_buf[10];
        info[0].pwr_in     = rd_buf[11] << 8 | rd_buf[12];

        info[1].vin        = rd_buf[14] << 8 | rd_buf[15];
        info[1].vout       = rd_buf[16] << 8 | rd_buf[17];
        info[1].iin        = rd_buf[18] << 8 | rd_buf[19];
        info[1].iout       = rd_buf[20] << 8 | rd_buf[21];
        info[1].pwr_out    = rd_buf[22] << 8 | rd_buf[23];
        info[1].pwr_in     = rd_buf[24] << 8 | rd_buf[25];

        err = BF_PLTFM_SUCCESS;
    }

    return err;
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

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v1dot2)) {
        // def
        uint8_t buf[10] = {0};
        uint8_t res[I2C_SMBUS_BLOCK_MAX + 2] = {0};
        int rdlen;
        int usec_delay_ps = 3000000;

        // check precense using psu fan status
        buf[0] = 0xaa;
        buf[1] = 0xaa;
        rdlen = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                        0xf, buf, 2, 0xff, res, 100000);
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
        if (!*present) {
            return BF_PLTFM_SUCCESS;
        }

        /* read values, if read error, just not recored and continue */
        // vin
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x88;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->vin = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
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
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 4) {
            n = res[1] & 0x1f;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            // vout
            buf[0] = 0x05;
            buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
            buf[2] = 0x8b;
            buf[3] = 0x2;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                            4, 0xff, res, usec_delay_ps);
            if (rdlen == 4) {
                value = res[1] + (res[2] << 8);
                y = value;
                y = y * pow (2, (double)n) + 0.05;
                info->vout = ((int)y << 8) + (int) ((y -
                                                    (int)y) * 10);
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
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iin = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
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
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iout = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
            if (debug_print) {
                fprintf (stdout, "iout is %d\n", info->iout);
            }
        }
        // pin
        buf[0] = 0x05;
        buf[1] = (pwr == POWER_SUPPLY1) ? 0x58 : 0x59;
        buf[2] = 0x97;
        buf[3] = 0x2;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x30, buf,
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 4) {
            value = res[1] + (res[2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_in = y;
            if (debug_print) {
                fprintf (stdout, "pwr_in is %d\n", info->pwr_in);
            }
        }
        // pout
        buf[0] = 0xaa;
        buf[1] = 0xaa;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0xb, buf,
                                        2, 0xff, res, 100000);
        if (rdlen == 5) {
            value = res[pwr * 2 - 1] + (res[pwr * 2] << 8);
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_out = y;
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
                                        4, 0xff, res, usec_delay_ps);
        if (rdlen == 12) {
            memcpy (info->serial, &res[1], 11);
            if (debug_print) {
                fprintf (stdout, "sn is %s\n", info->serial);
            }
        }
    }

    if (platform_subtype_equal(v1dot3)) {
        uint8_t pre_buf[2] = {0};
        uint8_t buf[10] = {0};
        int ret = -1;
        uint8_t psu_pout_data[3] = {0};
        uint8_t psu_pin_data[3] = {0};
        uint8_t psu_vout_data[3] = {0};
        uint8_t psu_vin_data[3] = {0};
        uint8_t psu_iout_data[3] = {0};
        uint8_t psu_iin_data[3] = {0};
        uint8_t psu_vout_mode[3] = {0};
        uint8_t psu_sn_data[11] = {0};
//        uint8_t psu_warn_data[3] = {0};
//        uint8_t psu_prod_name_data[3] = {0};
        uint8_t psu_present_data[2] = {0};
//        char *psu_status[2]= {"Psu ok", "Psu waring"};
//        char *psu_fan_rot[2]= {"Fan ok", "Fan warning"};
//        char *psu_ac_dc[2]= {"AC", "DC"};

        buf[0] = 0x5;
        buf[3] = 0x2;
        if (pwr == POWER_SUPPLY1) {
            buf[1] = 0x58;

            /*psu present 00: 01 <pin high/low>*/
            pre_buf[0] = 0xc;
            pre_buf[1] = 0xc;
            ret = bf_pltfm_bmc_write_read(0x3e, 0x20, pre_buf, 2, 0xff, psu_present_data, 10000);
            if (ret == -1) {
                LOG_ERROR("Read psu present error\n");
                return BF_PLTFM_COMM_FAILED;
            } else {
                *present = psu_present_data[1] ? false : true;
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "psu1 is %s\n",
                            *present ? "present" : "absent");
                }
            }
        } else if (pwr == POWER_SUPPLY2) {
            buf[1] = 0x59;

            /*psu present 00: 01 <pin high/low>*/
            pre_buf[0] = 0xd;
            pre_buf[1] = 0xc;
            ret = bf_pltfm_bmc_write_read(0x3e, 0x20, pre_buf, 2, 0xff, psu_present_data, 10000);
            if (ret == -1) {
                LOG_ERROR("Read psu present error\n");
                return BF_PLTFM_COMM_FAILED;
            } else {
                *present = psu_present_data[1] ? false : true;
                info->presence = *present;
                if (debug_print) {
                    fprintf (stdout, "psu2 is %s\n",
                            *present ? "present" : "absent");
                }
            }
        } else {
            return BF_PLTFM_COMM_FAILED;
        }

        /*PSU Pout 00: 02 <psu low> <psu high>*/
        buf[2] = 0x96;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_pout_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu pout error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_pout_data[2] << 8) | psu_pout_data[1];
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_out = y;
            if (debug_print) {
                fprintf (stdout, "pwr_out is %d\n",
                        info->pwr_out);
            }
        }

        /*PSU Pin 00: 02 <psu low> <psu high>*/
        buf[2] = 0x97;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_pin_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu pin error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_pin_data[2] << 8) | psu_pin_data[1];
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.5;
            info->pwr_in = y;
            if (debug_print) {
                fprintf (stdout, "pwr_in is %d\n",
                        info->pwr_in);
            }
        }

        /*PSU Vout_mode 00: 02 <Voutmode> <XX>*/
        buf[2] = 0x20;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vout_mode, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu vout_mode error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = psu_vout_mode[1];
            n = value & 0x1F;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
        }

        /*PSU Vout 00: 02 <psu low> <psu high>*/
        buf[2] = 0x8b;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vout_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu vout error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_vout_data[2] << 8) | psu_vout_data[1];
            y = value;
            y = y * pow (2, (double)n) + 0.05;
            info->vout = ((int)y << 8) + (int) ((y -
                                                    (int)y) * 10);
            if (debug_print) {
                fprintf (stdout, "vout is %d\n",
                        info->vout);
            }
        }

        /*PSU Vin 00: 02 <psu low> <psu high>*/
        buf[2] = 0x88;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_vin_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu vin error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_vin_data[2] << 8) | psu_vin_data[1];
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->vin = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
            if (debug_print) {
                fprintf (stdout, "vin is %d\n",
                        info->vin);
            }
        }

        /*PSU iout 00: 02 <psu low> <psu high>*/
        buf[2] = 0x8c;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_iout_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu iout error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_iout_data[2] << 8) | psu_iout_data[1];
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iout = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
            if (debug_print) {
                fprintf (stdout, "iout is %d\n",
                        info->iout);
            }
        }

        /*PSU iin 00: 02 <psu low> <psu high>*/
        buf[2] = 0x89;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_iin_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu iin error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            value = (psu_iin_data[2] << 8) | psu_iin_data[1];
            n = (value & 0xF800) >> 11;
            n = (n & 0x10) ? (n - 0x1F) - 1 : n;
            y = (value & 0x07FF);
            y = y * pow (2, (double)n) + 0.05;
            info->iin = ((int)y << 8) + (int) ((y -
                                                (int)y) * 10);
            if (debug_print) {
                fprintf (stdout, "iin is %d\n",
                        info->iin);
            }
        }

        /*PSU sn 00: 0a 09 <1> <2> <3> <4> <5> <6> <7> <8> <9> */
        buf[2] = 0x9e;
        buf[3] = 0xa;
        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_sn_data, 10000);
        if (ret == -1) {
            LOG_ERROR("Read psu sn error\n");
            return BF_PLTFM_COMM_FAILED;
        } else {
            memcpy (info->serial, &psu_sn_data[1], 11);
            if (debug_print) {
                fprintf (stdout, "sn is %s\n",
                        info->serial);
            }
        }

//        /*PSU warn 00: 02 <psu low> <psu high>*/
//        buf[2] = 0x79;
//        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_warn_data, 10000);
//        if (ret == -1) {
//            LOG_ERROR("Read psu warning error\n");
//            return BF_PLTFM_COMM_FAILED;
//        }
//
//        /*PSU product name 00: 02 <psu low> <psu high>*/
//        buf[2] = 0x9a;
//        ret = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, psu_prod_name_data, 10000);
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

        if (psu_present_data[1] == 0) {
            info->presence = true;
        } else {
            info->presence = false;
        }
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
    bf_pltfm_pwr_supply_info_t
    tmp_psu_data[MAX_PSU_COUNT] = {0};
    int err = BF_PLTFM_COMM_FAILED;

    if ((pwr != POWER_SUPPLY1 &&
         pwr != POWER_SUPPLY2) || info == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    memset (info, 0, sizeof (
                bf_pltfm_pwr_supply_info_t));

    if (platform_type_equal (X532P)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x532p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
    } else if (platform_type_equal (X564P)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x564p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
    } else if (platform_type_equal (X312P)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_x312p__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[pwr - 1]);
    } else if (platform_type_equal (HC)) {
        err = __bf_pltfm_chss_mgmt_pwr_supply_prsnc_get_hc36y24c__
              (pwr, present, (bf_pltfm_pwr_supply_info_t *)
               &tmp_psu_data[0]);
    }

    if (!err) {
        /* Write to cache. */
        memcpy (&bmc_psu_data[pwr - 1],
                &tmp_psu_data[pwr - 1],
                sizeof (bf_pltfm_pwr_supply_info_t));

        /* Read from cache. */
        memcpy (info, &bmc_psu_data[pwr - 1],
                sizeof (bf_pltfm_pwr_supply_info_t));
    }

    return err;
}


bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_supply_prsnc_get (
    bf_pltfm_pwr_supply_t pwr, bool *present)
{
    if (pwr != POWER_SUPPLY1 &&
        pwr != POWER_SUPPLY2) {
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

    if (pwr != POWER_SUPPLY1 &&
        pwr != POWER_SUPPLY2) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    memcpy (info, &bmc_psu_data[pwr - 1],
            sizeof (bf_pltfm_pwr_supply_info_t));

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_init()
{
    bf_pltfm_pwr_supply_t pwr;
    bf_pltfm_pwr_supply_info_t info;
    bool present = false;

    fprintf (stdout,
             "\n\n================== PWRs INIT ==================\n");

    memset (&info, 0, sizeof (info));
    pwr = POWER_SUPPLY1;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
        (pwr, &present,
         &info) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading power supply status : PWR%d\n",
                 pwr);
        return BF_PLTFM_COMM_FAILED;
    } else {
        if (present) {
            fprintf (stdout, "PWR%d\n",
                     pwr);

            fprintf (stdout, "  Presence        %s \n",
                     (info.presence ? "true" : "false"));
            fprintf (stdout,
                     "  Vin             %3d.%2d V\n", info.vin >> 8,
                     (info.vin & 0x00FF));
            fprintf (stdout,
                     "  Vout            %3d.%2d V\n", info.vout >> 8,
                     (info.vout & 0x00FF));
            fprintf (stdout,
                     "  Iin             %3d.%2d A\n", info.iin >> 8,
                     (info.iin & 0x00FF));
            fprintf (stdout,
                     "  Iout            %3d.%2d A\n", info.iout >> 8,
                     (info.iout & 0x00FF));
            fprintf (stdout,
                     "  Pin             %4d W\n",
                     info.pwr_in);
            fprintf (stdout,
                     "  Pout            %4d W\n",
                     info.pwr_out);
        }
    }

    memset (&info, 0, sizeof (info));
    pwr = POWER_SUPPLY2;
    if (__bf_pltfm_chss_mgmt_pwr_supply_prsnc_get__
        (pwr, &present,
         &info) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading power supply status : PWR%d\n",
                 pwr);
        return BF_PLTFM_COMM_FAILED;
    } else {
        if (present) {
            fprintf (stdout, "PWR%d\n",
                     pwr);

            fprintf (stdout, "Presence        %s \n",
                     (info.presence ? "true" : "false"));
            fprintf (stdout,
                     "  Vin             %3d.%2d V\n", info.vin >> 8,
                     (info.vin & 0x00FF));
            fprintf (stdout,
                     "  Vout            %3d.%2d V\n", info.vout >> 8,
                     (info.vout & 0x00FF));
            fprintf (stdout,
                     "  Iin             %3d.%2d A\n", info.iin >> 8,
                     (info.iin & 0x00FF));
            fprintf (stdout,
                     "  Iout            %3d.%2d A\n", info.iout >> 8,
                     (info.iout & 0x00FF));
            fprintf (stdout,
                     "  Pin             %4d W\n",
                     info.pwr_in);
            fprintf (stdout,
                     "  Pout            %4d W\n",
                     info.pwr_out);
        }
    }

    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}
