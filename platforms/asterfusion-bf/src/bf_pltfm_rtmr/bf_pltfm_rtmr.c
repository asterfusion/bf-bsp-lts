/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
/*!
 * @file bf_pltfm_rtmr.c
 * @date
 *
 * API's for reading and writing to retimers for Mavericks
 *
 */

/* Compile time knobs */
#define RTMR_DO_CP2112_I2C 0
#define RTMR_DO_CHECK_STATUS 1
#define RTMR_TOFINO_REG_WRITE_ENABLE 0

/* Standard includes */
#include <stdio.h>

/* Module includes */
#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_ext_phy.h>
#include <bf_pltfm_rtmr.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_types/bf_types.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>
#include <lld/lld_gpio_if.h>
#include <bf_mav_qsfp_i2c_lock.h>

/* Local header includes */
#include "bf_pltfm_rtmr_priv.h"

#if !RTMR_DO_CP2112_I2C
static uint8_t tf_i2c_init = 0;
#endif

#define TOFINO_PIN_PAIR BF_IO_PIN_PAIR_8_9
/* Just under 400Khz */
//#define TOFINO_I2C_PERIOD 0x2C
/* 400 Khz */
//#define TOFINO_I2C_PERIOD 0x26
/* 460 Khz */
#define TOFINO_I2C_PERIOD 0x20
/* 1M */
//#define TOFINO_I2C_PERIOD 0xb

/* Retimer I2C address map for Mavericks P0C board */
static bf_pltfm_rtmr_i2c_addr_t
rtmr_map[][MAX_RTMR] = {
    /* Rtmr_id    Cp2112_id    Channel Num   I2c Addr     */
    /*{RETIMER_X, CP2112_ID_X, PCA9548_CH_X, I2C_ADDR_XX} */
    {
        {
            RTMR_0,
            CP2112_ID_2,
            RTMR_PCA9548_CH_0,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_7
        },
        {
            RTMR_1,
            CP2112_ID_2,
            RTMR_PCA9548_CH_0,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_6
        },
        {
            RTMR_2,
            CP2112_ID_2,
            RTMR_PCA9548_CH_0,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_5
        },
        {
            RTMR_3,
            CP2112_ID_2,
            RTMR_PCA9548_CH_0,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_4
        },

        {
            RTMR_4,
            CP2112_ID_2,
            RTMR_PCA9548_CH_1,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_3
        },
        {
            RTMR_5,
            CP2112_ID_2,
            RTMR_PCA9548_CH_1,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_2
        },
        {
            RTMR_6,
            CP2112_ID_2,
            RTMR_PCA9548_CH_1,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_1
        },
        {
            RTMR_7,
            CP2112_ID_2,
            RTMR_PCA9548_CH_1,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_0
        },

        {
            RTMR_8,
            CP2112_ID_2,
            RTMR_PCA9548_CH_2,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_15
        },
        {
            RTMR_9,
            CP2112_ID_2,
            RTMR_PCA9548_CH_2,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_14
        },
        {
            RTMR_10,
            CP2112_ID_2,
            RTMR_PCA9548_CH_2,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_13
        },
        {
            RTMR_11,
            CP2112_ID_2,
            RTMR_PCA9548_CH_2,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_12
        },

        {
            RTMR_12,
            CP2112_ID_2,
            RTMR_PCA9548_CH_3,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_11
        },
        {
            RTMR_13,
            CP2112_ID_2,
            RTMR_PCA9548_CH_3,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_10
        },
        {
            RTMR_14,
            CP2112_ID_2,
            RTMR_PCA9548_CH_3,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_9
        },
        {
            RTMR_15,
            CP2112_ID_2,
            RTMR_PCA9548_CH_3,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_40,
            BF_RTMR_PCA9535_CH_8
        },

        {
            RTMR_16,
            CP2112_ID_2,
            RTMR_PCA9548_CH_4,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_7
        },
        {
            RTMR_17,
            CP2112_ID_2,
            RTMR_PCA9548_CH_4,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_6
        },
        {
            RTMR_18,
            CP2112_ID_2,
            RTMR_PCA9548_CH_4,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_5
        },
        {
            RTMR_19,
            CP2112_ID_2,
            RTMR_PCA9548_CH_4,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_4
        },

        {
            RTMR_20,
            CP2112_ID_2,
            RTMR_PCA9548_CH_5,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_3
        },
        {
            RTMR_21,
            CP2112_ID_2,
            RTMR_PCA9548_CH_5,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_2
        },
        {
            RTMR_22,
            CP2112_ID_2,
            RTMR_PCA9548_CH_5,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_1
        },
        {
            RTMR_23,
            CP2112_ID_2,
            RTMR_PCA9548_CH_5,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_0
        },

        {
            RTMR_24,
            CP2112_ID_2,
            RTMR_PCA9548_CH_6,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_15
        },
        {
            RTMR_25,
            CP2112_ID_2,
            RTMR_PCA9548_CH_6,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_14
        },
        {
            RTMR_26,
            CP2112_ID_2,
            RTMR_PCA9548_CH_6,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_13
        },
        {
            RTMR_27,
            CP2112_ID_2,
            RTMR_PCA9548_CH_6,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_12
        },

        {
            RTMR_28,
            CP2112_ID_2,
            RTMR_PCA9548_CH_7,
            RTMR_I2C_ADDR_20,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_11
        },
        {
            RTMR_29,
            CP2112_ID_2,
            RTMR_PCA9548_CH_7,
            RTMR_I2C_ADDR_22,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_10
        },
        {
            RTMR_30,
            CP2112_ID_2,
            RTMR_PCA9548_CH_7,
            RTMR_I2C_ADDR_24,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_9
        },
        {
            RTMR_31,
            CP2112_ID_2,
            RTMR_PCA9548_CH_7,
            RTMR_I2C_ADDR_26,
            BF_RTMR_PCA9535_DEV_42,
            BF_RTMR_PCA9535_CH_8
        },
    }
};

static char install_dir_l[RTMR_INSTALL_DIR_MAX];

/* Board ID */
bf_pltfm_board_id_t rtmr_bd_id;

/* i2c addresses of PCA9535 */
static const uint8_t
bf_rtmr_pca9535_i2c_addr[BF_RTMR_PCA9535_MAX] = {
    (0x20 << 1), (0x21 << 1)
};

/* return i2c address of a PCA9535 device */
static void bf_rtmr_mav_pca9535_i2c_addr_get (
    int pca9535, uint8_t *i2c_addr)
{
    if (pca9535 < BF_RTMR_PCA9535_MAX) {
        *i2c_addr = bf_rtmr_pca9535_i2c_addr[pca9535];
    } else {
        *i2c_addr = 0;
    }
}

/* write to a PCA9535 register
 * *** this function does not set the direction of the port in pca9535
 */
static bf_pltfm_status_t
bf_rtmr_mav_pca9535_reg_set (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    int pca9535,
    uint8_t reg_addr,
    uint16_t val)
{
    uint8_t buf[3];
    uint8_t i2c_addr;
    int rc;

    if (!hndl || pca9535 > BF_RTMR_PCA9535_MAX) {
        return BF_PLTFM_INVALID_ARG;
    }

    /* open up access to all PCA9535 */
    rc = bf_pltfm_cp2112_write_byte (
             hndl, ADDR_SWITCH_COMM, 0x80, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) in opening the common PCA9548\n",
                   rc);
        return -1;
    }

    bf_rtmr_mav_pca9535_i2c_addr_get (pca9535,
                                      &i2c_addr);

    buf[0] = reg_addr;
    buf[1] = val;
    buf[2] = (val >> 8);

    rc = bf_pltfm_cp2112_write (hndl, i2c_addr, buf,
                                3, DEFAULT_TIMEOUT_MS);

    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error (%d) in setting PCA9535 pca %d reg %d\n",
            rc, pca9535, reg_addr);
        /* intentional  fall-thru (no return) to close common PCA 9548 */
    }
    /* close access to all PCA9535 */
    rc |=
        bf_pltfm_cp2112_write_byte (hndl,
                                    ADDR_SWITCH_COMM, 0, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) in closing common PCA9548\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/* read a PCA9535 register
 * *** this function does not check the direction of the port in pca9535
 */
