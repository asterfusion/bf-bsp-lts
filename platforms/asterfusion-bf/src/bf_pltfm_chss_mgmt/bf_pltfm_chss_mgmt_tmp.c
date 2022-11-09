/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_tmp.c
 * @date
 *
 * API's for reading temperature from BMC
 *
 */

/* Standard includes */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Module includes */
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"
#include "bf_pltfm_master_i2c.h"
#include "bf_pltfm_uart.h"
#include "chss_mgmt_tmp.h"
#include "chss_mgmt.h"

#define MAX_SWITCH_SENSORS 10

#define BMC_CMD_SENSOR_CNT_GET 0x03
#define BMC_CMD_SENSOR_TMP_GET 0x04

static bf_pltfm_temperature_info_t bmc_tmp_data;
static bf_pltfm_switch_temperature_info_t
switch_tmp_data[MAX_SWITCH_SENSORS];

static inline void clr_temp_info(bf_pltfm_temperature_info_t *dst)
{
    dst->tmp1   = 0;
    dst->tmp2   = 0;
    dst->tmp3   = 0;
    dst->tmp4   = 0;
    dst->tmp5   = 0;
    dst->tmp6   = 0;
    dst->tmp7   = 0;
    dst->tmp8   = 0;
    dst->tmp9   = 0;
    dst->tmp10  = 0;
}

static inline void cpy_temp_info(bf_pltfm_temperature_info_t *dst, bf_pltfm_temperature_info_t *src)
{
    dst->tmp1   = src->tmp1;
    dst->tmp2   = src->tmp2;
    dst->tmp3   = src->tmp3;
    dst->tmp4   = src->tmp4;
    dst->tmp5   = src->tmp5;
    dst->tmp6   = src->tmp6;
    dst->tmp7   = src->tmp7;
    dst->tmp8   = src->tmp8;
    dst->tmp9   = src->tmp9;
    dst->tmp10  = src->tmp10;
}

static inline void clr_stemp_info(bf_pltfm_switch_temperature_info_t *dst)
{
    dst->main_sensor   = 0;
    dst->remote_sensor = 0;
}

