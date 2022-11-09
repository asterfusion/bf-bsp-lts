/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "tofino_spi_if.h"
#include "tofino_porting_spi.h"

#if 0
static int tofino_porting_spi_reg_wr (int dev_id,
                                      uint32_t reg,
                                      uint32_t data)
{
    if (lld_write_register (dev_id, reg,
                            data) == LLD_OK) {
        return 0;
    } else {
        return -1;
    }
}

static int tofino_porting_spi_reg_rd (int dev_id,
                                      uint32_t reg,
                                      uint32_t *data)
{
    if (lld_read_register (dev_id, reg,
                           data) == LLD_OK) {
        return 0;
    } else {
        return -1;
    }
}

/**
 * @brief  tofino_porting_spi_init
 *  initialize the SPI interface
 *
 * @param dev_id: int
 *  chip id
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
int tofino_porting_spi_init (int dev_id)
{
    /* perform any chip non-default initialization here */
    /* e.g., read idcode and determine the size of spi eeprom */
    (void)dev_id;
    return 0;
}
#endif

static uint32_t swap32 (uint32_t a)
{
    uint32_t b;
    b = ((a & 0xff) << 24) | ((a & 0xff00) << 8) | ((
                a & 0xff0000) >> 8) |
        ((a & 0xff000000) >> 24);
    return b;
}

/**
 * @brief  lld_spi_wr_
 *  triggers SPI write followed by an SPI read transaction
 *
 * @param dev_id: int
 *  chip id
 *
 * @param spi_code: uint8_t
 *  spi_code . This is always transmitted on SPI bus even if w_size or
 *             r_size is zero.
 *
 * @param w_buf: uint8_t *
 *  buf to transmit on SPI bus
 *
 * @param wr_size: int
 *  size of w_buf (zero is ok)
 *
 * @param rd_size: int
 *  number of bytes to expect to read on SPI bus (after writing)
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int lld_spi_wr (
    int dev_id, uint8_t spi_code, uint8_t *w_buf,
    int w_size, int r_size)
{
    uint32_t out_data0;
    uint32_t out_data1 = 0;
    uint32_t offset, bytes_to_wr;
    uint32_t spi_cmd;
    uint32_t i;

    if (w_size >= BF_SPI_MAX_OUTDATA ||
        r_size > BF_SPI_MAX_INDATA) {
        return -2;
    }

    bytes_to_wr = w_size + 1;
    out_data0 = ((uint32_t) (spi_code) & 0xFF);
    i = 1;
    /* prepeare 4 bytes in out_data0 word */
    while (w_size && i < 4) {
        out_data0 |= (((uint32_t) * w_buf & 0xFF) <<
                      (i * 8));
        w_size--;
        i++;
        w_buf++;
    }
    /* prepeare 4 bytes in out_data1 word */
    i = 0;
    while (w_size && i < 4) {
        out_data1 |= (((uint32_t) * w_buf & 0xFF) <<
                      (i * 8));
        w_size--;
        i++;
        w_buf++;
    }

    offset = SPI_OUTDATA0_REG;
    tofino_porting_spi_reg_wr (dev_id, offset,
                               out_data0);
    offset = SPI_OUTDATA1_REG;
    tofino_porting_spi_reg_wr (dev_id, offset,
                               out_data1);
    spi_cmd = bytes_to_wr | 0x80;
    spi_cmd |= (r_size << 4);
    /* printf("spi_wr out0 %08x out1 %08x cmd %08x\n", out_data0, out_data1,
     * spi_cmd); */
    offset = SPI_CMD_REG;
    tofino_porting_spi_reg_wr (dev_id, offset,
                               spi_cmd);
    return 0;
}

