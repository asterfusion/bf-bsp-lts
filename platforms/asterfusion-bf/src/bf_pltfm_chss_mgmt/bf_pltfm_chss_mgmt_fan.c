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
#define BMC_SUB2_SN     0x03
#define BMC_SUB2_MODEL  0x05
#define BMC_SUB2_MAX    0x07
#define BMC_SUB2_DIR    0x08

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
    dst->max_speed   = src->max_speed;
    dst->speed_level = src->speed_level;
    dst->fvalid      = src->fvalid;
    memcpy ((char *)&dst->model[0], (char *)&src->model[0], 32 - 1);
    memcpy ((char *)&dst->serial[0], (char *)&src->serial[0], 32 - 1);
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
        dst->max_speed   = 0;
        dst->speed_level = 0;
        dst->fvalid      = 0;
        memset ((char *)&dst->model[0], 0, 32);
        memset ((char *)&dst->serial[0], 0, 32);
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

        for (uint32_t i = 0; i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {
            wr_buf[0] = i + 1;

            wr_buf[1] = BMC_SUB2_SN;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;

                if ((strlen (bmc_fan_data.F[num].serial) == rd_buf[0]) &&
                    (memcmp (bmc_fan_data.F[num].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    num ++;
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    continue;
                }

                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;

                num ++;
                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on FAN module, then no need to read following info */
                continue;
            }

            wr_buf[1] = BMC_SUB2_MODEL;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;

                num ++;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;
            }

            wr_buf[1] = BMC_SUB2_MAX;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 5) && (ret == rd_buf[0] + 1)) {
                num = i * 2;
                fdata->F[num].max_speed  = (rd_buf[1] << 8) + rd_buf[2];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }

                num ++;
                fdata->F[num].max_speed  = (rd_buf[3] << 8) + rd_buf[4];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }
            }

            wr_buf[1] = BMC_SUB2_DIR;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 4) && (ret == rd_buf[0] + 1)) {
                if ((rd_buf[1] == 'F') && (rd_buf[2] == '2') && ((rd_buf[3] == 'R') || (rd_buf[3] == 'B'))) {
                    num = i * 2;
                    fdata->F[num].direction = 1;
                    fdata->F[num + 1].direction = 1;
                } else if (((rd_buf[1] == 'R') || (rd_buf[1] == 'B')) && (rd_buf[2] == '2') && (rd_buf[3] == 'F')) {
                    num = i * 2;
                    fdata->F[num].direction = 2;
                    fdata->F[num +1].direction = 2;
                }
            }
        }

    } else {
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {

            err = -1;

            // Status
            wr_buf[0] = i + 1;
            wr_buf[1] = BMC_SUB2_STATUS;

            ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
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
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
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
                fdata->F[i].direction   = 1;
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

        for (uint32_t i = 0; i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {
            wr_buf[0] = i + 1;

            wr_buf[1] = BMC_SUB2_SN;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;

                if ((strlen (bmc_fan_data.F[num].serial) == rd_buf[0]) &&
                    (memcmp (bmc_fan_data.F[num].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    num ++;
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    continue;
                }

                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;

                num ++;
                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on FAN module, then no need to read following info */
                continue;
            }

            wr_buf[1] = BMC_SUB2_MODEL;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;

                num ++;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;
            }

            wr_buf[1] = BMC_SUB2_MAX;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 5) && (ret == rd_buf[0] + 1)) {
                num = i * 2;
                fdata->F[num].max_speed  = (rd_buf[1] << 8) + rd_buf[2];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }

                num ++;
                fdata->F[num].max_speed  = (rd_buf[3] << 8) + rd_buf[4];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }
            }

            wr_buf[1] = BMC_SUB2_DIR;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 4) && (ret == rd_buf[0] + 1)) {
                if ((rd_buf[1] == 'F') && (rd_buf[2] == '2') && ((rd_buf[3] == 'R') || (rd_buf[3] == 'B'))) {
                    num = i * 2;
                    fdata->F[num].direction = 1;
                    fdata->F[num + 1].direction = 1;
                } else if (((rd_buf[1] == 'R') || (rd_buf[1] == 'B')) && (rd_buf[2] == '2') && (rd_buf[3] == 'F')) {
                    num = i * 2;
                    fdata->F[num].direction = 2;
                    fdata->F[num +1].direction = 2;
                }
            }
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
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
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
                                           BMC_CMD_FAN_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
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
    int ret;
    uint8_t num = 0;

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
        }

        /* Check BMC version first.
         * If the version is earlier than 1.0.6, do not read MODEL/SN/REV... */
        if (bf_pltfm_compare_bmc_ver("v1.0.6") < 0) {
            return BF_PLTFM_SUCCESS;
        }

        for (uint32_t i = 0; i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {
            wr_buf[0] = i + 1;

            wr_buf[1] = BMC_SUB2_SN;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;

                if ((strlen (bmc_fan_data.F[num].serial) == rd_buf[0]) &&
                    (memcmp (bmc_fan_data.F[num].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    num ++;
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    continue;
                }

                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;

                num ++;
                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on FAN module, then no need to read following info */
                continue;
            }

            wr_buf[1] = BMC_SUB2_MODEL;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;

                num ++;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;
            }

            wr_buf[1] = BMC_SUB2_MAX;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 5) && (ret == rd_buf[0] + 1)) {
                num = i * 2;
                fdata->F[num].max_speed  = (rd_buf[1] << 8) + rd_buf[2];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }

                num ++;
                fdata->F[num].max_speed  = (rd_buf[3] << 8) + rd_buf[4];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }
            }

            wr_buf[1] = BMC_SUB2_DIR;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 4) && (ret == rd_buf[0] + 1)) {
                if ((rd_buf[1] == 'F') && (rd_buf[2] == '2') && ((rd_buf[3] == 'R') || (rd_buf[3] == 'B'))) {
                    num = i * 2;
                    fdata->F[num].direction = 1;
                    fdata->F[num + 1].direction = 1;
                } else if (((rd_buf[1] == 'R') || (rd_buf[1] == 'B')) && (rd_buf[2] == '2') && (rd_buf[3] == 'F')) {
                    num = i * 2;
                    fdata->F[num].direction = 2;
                    fdata->F[num +1].direction = 2;
                }
            }
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x312p__ (
    bf_pltfm_fan_data_t *fdata)
{
    int usec_delay = bf_pltfm_get_312_bmc_comm_interval();
    uint32_t num = 0;
    int rdlen = 0;
    uint8_t buf[4] = {0};
    uint8_t data[I2C_SMBUS_BLOCK_MAX] = {0};

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(V2P0)) {

        // fan status
        for (uint32_t i = 0;
             i < bf_pltfm_mgr_ctx()->fan_group_count; i++) {
            buf[0] = i;
            buf[1] = 0x01;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x6, buf,
                                            2, 0xff, data, sizeof(data), usec_delay);
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
                                            2, 0xff, data, sizeof(data), usec_delay);
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
    } else if (platform_subtype_equal(V3P0) ||
               platform_subtype_equal(V4P0) ||
               platform_subtype_equal(V5P0)) {
        // fan status
        buf[0] = 0x03;
        buf[1] = 0x32;
        buf[2] = 0x02;
        buf[3] = 0x01;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, sizeof(data), usec_delay);
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
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, sizeof(data), usec_delay);
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
            rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, sizeof(data), usec_delay);
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
__bf_pltfm_chss_mgmt_fan_data_get_x732q__ (
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

        for (uint32_t i = 0; i < bf_pltfm_mgr_ctx()->fan_group_count; i ++) {
            wr_buf[0] = i + 1;

            wr_buf[1] = BMC_SUB2_SN;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;

                if ((strlen (bmc_fan_data.F[num].serial) == rd_buf[0]) &&
                    (memcmp (bmc_fan_data.F[num].serial, &rd_buf[1], rd_buf[0]) == 0)) {
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    num ++;
                    memcpy (fdata->F[num].serial, bmc_fan_data.F[num].serial, sizeof(fdata->F[num].serial));
                    memcpy (fdata->F[num].model,  bmc_fan_data.F[num].model,  sizeof(fdata->F[num].model));
                    fdata->F[num].direction = bmc_fan_data.F[num].direction;
                    fdata->F[num].max_speed = bmc_fan_data.F[num].max_speed;
                    if (fdata->F[num].max_speed != 0) {
                        fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    }
                    fdata->F[num].fvalid = bmc_fan_data.F[num].fvalid;

                    continue;
                }

                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;

                num ++;
                memset (fdata->F[num].serial, 0x00, sizeof(fdata->F[num].serial));
                memcpy (fdata->F[num].serial, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_SERIAL;
            } else {
                /* If there is no SN on FAN module, then no need to read following info */
                continue;
            }

            wr_buf[1] = BMC_SUB2_MODEL;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                num = i * 2;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;

                num ++;
                memset (fdata->F[num].model, 0x00, sizeof(fdata->F[num].model));
                memcpy (fdata->F[num].model, &rd_buf[1], rd_buf[0]);
                fdata->F[num].fvalid |= FAN_INFO_VALID_MODEL;
            }

            wr_buf[1] = BMC_SUB2_MAX;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 5) && (ret == rd_buf[0] + 1)) {
                num = i * 2;
                fdata->F[num].max_speed  = (rd_buf[1] << 8) + rd_buf[2];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }

                num ++;
                fdata->F[num].max_speed  = (rd_buf[3] << 8) + rd_buf[4];
                if (fdata->F[num].max_speed != 0) {
                    fdata->F[num].percent = fdata->F[num].front_speed * 100 / fdata->F[num].max_speed;
                    fdata->F[num].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                }
            }

            wr_buf[1] = BMC_SUB2_DIR;
            ret = bf_pltfm_bmc_uart_write_read (
                    BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                    BMC_COMM_INTERVAL_US*2);

            if ((ret == 4) && (ret == rd_buf[0] + 1)) {
                if ((rd_buf[1] == 'F') && (rd_buf[2] == '2') && ((rd_buf[3] == 'R') || (rd_buf[3] == 'B'))) {
                    num = i * 2;
                    fdata->F[num].direction = 1;
                    fdata->F[num + 1].direction = 1;
                } else if (((rd_buf[1] == 'R') || (rd_buf[1] == 'B')) && (rd_buf[2] == '2') && (rd_buf[3] == 'F')) {
                    num = i * 2;
                    fdata->F[num].direction = 2;
                    fdata->F[num +1].direction = 2;
                }
            }
        }

    }

    return err;
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
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, 0, BMC_COMM_INTERVAL_US);
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
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, 0, BMC_COMM_INTERVAL_US);
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
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, 0, BMC_COMM_INTERVAL_US);
    }

    /* read back to checkout. */

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x312p__ (
    bf_pltfm_fan_info_t *fdata)
{
    int usec_delay = bf_pltfm_get_312_bmc_comm_interval();
    fdata = fdata;

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(V2P0)) {
        uint8_t buf[2] = {0};
        uint8_t res[I2C_SMBUS_BLOCK_MAX + 2];
        int rdlen;

        // fan status
        buf[0] = 0x00;
        buf[1] = fdata->speed_level;
        rdlen = bf_pltfm_bmc_write_read (0x3e, 0x7, buf,
                                        2, 0xff, res, sizeof(res), usec_delay);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
    } else if (platform_subtype_equal(V3P0) ||
               platform_subtype_equal(V4P0) ||
               platform_subtype_equal(V5P0)) {
        uint8_t buf[5] = {0};
        uint8_t data[32] = {0};
        int rdlen = 0;

        buf[0] = 0x04;
        buf[1] = 0x4c;
        buf[2] = 0x4a;
        buf[3] = 0x01;
        buf[4] = 0x22;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x31, buf, 5, 0xff, data, sizeof(data), usec_delay);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x04;
        buf[1] = 0x4c;
        buf[2] = 0x4c;
        buf[3] = 0x01;
        buf[4] = fdata->speed_level;
        rdlen = bf_pltfm_bmc_write_read(0x3e, 0x31, buf, 5, 0xff, data, sizeof(data), usec_delay);
        if (rdlen == -1) {
            LOG_ERROR("write fan speed to bmc error!\n");
            return BF_PLTFM_COMM_FAILED;
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x732q__ (
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
                                  BMC_CMD_FAN_SET, wr_buf, 2, 0, NULL, 0, BMC_COMM_INTERVAL_US);
    }

    /* read back to checkout. */

    return err;
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

    if (platform_type_equal (AFN_X532PT)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x532p__
              (fdata);
    } else if (platform_type_equal (AFN_X564PT)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x564p__
              (fdata);
    } else if (platform_type_equal (AFN_X308PT)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x308p__
              (fdata);
    } else if (platform_type_equal (AFN_X312PT)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x312p__
              (fdata);
    } else if (platform_type_equal (AFN_X732QT)) {
        err = __bf_pltfm_chss_mgmt_fan_data_get_x732q__
              (fdata);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
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

    if (platform_type_equal (AFN_X532PT)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x532p__
              (fdata);
    } else if (platform_type_equal (AFN_X564PT)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x564p__
              (fdata);
    } else if (platform_type_equal (AFN_X308PT)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x308p__
              (fdata);
    } else if (platform_type_equal (AFN_X312PT)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x312p__
              (fdata);
    } else if (platform_type_equal (AFN_X732QT)) {
        err = __bf_pltfm_chss_mgmt_fan_speed_set_x732q__
              (fdata);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
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

    fprintf (stdout, "%3s  %3s  %9s  %8s  %6s  %16s  %16s\n",
             "FAN", "GRP", "FRONT-RPM", "REAR-RPM", "SPEED%", "MODEL", "SERIAL");

    for (uint32_t i = 0;
         i < (bf_pltfm_mgr_ctx()->fan_group_count *
              bf_pltfm_mgr_ctx()->fan_per_group); i++) {
        fprintf (stdout,
                 "%3d  %3d  %9d  %8d  %5d%%  %16s  %16s\n",
                 fdata.F[i].fan_num,
                 fdata.F[i].group,
                 fdata.F[i].front_speed,
                 fdata.F[i].rear_speed,
                 fdata.F[i].percent,
                 fdata.F[i].model,
                 fdata.F[i].serial);
    }

    if (fdata.fantray_present) {
        fprintf (stdout, "Fan tray not present \n");
    } else {
        /* should we set the fan as we wanted ? */
    }

    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_bmc_data_fan_decode__ (uint8_t* p_src)
{
    uint8_t len  = p_src[0];
    uint8_t type = p_src[1];
    uint8_t num  = p_src[2];
    bf_pltfm_fan_data_t temp_fan_data;
    static bool need_read_sn[12];

    if ((type != 2) || (len != num * 5 + 2))  {
        return BF_PLTFM_INVALID_ARG;
    }

    clr_fan_data(&temp_fan_data);

    for (int i = 0, j = 0, k = 3; i < num; i ++, j += 2, k += 5) {
        temp_fan_data.F[j+0].group       = i + 1;
        temp_fan_data.F[j+0].fan_num     = j + 1;
        temp_fan_data.F[j+0].present     = p_src[k] & 0x01;
        temp_fan_data.F[j+0].direction   = (p_src[k] & 0x02) ? 1 : 2;
        temp_fan_data.F[j+0].front_speed = p_src[k + 1] + (p_src[k + 2] << 8);
        temp_fan_data.F[j+1].group       = i + 1;
        temp_fan_data.F[j+1].fan_num     = j + 2;
        temp_fan_data.F[j+1].present     = p_src[k] & 0x01;
        temp_fan_data.F[j+1].direction   = (p_src[k] & 0x02) ? 1 : 2;
        temp_fan_data.F[j+1].front_speed = p_src[k + 3] + (p_src[k + 4] << 8);

        if ((temp_fan_data.F[j+0].present == true) &&
            (bmc_fan_data.F[j+0].present == false)) {
            need_read_sn[j+0] = true;
        } else if (temp_fan_data.F[j+0].present == true) {
            if (need_read_sn[j+0]) {
                need_read_sn[j+0] = false;

                uint8_t wr_buf[2];
                uint8_t rd_buf[128];
                int ret = BF_PLTFM_COMM_FAILED;

                wr_buf[0] = i + 1;
                wr_buf[1] = BMC_SUB2_SN;
                ret = bf_pltfm_bmc_uart_write_read (
                        BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                        BMC_COMM_INTERVAL_US*2);

                if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                    memcpy (temp_fan_data.F[j+0].serial, &rd_buf[1], rd_buf[0]);
                    temp_fan_data.F[j+0].fvalid |= FAN_INFO_VALID_SERIAL;
                    memcpy (temp_fan_data.F[j+1].serial, &rd_buf[1], rd_buf[0]);
                    temp_fan_data.F[j+1].fvalid |= FAN_INFO_VALID_SERIAL;
                } else {
                    /* If there is no SN on FAN module, then no need to read following info */
                    continue;
                }

                wr_buf[1] = BMC_SUB2_MODEL;
                ret = bf_pltfm_bmc_uart_write_read (
                        BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                        BMC_COMM_INTERVAL_US*2);

                if ((ret == rd_buf[0] + 1) && (ret > 1)) {
                    memcpy (temp_fan_data.F[j+0].model, &rd_buf[1], rd_buf[0]);
                    temp_fan_data.F[j+0].fvalid |= FAN_INFO_VALID_MODEL;
                    memcpy (temp_fan_data.F[j+1].model, &rd_buf[1], rd_buf[0]);
                    temp_fan_data.F[j+1].fvalid |= FAN_INFO_VALID_MODEL;
                }

                wr_buf[1] = BMC_SUB2_MAX;
                ret = bf_pltfm_bmc_uart_write_read (
                        BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                        BMC_COMM_INTERVAL_US*2);

                if ((ret == 5) && (ret == rd_buf[0] + 1)) {
                    temp_fan_data.F[j+0].max_speed  = (rd_buf[1] << 8) + rd_buf[2];
                    if (temp_fan_data.F[j+0].max_speed != 0) {
                        temp_fan_data.F[j+0].percent = temp_fan_data.F[j+0].front_speed * 100 / temp_fan_data.F[j+0].max_speed;
                        temp_fan_data.F[j+0].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                    }
                    temp_fan_data.F[j+1].max_speed  = (rd_buf[3] << 8) + rd_buf[4];
                    if (temp_fan_data.F[j+1].max_speed != 0) {
                        temp_fan_data.F[j+1].percent = temp_fan_data.F[j+1].front_speed * 100 / temp_fan_data.F[j+1].max_speed;
                        temp_fan_data.F[j+1].fvalid |= FAN_INFO_VALID_MAX_SPEED;
                    }
                }

                wr_buf[1] = BMC_SUB2_DIR;
                ret = bf_pltfm_bmc_uart_write_read (
                        BMC_CMD_FAN_GET, wr_buf, 2, rd_buf, (128 - 1),
                        BMC_COMM_INTERVAL_US*2);

                if ((ret == 4) && (ret == rd_buf[0] + 1)) {
                    if ((rd_buf[1] == 'F') && (rd_buf[2] == '2') && ((rd_buf[3] == 'R') || (rd_buf[3] == 'B'))) {
                        temp_fan_data.F[j+0].direction = 1;
                        temp_fan_data.F[j+1].direction = 1;
                    } else if (((rd_buf[1] == 'R') || (rd_buf[1] == 'B')) && (rd_buf[2] == '2') && (rd_buf[3] == 'F')) {
                        temp_fan_data.F[j+0].direction = 2;
                        temp_fan_data.F[j+1].direction = 2;
                    }
                }
            } else {
                memcpy (temp_fan_data.F[j+0].serial, bmc_fan_data.F[j+0].serial, 32);
                memcpy (temp_fan_data.F[j+0].model,  bmc_fan_data.F[j+0].model,  32);
                temp_fan_data.F[j+0].direction = bmc_fan_data.F[j+0].direction;
                temp_fan_data.F[j+0].max_speed = bmc_fan_data.F[j+0].max_speed;
                if (temp_fan_data.F[j+0].max_speed != 0) {
                    temp_fan_data.F[j+0].percent = temp_fan_data.F[j+0].front_speed * 100 / temp_fan_data.F[j+0].max_speed;
                }
                temp_fan_data.F[j+0].fvalid = bmc_fan_data.F[j+0].fvalid;

                memcpy (temp_fan_data.F[j+1].serial, bmc_fan_data.F[j+1].serial, 32);
                memcpy (temp_fan_data.F[j+1].model,  bmc_fan_data.F[j+1].model,  32);
                temp_fan_data.F[j+1].direction = bmc_fan_data.F[j+1].direction;
                temp_fan_data.F[j+1].max_speed = bmc_fan_data.F[j+1].max_speed;
                if (temp_fan_data.F[j+1].max_speed != 0) {
                    temp_fan_data.F[j+1].percent = temp_fan_data.F[j+1].front_speed * 100 / temp_fan_data.F[j+1].max_speed;
                }
                temp_fan_data.F[j+1].fvalid = bmc_fan_data.F[j+1].fvalid;
            }
        } else {
            memset(&temp_fan_data.F[j+0], 0, sizeof (bf_pltfm_fan_info_t));
            memset(&temp_fan_data.F[j+1], 0, sizeof (bf_pltfm_fan_info_t));
        }
    }

    cpy_fan_data (&bmc_fan_data, &temp_fan_data);

    return BF_PLTFM_SUCCESS;
}