static bf_pltfm_status_t
bf_rtmr_mav_pca9535_reg_get (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    int pca9535,
    uint8_t reg_addr,
    uint16_t *val)
{
    uint8_t buf_out[1];
    uint8_t buf_in[2];
    uint8_t i2c_addr;
    int rc;

    if (!hndl || pca9535 > BF_RTMR_PCA9535_MAX) {
        return BF_PLTFM_INVALID_ARG;
    }

    /* open up access to all PCA9535 */
    rc = bf_pltfm_cp2112_write_byte (
             hndl, ADDR_SWITCH_COMM, 0x80, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_DEBUG ("Error (%d) in opening the common PCA9548\n",
                   rc);
        return -1;
    }

    bf_rtmr_mav_pca9535_i2c_addr_get (pca9535,
                                      &i2c_addr);
    buf_out[0] = reg_addr;

    rc = bf_pltfm_cp2112_write_read_unsafe (
             hndl, i2c_addr, buf_out, buf_in, 1, 2,
             DEFAULT_TIMEOUT_MS);

    if (rc != BF_PLTFM_SUCCESS) {
        LOG_DEBUG (
            "Error (%d) in reading PCA9535 %d reg %d\n", rc,
            pca9535, reg_addr);
        /* intentional  fall-thru (no return) to close common PCA 9548 */
    } else {
        *val = buf_in[0] | (buf_in[1] << 8);
    }

    /* close access to all PCA9535 */
    rc |=
        bf_pltfm_cp2112_write_byte (hndl,
                                    ADDR_SWITCH_COMM, 0, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_DEBUG ("Error (%d) in closing common PCA9548\n",
                   rc);
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

/* Read a retimer register.
 * This routine does not set the i2x muxes on the path to the retimers.
 */

static bf_pltfm_status_t
bf_pltfm_rtmr_raw_reg_read (
    uint8_t rtmr_i2c_addr,
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle,
    uint16_t offset,
    uint16_t *rd_buf)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint8_t wr_buf[2];
    uint8_t *ptr = (uint8_t *)rd_buf;
    uint8_t tmp;

    wr_buf[0] = 0xff & (offset >> 8);
    wr_buf[1] = 0xff & offset;

    res = bf_pltfm_cp2112_write_read_unsafe (
              cp2112_handle,
              rtmr_i2c_addr,
              wr_buf,
              (uint8_t *)rd_buf,
              2,
              2,
              TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 read failed for device: 0x%x offset 0x%x.\n",
                   res,
                   rtmr_i2c_addr,
                   offset);
    }

    /* Swap for endianess */
    tmp = ptr[1];
    ptr[1] = ptr[0];
    ptr[0] = tmp;

    return res;
}

#if !RTMR_DO_CP2112_I2C && RTMR_DO_CHECK_STATUS
static bf_pltfm_status_t
bf_pltfm_rtmr_tf_raw_reg_read (
    uint8_t rtmr_i2c_addr,
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle,
    uint16_t offset,
    uint16_t *rd_buf)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint8_t buf[4];
    uint8_t *ptr = (uint8_t *)rd_buf;
    uint32_t n_offset;

    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;

    if (!tf_i2c_init) {
        res |= bfn_io_set_mode_i2c (0, sub_devid, TOFINO_PIN_PAIR);
        res |= bfn_i2c_set_clk (0, sub_devid, TOFINO_PIN_PAIR,
                               TOFINO_I2C_PERIOD);
        res |= bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                                   BF_I2C_MODE_REG);
        tf_i2c_init = 1;
    }

    n_offset = RTMR_SWAP_BYTE (offset);

    res |= bfn_i2c_rd_reg_blocking (
               0, sub_devid, TOFINO_PIN_PAIR, (rtmr_i2c_addr >> 1),
               n_offset, 2, buf, 2);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error (%d) Tofino i2c read failed for device: 0x%x offset 0x%x.\n",
            res,
            rtmr_i2c_addr,
            offset);
        /* Reset i2c in case i2c is locked on error */
        res |= bfn_i2c_reset (0, sub_devid, TOFINO_PIN_PAIR);
    }

    ptr[1] = buf[0];
    ptr[0] = buf[1];
    /* Swap for endianess */

    return res;
}
#endif

/**
 * Routine reads the specified register from the retimer.
 * This routine handles setting the entire path to the retimer based on the
 * rtmr_map info
 * @param port_info contains the information on the retimer to be read from.
 * @param offset register to read from.
 * @param rd_buf buffer to write read data.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_reg_read (
    bf_pltfm_port_info_t *port_info,
    uint16_t offset,
    uint16_t *rd_buf)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_status_t res2 = BF_PLTFM_SUCCESS;
    bf_pltfm_rtmr_i2c_addr_t rtmr_i2c_info;
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;
    uint8_t wr_buf;

    /* Retrieve retimer i2c info from table */
    /* TODO: Replace hard coded platform index from 0 to a macro */
    memcpy (&rtmr_i2c_info,
            &rtmr_map[0][BF_PLTFM_GET_RTMR_INDEX (
                             port_info->conn_id)],
            sizeof (bf_pltfm_rtmr_i2c_addr_t));

    /* Lock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_lock();

    /* Get cp2112 device info */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        rtmr_i2c_info.cp2112_num);

    if (cp2112_handle == NULL) {
        LOG_ERROR ("Unable to find CP2112 handle for device #: %d.\n",
                   rtmr_i2c_info.cp2112_num);
        res = BF_PLTFM_COMM_FAILED;
        goto cleanup;
    }

    /* Set I2C MUX to the proper channel */
    wr_buf = (1 << rtmr_i2c_info.sw_chnl_num);
    res = bf_pltfm_cp2112_write (
              cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
              sizeof (wr_buf), TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res);
        goto cleanup;
    }

    /* Read from retimer device */
    res = bf_pltfm_rtmr_raw_reg_read (
              rtmr_i2c_info.dev_i2c_addr, cp2112_handle, offset,
              rd_buf);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("CP2112 read failed for device: 0x%x offset 0x%x.\n",
                   rtmr_i2c_info.dev_i2c_addr,
                   offset);
        goto cleanup;
    }

cleanup:
    /*
     * Reset I2C MUX value to default.
     * Clearing all channels should be default setting.
     * Therefore clearing all channels on any
     * cleanup should be fine.
     */
    wr_buf = 0;

    res2 = bf_pltfm_cp2112_write (
               cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
               sizeof (wr_buf), TIMEOUT_MS);

    if (res2 != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res2);
    }

    /* unLock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_unlock();

    return res;
}

/* Write a retimer register.
 * This routine does not set the i2x muxes on the path to the retimers.
 */
static bf_pltfm_status_t
bf_pltfm_rtmr_raw_reg_write (
    uint8_t rtmr_i2c_addr,
    uint16_t offset,
    uint16_t data,
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint8_t wr_buf[4];

    /* Write to retimer device */
    wr_buf[0] = 0xff & (offset >> 8);
    wr_buf[1] = 0xff & offset;

    wr_buf[2] = 0xff & (data >> 8);
    wr_buf[3] = 0xff & data;

    res = bf_pltfm_cp2112_write (
              cp2112_handle, rtmr_i2c_addr, wr_buf, 4,
              TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error (%d) CP2112 write failed for address: 0x%x offset 0x%x data: "
            "0x%x.\n",
            res,
            rtmr_i2c_addr,
            offset,
            data);
    }

    return res;
}

#if RTMR_TOFINO_REG_WRITE_ENABLE
static bf_pltfm_status_t
bf_pltfm_rtmr_tf_raw_reg_write (uint8_t
                                rtmr_i2c_addr,
                                uint16_t offset,
                                uint16_t offset_size,
                                uint16_t data,
                                uint16_t data_size)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint8_t wr_buf[4];
    uint32_t n_offset;

    wr_buf[0] = 0xff & (data >> 8);
    wr_buf[1] = 0xff & data;
    wr_buf[2] = 0;
    wr_buf[3] = 0;

    n_offset = RTMR_SWAP_BYTE (offset);

    if (!tf_i2c_init) {
        res |= bfn_io_set_mode_i2c (0, sub_devid, TOFINO_PIN_PAIR);
        res |= bfn_i2c_set_clk (0, sub_devid, TOFINO_PIN_PAIR,
                               TOFINO_I2C_PERIOD);
        res |= bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                                   BF_I2C_MODE_REG);
        tf_i2c_init = 1;
    }

    res |= bfn_i2c_wr_reg_blocking (0, sub_devid,
                                   TOFINO_PIN_PAIR,
                                   (rtmr_i2c_addr >> 1),
                                   n_offset,
                                   offset_size,
                                   wr_buf,
                                   data_size);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error (%d) Tofino i2c write failed for address: 0x%x offset 0x%x "
            "data: "
            "0x%x.\n",
            res,
            rtmr_i2c_addr,
            offset,
            data);
        res |= bfn_i2c_reset (0, sub_devid, TOFINO_PIN_PAIR);
    }

    return res;
}
#endif