static inline void cpy_stemp_info(bf_pltfm_switch_temperature_info_t *dst, bf_pltfm_switch_temperature_info_t *src)
{
    dst->main_sensor   = src->main_sensor;
    dst->remote_sensor = src->remote_sensor;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get_x532p__ (
    bf_pltfm_temperature_info_t *tmp)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_SENSOR_TMP_GET, wr_buf, 2, rd_buf,
                  (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_SENSOR_TMP_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);

    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        tmp->tmp1 = (float)rd_buf[1];
        tmp->tmp2 = (float)rd_buf[2];
        tmp->tmp3 = (float)rd_buf[3];
        tmp->tmp4 = (float)rd_buf[4];
        tmp->tmp5 = (float)rd_buf[5];
        tmp->tmp6 = (float)rd_buf[6];

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get_x564p__ (
    bf_pltfm_temperature_info_t *tmp)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_SENSOR_TMP_GET, wr_buf, 2, rd_buf,
                  (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_SENSOR_TMP_GET, wr_buf, 2, 0xFF, rd_buf,
                                       BMC_COMM_INTERVAL_US);

    }

    if ((ret == 7) && (rd_buf[0] == 6)) {
        tmp->tmp1 = (float)rd_buf[1];
        tmp->tmp2 = (float)rd_buf[2];
        tmp->tmp3 = (float)rd_buf[3];
        tmp->tmp4 = (float)rd_buf[4];
        tmp->tmp5 = (float)rd_buf[5];
        tmp->tmp6 = (float)rd_buf[6];

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get_x308p__ (
    bf_pltfm_temperature_info_t *tmp)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_SENSOR_TMP_GET, wr_buf, 2, rd_buf,
                  (128 - 1),
                  BMC_COMM_INTERVAL_US);

        if ((ret == 11) && (rd_buf[0] == 10)) {
            tmp->tmp1 = (float)rd_buf[1];
            tmp->tmp2 = (float)rd_buf[2];
            tmp->tmp3 = (float)rd_buf[3];
            tmp->tmp10 = (float)rd_buf[4];

            /* Map Tofino temp to tmp8 and tmp9. And these two values come from BMC instead of from tofino. */
            tmp->tmp9 = (float)rd_buf[5];
            tmp->tmp8 = (float)rd_buf[6];

            if (rd_buf[7] == 0x9C) {
                tmp->tmp5 = -100.0;
                tmp->tmp4 = -100.0;
            } else {
                tmp->tmp5 = (float)rd_buf[7];
                tmp->tmp4 = (float)rd_buf[8];
            }
            if (rd_buf[9] == 0x9C) {
                tmp->tmp7 = -100.0;
                tmp->tmp6 = -100.0;
            } else {
                tmp->tmp7 = (float)rd_buf[9];
                tmp->tmp6= (float)rd_buf[10];
            }

            err = BF_PLTFM_SUCCESS;
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get_x312p__ (
    bf_pltfm_temperature_info_t *tmp)
{
    int usec_delay = BMC_COMM_INTERVAL_US/25;
    uint8_t buf[4] = {0};
    uint8_t ghc1_temp[2][3] = {{0}};
    uint8_t ghc2_temp[2][3] = {{0}};
    uint8_t tmpdata[3] = {0};
    uint8_t tmpbuff[4] = {0};
    int err, rdlen1, rdlen2;
    bf_pltfm_switch_temperature_info_t stemp = {0, 0};

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(v2dot0)) {
        uint8_t lm75_63[3] = {0};

        /*00: 02 <LM75 temp> <LM63 temp>*/
        buf[0] = 0xaa;
        buf[1] = 0xaa;
        rdlen1 = bf_pltfm_bmc_write_read (0x3e, 0x4, buf,
                                        2, 0xff, lm75_63, usec_delay);
        if (rdlen1 < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        if (rdlen1 == 3) {
            tmp->tmp1 = (float)lm75_63[1];      // LM75
            tmp->tmp2 = (float)lm75_63[2];      // LM63
        }

        /* No LM86 in hw version 2. */
        tmp->tmp3 = 0;

        /*** GHC ***/
        /* There's a bug during reading GHC's temp while actually the GHC is absent (return previous data).
         * To avoid this, we have to read at least once to clear the sticky data. */
        tmpbuff[0] = 0x03;
        tmpbuff[1] = 0x32;
        tmpbuff[2] = 0x2;
        tmpbuff[3] = 0x1;
        rdlen1 = bf_pltfm_bmc_write_read(0x3e, 0x30, tmpbuff, 4, 0xff, tmpdata, usec_delay);
        if (rdlen1 < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x03;
        buf[1] = 0x18;
        buf[2] = 0x0;
        buf[3] = 0x1;
        rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc1_temp[0][0], usec_delay);
        if ((rdlen1 == 3) && (rdlen2 == 3) && (tmpdata[2] != ghc1_temp[0][2])) {
            tmp->tmp5 = ghc1_temp[0][2];

            buf[2] = 0x1;
            rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc1_temp[1][0], usec_delay);
            if (rdlen2 == 3) {
                tmp->tmp4 = ghc1_temp[1][2];
            }
        }

        tmpbuff[0] = 0x03;
        tmpbuff[1] = 0x32;
        tmpbuff[2] = 0x2;
        tmpbuff[3] = 0x1;
        rdlen1 = bf_pltfm_bmc_write_read(0x3e, 0x30, tmpbuff, 4, 0xff, tmpdata, usec_delay);
        if (rdlen1 < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x03;
        buf[1] = 0x18;
        buf[2] = 0x0;
        buf[3] = 0x1;
        rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc2_temp[0][0], usec_delay);
        if ((rdlen1 == 3) && (rdlen2 == 3) && (tmpdata[2] != ghc2_temp[0][2])) {
            tmp->tmp7 = ghc2_temp[0][2];

            buf[2] = 0x1;
            rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc2_temp[1][0], usec_delay);
            if (rdlen2 == 3) {
                tmp->tmp6 = ghc2_temp[1][2];
            }
        }
        /* Map Tofino temp to tmp8 and tmp9. And these two values come from tofino instead of from BMC. */
        bf_pltfm_chss_mgmt_switch_temperature_get (0, 0, &stemp);
        tmp->tmp8 = (float)(stemp.main_sensor / 1000);
        tmp->tmp9 = (float)(stemp.remote_sensor / 1000);

    } else if (platform_subtype_equal(v3dot0) ||
               platform_subtype_equal(v4dot0)) {
        uint8_t lm75[3] = {0};
        uint8_t lm63[3] = {0};
        uint8_t lm86[3] = {0};

        /*00: 02 00 <LM75>*/
        buf[0] = 0x04;
        buf[1] = 0x4a;
        buf[2] = 0x0;
        buf[3] = 0x1;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, lm75, usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        /*00: 02 00 <LM63>*/
        buf[0] = 0x04;
        buf[1] = 0x4c;
        buf[2] = 0x0;
        buf[3] = 0x1;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, lm63, usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        /*00: 02 00 <LM86>*/
        buf[0] = 0x03;
        buf[1] = 0x4c;
        buf[2] = 0x0;
        buf[3] = 0x1;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, lm86, usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        tmp->tmp1 = (float)lm75[2];
        tmp->tmp2 = (float)lm63[2];
        tmp->tmp3 = (float)lm86[2];

        /*** GHC ***/
        /* There's a bug during reading GHC's temp while actually the GHC is absent (return previous data).
         * To avoid this, we have to read at least once to clear the sticky data. */
        tmpbuff[0] = 0x03;
        tmpbuff[1] = 0x32;
        tmpbuff[2] = 0x2;
        tmpbuff[3] = 0x1;
        rdlen1 = bf_pltfm_bmc_write_read(0x3e, 0x30, tmpbuff, 4, 0xff, tmpdata, usec_delay);
        if (rdlen1 < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x02;
        buf[1] = 0x18;
        buf[2] = 0x0;
        buf[3] = 0x1;
        rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc1_temp[0][0], usec_delay);
        if ((rdlen1 == 3) && (rdlen2 == 3) && (tmpdata[2] != ghc1_temp[0][2])) {
            tmp->tmp5 = ghc1_temp[0][2];

            buf[2] = 0x1;
            rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc1_temp[1][0], usec_delay);
            if (rdlen2 == 3) {
                tmp->tmp4 = ghc1_temp[1][2];
            }
        }

        tmpbuff[0] = 0x03;
        tmpbuff[1] = 0x32;
        tmpbuff[2] = 0x2;
        tmpbuff[3] = 0x1;
        rdlen1 = bf_pltfm_bmc_write_read(0x3e, 0x30, tmpbuff, 4, 0xff, tmpdata, usec_delay);
        if (rdlen1 < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        buf[0] = 0x06;
        buf[1] = 0x18;
        buf[2] = 0x0;
        buf[3] = 0x1;
        rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc2_temp[0][0], usec_delay);
        if ((rdlen1 == 3) && (rdlen2 == 3) && (tmpdata[2] != ghc2_temp[0][2])) {
            tmp->tmp7 = ghc2_temp[0][2];

            buf[2] = 0x1;
            rdlen2 = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, &ghc2_temp[1][0], usec_delay);
            if (rdlen2 == 3) {
                tmp->tmp6 = ghc2_temp[1][2];
            }
        }

        bf_pltfm_chss_mgmt_switch_temperature_get (0, 0, &stemp);
        tmp->tmp8 = (float)(stemp.main_sensor / 1000);
        tmp->tmp9 = (float)(stemp.remote_sensor / 1000);
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get_hc36y24c__ (
    bf_pltfm_temperature_info_t *tmp)
{
    tmp = tmp;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_temperature_get__ (
    bf_pltfm_temperature_info_t *tmp)
{
    int err = BF_PLTFM_COMM_FAILED;

    if (tmp == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    clr_temp_info (tmp);

    if (platform_type_equal (X532P)) {
        err = __bf_pltfm_chss_mgmt_temperature_get_x532p__
              (tmp);
    } else if (platform_type_equal (X564P)) {
        err = __bf_pltfm_chss_mgmt_temperature_get_x564p__
              (tmp);
    } else if (platform_type_equal (X308P)) {
        err = __bf_pltfm_chss_mgmt_temperature_get_x308p__
              (tmp);
    } else if (platform_type_equal (X312P)) {
        err = __bf_pltfm_chss_mgmt_temperature_get_x312p__
              (tmp);
    } else if (platform_type_equal (HC)) {
        err = __bf_pltfm_chss_mgmt_temperature_get_hc36y24c__
              (tmp);
    }

    if (!err) {
        /* Write to cache. */
        cpy_temp_info (&bmc_tmp_data, tmp);
    }

    return err;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_switch_temperature_get__ (
    bf_dev_id_t dev_id, int sensor,
    bf_pltfm_switch_temperature_info_t *tmp)
{
    uint32_t temp_mC = 0;
    uint32_t timeout = 0;
    bf_status_t err;

    clr_stemp_info (tmp);

    if (BF_SUCCESS !=
        bf_serdes_temperature_read_start (
            dev_id, sensor, BF_SDS_MAIN_TEMP_SENSOR_CH)) {
        LOG_ERROR ("Error in starting main temperature read of tofino\n");
        return BF_PLTFM_COMM_FAILED;
    }

    do {
        usleep (TEN_MILLISECOND);
        timeout = timeout + TEN_MILLISECOND;
        err = bf_serdes_temperature_read_get (
                  dev_id, sensor, BF_SDS_MAIN_TEMP_SENSOR_CH,
                  &temp_mC);
    } while ((err == BF_NOT_READY) &&
             (timeout < HUNDRED_MILLISECOND));

    if (err != BF_SUCCESS) {
        LOG_ERROR ("Error in getting main temperature from tofino\n");
        return BF_PLTFM_COMM_FAILED;
    }

    tmp->main_sensor = temp_mC;

    temp_mC = 0;
    timeout = 0;

    if (BF_SUCCESS !=
        bf_serdes_temperature_read_start (
            dev_id, sensor, BF_SDS_REMOTE_TEMP_SENSOR_0_CH)) {
        LOG_ERROR ("Error in starting remote temperature read of tofino\n");
        return BF_PLTFM_COMM_FAILED;
    }

    bf_serdes_temperature_read_get (
        dev_id, sensor, BF_SDS_REMOTE_TEMP_SENSOR_0_CH,
        &temp_mC);

    do {
        usleep (TEN_MILLISECOND);
        timeout = timeout + TEN_MILLISECOND;
        err = bf_serdes_temperature_read_get (
                  dev_id, sensor, BF_SDS_REMOTE_TEMP_SENSOR_0_CH,
                  &temp_mC);
    } while ((err == BF_NOT_READY) &&
             (timeout < HUNDRED_MILLISECOND));

    if (err != BF_SUCCESS) {
        LOG_ERROR ("Error in getting remote temperature from tofino\n");
        return BF_PLTFM_COMM_FAILED;
    }

    tmp->remote_sensor = temp_mC;

    /* Write to cache. */
    cpy_stemp_info (&switch_tmp_data[sensor %
                                    MAX_SWITCH_SENSORS], tmp);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_temperature_get (
    bf_pltfm_temperature_info_t *tmp)
{
    if (tmp == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    cpy_temp_info (tmp, &bmc_tmp_data);

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_switch_temperature_get (
    bf_dev_id_t dev_id, int sensor,
    bf_pltfm_switch_temperature_info_t *tmp)
{
    if ((tmp == NULL) || (sensor != 0)) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    cpy_stemp_info (tmp, &switch_tmp_data[sensor %
                                         MAX_SWITCH_SENSORS]);
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t pltfm_mgr_sensor_out_get (
    const char *options,
    char *info,
    size_t info_size)
{
    char sensors_out[BF_SNSR_OUT_BUF_SIZE] = {0};
    /* Prepare sensors_out for callers who want get these by RPC. */
    options = options;
    info = info;

    /* Read from cache. */
    /* Fixed compiled errors with gcc-8 in debian 10 */
    strncpy (info, (char *)&sensors_out[0], info_size > strlen ((char *)&sensors_out[0]) ? strlen ((char *)&sensors_out[0]) : (info_size - 1));

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_tmp_init()
{
    bf_pltfm_temperature_info_t t;
    bf_pltfm_switch_temperature_info_t *s;

    fprintf (stdout,
             "\n\n================== TMPs INIT ==================\n");

    clr_temp_info (&bmc_tmp_data);
    for (int i = 0; i < MAX_SWITCH_SENSORS; i ++) {
        s = &switch_tmp_data[i];
        clr_stemp_info (s);
    }

    if (__bf_pltfm_chss_mgmt_temperature_get__ (
            &t) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading temperature \n");
    } else {
        if (platform_type_equal (X532P) ||
            platform_type_equal (X564P)) {
            fprintf (stdout, "tmp1    %.1f C   \"%s\"\n",
                     t.tmp1, "Far left of mainboard");
            fprintf (stdout, "tmp2    %.1f C   \"%s\"\n",
                     t.tmp2, "Far right of mainboard");
            fprintf (stdout, "tmp3    %.1f C   \"%s\"\n",
                     t.tmp3, "ASIC Ambient");
            fprintf (stdout, "tmp4    %.1f C   \"%s\"\n",
                     t.tmp4, "ASIC Junction");
            fprintf (stdout, "tmp5    %.1f C   \"%s\"\n",
                     t.tmp5, "Fan 1");
            fprintf (stdout, "tmp6    %.1f C   \"%s\"\n",
                     t.tmp6, "Fan 2");
            /* Added more sensors. */
        } else if (platform_type_equal (X308P)) {
            fprintf (stdout,
                "tmp1    %.1f C   \"%s\"\n",
                        t.tmp1, "Far left of mainboard");
            fprintf (stdout,
                "tmp2    %.1f C   \"%s\"\n",
                        t.tmp2, "Far right of mainboard");
            fprintf (stdout,
                "tmp3    %.1f C   \"%s\"\n",
                        t.tmp3, "Fan 1");
            // if == -100.0, means no GHC-0 installed
            if (t.tmp4 != -100.0) {
                fprintf (stdout,
                    "tmp4    %.1f C   \"%s\"\n",
                            t.tmp4, "GHC-1 Junction");
                fprintf (stdout,
                    "tmp5    %.1f C   \"%s\"\n",
                            t.tmp5, "GHC-1 Ambient");
            }
            // if == -100.0, means no GHC-0 installed
            if (t.tmp6 != -100.0) {
                fprintf (stdout,
                    "tmp6   %.1f C   \"%s\"\n",
                            t.tmp6, "GHC-2 Junction");
                fprintf (stdout,
                    "tmp7    %.1f C   \"%s\"\n",
                            t.tmp7, "GHC-2 Ambient");
            }
            fprintf (stdout,
                "tmp8    %.1f C   \"%s\"\n",
                        t.tmp8, "BF Junction");
            fprintf (stdout,
                "tmp9    %.1f C   \"%s\"\n",
                        t.tmp9, "BF Ambient");
            fprintf (stdout,
                "tmp10    %.1f C   \"%s\"\n",
                        t.tmp10, "Fan 2");
            // The belowing code makes no sense due to there's no necessarily logic relationship about how many fantray should be inserted.
            // We can still have all 6 fans even though without GHCs and on the other hand we can keep 4 fans even though with 1 or 2 GHCs.
            // And in last case there may be a risk of heat dissipation but the logic is no problem.
            // By tsihang, 2022-07-26.
            // if no GHC0 and GHC1, only have 4 * 2 fans
            //if (bf_pltfm_mgr_ctx()->sensor_count == 6) {
            //    bf_pltfm_mgr_ctx()->fan_group_count = 4;
            //}
            /* Added more sensors. */
        } else if (platform_type_equal (X312P)) {
            fprintf (stdout, "tmp1    %.1f C   \"%s\"\n",
                     t.tmp1, "lm75");
            fprintf (stdout, "tmp2    %.1f C   \"%s\"\n",
                     t.tmp2, "lm63");
            fprintf (stdout, "tmp3    %.1f C   \"%s\"\n",
                     t.tmp3, (platform_subtype_equal(v3dot0) || platform_subtype_equal(v4dot0)) ? "lm86" : "Not Defined");
            // if == -100.0, means no GHC-0 installed
            if (t.tmp4 != -100.0) {
                fprintf (stdout,
                    "tmp4    %.1f C   \"%s\"\n",
                            t.tmp4, "GHC-1 Junction");
                fprintf (stdout,
                    "tmp5    %.1f C   \"%s\"\n",
                            t.tmp5, "GHC-1 Ambient");
            }
            // if == -100.0, means no GHC-0 installed
            if (t.tmp6 != -100.0) {
                fprintf (stdout,
                    "tmp6   %.1f C   \"%s\"\n",
                            t.tmp6,"GHC-2 Junction");
                fprintf (stdout,
                    "tmp7    %.1f C   \"%s\"\n",
                            t.tmp7, "GHC-2 Ambient");
            }
            fprintf (stdout,
                "tmp8    %.1f C   \"%s\"\n",
                        t.tmp8, "BF Junction");
            fprintf (stdout,
                "tmp9    %.1f C   \"%s\"\n",
                        t.tmp9, "BF Ambient");
            fprintf (stdout,
                "tmp10    %.1f C   \"%s\"\n",
                        t.tmp10, "Not Defined");
            /* Added more sensors. */
        } else if (platform_type_equal (HC)) {
            fprintf (stdout, "tmp1    %.1f C   \"%s\"\n",
                     t.tmp1, "Unknown");
            fprintf (stdout, "tmp2    %.1f C   \"%s\"\n",
                     t.tmp2, "Unknown");
            fprintf (stdout, "tmp3    %.1f C   \"%s\"\n",
                     t.tmp3, "Unknown");
            fprintf (stdout, "tmp4    %.1f C   \"%s\"\n",
                     t.tmp4, "Unknown");
            fprintf (stdout, "tmp5    %.1f C   \"%s\"\n",
                     t.tmp5, "Unknown");
            fprintf (stdout, "tmp6    %.1f C   \"%s\"\n",
                     t.tmp6, "Unknown");
            /* Added more sensors. */
        }
    }

    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}