#if 0
/**
 * @brief  lld_spi_get_idcode
 *  returns the spi idcode read during bootup
 *
 * @param dev_id: int
 *  chip id
 *
 * @param spi_idcode: uint32_t *
 *  spi ID code
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int lld_spi_get_idcode (int dev_id,
                               uint32_t *spi_idcode)
{
    uint32_t offset;

    offset = SPI_IDCODE_REG;
    return (tofino_porting_spi_reg_rd (dev_id, offset,
                                       spi_idcode));
}
#endif

/**
 * @brief  lld_spi_get_rd_data
 *  returns the spi read data during  the last SPI read operation
 *
 * @param dev_id: int
 *  chip id
 *
 * @param spi_rd_data: uint32_t *
 *  spi read data
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int lld_spi_get_rd_data (int dev_id,
                                uint32_t *spi_rd_data)
{
    uint32_t offset;

    offset = SPI_INDATA_REG;
    return (tofino_porting_spi_reg_rd (dev_id, offset,
                                       spi_rd_data));
}

/**
 * @brief lld_spi_eeprom_wr_enable
 *  enable the write enable latch on the eeprom on SPI bus
 *
 * @param dev_id: int
 *  chip id
 *
 * @param en: int
 *  enable = 1, disable = 0
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int lld_spi_eeprom_wr_enable (int dev_id,
                                     int en)
{
    uint8_t spi_code = en ? BF_SPI_OPCODE_WR_EN :
                       BF_SPI_OPCODE_WR_DI;
    /* w_size = r_size = 0 */
    return (lld_spi_wr (dev_id, spi_code, NULL, 0,
                        0));
}

/**
 * @brief bf_spi_eeprom_wr
 *  write to eeprom address
 *
 * @param dev_id: int
 *  chip id
 *
 * @param addr: uint32_t
 *  address offset in EEPROM
 *
 * @param buf: uint8_t *
 *   buffer to write  to EEPROM
 *
 * @param buf_size:  int
 *   buffer  size
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int bf_spi_eeprom_wr (int dev_id,
                             uint32_t addr,
                             uint8_t *buf,
                             int buf_size)
{
    uint8_t wr_buf[8];

    if (buf_size < 0 ||
        (addr + buf_size) > BF_SPI_EEPROM_SIZE) {
        return -2;
    }
    /* write 4 bytes (maximum) into EEPROM at at time */
    while (buf_size > BF_SPI_EEPROM_WR_SIZE) {
        /* do wr_enable, wr_disable is automatic in h/w at the end of the
         * write operation
         */
        if (lld_spi_eeprom_wr_enable (dev_id, 1) != 0) {
            return -1;
        }
        usleep (1000);
        wr_buf[0] = addr >> 8;
        wr_buf[1] = addr;
        memcpy (&wr_buf[2], buf, BF_SPI_EEPROM_WR_SIZE);
        if (lld_spi_wr (dev_id,
                        BF_SPI_OPCODE_WR_DATA,
                        &wr_buf[0],
                        BF_SPI_EEPROM_WR_SIZE + 2,
                        0) != 0) {
            return -1;
        }
        buf += BF_SPI_EEPROM_WR_SIZE;
        buf_size -= BF_SPI_EEPROM_WR_SIZE;
        addr += BF_SPI_EEPROM_WR_SIZE;
        usleep (5000);
    }
    if (buf_size) {
        if (lld_spi_eeprom_wr_enable (dev_id, 1) != 0) {
            return -1;
        }
        usleep (1000);
        wr_buf[0] = addr >> 8;
        wr_buf[1] = addr;
        memcpy (&wr_buf[2], buf, buf_size);
        if (lld_spi_wr (
                dev_id, BF_SPI_OPCODE_WR_DATA, &wr_buf[0],
                buf_size + 2, 0) != 0) {
            return -1;
        }
        usleep (5000);
    }
    lld_spi_eeprom_wr_enable (dev_id, 0);
    return 0;
}