#if !RTMR_DO_CP2112_I2C
static bf_pltfm_status_t
bf_pltfm_rtmr_tf_stream_wr (uint8_t rtmr_i2c_addr,
                            uint16_t offset,
                            uint8_t *buf,
                            uint32_t buf_size)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_status_t status;
    int wait_cnt;
    bool complete;

    uint32_t n_offset;

    n_offset = RTMR_SWAP_BYTE (offset);

    if (!tf_i2c_init) {
        res |= bfn_io_set_mode_i2c (0, sub_devid, TOFINO_PIN_PAIR);
        res |= bfn_i2c_set_clk (0, sub_devid, TOFINO_PIN_PAIR,
                               TOFINO_I2C_PERIOD);
        tf_i2c_init = 1;
    }
    res |= bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                               BF_I2C_MODE_STATEOUT);

    res |= bfn_write_stateout_buf (0, sub_devid, TOFINO_PIN_PAIR,
                                  0, buf, buf_size);

    if (res |= bfn_i2c_issue_stateout (
                   0, sub_devid, TOFINO_PIN_PAIR, (rtmr_i2c_addr >> 1),
                   n_offset, 2, 0, buf_size)) {
        res |= bfn_i2c_reset (0, sub_devid, TOFINO_PIN_PAIR);
        res |= bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                                   BF_I2C_MODE_REG);
        return res;
    }

    /* perform a busy wait with sleep */
    wait_cnt = 0;
    while (wait_cnt++ < 50) {
        /* sleep for 2 byte i2c wire delay */
        bf_sys_usleep (10);
        status = bfn_i2c_get_completion_status (0, sub_devid,
                                               TOFINO_PIN_PAIR, &complete);
        if (status != BF_SUCCESS) {
            /* Restore GPIO to i2c register mode */
            bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                                BF_I2C_MODE_REG);
            bfn_i2c_reset (0, sub_devid, TOFINO_PIN_PAIR);
            return status;
        }
        if (!complete) {
            /* Restore GPIO to i2c register mode */
            bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                                BF_I2C_MODE_REG);
            return BF_SUCCESS;
        }
    }

    /* Restore GPIO to i2c register mode */
    res |= bfn_i2c_set_submode (0, sub_devid, TOFINO_PIN_PAIR,
                               BF_I2C_MODE_REG);
    return res;
}
#endif

/**
 * Routine writes the specified register from the retimer.
 * This routine handles setting the entire path to the retimer based on the
 * rtmr_map info
 * @param port_info retimer number to write to.
 * @param offset register to write data to.
 * @param data buffer containing data to be written.
 * @return Status of the API call.
 */

bf_pltfm_status_t bf_pltfm_rtmr_reg_write (
    bf_pltfm_port_info_t *port_info,
    uint16_t offset,
    uint16_t data)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_status_t res2 = BF_PLTFM_SUCCESS;
    bf_pltfm_rtmr_i2c_addr_t rtmr_i2c_info;
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;
    uint8_t wr_buf;
    uint16_t rd_buf = 0;

    /* Retrieve retimer i2c info from table */
    /* TODO: Replace hard coded platform index from 0 to a macro */
    memcpy (&rtmr_i2c_info,
            &rtmr_map[0][BF_PLTFM_GET_RTMR_INDEX (
                             port_info->conn_id)],
            sizeof (bf_pltfm_rtmr_i2c_addr_t));

    /* Lock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_lock();

    /* Get cp2112 device info */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        rtmr_i2c_info.cp2112_num);

    if (cp2112_handle == NULL) {
        LOG_ERROR ("Unable to find CP2112 handle for device #: %d.\n",
                   rtmr_i2c_info.cp2112_num);
        res = BF_PLTFM_COMM_FAILED;
        goto cleanup;
    }

    /* Set I2C MUX to the proper channel */
    wr_buf = (1 << rtmr_i2c_info.sw_chnl_num);
    res = bf_pltfm_cp2112_write (
              cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
              sizeof (wr_buf), TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res);
        goto cleanup;
    }

    /* Do actual write operation to retimer register */
    res = bf_pltfm_rtmr_raw_reg_write (
              rtmr_i2c_info.dev_i2c_addr, offset, data,
              cp2112_handle);

    if (offset == RTMR_FW_IND_CTRL_REG) {
        res = bf_pltfm_rtmr_raw_reg_read (
                  rtmr_i2c_info.dev_i2c_addr, cp2112_handle, offset,
                  &rd_buf);
        if ((data == RTMR_RD_CRC) &&
            (rd_buf != RTMR_RD_CRC_CFM)) {
            LOG_DEBUG (
                "Unable to do CRC read operation. Maybe after power cycle: 0x%x to "
                "offset: 0x%x properly.  Return "
                "code: 0x%x\n",
                data,
                offset,
                rd_buf);
        } else if ((data != RTMR_RD_CRC) &&
                   ((rd_buf != RTMR_INIT_REG_CFM) &&
                    ((rd_buf & 0xf00) != 0xf00))) {
            LOG_ERROR (
                "2 Error failed to write 0x%x to offset: 0x%x properly.  Return "
                "code: 0x%x\n",
                data,
                offset,
                rd_buf);
        }
    }

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Register write operation failed.\n");
        goto cleanup;
    }

cleanup:
    /*
     * Reset I2C MUX value to default.
     * Clearing all channels should be default setting.
     * Therefore clearing all channels on any
     * cleanup should be fine.
     */
    wr_buf = 0;
    res2 = bf_pltfm_cp2112_write (
               cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
               sizeof (wr_buf), TIMEOUT_MS);

    if (res2 != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res2);
    }

    /* unLock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_unlock();

    return res;
}

static uint32_t get_int32 (const unsigned char
                           *data)
{
    uint32_t result = 0;
    int i;
    for (i = 0; i < 4; i++) {
        result <<= 8;
        result |= data[i];
    }
    return result;
}

static uint16_t get_int16 (const unsigned char
                           *data)
{
    uint16_t result = 0;
    int i;
    for (i = 0; i < 2; i++) {
        result <<= 8;
        result |= data[i];
    }
    return result;
}

/**
 * Routine will download the retimer firmware image to a retimer with the
 * specified i2c address.
 * @param rtmr_i2c_addr address of retimer to receiving the firmware download.
 * @param cp2112_handle handle of the cp2112 usb controller used to access the
 * retimer.
 * @param buffer pointer to the buffer containing the firmware image to be
 * written to the retimer.
 * @buf_size size of the firmware image to be downloaded.
 * @return Status of the API call.
 */

static bf_pltfm_status_t bf_pltfm_rtmr_do_fw_dl (
    uint8_t rtmr_i2c_addr,
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle,
    uint8_t *buffer,
    uint32_t buf_size)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint32_t image_len;
    uint32_t enter_point;
    uint32_t ram_addr;
    uint8_t *data_ptr;
    uint32_t num_sections;
    uint32_t checksum;
    uint16_t offset;
    uint16_t wr_buf;
#if RTMR_DO_CHECK_STATUS
    uint16_t rd_buf;
    uint32_t checktime;
