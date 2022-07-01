/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include "tofino_spi_if.h"
#include "tofino_porting_spi.h"

#if 0 /* turn this on for target platform and populate the functions functions \
       * appropriately                                                         \
       */
/* Tofino SPI Init function  */
/* @brief bf_porting_spi_init
 *   perform any subsystem specific initialization necessary to
 *   access the ASIC thru i2c or pci is done here
 * @param dev_id
 *   ASIC id (to which SPI eeprom is connected)
 *   0: on success , -1: failure
 */
int tofino_porting_spi_init (int dev_id)
{
    (void)dev_id;
    return 0;
}

int tofino_porting_spi_finish (int dev_id)
{
    (void)dev_id;
    return 0;
}

int tofino_porting_spi_reg_wr (int dev_id,
                               uint32_t reg, uint32_t data) {}

int tofino_porting_spi_reg_rd (int dev_id,
                               uint32_t reg, uint32_t *data) {}

#else /***** For barefoot platforms ********/

#include <unistd.h>
#include <bf_types/bf_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_cp2112_intf.h>

static bf_pltfm_cp2112_device_ctx_t *tfn_i2c_hdl =
    NULL;

#define I2C_ADDR_TOFINO (0x58 << 1)
#define I2C_ADDR_COMM_PCA9548 0xe8
#define MAV_PCA9535_MISC 6

typedef enum {
    TOFINO_I2c_BAR0 = 0,    /* regular PCIe space */
    TOFINO_I2c_BAR_MSI = 1, /* msix capability register space */
    TOFINO_I2c_BAR_CFG = 2, /* 4k pcie cap + 4K config space */
} Tofino_I2C_BAR_SEG;

typedef enum {
    TOFINO_I2C_OPCODE_LOCAL_WR = (0 << 5),
    TOFINO_I2C_OPCODE_LOCAL_RD = (1 << 5),
    TOFINO_I2C_OPCODE_IMM_WR = (2 << 5),
    TOFINO_I2C_OPCODE_IMM_RD = (3 << 5),
    TOFINO_I2C_OPCODE_DIR_WR = (4 << 5),
    TOFINO_I2C_OPCODE_DIR_RD = (5 << 5),
    TOFINO_I2C_OPCODE_INDIR_WR = (6 << 5),
    TOFINO_I2C_OPCODE_INDIR_RD = (7 << 5)
} TOFINO_I2C_CMD_OPCODE;

#define TOFINO_I2C_CMD_OPCODE_MASK 0xE0
#define TOFINO_I2C_BAR_SEG_SHFT 28

static int raw_read (bf_pltfm_cp2112_device_ctx_t
                     *hndl,
                     uint8_t address,
                     uint8_t *offset,
                     int size_offset,
                     uint8_t *buf,
                     int len)
{
    /* Note that we don't use the writeRead() command, since this
     * locks up the CP2112 chip if it times out.  We perform a separate write,
     * followed by a read.  This releases the I2C bus between operations, but
     * that's okay since there aren't any other master devices on the bus.
     *
     * Also note that we can't read more than 128 bytes at a time.
     */
    int ret;

    ret = bf_pltfm_cp2112_write (hndl, address,
                                 offset, size_offset, 100);
    if (ret) {
        return -1;
    }
    ret = bf_pltfm_cp2112_read (hndl, address, buf,
                                len, 100);
    return ret;
}

static int raw_write (bf_pltfm_cp2112_device_ctx_t
                      *hndl,
                      uint8_t address,
                      uint8_t *buf,
                      int len)
{
    int ret;

    ret = bf_pltfm_cp2112_write (hndl, address, buf,
                                 len, 100);
    return ret;
}

static int bf_tofino_i2c_init (int chip_id)
{
    int rc;
    uint8_t bit;

    tfn_i2c_hdl = bf_pltfm_cp2112_get_handle (
                      CP2112_ID_1);
    if (!tfn_i2c_hdl) {
        TOFINO_PORTING_LOG_ERR ("error in bf_tofino_slave_i2c_init\n");
        return -1;
    }
    bit = (1 << MAV_PCA9535_MISC);
    rc = bf_pltfm_cp2112_write_byte (tfn_i2c_hdl,
                                     I2C_ADDR_COMM_PCA9548, bit, 100);
    if (rc != BF_PLTFM_SUCCESS) {
        TOFINO_PORTING_LOG_ERR ("error in setting MISC Channel\n");
        return -1;
    }
    return 0;
}

