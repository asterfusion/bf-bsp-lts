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

#define BMC_CMD_FAN_ALL 0x05
#define BMC_CMD_FAN_GET 0x06
#define BMC_CMD_FAN_SET 0x07
#define BMC_SUB2_RPM    0x00
#define BMC_SUB2_STATUS 0x01

static bf_pltfm_fan_data_t bmc_fan_data = {0};

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x532p__ (
    bf_pltfm_fan_data_t *fdata)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    uint32_t i = 0;
    int err = BF_PLTFM_COMM_FAILED, ret;

    if (g_access_bmc_through_uart) {
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;

        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_ALL, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
        if ((ret == 23) && (rd_buf[0] == 22)) {
            for (i = 0;
                 i < bf_pltfm_mgr_ctx()->fan_group_count *
                 bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
                fdata->F[i].fan_num     = i + 1;
                fdata->F[i].present     = (rd_buf[1] & (0x01 <<
                                                        (i >> 1))) ? 1 : 0;
                fdata->F[i].direction   = 1;
                fdata->F[i].front_speed = (rd_buf[2 * i + 3] << 8)
                                          + rd_buf[2 * i + 4];
                err = BF_PLTFM_SUCCESS;
            }
        }
    } else {
        for (i = 0;
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

    if (g_access_bmc_through_uart) {
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
                err = BF_PLTFM_SUCCESS;
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
    uint32_t i = 0;
    int err = BF_PLTFM_COMM_FAILED, ret;

    if (g_access_bmc_through_uart) {
        wr_buf[0] = 0xAA;
        wr_buf[1] = 0xAA;

        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_ALL, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
        if ((ret == 27) && (rd_buf[0] == 26)) {
            for (i = 0;
                 i < bf_pltfm_mgr_ctx()->fan_group_count *
                 bf_pltfm_mgr_ctx()->fan_per_group; i ++) {
                fdata->F[i].fan_num     = i + 1;
                fdata->F[i].present     = (rd_buf[1] & (0x01 <<
                                                        (i >> 1))) ? 1 : 0;
                fdata->F[i].direction   = 1;
                fdata->F[i].front_speed = (rd_buf[2 * i + 3] << 8)
                                          + rd_buf[2 * i + 4];
                err = BF_PLTFM_SUCCESS;
            }
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_data_get_x312p__ (
    bf_pltfm_fan_data_t *fdata)
{
    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v1dot2)) {
        uint8_t buf[2] = {0};
        uint8_t res[I2C_SMBUS_BLOCK_MAX + 2];
        int rdlen;
        int i;

        // fan status
        for (i = 0; i < 5; i++) {
            buf[0] = i;
            buf[1] = 0x01;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x6, buf,
                                            2, 0xff, res, 100000);
            if (rdlen == 4) {
                fdata->F[2 * i].fan_num = 2 * i + 1;
                fdata->F[2 * i].present = res[1] ? 1 : 0;
                fdata->F[2 * i].direction = res[2];

                fdata->F[2 * i + 1].fan_num = 2 * i + 2;
                fdata->F[2 * i + 1].present = res[1] ? 1 : 0;
                fdata->F[2 * i + 1].direction = res[2];
            }
        }

        // fan speed
        for (i = 0; i < 10; i++) {
            buf[0] = i;
            buf[1] = 0x00;
            rdlen = bf_pltfm_bmc_write_read (0x3e, 0x6, buf,
                                            2, 0xff, res, 100000);
            if (rdlen == 4) {
                if (i < 5) {
                    fdata->F[ (i % 5) * 2 + 0].front_speed = res[2] *
                            100;
                } else {
                    fdata->F[ (i % 5) * 2 + 1].front_speed = res[2] *
                            100;
                }
            }
        }
    }

    if (platform_subtype_equal(v1dot3)) {
        uint8_t buf[4] = {0};
        uint8_t data[3] = {0};
        uint8_t i = 0;
        int rdlen = 0;
        uint8_t num = 0;

        buf[0] = 0x1;
        buf[1] = 0x32;
        buf[3] = 0x1;
        for (i = 0; i < 10; i++) {
            buf[2] = i;
            rdlen = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, data, 10000);
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
    wr_buf[1] = fdata->percent;

    if (g_access_bmc_through_uart) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2);
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
    wr_buf[1] = fdata->percent;

    if (g_access_bmc_through_uart) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2);
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
    wr_buf[1] = fdata->percent;

    if (g_access_bmc_through_uart) {
        err = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_FAN_SET, wr_buf, 2, NULL, 0,
                  BMC_COMM_INTERVAL_US);
    } else {
        err = bf_pltfm_bmc_write (bmc_i2c_addr,
                                  BMC_CMD_FAN_SET, wr_buf, 2);
    }

    /* read back to checkout. */

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_fan_speed_set_x312p__ (
    bf_pltfm_fan_info_t *fdata)
{
    fdata = fdata;

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v1dot2)) {
        
    }

    if (platform_subtype_equal(v1dot3)) {

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

    memset (fdata, 0, sizeof (bf_pltfm_fan_data_t));

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
        memcpy (&bmc_fan_data, fdata,
                sizeof (bf_pltfm_fan_data_t));
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
    memcpy (fdata, &bmc_fan_data,
            sizeof (bf_pltfm_fan_data_t));

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
             "FAN  FRONT RPM  REAR RPM  MAX SPEED%%  \n");

    for (uint32_t i = 0;
         i < (bf_pltfm_mgr_ctx()->fan_group_count *
              bf_pltfm_mgr_ctx()->fan_per_group); i++) {
        fprintf (stdout,
                 "%2d     %5d     %5d      %3d%% \n",
                 fdata.F[i].fan_num,
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