#endif
    uint32_t x;
    uint32_t y;
    uint16_t sec_buf[17];

    /* Get embedded image information from image */
    image_len = get_int32 (buffer +
                           RTMR_FW_IMAGE_LEN_OFFSET);
    enter_point = get_int32 (buffer +
                             RTMR_FW_ENTER_POINT_OFFSET);
    ram_addr = get_int32 (buffer +
                          RTMR_FW_RAM_ADDR_OFFSET);
    data_ptr = buffer + RTMR_FW_DATA_START_OFFSET;

    num_sections = (image_len + 23) / 24;

    /* Set up Firmware holding register: Start FW load */
    res = bf_pltfm_rtmr_raw_reg_write (
              rtmr_i2c_addr, FW2, BF_RTMR_START_FW_LOAD,
              cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Starting FW load failed.\n");
        goto cleanup;
    }

    /* Reset internal MCU in retimer */
    res = bf_pltfm_rtmr_raw_reg_write (
              rtmr_i2c_addr, FW1, BF_RTMR_RESET_MCU,
              cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Starting FW load failed.\n");
        goto cleanup;
    }

    /* Clear MCU reset */
    res = bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                       FW1, 0, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Starting FW load failed.\n");
        goto cleanup;
    }

    /* Clear firmware holding register */
    res = bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                       FW2, 0, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Starting FW load failed.\n");
        goto cleanup;
    }

    /* Write firmware 24 bytes at a time (12 x 16 bit data registers per
     * transaction) */

    for (x = 0; x < num_sections; x++) {
        checksum = 0x800c;
        fflush (stdout);

        /* Set address hi and lo */
        wr_buf = ram_addr >> 16;
        sec_buf[RTMR_FW_DL_ADDR_HI_OFFSET + 1] =
            RTMR_SWAP_BYTE (wr_buf);

        wr_buf = ram_addr & 0xffff;
        sec_buf[RTMR_FW_DL_ADDR_LO_OFFSET + 1] =
            RTMR_SWAP_BYTE (wr_buf);

        sec_buf[0] = RTMR_SWAP_BYTE (FW0);

        /* Running checksum */
        checksum += (ram_addr >> 16) + (ram_addr &
                                        0xffff);

        /* Write the 24 bytes of data to data registers */
        for (y = 0; y < 12; y++) {
            /* TODO: Verify calculation below is as expected */
            wr_buf = get_int16 (data_ptr);
            sec_buf[ (y + 1)] = RTMR_SWAP_BYTE (wr_buf);

            checksum += (wr_buf & 0xffff);
            data_ptr += 2;
            ram_addr += 2;
        }

        /* Load checksum */
        wr_buf = (-checksum) & 0xffff;
        sec_buf[RTMR_FW_DL_CHECKSUM_OFFSET + 1] =
            RTMR_SWAP_BYTE (wr_buf);

        /* Do the command */
        wr_buf = RTMR_FW_DL_TRIGGER_CMD;
        sec_buf[RTMR_FW_DL_CTRL_OFFSET + 1] =
            RTMR_SWAP_BYTE (wr_buf);

#if RTMR_DO_CP2112_I2C
        res = bf_pltfm_cp2112_write (cp2112_handle,
                                     rtmr_i2c_addr,
                                     (uint8_t *)sec_buf,
                                     sizeof (sec_buf),
                                     TIMEOUT_MS);

        if (res != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error (%d) CP2112 write failed for address: 0x%x.\n",
                       res,
                       rtmr_i2c_addr);
        }
#else
        bf_pltfm_rtmr_tf_stream_wr (rtmr_i2c_addr, FW0,
                                    (uint8_t *)&sec_buf[1], 32);

#endif

#if RTMR_DO_CHECK_STATUS
        /* Read status */
        checktime = 0;
        offset = FW0 + 15;

        do {
#if RTMR_DO_CP2112_I2C
            res = bf_pltfm_rtmr_raw_reg_read (
                      rtmr_i2c_addr, cp2112_handle, offset, &rd_buf);
#else
            res = bf_pltfm_rtmr_tf_raw_reg_read (
                      rtmr_i2c_addr, cp2112_handle, offset, &rd_buf);
#endif

            if (res != BF_PLTFM_SUCCESS) {
                LOG_ERROR ("Failed to read status after fw write.\n");
                goto cleanup;
            }

            checktime++;
            if (checktime > 1000) {
                /* Error: max loop */
                LOG_ERROR (
                    "Failed to complete fw write. Maximum loops reached. status: "
                    "0x%x\n",
                    rd_buf);
                goto cleanup;
            }
        } while (rd_buf == RTMR_FW_DL_TRIGGER_CMD);

        if (rd_buf != 0x0000) {
            /* Error */
            LOG_ERROR ("Status shows error. status: 0x%x\n",
                       rd_buf);
            goto cleanup;
        }
#endif
    }

    /* Program Firmware enter_point */
    offset = FW0 + RTMR_FW_DL_ADDR_HI_OFFSET;
    wr_buf = enter_point >> 16;
    res =
        bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                     offset, wr_buf, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write FW entry point higher 16 bits.\n");
        goto cleanup;
    }

    offset = FW0 + RTMR_FW_DL_ADDR_LO_OFFSET;
    wr_buf = enter_point & 0xffff;
    res =
        bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                     offset, wr_buf, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write FW entry point lower 16 bits.\n");
        goto cleanup;
    }

    /* Load proper checksum */
    checksum = (enter_point >> 16) + (enter_point &
                                      0xffff) + 0x4000;
    offset = FW0 + RTMR_FW_DL_CHECKSUM_OFFSET;
    wr_buf = (-checksum) & 0xffff;
    res =
        bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                     offset, wr_buf, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write checksum.\n");
        goto cleanup;
    }

    /* Start running firmware on MCU */
    offset = FW0 + RTMR_FW_DL_CTRL_OFFSET;
    wr_buf = BF_START_FW;
    res =
        bf_pltfm_rtmr_raw_reg_write (rtmr_i2c_addr,
                                     offset, wr_buf, cp2112_handle);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to start MCU.\n");
        goto cleanup;
    }

cleanup:

    return res;
}

/**
 * Do firmware download to retimer specifed by the retimer info passed in.
 * @param port_info information of the retimer to be programmed.
 * @param buffer pointer to the buffer containing the firmware image.
 * @param buf_size size of the firmware image to be downloaded to retimer.
 * @return Status of the API call.
 */

bf_pltfm_status_t bf_pltfm_rtmr_fw_dl (
    bf_pltfm_port_info_t *port_info,
    uint8_t *buffer,
    uint32_t buf_size)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_status_t res2 = BF_PLTFM_SUCCESS;
    bf_pltfm_rtmr_i2c_addr_t rtmr_i2c_info;
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;
    uint8_t wr_buf;

    /* Retrieve retimer i2c info from table */
    /* TODO: Replace hard coded platform index from 0 to a macro */
    memcpy (&rtmr_i2c_info,
            &rtmr_map[0][BF_PLTFM_GET_RTMR_INDEX (
                             port_info->conn_id)],
            sizeof (bf_pltfm_rtmr_i2c_addr_t));

    /* Lock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_lock();

    /* Get cp2112 device info */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        rtmr_i2c_info.cp2112_num);

    if (cp2112_handle == NULL) {
        LOG_ERROR ("Unable to find CP2112 handle for device #: %d.\n",
                   rtmr_i2c_info.cp2112_num);
        res = BF_PLTFM_COMM_FAILED;
        goto cleanup;
    }

    /* Set I2C MUX to the proper channel */
    wr_buf = (1 << rtmr_i2c_info.sw_chnl_num);
    res = bf_pltfm_cp2112_write (
              cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
              sizeof (wr_buf), TIMEOUT_MS);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res);
        goto cleanup;
    }

    /* Do firmware download to retimer */
    res = bf_pltfm_rtmr_do_fw_dl (
              rtmr_i2c_info.dev_i2c_addr, cp2112_handle, buffer,
              buf_size);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Firmware download to retimer failed.\n");
    }

cleanup:
    /*
     * Reset I2C MUX value to default.
     * Clearing all channels should be default setting.
     * Therefore clearing all channels on any
     * cleanup should be fine.
     */
    wr_buf = 0;
    res2 = bf_pltfm_cp2112_write (
               cp2112_handle, PCA9548_I2C_ADDR, &wr_buf,
               sizeof (wr_buf), TIMEOUT_MS);

    if (res2 != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error (%d) CP2112 write failed while opening PCA i2c switch \n",
                   res2);
    }

    /* unLock mutex to ensure atomic sequence of writes to Repeater */
    bf_pltfm_rtmr_i2c_unlock();

    return res;
}

/**
 * Read CRC from retimer firmware file.
 * @param crc pointer to crc.
 * @return Status of the API call.
 */
bf_pltfm_status_t
bf_pltfm_rtmr_crc_rd_from_bin_file (
    uint16_t *crc)
{
    FILE *fp;
    uint16_t buffer;
    uint32_t buf_size;
    char install_dir_f[RTMR_INSTALL_DIR_FULL_MAX];

    snprintf (install_dir_f,
              RTMR_INSTALL_DIR_FULL_MAX,
              "%s/%s",
              install_dir_l,
              RTMR_FW_FILE_LOC);
    fp = fopen (install_dir_f, "rb");
    if (!fp) {
        LOG_ERROR ("Failed to open firmware file: %s\n",
                   RTMR_FW_FILE_LOC);
        return BF_PLTFM_COMM_FAILED;
    }
    fseek (fp, RTMR_FW_CRC_OFFSET, SEEK_SET);
    buf_size = fread (&buffer, 1, sizeof (buffer),
                      fp);
    if (buf_size == 0) {
        LOG_ERROR ("Failed to read crc from file: %s\n",
                   RTMR_FW_FILE_LOC);
        fclose (fp);
        return BF_PLTFM_COMM_FAILED;
    }

    fclose (fp);

    *crc = RTMR_SWAP_BYTE (buffer);

    return BF_PLTFM_SUCCESS;
}

