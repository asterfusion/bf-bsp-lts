/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_chss_mgmt_vrail.c
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

/* Local header includes */
#include "bf_pltfm_bd_eeprom.h"
#include "bf_pltfm_master_i2c.h"
#include "bf_pltfm_uart.h"
#include "chss_mgmt_tmp.h"
#include "chss_mgmt.h"

#define BMC_CMD_VRAIL_GET 0x08

bf_pltfm_pwr_rails_info_t bmc_vrail_data;


static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_x532p__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_VRAIL_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_VRAIL_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }
    if ((ret == 17) && (rd_buf[0] == 16)) {
        pwr_rails->vrail1 = rd_buf[11] * 1000 +
                            rd_buf[12] *
                            100;     /* For SONIC, 1 pwr_rails needed. */
        pwr_rails->vrail2 = rd_buf[13] * 1000 +
                            rd_buf[14] * 100;
        pwr_rails->vrail3 = rd_buf[1]  * 1000 +
                            rd_buf[2]  * 100;
        pwr_rails->vrail4 = rd_buf[3]  * 1000 +
                            rd_buf[4]  * 100;
        pwr_rails->vrail5 = rd_buf[5]  * 1000 +
                            rd_buf[6]  * 100;
        pwr_rails->vrail6 = rd_buf[15] * 1000 +
                            rd_buf[16] * 100;
        pwr_rails->vrail7 = rd_buf[7]  * 1000 +
                            rd_buf[8]  * 100;
        pwr_rails->vrail8 = rd_buf[9]  * 1000 +
                            rd_buf[10] * 100;

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_x564p__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_VRAIL_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_VRAIL_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }
    if ((ret == 17) && (rd_buf[0] == 16)) {
        pwr_rails->vrail1 = rd_buf[11] * 1000 +
                            rd_buf[12] *
                            100;     /* For SONIC, 1 pwr_rails needed. */
        pwr_rails->vrail2 = rd_buf[13] * 1000 +
                            rd_buf[14] * 100;
        pwr_rails->vrail3 = rd_buf[1]  * 1000 +
                            rd_buf[2]  * 100;
        pwr_rails->vrail4 = rd_buf[3]  * 1000 +
                            rd_buf[4]  * 100;
        pwr_rails->vrail5 = rd_buf[5]  * 1000 +
                            rd_buf[6]  * 100;
        pwr_rails->vrail6 = rd_buf[15] * 1000 +
                            rd_buf[16] * 100;
        pwr_rails->vrail7 = rd_buf[7]  * 1000 +
                            rd_buf[8]  * 100;
        pwr_rails->vrail8 = rd_buf[9]  * 1000 +
                            rd_buf[10] * 100;

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_x308p__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_VRAIL_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);

        if ((ret == 17) && (rd_buf[0] == 16)) {
            pwr_rails->vrail1 = rd_buf[11] * 1000 +
                                rd_buf[12] *
                                100;     /* For SONIC, 1 pwr_rails needed. */
            pwr_rails->vrail2 = rd_buf[13] * 1000 +
                                rd_buf[14] * 100;
            pwr_rails->vrail3 = rd_buf[1]  * 1000 +
                                rd_buf[2]  * 100;
            pwr_rails->vrail4 = rd_buf[3]  * 1000 +
                                rd_buf[4]  * 100;
            pwr_rails->vrail5 = rd_buf[5]  * 1000 +
                                rd_buf[6]  * 100;
            pwr_rails->vrail6 = rd_buf[15] * 1000 +
                                rd_buf[16] * 100;
            pwr_rails->vrail7 = rd_buf[7]  * 1000 +
                                rd_buf[8]  * 100;
            pwr_rails->vrail8 = rd_buf[9]  * 1000 +
                                rd_buf[10] * 100;

            err = BF_PLTFM_SUCCESS;
        }
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_x312p__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    int usec_delay = bf_pltfm_get_312_bmc_comm_interval();

    /* Example code for a subversion in a given platform. */
    if (platform_subtype_equal(V2P0)) {
        /* Not supported in v2. */
        pwr_rails = pwr_rails;
    } else if (platform_subtype_equal(V3P0) ||
               platform_subtype_equal(V4P0) ||
               platform_subtype_equal(V5P0)) {
        uint8_t buf[4] = {0};
        int err;
        uint8_t vrail_data[3] = {0};

        /*00: 02 <byte low> <byte high>>*/
        buf[0] = 0x07;
        buf[1] = 0x66;
        buf[2] = 0xd4;
        buf[3] = 0x02;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, vrail_data, sizeof(vrail_data), usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }

        pwr_rails->vrail1 = ((vrail_data[2] << 8) + vrail_data[1])
                            * 1000 / 512;
                            /* For SONIC, 1 pwr_rails needed. */

        /*00: 02 <byte low> <byte high>>*/
        buf[0] = 0x07;
        buf[1] = 0x65;
        buf[2] = 0xd4;
        buf[3] = 0x02;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, vrail_data, sizeof(vrail_data), usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }
        pwr_rails->vrail6 = ((vrail_data[2] << 8) + vrail_data[1])
                            * 1000 / 512;
                            /* For SONIC, 1 pwr_rails needed. */

        /*00: 02 <byte low> <byte high>>*/
        buf[0] = 0x07;
        buf[1] = 0x64;
        buf[2] = 0xd4;
        buf[3] = 0x02;
        err = bf_pltfm_bmc_write_read(0x3e, 0x30, buf, 4, 0xff, vrail_data, sizeof(vrail_data), usec_delay);
        if (err < 0) {
            LOG_ERROR("BMC read write error \n");
            return BF_PLTFM_COMM_FAILED;
        }
        pwr_rails->vrail2 = ((vrail_data[2] << 8) + vrail_data[1])
                            * 1000 / 512;
                            /* For SONIC, 1 pwr_rails needed. */
    }
    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_x732q__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    uint8_t wr_buf[2];
    uint8_t rd_buf[128];
    int err = BF_PLTFM_COMM_FAILED, ret;

    wr_buf[0] = 0xAA;
    wr_buf[1] = 0xAA;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
        ret = bf_pltfm_bmc_uart_write_read (
                  BMC_CMD_VRAIL_GET, wr_buf, 2, rd_buf, (128 - 1),
                  BMC_COMM_INTERVAL_US);
    } else {
        ret = bf_pltfm_bmc_write_read (bmc_i2c_addr,
                                       BMC_CMD_VRAIL_GET, wr_buf, 2, 0xFF, rd_buf, sizeof(rd_buf),
                                       BMC_COMM_INTERVAL_US);
    }
    if ((ret == 17) && (rd_buf[0] == 16)) {
        pwr_rails->vrail1 = rd_buf[11] * 1000 +
                            rd_buf[12] *
                            100;     /* For SONIC, 1 pwr_rails needed. */
        pwr_rails->vrail2 = rd_buf[13] * 1000 +
                            rd_buf[14] * 100;
        pwr_rails->vrail3 = rd_buf[1]  * 1000 +
                            rd_buf[2]  * 100;
        pwr_rails->vrail4 = rd_buf[3]  * 1000 +
                            rd_buf[4]  * 100;
        pwr_rails->vrail5 = rd_buf[5]  * 1000 +
                            rd_buf[6]  * 100;
        pwr_rails->vrail6 = rd_buf[15] * 1000 +
                            rd_buf[16] * 100;
        pwr_rails->vrail7 = rd_buf[7]  * 1000 +
                            rd_buf[8]  * 100;
        pwr_rails->vrail8 = rd_buf[9]  * 1000 +
                            rd_buf[10] * 100;

        err = BF_PLTFM_SUCCESS;
    }

    return err;
}

static bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get_hc36y24c__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    pwr_rails = pwr_rails;
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_pwr_rails_get__ (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    int err = BF_PLTFM_COMM_FAILED;

    if (pwr_rails == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    memset (pwr_rails, 0,
            sizeof (bf_pltfm_pwr_rails_info_t));
    if (platform_type_equal (AFN_X532PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_x532p__
              (pwr_rails);
    } else if (platform_type_equal (AFN_X564PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_x564p__
              (pwr_rails);
    } else if (platform_type_equal (AFN_X308PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_x308p__
              (pwr_rails);
    } else if (platform_type_equal (AFN_X312PT)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_x312p__
              (pwr_rails);
    } else if (platform_type_equal (AFN_X732QT)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_x732q__
              (pwr_rails);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        err = __bf_pltfm_chss_mgmt_pwr_rails_get_hc36y24c__
              (pwr_rails);
    }

    memcpy (&bmc_vrail_data, pwr_rails,
            sizeof (bf_pltfm_pwr_rails_info_t));

    return err;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_rails_get (
    bf_pltfm_pwr_rails_info_t *pwr_rails)
{
    if (pwr_rails == NULL) {
        LOG_ERROR ("Invalid paramter NULL\n");
        return BF_PLTFM_INVALID_ARG;
    }

    /* Read from cache. */
    memcpy (pwr_rails, &bmc_vrail_data,
            sizeof (bf_pltfm_pwr_rails_info_t));

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
bf_pltfm_chss_mgmt_pwr_rails_init()
{
    bf_pltfm_pwr_rails_info_t t;

    memset (&bmc_vrail_data, 0,
        sizeof (bf_pltfm_pwr_rails_info_t));

    if (__bf_pltfm_chss_mgmt_pwr_rails_get__ (
            &t) != BF_PLTFM_SUCCESS) {
        fprintf (stdout,
                 "Error in reading rails voltages\n");
        return -1;
    }
    if (platform_type_equal (AFN_X312PT)) {
        if (platform_subtype_equal (V2P0)) {
            /* Not supported. */
        } else {
            /* V3 and later. */
            fprintf (stdout,
                     "Barefoot_Core_0V9   %5d mV\n", t.vrail1);
            fprintf (stdout,
                     "Barefoot_AVDD_0V9   %5d mV\n", t.vrail2);
            fprintf (stdout,
                     "Payload_2V5         %5d mV\n", t.vrail6);
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        fprintf (stdout,
                 "Barefoot_VDDA_1V8   %5d mV\n", t.vrail1);
        fprintf (stdout,
                 "Barefoot_VDDA_1V7   %5d mV\n", t.vrail2);
        fprintf (stdout,
                 "Payload_12V         %5d mV\n", t.vrail3);
        fprintf (stdout,
                 "Payload_3V3         %5d mV\n", t.vrail4);
        fprintf (stdout,
                 "Payload_5V0         %5d mV\n", t.vrail5);
        fprintf (stdout,
                 "Barefoot_VDD_1V8    %5d mV\n", t.vrail6);
        fprintf (stdout,
                 "Barefoot_Core_Volt  %5d mV\n", t.vrail7);
        fprintf (stdout,
                 "Barefoot_VDDT_0V86  %5d mV\n", t.vrail8);
    } else {
        fprintf (stdout,
                 "Barefoot_Core_0V9   %5d mV\n", t.vrail1);
        fprintf (stdout,
                 "Barefoot_AVDD_0V9   %5d mV\n", t.vrail2);
        fprintf (stdout,
                 "Payload_12V         %5d mV\n", t.vrail3);
        fprintf (stdout,
                 "Payload_3V3         %5d mV\n", t.vrail4);
        fprintf (stdout,
                 "Payload_5V0         %5d mV\n", t.vrail5);
        fprintf (stdout,
                 "Payload_2V5         %5d mV\n", t.vrail6);
        fprintf (stdout,
                 "88E6131_1V9         %5d mV\n", t.vrail7);
        fprintf (stdout,
                 "88E6131_1V2         %5d mV\n", t.vrail8);
    }
    fprintf (stdout, "\n\n");
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t
__bf_pltfm_chss_mgmt_bmc_data_vrail_decode__ (uint8_t* p_src)
{
    uint8_t len  = p_src[0];
    uint8_t type = p_src[1];
    uint8_t num  = p_src[2];
    bf_pltfm_pwr_rails_info_t temp_vrail_data;

    if ((type != 4) || (len != num * 2 + 2))  {
        return BF_PLTFM_INVALID_ARG;
    }

    memset (&temp_vrail_data, 0, sizeof(bf_pltfm_pwr_rails_info_t));

    temp_vrail_data.vrail1 = p_src[13] * 1000 + p_src[14] * 100;
    temp_vrail_data.vrail2 = p_src[15] * 1000 + p_src[16] * 100;
    temp_vrail_data.vrail3 = p_src[3]  * 1000 + p_src[4]  * 100;
    temp_vrail_data.vrail4 = p_src[5]  * 1000 + p_src[6]  * 100;
    temp_vrail_data.vrail5 = p_src[7]  * 1000 + p_src[8]  * 100;
    temp_vrail_data.vrail6 = p_src[17] * 1000 + p_src[18] * 100;
    temp_vrail_data.vrail7 = p_src[9]  * 1000 + p_src[10] * 100;
    temp_vrail_data.vrail8 = p_src[11] * 1000 + p_src[12] * 100;

    memcpy (&bmc_vrail_data, &temp_vrail_data, sizeof (bf_pltfm_pwr_rails_info_t));

    return BF_PLTFM_SUCCESS;
}
