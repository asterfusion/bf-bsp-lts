/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_fan.c
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


/* Module includes */
#include <bf_switchd/bf_switchd.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm.h>

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"
#include "bf_pltfm_master_i2c.h"
#include "bf_pltfm_uart.h"
#include "chss_mgmt.h"
#include "pltfm_types.h"

#define BMC_CMD_FAN_ALL 0x05
#define BMC_CMD_FAN_GET 0x06
#define BMC_CMD_FAN_SET 0x07
#define BMC_SUB2_RPM    0x00
#define BMC_SUB2_STATUS 0x01

static bf_pltfm_fan_data_t bmc_fan_data;

static inline void cpy_fan_info(bf_pltfm_fan_info_t *dst, bf_pltfm_fan_info_t *src)
{
    dst->direction   = src->direction;
    dst->fan_num     = src->fan_num;
    dst->front_speed = src->front_speed;
    dst->group       = src->group;
    dst->percent     = src->percent;
    dst->present     = src->present;
    dst->rear_speed  = src->rear_speed;
    dst->speed_level = src->speed_level;
}

static inline void clr_fan_data(bf_pltfm_fan_data_t *data)
{
    bf_pltfm_fan_info_t *dst;
    int block = sizeof (data->F) / sizeof (bf_pltfm_fan_info_t);

    foreach_element(0, block) {
        dst = &data->F[each_element];
        dst->direction   = 0;
        dst->fan_num     = 0;
        dst->front_speed = 0;
        dst->group       = 0;
        dst->percent     = 0;
        dst->present     = 0;
        dst->rear_speed  = 0;
        dst->speed_level = 0;
    }
    data->fantray_present = 0;
}