/**
 * Read CRC from retimer.
 * @param port_info pointer to port information.
 * @param crc pointer to crc.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_crc_rd (
    bf_pltfm_port_info_t *port_info,
    uint16_t *crc)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint16_t offset;
    uint16_t data = 0;

    /* Write to control register */
    offset = RTMR_FW_IND_CTRL_REG;
    data = RTMR_RD_CRC;

    res = bf_pltfm_rtmr_reg_write (port_info, offset,
                                   data);
    if (res != BF_PLTFM_SUCCESS) {
        /* This error can be caused by retimers after a power cycle. */
        return res;
    }

    /* Read back control register */
    res = bf_pltfm_rtmr_reg_read (port_info, offset,
                                  &data);
    if (res != BF_PLTFM_SUCCESS) {
        /* This error can be caused by retimers after a power cycle. */
        return res;
    }

    if (data != RTMR_RD_CRC_CFM) {
        /* This error can be caused by retimers after a power cycle. */
        return BF_PLTFM_COMM_FAILED;
    }

    /* Read CRC value */
    offset = RTMR_FW_IND_OFFSET_REG;
    data = 0xff;

    res = bf_pltfm_rtmr_reg_read (port_info, offset,
                                  &data);
    if (res != BF_PLTFM_SUCCESS) {
        /* This error can be caused by retimers after a power cycle. */
        return res;
    }

    *crc = data;

    return 0;
}

/**
 * Performs read and write operation on retimer
 * @param port_info retimer number and channel
 * @param operation operation to be done on retimer
 * @param data void pointer containing operation specific data
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_operation (
    bf_pltfm_port_info_t *port_info,
    uint8_t operation,
    void *data)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;

    /* Check for supported board type */
    switch (rtmr_bd_id) {
        case BF_PLTFM_BD_ID_MAVERICKS_P0C:
            break;
        default:
            LOG_ERROR ("Invalid board id passed \n");
            return BF_PLTFM_INVALID_ARG;
    }

    if (data == NULL) {
        LOG_ERROR ("Invalid data pointer passed.\n");
        return BF_PLTFM_INVALID_ARG;
    }

    switch (operation) {
        case RTMR_READ:
            res = bf_pltfm_rtmr_reg_read (port_info,
                                          ((bf_pltfm_rtmr_reg_info_t *)data)->offset,
                                          & (((bf_pltfm_rtmr_reg_info_t *)data)->data));
            break;
        case RTMR_WRITE:
            res = bf_pltfm_rtmr_reg_write (port_info,
                                           ((bf_pltfm_rtmr_reg_info_t *)data)->offset,
                                           ((bf_pltfm_rtmr_reg_info_t *)data)->data);
            break;
        case RTMR_FW_DL:
            res = bf_pltfm_rtmr_fw_dl (port_info,
                                       ((bf_pltfm_rtmr_fw_info_t *)data)->image,
                                       ((bf_pltfm_rtmr_fw_info_t *)data)->size);
            break;
        default:
            LOG_ERROR ("Invalid Retimer operation\n");
            return BF_PLTFM_INVALID_ARG;
    }

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Retimer operation: %x failed.\n",
                   operation);
    }

    return res;
}

/**
 * Set polarity swap setting for a specified retimer channel.
 * @param conn_id retimer port number.
 * @chnl_id channel number to set the polarity for.
 */
bf_pltfm_status_t bf_pltfm_rtmr_set_pol (
    uint8_t conn_id, uint8_t chnl_id)
{
    bf_pltfm_port_info_t port_info;
    bf_pltfm_rtmr_info_t rtmr_info;
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bool has_rtmr = false;
    uint16_t addr_mask;
    uint16_t addr;
    uint16_t data;

    port_info.conn_id = conn_id;
    port_info.chnl_id = chnl_id;

    bf_bd_cfg_rtmr_info_get (&port_info, &rtmr_info);

    bf_bd_cfg_port_has_rtmr (&port_info, &has_rtmr);

    if (!has_rtmr) {
        /* No retimer on this port */
        goto cleanup;
    }

    /* host_side_tx_pn_swap */
    addr_mask = ((rtmr_info.host_side_lane + 4) << 8);
    addr = 0x80a0 | addr_mask;
    /* Read data from addr */
    res = bf_pltfm_rtmr_reg_read (&port_info, addr,
                                  &data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Reading register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    data &= ~0x80;
    /* To set Bit 7 for swap, bit 6 need to be set as well */
    data |= (((rtmr_info.host_side_tx_pn_swap) ? 3 :
              1) << 6);
    /* Write back data to addr */
    res = bf_pltfm_rtmr_reg_write (&port_info, addr,
                                   data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Writing register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    /* host_side_rx_pn_swap */
    addr_mask = ((rtmr_info.host_side_lane) << 8);
    addr = 0x8061 | addr_mask;
    /* Read data from addr */
    res = bf_pltfm_rtmr_reg_read (&port_info, addr,
                                  &data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Reading register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    data &= ~0x4000;
    data |= (rtmr_info.host_side_rx_pn_swap << 14);
    /* Write back data to addr */
    res = bf_pltfm_rtmr_reg_write (&port_info, addr,
                                   data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Writing register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    /* line_side_tx_pn_swap */
    addr_mask = ((rtmr_info.line_side_lane) << 8);
    addr = 0x80a0 | addr_mask;
    /* Read data from addr */
    res = bf_pltfm_rtmr_reg_read (&port_info, addr,
                                  &data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Reading register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    data &= ~0x80;
    /* To set Bit 7 for swap, bit 6 need to be set as well */
    data |= (((rtmr_info.line_side_tx_pn_swap) ? 3 :
              1) << 6);
    /* Write back data to addr */
    res = bf_pltfm_rtmr_reg_write (&port_info, addr,
                                   data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Writing register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

    /* line_side_rx_pn_swap */
    addr_mask = ((rtmr_info.line_side_lane + 4) << 8);
    addr = 0x8061 | addr_mask;
    /* Read data from addr */
    res = bf_pltfm_rtmr_reg_read (&port_info, addr,
                                  &data);
    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Reading register 0x8061\n");
        goto cleanup;
    }

    data &= ~0x4000;
    data |= (rtmr_info.line_side_rx_pn_swap << 14);
    /* Write back data to addr */
    res = bf_pltfm_rtmr_reg_write (&port_info, addr,
                                   data);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error Writing register: %d channel %d.\n",
                   conn_id, chnl_id);
        goto cleanup;
    }

cleanup:
    return res;
}

/**
 * Initialize retimer polarity settings for all lanes on all retimers.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_set_pol_all (
    void)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint8_t conn_id;
    uint8_t chnl_id;

    for (conn_id = MIN_MVR_RTMR_PORTS;
         conn_id <= MAX_MVR_RTMR_PORTS; conn_id++) {
        for (chnl_id = 0; chnl_id < MAX_RTMR_CHAN;
             chnl_id++) {
            res = bf_pltfm_rtmr_set_pol (conn_id, chnl_id);
            if (res != BF_PLTFM_SUCCESS) {
                LOG_ERROR ("Error setting polarity for port: %d channel %d.\n",
                           conn_id,
                           chnl_id);
                /* Continue and try the rest of the retimers */
                // return res;
            }
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t
bf_pltfm_rtmr_reset_mask (uint8_t pca9535_id,
                          uint16_t mask)
{
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;
    uint16_t n_val;
    uint16_t val = 0;

    /* Take lock */
    bf_pltfm_rtmr_i2c_lock();

    /* Get cp2112 device info */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        CP2112_ID_2);

    /* Get the current value */
    bf_rtmr_mav_pca9535_reg_get (
        cp2112_handle, pca9535_id, MAV_PCA9535_REG_OUT,
        &val);

    n_val = val & ~mask;
    /* Set output direction for LP mode and disable LP mode */
    bf_rtmr_mav_pca9535_reg_set (
        cp2112_handle, pca9535_id, MAV_PCA9535_REG_OUT,
        n_val);
    /* make these 9535 direction = output */
    bf_rtmr_mav_pca9535_reg_set (
        cp2112_handle, pca9535_id, MAV_PCA9535_REG_CFG,
        0);

    bf_sys_usleep (10000);

    bf_rtmr_mav_pca9535_reg_set (
        cp2112_handle, pca9535_id, MAV_PCA9535_REG_OUT,
        val);

    /* Release lock */
    bf_pltfm_rtmr_i2c_unlock();

    bf_sys_usleep (1000000);

    return 0;
}

/**
 * Reset the specified retimer by strobing the reset pin of each retimer.
 * @param port_info port number and channel
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_reset (
    bf_pltfm_port_info_t *port_info)
{
    bf_pltfm_rtmr_i2c_addr_t rtmr_i2c_info;

    uint16_t mask;

    /* Retrieve retimer i2c info from table */
    memcpy (&rtmr_i2c_info,
            &rtmr_map[0][BF_PLTFM_GET_RTMR_INDEX (
                             port_info->conn_id)],
            sizeof (bf_pltfm_rtmr_i2c_addr_t));

    mask = (1 << rtmr_i2c_info.pca9535_chan);
    return bf_pltfm_rtmr_reset_mask (
               rtmr_i2c_info.pca9535_id, mask);
}