/**
 * @brief lld_spi_eeprom_rd
 *  write to eeprom address
 *
 * @param dev_id: int
 *  chip id
 *
 * @param addr: uint32_t
 *  address offset in EEPROM
 *
 * @param buf: uint8_t *
 *   buffer to read into  from EEPROM
 *
 * @param buf_size:  int
 *   buffer  size
 *
 * @return status
 *   0 on success
 *   BF ERROR code on failure
 *
 */
static int bf_spi_eeprom_rd (int dev_id,
                             uint32_t addr,
                             uint8_t *buf,
                             int buf_size)
{
    uint8_t wr_buf[BF_SPI_EEPROM_WR_SIZE];
    int spi_delay_us = (700 * BF_SPI_PERIOD_NS) /
                       1000;
    uint32_t data, swap_data;

    if ((addr + buf_size) > BF_SPI_EEPROM_SIZE) {
        return -2;
    }
    while (buf_size > BF_SPI_EEPROM_RD_SIZE) {
        /* create the SPI write command */
        wr_buf[0] = (uint8_t) (addr >> 8);
        wr_buf[1] = (uint8_t)addr;
        wr_buf[2] = 0;
        if (lld_spi_wr (dev_id,
                        BF_SPI_OPCODE_RD_DATA,
                        wr_buf,
                        BF_SPI_EEPROM_RD_CMD_SIZE,
                        BF_SPI_EEPROM_RD_SIZE) != 0) {
            return -1;
        }
        /* read the EEPROM content */
        /* wait for data to be read back on SPI bus */
        usleep (spi_delay_us);
        if (lld_spi_get_rd_data (dev_id, &data) != 0) {
            return -1;
        }
        swap_data = swap32 (data);
        * (uint32_t *)buf = swap_data;
        buf += BF_SPI_EEPROM_RD_SIZE;
        buf_size -= BF_SPI_EEPROM_RD_SIZE;
        addr += BF_SPI_EEPROM_RD_SIZE;
    }
    if (buf_size) {
        /* create the SPI write command */
        wr_buf[0] = (uint8_t) (addr >> 8);
        wr_buf[1] = (uint8_t)addr;
        wr_buf[2] = 0;
        if (lld_spi_wr (dev_id,
                        BF_SPI_OPCODE_RD_DATA,
                        wr_buf,
                        BF_SPI_EEPROM_RD_CMD_SIZE,
                        BF_SPI_EEPROM_RD_SIZE) != 0) {
            return -1;
        }
        /* read the EEPROM content */
        usleep (spi_delay_us);
        if (lld_spi_get_rd_data (dev_id, &data) != 0) {
            return -1;
        }
        swap_data = swap32 (data);
        while (buf_size) {
            *buf = swap_data;
            buf++;
            buf_size--;
            swap_data >>= 8;
        }
    }
    return 0;
}

/** Tofino SPI eeprom write
 *
 *  @param chip_id
 *    chip_id
 *  @param fd
 *    file pointer containing the eeprom contents
 *  @param offset
 *    offset in SPI eeprom to write data to
 *  @param size
 *    byte count to write into SPI eeprom
 *  @return
 *    0 on success and -1 on error
 */
int bf_pltfm_spi_wr (int chip_id, FILE *fd,
                     int offset, int size)
{
    int err;
    uint8_t *buf;

    if (offset < 0 ||
        (offset + size) > BF_SPI_EEPROM_SIZE) {
        TOFINO_PORTING_LOG_ERR ("invalid parameters\n");
        return -1;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        TOFINO_PORTING_LOG_ERR ("error in malloc\n");
        return -1;
    }
    if (fread (buf, 1, size, fd) != (size_t)size) {
        TOFINO_PORTING_LOG_ERR ("error reading from file\n");
        free (buf);
        return -1;
    }
    err = 0;
    if (bf_spi_eeprom_wr (chip_id, offset, buf,
                          size) != 0) {
        TOFINO_PORTING_LOG_ERR ("error writing to SPI EEPROM\n");
        err = -1;
    }
    free (buf);
    return err;
}