static inline void cpy_fan_data(bf_pltfm_fan_data_t *dst, bf_pltfm_fan_data_t *src)
{
    bf_pltfm_fan_info_t *idst, *isrc;
    int block = sizeof (dst->F) / sizeof (bf_pltfm_fan_info_t);

    foreach_element(0, block) {
        isrc = &src->F[each_element];
        idst = &dst->F[each_element];
        cpy_fan_info (idst, isrc);
    }
    dst->fantray_present = src->fantray_present;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x532p__ (
    bf_pltfm_fan_data_t *fdata)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;
    uint32_t num = 0;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;

        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_ALL, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
        if ((ret == 23) && (rd_buf[0] == 22)) {
            for (uint32_t i = 0;
                 i < bf_pltfm_mgr_ctx()->fan_group_count *
                     bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
                fdata->F[i].fan_num     = i + 1;
                fdata->F[i].present     = (rd_buf[1] & (0x01 <<
                                                        (i >> 1))) ? 1 : 0;
                fdata->F[i].direction   = 1;
                fdata->F[i].front_speed = (rd_buf[2 * i + 3] << 8)
                                          + rd_buf[2 * i + 4];

                num = i;
                /* Map to group */
                if (fdata->F[num].fan_num == 1 || fdata->F[num].fan_num == 2)
                    fdata->F[num].group = 1;
                if (fdata->F[num].fan_num == 3 || fdata->F[num].fan_num == 4)
                    fdata->F[num].group = 2;
                if (fdata->F[num].fan_num == 5 || fdata->F[num].fan_num == 6)
                    fdata->F[num].group = 3;
                if (fdata->F[num].fan_num == 7 || fdata->F[num].fan_num == 8)
                    fdata->F[num].group = 4;
                if (fdata->F[num].fan_num == 9 || fdata->F[num].fan_num == 10)
                    fdata->F[num].group = 5;
            }
            err = BF_PLTFM_SUCCESS;
        }
    } else {
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {

            err = -1;

            // Status
            wr_buf[0] = i + 1;
            wr_buf[1] = BMC_SUB2_STATUS;

            ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf,
                                           BMC_COMM_INTERVAL_US);
            if ((ret == 2) && (rd_buf[0] == 1)) {
                fdata->F[2 * i].fan_num   = 2 * i + 1;
                fdata->F[2 * i].present   = rd_buf[1];
                fdata->F[2 * i].direction = 1;
                fdata->F[2 * i + 1].fan_num   = 2 * i + 2;
                fdata->F[2 * i + 1].present   = rd_buf[1];
                fdata->F[2 * i + 1].direction = 1;
                err = BF_PLTFM_SUCCESS;
            }

            // RPM
            wr_buf[0] = i + 1;
            wr_buf[1] = BMC_SUB2_RPM;

            ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf,
                                           BMC_COMM_INTERVAL_US);
            if ((ret == 5) && (rd_buf[0] == 4)) {
                fdata->F[2 * i].front_speed   = (rd_buf[1] << 8 |
                                                 rd_buf[2]);
                fdata->F[2 * i + 1].front_speed = (rd_buf[3] << 8 |
                                                   rd_buf[4]);
                err = BF_PLTFM_SUCCESS;
            }
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x564p__ (
    bf_pltfm_fan_data_t *fdata)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;
    uint32_t num = 0;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;

        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_ALL, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
        if ((ret == 11) && (rd_buf[0] == 10)) {
            for (uint32_t i = 0;
                 i < bf_pltfm_mgr_ctx()->fan_group_count *
                     bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
                fdata->F[i].fan_num     = i + 1;
                fdata->F[i].present     = (rd_buf[1] >> i) & 0x01;
                fdata->F[i].direction   = (rd_buf[2] >> 2 * i) &
                                          0x03;
                fdata->F[i].front_speed = (rd_buf[2 * i + 3] << 8)
                                          + rd_buf[2 * i + 4];

                num = i;
                /* Map to group */
                if (fdata->F[num].fan_num == 1 || fdata->F[num].fan_num == 2)
                    fdata->F[num].group = 1;
                if (fdata->F[num].fan_num == 3 || fdata->F[num].fan_num == 4)
                    fdata->F[num].group = 2;
            }
            err = BF_PLTFM_SUCCESS;
        }
    } else {
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count *
                 bf_pltfm_mgr_ctx()->fan_per_group; i ++) {

            err = -1;

            // Status
            wr_buf[0] = i + 1;
            wr_buf[1] = BMC_SUB2_STATUS;

            ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf,
                                           BMC_COMM_INTERVAL_US);
            if ((ret == 4) && (rd_buf[0] == 3)) {
                fdata->F[i].fan_num   = i + 1;
                fdata->F[i].present   = rd_buf[1];
                fdata->F[i].direction = rd_buf[2];
                err = BF_PLTFM_SUCCESS;
            }

            // RPM
            wr_buf[0] = i + 1;
            wr_buf[1] = BMC_SUB2_RPM;

            ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf,
                                           BMC_COMM_INTERVAL_US);
            if ((ret == 4) && (rd_buf[0] == 3)) {
                fdata->F[i].front_speed = (rd_buf[1] << 8 |
                                           rd_buf[2]);
                err = BF_PLTFM_SUCCESS;
            }
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x308p__ (
    bf_pltfm_fan_data_t *fdata)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;
    uint32_t num = 0;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;

        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_ALL, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
        if ((ret == 27) && (rd_buf[0] == 26)) {
            for (uint32_t i = 0;
                 i < bf_pltfm_mgr_ctx()->fan_group_count *
                     bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
                fdata->F[i].fan_num     = i + 1;
                fdata->F[i].present     = (rd_buf[1] & (0x01 <<
                                                        (i >> 1))) ? 1 : 0;
                fdata->F[i].direction   = 1;
                fdata->F[i].front_speed = (rd_buf[2 * i + 3] << 8)
                                          + rd_buf[2 * i + 4];
                num = i;
                /* Map to group */
                if (fdata->F[num].fan_num == 1 || fdata->F[num].fan_num == 2)
                    fdata->F[num].group = 1;
                if (fdata->F[num].fan_num == 3 || fdata->F[num].fan_num == 4)
                    fdata->F[num].group = 2;
                if (fdata->F[num].fan_num == 5 || fdata->F[num].fan_num == 6)
                    fdata->F[num].group = 3;
                if (fdata->F[num].fan_num == 7 || fdata->F[num].fan_num == 8)
                    fdata->F[num].group = 4;
                if (fdata->F[num].fan_num == 9 || fdata->F[num].fan_num == 10)
                    fdata->F[num].group = 5;
                if (fdata->F[num].fan_num == 11 || fdata->F[num].fan_num == 12)
                    fdata->F[num].group = 6;
            }
             err = BF_PLTFM_SUCCESS;
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x312p__ (
    bf_pltfm_fan_data_t *fdata)
{
    int usec_delay = BMC_COMM_INTERVAL_US/25;
    uint32_t num = 0;
    int rdlen = 0;
    uint8_t buf[4] = {0};
    uint8_t data[I2C_SMBUS_BLOCK_MAX] = {0};

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v2dot0)) {

        // fan status
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count; i++) {
            buf[0] = i;
            buf[1] = 0x01;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x6, buf,
                                            2, 0xff, data, 100000);
            if (rdlen == 4) {
                fdata->F[2 * i].fan_num = 2 * i + 1;
                fdata->F[2 * i].present = data[1] ? 1 : 0;
                fdata->F[2 * i].direction = data[2];

                fdata->F[2 * i + 1].fan_num = 2 * i + 2;
                fdata->F[2 * i + 1].present = data[1] ? 1 : 0;
                fdata->F[2 * i + 1].direction = data[2];
            }
        }

        // fan speed
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count *
                 bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
            buf[0] = i;
            buf[1] = 0x00;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x6, buf,
                                            2, 0xff, data, 100000);
            if (rdlen == 4) {
                if (i < 5) {
                    fdata->F[ (i % 5) * 2 + 0].front_speed = data[2] *
                            100;
                } else {
                    fdata->F[ (i % 5) * 2 + 1].front_speed = data[2] *
                            100;
                }
            }

            num = i;
            /* Map to group */
            if (fdata->F[num].fan_num == 1 || fdata->F[num].fan_num == 6)
                fdata->F[num].group = 1;
            if (fdata->F[num].fan_num == 2 || fdata->F[num].fan_num == 7)
                fdata->F[num].group = 2;
            if (fdata->F[num].fan_num == 3 || fdata->F[num].fan_num == 8)
                fdata->F[num].group = 3;
            if (fdata->F[num].fan_num == 4 || fdata->F[num].fan_num == 9)
                fdata->F[num].group = 4;
            if (fdata->F[num].fan_num == 5 || fdata->F[num].fan_num == 10)
                fdata->F[num].group = 5;
        }
    } else if (platform_subtype_equal(v3dot0) ||
               platform_subtype_equal(v4dot0)) {
        // fan status
        buf[0] = 0x03;
        buf[1] = 0x32;
        buf[2] = 0x02;
        buf[3] = 0x01;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, 10000);
        if (rdlen != 3) {
            LOG_ERROR("read fan status from bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
        /* 0x01 means group 1 (ID 1 and ID 6). */
        fdata->F[0].present = (data[2] & 0x01) ? 0 : 1;
        fdata->F[5].present = fdata->F[0].present;
        /* 0x02 means group 2 (ID 2 and ID 7). */
        fdata->F[1].present = (data[2] & 0x02) ? 0 : 1;
        fdata->F[6].present = fdata->F[1].present;
        /* 0x04 means group 3 (ID 3 and ID 8). */
        fdata->F[2].present = (data[2] & 0x04) ? 0 : 1;
        fdata->F[7].present = fdata->F[2].present;
        /* 0x08 means group 4 (ID 4 and ID 9). */
        fdata->F[3].present = (data[2] & 0x08) ? 0 : 1;
        fdata->F[8].present = fdata->F[3].present;
        /* 0x10 means group 5 (ID 5 and ID 10). */
        fdata->F[4].present = (data[2] & 0x10) ? 0 : 1;
        fdata->F[9].present = fdata->F[4].present;

        // fan direction
        buf[0] = 0x03;
        buf[1] = 0x32;
        buf[2] = 0x03;
        buf[3] = 0x01;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, 10000);
        if (rdlen != 3) {
            LOG_ERROR("read fan direction from bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
        /* 0x01 means group 1 (ID 1 and ID 6). */
        fdata->F[0].direction = (data[2] & 0x01) ? 0 : 1;
        fdata->F[5].direction = fdata->F[0].direction;
        /* 0x02 means group 2 (ID 2 and ID 7). */
        fdata->F[1].direction = (data[2] & 0x02) ? 0 : 1;
        fdata->F[6].direction = fdata->F[1].direction;
        /* 0x04 means group 3 (ID 3 and ID 8). */
        fdata->F[2].direction = (data[2] & 0x04) ? 0 : 1;
        fdata->F[7].direction = fdata->F[2].direction;
        /* 0x08 means group 4 (ID 4 and ID 9). */
        fdata->F[3].direction = (data[2] & 0x08) ? 0 : 1;
        fdata->F[8].direction = fdata->F[3].direction;
        /* 0x10 means group 5 (ID 5 and ID 10). */
        fdata->F[4].direction = (data[2] & 0x10) ? 0 : 1;
        fdata->F[9].direction = fdata->F[4].direction;

        // fan speed
        buf[0] = 0x1;
        buf[1] = 0x32;
        buf[3] = 0x1;
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count *
                 bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
            buf[2] = i;
            rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, usec_delay);
            if (rdlen != 3) {
                LOG_ERROR("read fan speed from bmc error!\n");
                return BF_PLTFM_COMM_FAILED;
            }

            num = i;
            fdata->F[num].front_speed = data[2] * 200;
            if (fdata->F[num].front_speed > 25600) {
                fdata->F[num].percent = 100;
            } else {
                fdata->F[num].percent = (fdata->F[num].front_speed*100)/25600;
            }

            /* FAN number start from 1. */
            fdata->F[num].fan_num = (num + 1);

            /* Map to group */
            if (fdata->F[num].fan_num == 1 || fdata->F[num].fan_num == 6)
                fdata->F[num].group = 1;
            if (fdata->F[num].fan_num == 2 || fdata->F[num].fan_num == 7)
                fdata->F[num].group = 2;
            if (fdata->F[num].fan_num == 3 || fdata->F[num].fan_num == 8)
                fdata->F[num].group = 3;
            if (fdata->F[num].fan_num == 4 || fdata->F[num].fan_num == 9)
                fdata->F[num].group = 4;
            if (fdata->F[num].fan_num == 5 || fdata->F[num].fan_num == 10)
                fdata->F[num].group = 5;
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_hc36y24c__ (
    bf_pltfm_fan_data_t *fdata)
{
    // TODO : HC support fan info get???
    fdata = fdata;
    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x532p__ (
    bf_pltfm_fan_info_t *fdata)
{
    uint8_t wr_buf[2];
    int err = BF_PLTFM_COMM_FAILED;

    wr_buf[0] = fdata->fan_num;
    wr_buf[1] = fdata->speed_level;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, BMC_COMM_INTERVAL_US);
    }

    /* read back to checkout. */

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x564p__ (
    bf_pltfm_fan_info_t *fdata)
{
    uint8_t wr_buf[2];
    int err = BF_PLTFM_COMM_FAILED;

    wr_buf[0] = fdata->fan_num;
    wr_buf[1] = fdata->speed_level;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, BMC_COMM_INTERVAL_US);
    }

    /* read back to checkout. */

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x308p__ (
    bf_pltfm_fan_info_t *fdata)
{
    uint8_t wr_buf[2];
    int err = BF_PLTFM_COMM_FAILED;

    wr_buf[0] = fdata->fan_num;
    wr_buf[1] = fdata->speed_level;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, BMC_COMM_INTERVAL_US);
    }

    /* read back to checkout. */

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x312p__ (
    bf_pltfm_fan_info_t *fdata)
{
    int usec_delay = BMC_COMM_INTERVAL_US/25;
    fdata = fdata;

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v2dot0)) {
        uint8_t buf[2] = {0};
        uint8_t res[I2C_SMBUS_BLOCK_MAX + 2];
        int rdlen;

        // fan status
        buf[0] = 0x00;
        buf[1] = fdata->speed_level;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x7, buf,
                                        2, 0xff, res, 100000);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
    } else if (platform_subtype_equal(v3dot0) ||
               platform_subtype_equal(v4dot0)) {
        uint8_t buf[5] = {0};
        uint8_t data[32] = {0};
        int rdlen = 0;

        buf[0] = 0x04;
        buf[1] = 0x4c;
        buf[2] = 0x4a;
        buf[3] = 0x01;
        buf[4] = 0x22;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x31, buf, 5, 0xff, data, usec_delay);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x04;
        buf[1] = 0x4c;
        buf[2] = 0x4c;
        buf[3] = 0x01;
        buf[4] = fdata->speed_level;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x31, buf, 5, 0xff, data, usec_delay);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_hc36y24c__ (
    bf_pltfm_fan_info_t *fdata)
{
    fdata = fdata;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get__ (
    bf_pltfm_fan_data_t *fdata)
{
    int err = BF_PLTFM_COMM_FAILED;

    if (fdata == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    clr_fan_data(fdata);

    if (platform_type_equal (X532P)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x532p__
              (fdata);
    } else if (platform_type_equal (X564P)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x564p__
              (fdata);
    } else if (platform_type_equal (X308P)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x308p__
              (fdata);
    } else if (platform_type_equal (X312P)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x312p__
              (fdata);
    } else if (platform_type_equal (HC)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_hc36y24c__
              (fdata);
    }

    if (!err) {
        /* Write to cache. */
        cpy_fan_data (&bmc_fan_data, fdata);
    }

    return err;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set__ (
    bf_pltfm_fan_info_t *fdata)
{
    int err = BF_PLTFM_COMM_FAILED;

    if (!fdata || (fdata->fan_num >
                   (bf_pltfm_mgr_ctx()->fan_group_count *
                    bf_pltfm_mgr_ctx()->fan_per_group))) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    if (platform_type_equal (X532P)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x532p__
              (fdata);
    } else if (platform_type_equal (X564P)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x564p__
              (fdata);
    } else if (platform_type_equal (X308P)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x308p__
              (fdata);
    } else if (platform_type_equal (X312P)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x312p__
              (fdata);
    } else if (platform_type_equal (HC)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_hc36y24c__
              (fdata);
    }

    return err;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_data_get (
    bf_pltfm_fan_data_t *fdata)
{
    if (fdata == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    cpy_fan_data (fdata, &bmc_fan_data);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_speed_set (
    bf_pltfm_fan_info_t *fdata)
{
    return __bf_pltfm_chss_mgmt_fan_speed_set__
           (fdata);
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_fan_init()
{
    bf_pltfm_fan_data_t fdata;

    fprintf (stdout,
             "\n\n================== FANs INIT ==================\n");

    clr_fan_data(&bmc_fan_data);

    if (__bf_pltfm_chss_mgmt_fan_data_get__ (
            &fdata) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading fans \n");
        return -1;
    }

    if (!fdata.fantray_present) {
        fprintf (stdout, "Fan tray present \n");
    } else {
        fprintf (stdout, "Fan tray not present \n");
        return 0;
    }

    fprintf (stdout,
             "FAN   GRP FRONT-RPM  REAR-RPM    SPEED%%\n");

    for (uint32_t i = 0;
         i < (bf_pltfm_mgr_ctx()->fan_group_count *
              bf_pltfm_mgr_ctx()->fan_per_group); i++) {
        fprintf (stdout,
                 "%2d     %2d     %5d     %5d      %3d%%\n",
                 fdata.F[i].fan_num,
                 fdata.F[i].group,
                 fdata.F[i].front_speed,
                 fdata.F[i].rear_speed,
                 fdata.F[i].percent);
    }

    if (fdata.fantray_present) {
        fprintf (stdout, "Fan tray not present \n");
    } else {
        /* should we set the fan as we wanted ? */
    }

    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}