/**
 * Recover the specified retimer by  writing a specific sequence.
 * Need to reinitialize the retimer after this
 * @param port_info port number and channel
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_recover (
    bf_pltfm_port_info_t *port_info)
{
    bf_pltfm_rtmr_reg_write (port_info, FW1, 0x0aaa);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x981e,
                             0x8042);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x981f,
                             0x0043);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9820,
                             0x0044);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9826,
                             0x1be0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9827,
                             0x0100);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9828,
                             0xd4ff);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9829,
                             0x07fc);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x982a,
                             0xd4ff);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x982b,
                             0x07f8);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, FW1, 0x0000);
    bf_sys_usleep (1000);
    bf_pltfm_rtmr_reg_write (port_info, 0x981e, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x981f, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9820, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9826, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9827, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9828, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x9829, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x982a, 0x0);
    bf_sys_usleep (10);
    bf_pltfm_rtmr_reg_write (port_info, 0x982b, 0x0);
    bf_sys_usleep (10);
    return BF_PLTFM_SUCCESS;
}

/**
 * Reset all retimers by strobing the reset pin of each retimer.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_reset_all (void)
{
    bf_pltfm_cp2112_device_ctx_t *cp2112_handle;

    /* Take lock */
    bf_pltfm_rtmr_i2c_lock();

    /* Get cp2112 device info */
    cp2112_handle = bf_pltfm_cp2112_get_handle (
                        CP2112_ID_2);

    /* Set output direction for LP mode and disable LP mode */
    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 0,
                                 MAV_PCA9535_REG_OUT, 0);
    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 1,
                                 MAV_PCA9535_REG_OUT, 0);
    /* make these 9535 direction = output */
    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 0,
                                 MAV_PCA9535_REG_CFG, 0);
    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 1,
                                 MAV_PCA9535_REG_CFG, 0);

    bf_sys_usleep (10000);

    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 0,
                                 MAV_PCA9535_REG_OUT, 0xffff);
    bf_rtmr_mav_pca9535_reg_set (cp2112_handle, 1,
                                 MAV_PCA9535_REG_OUT, 0xffff);

    /* Release lock */
    bf_pltfm_rtmr_i2c_unlock();

    bf_sys_usleep (1000000);

    return 0;
}

/**
 * Download firmware to all retimers.
 * @return Status of the API call.
 */
static bf_pltfm_status_t
bf_pltfm_rtmr_fware_dl_ports (uint8_t start_port,
                              uint8_t end_port,
                              uint16_t crc,
                              bool recover)
{
    bf_pltfm_rtmr_fw_info_t fw_info;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    FILE *fp;
    uint8_t buffer[BF_RTMR_MAX_FW];
    uint32_t buf_size;
    bool has_rtmr = false;
    uint8_t port_num;
    char install_dir_f[RTMR_INSTALL_DIR_FULL_MAX];
    uint16_t rtmr_crc = 0;

    snprintf (install_dir_f,
              RTMR_INSTALL_DIR_FULL_MAX,
              "%s/%s",
              install_dir_l,
              RTMR_FW_FILE_LOC);
    fp = fopen (install_dir_f, "rb");
    if (!fp) {
        LOG_ERROR ("Failed to open firmware file: %s\n",
                   RTMR_FW_FILE_LOC);
        return BF_PLTFM_COMM_FAILED;
    }
    fseek (fp, 4096, SEEK_SET);
    buf_size = fread (buffer, 1, BF_RTMR_MAX_FW, fp);
    fclose (fp);

    fw_info.image = buffer;
    fw_info.size = buf_size;

    for (port_num = start_port; port_num <= end_port;
         port_num++) {
        port_info.conn_id = port_num;
        port_info.chnl_id = 0;

        bf_bd_cfg_port_has_rtmr (&port_info, &has_rtmr);

        if (!has_rtmr) {
            /* No retimer on this port */
            continue;
        }

        /* Read crc from retimer */
        if (recover) {
            /* force perform recovery sequence followed by firmware download */
            LOG_WARNING ("force recovering retimer at port %d\n",
                         port_num);
            bf_pltfm_rtmr_recover (&port_info);
        } else {
            /* Read crc from retimer */
            res = bf_pltfm_rtmr_crc_rd (&port_info,
                                        &rtmr_crc);
            if (res) {
                LOG_DEBUG ("Retimer uninitialized firmware download needed. %d\n",
                           port_info.conn_id);
            }

            if (rtmr_crc == crc) {
                continue;
            }
        }

        /* Download firmware */
        res = bf_pltfm_rtmr_operation (&port_info,
                                       RTMR_FW_DL, (void *)&fw_info);

        if (res != BF_PLTFM_SUCCESS) {
            /* Log error and continue */
            LOG_ERROR ("Failed to download firmware for port: %d\n",
                       port_num);
            continue;
        }
    }

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t bf_pltfm_rtmr_check_crc (
    bf_pltfm_port_info_t *port_info, uint16_t crc)
{
    bf_pltfm_status_t res = 0;
    uint16_t rtmr_crc = 0xffff;

    /* Read crc from retimer */
    res = bf_pltfm_rtmr_crc_rd (port_info, &rtmr_crc);
    if (res) {
        /* Fail case */
        LOG_ERROR ("Unable to read crc from retimer for port %d\n",
                   port_info->conn_id);
        /* do not return, but attempt to recover the retimer */
    }

    /* Compare with expected crc */
    if (rtmr_crc != crc) {
        LOG_ERROR (
            "Retrieved crc from retimer port# %d: 0x%x does not match expected "
            "crc: 0x%x\n",
            port_info->conn_id,
            rtmr_crc,
            crc);

        /* retimer might be stuck. Run the Credo provided recovery sequence
           before trying to download the firmware again
         */
        /* retry download on fail */
        res = bf_pltfm_rtmr_fware_dl_ports (
                  port_info->conn_id, port_info->conn_id, crc,
                  true);
        res |= bf_pltfm_rtmr_crc_rd (port_info,
                                     &rtmr_crc);
        if (res) {
            LOG_ERROR ("Failed to download or read crc from retimer for port %d\n",
                       port_info->conn_id);
            return BF_PLTFM_COMM_FAILED;
        }

        /* Re-read crc from retimer */
        if (rtmr_crc != crc) {
            /* Still fail. */
            LOG_ERROR ("Failed to match crc after retry for port %d\n",
                       port_info->conn_id);
            return BF_PLTFM_COMM_FAILED;
        }
    }

    return BF_PLTFM_SUCCESS;
}

/**
 * Set retimer to the proper mode based on port configuration passed in.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_ext_phy_set_mode (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_status_t res2 = BF_PLTFM_SUCCESS;
    bool an_mode = port_cfg->is_an_on;
    bf_port_speed_t speed = port_cfg->speed_cfg;
    uint8_t port_num = port_info->conn_id;
    uint8_t mode = 0;
    uint16_t buffer = 0;
    bool is_x4 = false;
    bf_pltfm_rtmr_info_t rtmr_info;

    /* If optical module, let retimer know */
    res |= bf_pltfm_rtmr_reg_read (port_info,
                                   RTMR_FW_OPT_MOD_REG, &buffer);
    buffer &= ~ (1 << RTMR_OPT_PRES_BIT_POS);
    buffer |= (port_cfg->is_optic <<
               RTMR_OPT_PRES_BIT_POS);
    res |= bf_pltfm_rtmr_reg_write (port_info,
                                    RTMR_FW_OPT_MOD_REG, buffer);

    if ((speed == BF_SPEED_40G) ||
        (speed == BF_SPEED_100G)) {
        is_x4 = true;
    }

    res2 = bf_bd_cfg_rtmr_info_get (port_info,
                                    &rtmr_info);
    if (res2 != BF_PLTFM_SUCCESS) {
        /* Do not continue as rtmr_info is not valid. */
        LOG_ERROR ("Failed to get retimer information.%d:%d\n",
                   port_info->conn_id,
                   port_info->chnl_id);
        return BF_PLTFM_COMM_FAILED;
    }

    /* Select mode */
    if (is_x4) {
        buffer = RTMR_INIT_MODE_ALL;
    } else {
        buffer = RTMR_INIT_REG_MODE;
        buffer |= rtmr_info.host_side_lane;
    }

    if (an_mode) {
        mode = 1;
        if (is_x4) {
            /* Set x4 mode */
            res |= bf_pltfm_rtmr_reg_write (port_info,
                                            RTMR_FW_AN_MODE_REG, 0);
        } else {
            /* Set x1 mode */
            res |= bf_pltfm_rtmr_reg_write (port_info,
                                            RTMR_FW_AN_MODE_REG, 1);
        }
    } else if ((speed == BF_SPEED_25G) ||
               (speed == BF_SPEED_50G) ||
               (speed == BF_SPEED_100G) ||
               /*(speed == BF_SPEED_40G_NB)*/1) {
        mode = 2;
    } else if ((speed == BF_SPEED_10G) ||
               (speed == BF_SPEED_40G)) {
        mode = 3;
    }

    buffer |= mode << 4;

    res |= bf_pltfm_rtmr_reg_write (port_info,
                                    RTMR_FW_IND_CTRL_REG, buffer);

    if (res) {
        LOG_ERROR ("Failed to set mode for port# %d\n",
                   port_num);
        return BF_PLTFM_COMM_FAILED;
    }

    uint32_t first_ln, last_ln;
    if ((speed == BF_SPEED_100G) ||
        (speed == BF_SPEED_40G)) {
        first_ln = 0;
        last_ln = 3;
    } else if (speed == BF_SPEED_50G) {
        first_ln = port_info->chnl_id;
        last_ln = port_info->chnl_id + 1;
    } else {
        first_ln = port_info->chnl_id;
        last_ln = first_ln;
    }
    for (uint32_t ln = first_ln; ln <= last_ln;
         ln++) {
        port_info->chnl_id = ln;
        uint32_t cur_buffer;
        bf_bd_cfg_rtmr_info_get (port_info, &rtmr_info);
        /* for optical links, increase the signal threshold to prevent the retimer
        * trying to lock to noise output from the QSFP */
        uint32_t rtmr_reg = RX_NRZ_SD_REG | ((
                rtmr_info.line_side_lane + 4) << 8);
        res = bf_pltfm_rtmr_reg_read (port_info, rtmr_reg,
                                      &buffer);
        LOG_DEBUG ("RTMR: %2d:%d : Cur SD_THR : addr=%04x = %04x",
                   port_info->conn_id,
                   port_info->chnl_id,
                   rtmr_reg,
                   buffer);
        cur_buffer = buffer;
        buffer &= ~ (0x7ff);
        if (port_cfg->is_optic) {
            buffer |= 0x333;
        } else {
            buffer |= 0x133;
        }
        if (cur_buffer != buffer) {
            LOG_DEBUG ("RTMR: %2d:%d : Set SD_THR : addr=%04x = %04x <is_opt=%d>",
                       port_info->conn_id,
                       port_info->chnl_id,
                       rtmr_reg,
                       buffer,
                       port_cfg->is_optic);
            res |= bf_pltfm_rtmr_reg_write (port_info,
                                            rtmr_reg, buffer);
            if (res) {
                LOG_ERROR ("Failed to set SD_THR for port# %d\n",
                           port_num);
                return BF_PLTFM_COMM_FAILED;
            }
        } else {
            LOG_DEBUG (
                "RTMR: %2d:%d : SD_THR correct already : addr=%04x = %04x "
                "<is_opt=%d>",
                port_info->conn_id,
                port_info->chnl_id,
                rtmr_reg,
                buffer,
                port_cfg->is_optic);
        }
    }
    return BF_PLTFM_SUCCESS;
}