/** Tofino SPI eeprom read
 *
 *  @param chip_id
 *    chip_id
 *  @param fd
 *    file pointer to read the eeprom contents into
 *  @param offset
 *    offset in SPI eeprom to read from
 *  @param size
 *    byte count to  read from SPI eeprom
 *  @return
 *    0 on success and -1 on error
 */
static int bf_pltfm_spi_rd (int chip_id, FILE *fd,
                            int offset, int size)
{
    int err;
    uint8_t *buf;

    if (offset < 0 ||
        (offset + size) > BF_SPI_EEPROM_SIZE) {
        TOFINO_PORTING_LOG_ERR ("invalid parameters\n");
        return -1;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        TOFINO_PORTING_LOG_ERR ("error in malloc\n");
        return -1;
    }
    err = 0;
    if (bf_spi_eeprom_rd (chip_id, offset, buf,
                          size) == 0) {
        if (fwrite (buf, 1, size, fd) != (size_t)size) {
            TOFINO_PORTING_LOG_ERR ("error writing to file\n");
            err = -1;
        }
    } else {
        TOFINO_PORTING_LOG_ERR ("error reading SPI EEPROM\n");
        err = -1;
    }
    free (buf);
    return err;
}

static void print_usage (char *cmd)
{
    TOFINO_PORTING_LOG_ERR ("Usage:  \n");
    TOFINO_PORTING_LOG_ERR (
        "%s <dev_id> <rd/wr> <custom-init-string 0: X532P-T 1: X564P-T 2: X308P-T>\n"
        "<file_name> [eeprom_offset (default 0] [size "
        "(default : file size when writing and eeprom size when reading)]\n",
        cmd);
}

int main (int argc, char *argv[])
{
    FILE *fp;
    int dev_id, wr_sel, offset, size, rc;
    char *fname, *spi_init_str;

    if (argc < 5) {
        print_usage (argv[0]);
        return -1;
    }

    dev_id = atoi (argv[1]);

    spi_init_str = argv[3];
    fname = argv[4];

    if (strncmp (argv[2], "wr", 2) == 0) {
        wr_sel = 1;
        fp = fopen (fname, "rb");
        if (!fp) {
            TOFINO_PORTING_LOG_ERR ("error opening %s\n",
                                    fname);
            return -1;
        }
    } else if (strncmp (argv[2], "rd", 2) == 0) {
        wr_sel = 0;
        fp = fopen (fname, "wb");
        if (!fp) {
            TOFINO_PORTING_LOG_ERR ("error opening %s\n",
                                    fname);
            return -1;
        }
    } else {
        print_usage (argv[0]);
        return -1;
    }

    if (argc > 5) {
        offset = strtol (argv[5], NULL, 0);
    } else {
        offset = 0;
    }

    if (argc > 6) {
        size = strtol (argv[6], NULL, 0);
    } else {
        if (wr_sel) {
            fseek (fp, 0, SEEK_END);
            size = ftell (fp);
            fseek (fp, 0, SEEK_SET);
        } else {
            size = BF_SPI_EEPROM_SIZE;
        }
    }

    if (tofino_porting_spi_init (dev_id,
                                 spi_init_str) != 0) {
        TOFINO_PORTING_LOG_ERR ("error in spi_init\n");
        return -1;
    }
    if (wr_sel) {
        rc = bf_pltfm_spi_wr (dev_id, fp, offset, size);
    } else {
        rc = bf_pltfm_spi_rd (dev_id, fp, offset, size);
    }
    tofino_porting_spi_finish (dev_id);
    if (rc) {
        TOFINO_PORTING_LOG_ERR ("error accessing SPI eeprom\n");
    } else {
        TOFINO_PORTING_LOG_ERR ("SPI %s OK\n",
                                (wr_sel ? "write" : "read"));
    }

    /* A bug ? */
    fclose (fp);

    return rc;
}