static int bf_tofino_i2c_deinit (int chip_id)
{
    int rc;

    tfn_i2c_hdl = bf_pltfm_cp2112_get_handle (
                      CP2112_ID_1);
    if (!tfn_i2c_hdl) {
        TOFINO_PORTING_LOG_ERR ("error in bf_tofino_slave_i2c_init\n");
        return -1;
    }
    rc =
        bf_pltfm_cp2112_write_byte (tfn_i2c_hdl,
                                    I2C_ADDR_COMM_PCA9548, 0x00, 100);
    if (rc != BF_PLTFM_SUCCESS) {
        TOFINO_PORTING_LOG_ERR ("error in setting MISC Channel\n");
        return -1;
    }
    return 0;
}

/** Tofino slave mode i2c read with "direct read" command
 *
 *  @param chip_id
 *    chip_id
 *  @param seg
 *    BAR segment (0: regular PCIe space, 1: MSIX cap space,
 *                 2: 4K pcie cap/cfg space)
 *  @param dir_reg
 *     direct address space register to access
 *  @param data
 *    data to read from dir_addr
 *  @return
 *    0 on success and any other value on error
 */
int bf_tofino_dir_i2c_rd (int chip_id,
                          Tofino_I2C_BAR_SEG seg,
                          uint32_t dir_addr,
                          uint32_t *data)
{
    bf_pltfm_cp2112_device_ctx_t *upper = tfn_i2c_hdl;
    uint8_t out[7];

    out[0] = TOFINO_I2C_OPCODE_DIR_RD;
    dir_addr |= (seg << TOFINO_I2C_BAR_SEG_SHFT);
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &dir_addr, 4);
    return (raw_read (upper, I2C_ADDR_TOFINO, &out[0],
                      5, (uint8_t *)data, 4));
}

/** Tofino slave mode i2c write with "direct write" command
 *
 *  @param chip_id
 *    chip_id
 *  @param seg
 *    BAR segment (0: regular PCIe space, 1: MSIX cap space,
 *                 2: 4K pcie cap/cfg space)
 *  @param dir_reg
 *     direct address space register to access
 *  @param data
 *    data to write to dir_addr
 *  @return
 *    0 on success and any other value on error
 */
int bf_tofino_dir_i2c_wr (int chip_id,
                          Tofino_I2C_BAR_SEG seg,
                          uint32_t dir_addr,
                          uint32_t data)
{
    bf_pltfm_cp2112_device_ctx_t *upper = tfn_i2c_hdl;
    uint8_t out[16];

    out[0] = TOFINO_I2C_OPCODE_DIR_WR;
    dir_addr |= (seg << TOFINO_I2C_BAR_SEG_SHFT);
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &dir_addr, 4);
    memcpy (&out[5], &data, 4);

    return (raw_write (upper, I2C_ADDR_TOFINO,
                       &out[0], 9));
}

int tofino_porting_spi_init (int dev_id,
                             void *arg)
{
    bf_status_t sts;
    uint8_t board_id = (uint8_t)strtol ((char *)arg,
                                        NULL, 0);
    bf_pltfm_board_id_t bd_id =
        BF_PLTFM_BD_ID_UNKNOWN;

    sts = bf_pltfm_cp2112_util_init (bd_id);
    if (sts != BF_PLTFM_SUCCESS) {
        TOFINO_PORTING_LOG_ERR (
            "Error: Not able to initialize the cp2112 device(s)\n");
        return -1;
    }

    if (bf_tofino_i2c_init (dev_id) != 0) {
        return -1;
    }

    /* setup the i2c muxes to enable tofino i2c channel */

    return 0;
}

int tofino_porting_spi_finish (int dev_id)
{
    (void)dev_id;
    bf_tofino_i2c_deinit (dev_id);
    return (bf_pltfm_cp2112_de_init());
    sleep (5);
}

int tofino_porting_spi_reg_wr (int dev_id,
                               uint32_t reg, uint32_t data)
{
    return (bf_tofino_dir_i2c_wr (dev_id, 0, reg,
                                  data));
}

int tofino_porting_spi_reg_rd (int dev_id,
                               uint32_t reg, uint32_t *data)
{
    return (bf_tofino_dir_i2c_rd (dev_id, 0, reg,
                                  data));
}

/* stub functions to make tofino_spi_util build */
int platform_num_ports_get (void)
{
    return 0;
}

struct pltfm_bd_map_t *platform_pltfm_bd_map_get (
    int *rows)
{
    *rows = 0;
    return NULL;
}

#endif