/**
 * Reset retimer mode to unconfigured for the given retimer channel.
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_ext_phy_del_mode (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_port_speed_t speed = port_cfg->speed_cfg;
    bool is_x4 = false;
    uint16_t buffer = 0;
    bf_pltfm_rtmr_info_t rtmr_info;

    if ((speed == BF_SPEED_40G) ||
        (speed == BF_SPEED_100G)) {
        is_x4 = true;
    }

    res = bf_bd_cfg_rtmr_info_get (port_info,
                                   &rtmr_info);
    if (res != BF_PLTFM_SUCCESS) {
        /* Do not continue as rtmr_info is not valid. */
        LOG_ERROR ("Failed to get retimer information.%d:%d\n",
                   port_info->conn_id,
                   port_info->chnl_id);
        return BF_PLTFM_COMM_FAILED;
    }

    if (is_x4) {
        buffer = RTMR_INIT_STATE_MODE_ALL;
    } else {
        buffer = RTMR_INIT_STATE_MODE;
        buffer |= rtmr_info.host_side_lane;
    }

    res = bf_pltfm_rtmr_reg_write (port_info,
                                   RTMR_FW_IND_CTRL_REG, buffer);

    buffer = 0;
    /* In case configured for optical module, clear bit */
    res |= bf_pltfm_rtmr_reg_read (port_info,
                                   RTMR_FW_OPT_MOD_REG, &buffer);
    buffer &= ~ (1 << RTMR_OPT_PRES_BIT_POS);
    res |= bf_pltfm_rtmr_reg_write (port_info,
                                    RTMR_FW_OPT_MOD_REG, buffer);

    return res;
}

bf_pltfm_status_t bf_pltfm_ext_phy_init_speed (
    uint8_t port_num,
    bf_port_speed_t port_speed)
{
    bool is_present = false;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_ext_phy_cfg_t port_cfg;

    port_info.conn_id = port_num;
    port_info.chnl_id = 0;

    /* Check if retimer is present on this port */
    bf_bd_cfg_port_has_rtmr (&port_info, &is_present);

    if (!is_present) {
        return BF_PLTFM_SUCCESS;
    }

    port_cfg.speed_cfg = port_speed;
    port_cfg.is_an_on = 0;
    port_cfg.is_optic = 0;

    return bf_pltfm_ext_phy_set_mode (&port_info,
                                      &port_cfg);
}

char *bf_pltfm_rtmr_install_dir_get (
    char *install_dir, uint16_t buf_size)
{
    char *cnt;

    if (install_dir == NULL) {
        return 0;
    }
    if (buf_size < RTMR_INSTALL_DIR_MAX) {
        return 0;
    }

    cnt = strncpy (install_dir, install_dir_l,
                   buf_size);

    return cnt;
}

bf_pltfm_board_id_t bf_pltfm_rtmr_bd_id_get (
    void)
{
    return rtmr_bd_id;
}

/**
 * Pre-Initialize retimer library
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_rtmr_pre_init (
    const char *install_dir,
    bool is_in_ha)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;

    bf_pltfm_chss_mgmt_bd_type_get (&rtmr_bd_id);

    if (rtmr_bd_id != BF_PLTFM_BD_ID_MAVERICKS_P0C) {
        // No retimer on board, so simply return success
        return BF_PLTFM_SUCCESS;
    }

    if (!is_in_ha) {
        /* Reset all retimers */
        res |= bf_pltfm_rtmr_reset_all();

        /* Set polarity swaps */
        res |= bf_pltfm_rtmr_set_pol_all();
    }

    if (install_dir) {
        /* Store install directory path locally */
        strncpy (install_dir_l, install_dir,
                 RTMR_INSTALL_DIR_MAX - 1);
        install_dir_l[RTMR_INSTALL_DIR_MAX - 1] = '\0';
    }

    return res;
}

/**
 * Initialize retimer library
 * @return Status of the API call.
 */
bf_pltfm_status_t bf_pltfm_ext_phy_init (void)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_port_info_t port_info;
    uint16_t crc;

    bf_pltfm_chss_mgmt_bd_type_get (&rtmr_bd_id);

    if (rtmr_bd_id != BF_PLTFM_BD_ID_MAVERICKS_P0C) {
        // No retimer on board, so simply return success
        return BF_PLTFM_SUCCESS;
    }

    /* Get the expected crc from the bin file */
    res |= bf_pltfm_rtmr_crc_rd_from_bin_file (&crc);

    LOG_DEBUG ("Retimer firmware CRC: 0x%x", crc);

    /* Stop qsfp polling to prevent i2c conflict */
    bf_pltfm_pm_qsfp_scan_poll_stop();
    bf_sys_sleep (
        1); /* wait for on going qsfp scan to be over */

    /* Download firmware to all retimers through Tofino */
    res |= bf_pltfm_rtmr_fware_dl_ports (
               MIN_MVR_RTMR_PORTS, MAX_MVR_RTMR_PORTS, crc,
               false);

    /* Read and compare all crc from retimer with expected crc */
    port_info.chnl_id = 0;

    for (port_info.conn_id = MIN_MVR_RTMR_PORTS;
         port_info.conn_id <= MAX_MVR_RTMR_PORTS;
         port_info.conn_id++) {
        res |= bf_pltfm_rtmr_check_crc (&port_info, crc);
        res |= bf_pltfm_rtmr_reg_write (
                   &port_info, RTMR_FW_IND_CTRL_REG,
                   RTMR_INIT_REG_MODE);
        bf_sys_usleep (100); // drv-1408
        res |= bf_pltfm_rtmr_reg_write (
                   &port_info, RTMR_FW_IND_CTRL_REG,
                   RTMR_INIT_25G_NO_AN);
        res |= bf_pltfm_rtmr_reg_write (
                   &port_info, RTMR_FW_IND_CTRL_REG,
                   RTMR_INIT_STATE_MODE_ALL);
        /* Continue on any failure */
    }

    /* Restart qsfp polling */
    bf_pltfm_pm_qsfp_scan_poll_start();

    return res;
}

bf_pltfm_status_t bf_pltfm_ext_phy_conn_eq_set (
    bf_pltfm_port_info_t *port_info,
    bf_pltfm_ext_phy_cfg_t *port_cfg)
{
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    bf_pltfm_rtmr_info_t rtmr_info;
    bf_pltfm_rtmr_reg_info_t reg_info;

    if (!port_cfg->is_optic) {
        return res;
    }

    res = bf_bd_cfg_rtmr_info_get (port_info,
                                   &rtmr_info);
    if (res != BF_PLTFM_SUCCESS) {
        /* Do not continue as rtmr_info is not valid. */
        LOG_ERROR ("Failed to get retimer information.%d:%d\n",
                   port_info->conn_id,
                   port_info->chnl_id);
        return BF_PLTFM_COMM_FAILED;
    }

    /* TX_MAIN_TAP_COEFF */
    reg_info.offset = TX_MAIN_TAP_COEFF |
                      RTMR_CHAN_ADDR_SET (port_info->chnl_id);
    reg_info.data = 0xff;

    /* read current setting */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear current contents */
    reg_info.data &= ~ (TX_COEFF_DATA_SET (0x3f));

    /* Modify current data with new setting */
    reg_info.data |= TX_COEFF_DATA_SET (
                         rtmr_info.tx_main_opt);

    /* Write back modified field */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* TX_MAIN_PRE_CURSOR_COEFF */
    reg_info.offset =
        TX_MAIN_PRE_CURSOR_COEFF | RTMR_CHAN_ADDR_SET (
            port_info->chnl_id);
    reg_info.data = 0xff;

    /* read current setting */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear current contents */
    reg_info.data &= ~ (TX_COEFF_DATA_SET (0x3f));

    /* Modify current data with new setting */
    reg_info.data |= TX_COEFF_DATA_SET (
                         rtmr_info.tx_precursor_opt);

    /* Write back modified field */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* TX_POST_CURSOR_TAP1_COEFF */
    reg_info.offset =
        TX_POST_CURSOR_TAP1_COEFF | RTMR_CHAN_ADDR_SET (
            port_info->chnl_id);
    reg_info.data = 0xff;

    /* read current setting */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_READ, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to read current register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    /* Clear current contents */
    reg_info.data &= ~ (TX_COEFF_DATA_SET (0x3f));

    /* Modify current data with new setting */
    reg_info.data |= TX_COEFF_DATA_SET (
                         rtmr_info.tx_post_1_opt);

    /* Write back modified field */
    res = bf_pltfm_rtmr_operation (port_info,
                                   RTMR_WRITE, (void *)&reg_info);

    if (res != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write new register value.\n");
        return UCLI_STATUS_E_ERROR;
    }

    return 0;
}

bool bf_pltfm_ext_phy_lane_reset_reqd (
    bf_pltfm_port_info_t *port_info)
{
    bf_pltfm_rtmr_reg_info_t reg_info;
    bf_pltfm_rtmr_info_t rtmr_info;
    uint32_t chan_num;
    /* Read 9806 */
    reg_info.offset = (uint16_t)0x9806;
    reg_info.data = 0xff;
    bf_pltfm_rtmr_operation (port_info, RTMR_READ,
                             (void *)&reg_info);
    bf_bd_cfg_rtmr_info_get (port_info, &rtmr_info);
    chan_num = rtmr_info.line_side_lane;
    if ((((reg_info.data) >> (13 - (3 *
                                    (chan_num)))) & 0x7) == 3) {
        LOG_DEBUG ("RTMR: %2d:%d : SM RESET : Line side ln : %d : reg=%04x = %04x",
                   port_info->conn_id,
                   port_info->chnl_id,
                   chan_num,
                   reg_info.offset,
                   reg_info.data);
        return true;
        // return false; // see how common it is
    } else {
        LOG_DEBUG (
            "RTMR: %2d:%d : fifoptrs %x : : Line side ln : %d : reg=%04x = %04x",
            port_info->conn_id,
            port_info->chnl_id,
            (((reg_info.data) >> (13 - (3 * (chan_num)))) &
             0x7),
            chan_num,
            reg_info.offset,
            reg_info.data);
    }
    return false;
}

bf_pltfm_status_t bf_pltfm_ext_phy_lane_reset (
    bf_pltfm_port_info_t *port_info,
    uint32_t num_lanes)
{
    uint32_t first_ln, last_ln, ln;
    bf_pltfm_rtmr_info_t rtmr_info;
    bf_pltfm_status_t res = BF_PLTFM_SUCCESS;
    uint16_t buffer = 0;
    if (num_lanes == 4) {
        first_ln = 0;
        last_ln = 3;
    } else if (num_lanes == 2) {
        first_ln = port_info->chnl_id;
        last_ln = port_info->chnl_id + 1;
    } else {
        first_ln = port_info->chnl_id;
        last_ln = first_ln;
    }
    for (ln = first_ln; ln <= last_ln; ln++) {
        port_info->chnl_id = ln;
        uint32_t cur_buffer;
        if (!bf_pltfm_ext_phy_lane_reset_reqd (
                port_info)) {
            continue;
        }
        bf_bd_cfg_rtmr_info_get (port_info, &rtmr_info);
        /* for optical links, restart adaptation on the link after the QSFP
        * reports !LOS */
        uint32_t rtmr_reg =
            RX_NRZ_SM_RESET_REG | ((rtmr_info.line_side_lane +
                                    4) << 8);
        res = bf_pltfm_rtmr_reg_read (port_info, rtmr_reg,
                                      &buffer);
        LOG_DEBUG ("RTMR: %2d:%d : Cur SM_RESET: addr=%04x = %04x",
                   port_info->conn_id,
                   port_info->chnl_id,
                   rtmr_reg,
                   buffer);
        cur_buffer = buffer;
        buffer |= (1 << 11);
        if (cur_buffer != buffer) {
            LOG_DEBUG ("RTMR: %2d:%d : Set SM_RESET : addr=%04x = %04x",
                       port_info->conn_id,
                       port_info->chnl_id,
                       rtmr_reg,
                       buffer);
            res |= bf_pltfm_rtmr_reg_write (port_info,
                                            rtmr_reg, buffer);
            if (res) {
                LOG_ERROR ("Failed to set SM_RESET for %d/%d\n",
                           port_info->conn_id,
                           port_info->chnl_id);
                return BF_PLTFM_COMM_FAILED;
            }
            buffer &= ~ (1 << 11);
            res |= bf_pltfm_rtmr_reg_write (port_info,
                                            rtmr_reg, buffer);
            if (res) {
                LOG_ERROR ("Failed(2) to set SM_RESET for %d/%d\n",
                           port_info->conn_id,
                           port_info->chnl_id);
                return BF_PLTFM_COMM_FAILED;
            }
        } else {
            LOG_DEBUG ("RTMR: %2d:%d : SD_THR correct already : addr=%04x = %04x ",
                       port_info->conn_id,
                       port_info->chnl_id,
                       rtmr_reg,
                       buffer);
        }
    }
    return BF_PLTFM_SUCCESS;
}
