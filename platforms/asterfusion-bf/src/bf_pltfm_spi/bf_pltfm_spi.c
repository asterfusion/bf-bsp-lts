/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <dvm/bf_drv_intf.h>
#include <lld/lld_spi_if.h>
#include <bf_pltfm_spi.h>
#include <pltfm_types.h>

#define SPI_DEBUG 0 /* Make this 1 for debugging */
#define logerr printf
#define logdebug \
  if (SPI_DEBUG) printf

/* The database of SPI ROM Image for PCIe Gen2 CPU Link.
 * This SPI ROM image includes latest PCIe SerDes FW and
 * device setting overwrite needed for proper PCIe link up.
 * PCIe will only advertise Gen2 capable.
 * by tsihang, 2022-11-15. */
struct spi_firmware_t {
    char *ver;
    char *md5val;
};

/* There is no way to get spi_firmware version at this moment.
 * And there's a fact that we have to upgrade the firmware for
 * those devices in shipping or already in customer's hands.
 * So here, we record all firmwares came from official to evidence
 * whether there's a need to upgrade those ones or not.
 * by tsihang, 2022-11-15. */
struct spi_firmware_t firmware_db[] = {
    {"tofino_B0_spi_gen2_rev07.bin",                  "eeda1f0f20e3495c87545d1ba9bd056d"},
    {"tofino_B0_spi_gen2_rev09_corrected_220815.bin", "4972e63341c58887c5940472e67781d2"},
    {NULL, NULL}
};

static size_t bf_pltfm_get_file_size (const char *file) {
    size_t size;
    FILE *fd;

    fd = fopen (file, "rb");
    if (!fd) {
        LOG_ERROR ("error getting size %s : %s\n", file, oryx_safe_strerror(errno));
        return -1;
    }

    fseek (fd, 0, SEEK_END);
    size = (size_t)ftell (fd);
    fseek (fd, 0, SEEK_SET);
    fclose (fd);

    return size;
}

static size_t bf_pltfm_get_file_context (const char *file, void **context, size_t len) {
    size_t size = -1;
    FILE *fd = NULL;
    char *buf = NULL;

    *context = NULL;

    size = ((int)len > 0) ? len : bf_pltfm_get_file_size (file);
    if ((int)size < 0) {
        goto finish;
    }

    fd = fopen (file, "rb");
    if (!fd) {
        LOG_ERROR ("error opening  %s : %s\n", file, oryx_safe_strerror(errno));
        goto finish;
    }

    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        LOG_ERROR ("error allocating  %d : %s\n", BF_SPI_EEPROM_SIZE, oryx_safe_strerror(errno));
        goto finish;
    }

    if (fread (buf, 1, size, fd) != (size_t)size) {
        LOG_ERROR ("error reading  %s : %s\n", file, oryx_safe_strerror(errno));
        size = -1;
        free (buf);
        goto finish;
    }

    logdebug ("%s : %lu byte(s)\n", file, size);
    *context = buf;

finish:
    if (fd) fclose (fd);
    return size;
}

int bf_pltfm_spi_cmp (const char *src, const char *dst)
{
    size_t size0, size1;
    char *sbuf, *dbuf;
    int err = -1;

    sbuf = dbuf = NULL;

    size0 = bf_pltfm_get_file_context (src, (void **)&sbuf, 0);
    if (size0 <= 0) {
        goto finish;
    }

    size1 = bf_pltfm_get_file_context (dst, (void **)&dbuf, 0);
    if (size1 <= 0) {
        goto finish;
    }

    if (size0 != size1 || (sbuf == NULL || dbuf == NULL)) {
        goto finish;
    }

    if (strncmp ((const char *)sbuf, (const char *)dbuf, size0)) {
        goto finish;
    }

    /* Finally everything seems fine. */
    err = 0;

finish:
    if (sbuf) free (sbuf);
    if (dbuf) free (dbuf);
    return err;
}


/** Tofino SPI eeprom write
 *
 *  @param chip_id
 *    chip_id
 *  @param fname
 *    file containing the eeprom contents
 *  @param offset
 *    offset in SPI eeprom to write data to
 *  @param size
 *    byte count to write into SPI eeprom
 *  @return
 *    0 on success and -1 on error
 */
int bf_pltfm_spi_wr (int chip_id,
                     const char *fname, int offset, int size)
{
    int err = -1;
    FILE *fd = NULL;
    uint8_t *buf = NULL;

    if (offset < 0 ||
        (offset + size) >= BF_SPI_EEPROM_SIZE) {
        LOG_ERROR ("invalid parameters\n");
        goto finish;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        LOG_ERROR ("error in malloc\n");
        goto finish;
    }
    fd = fopen (fname, "rb");
    if (!fd) {
        LOG_ERROR ("error opening %s\n", fname);
        goto finish;
    }
    if (fread (buf, 1, size, fd) != (size_t)size) {
        LOG_ERROR ("error reading %s\n", fname);
        goto finish;
    }

    /* This API comes from pkgsrc\bf-drivers\src\lld\lld_spi_if.c.
     * by tsihang, 2022-11-07. */
    if (bfn_spi_eeprom_wr (chip_id, sub_devid, offset, buf,
                          size) != BF_SUCCESS) {
        LOG_ERROR ("error writing to SPI EEPROM\n");
        goto finish;
    }

    /* Finally everything seems fine. */
    err = 0;

finish:
    if (buf) free (buf);
    if (fd)  fclose (fd);
    return err;
}

/** Tofino SPI eeprom read
 *
 *  @param chip_id
 *    chip_id
 *  @param fname
 *    file to read the eeprom contents into
 *  @param offset
 *    offset in SPI eeprom to read from
 *  @param size
 *    byte count to  read from SPI eeprom
 *  @return
 *    0 on success and -1 on error
 */
int bf_pltfm_spi_rd (int chip_id,
                     const char *fname, int offset, int size)
{
    int err = -1;
    FILE *fd = NULL;
    uint8_t *buf = NULL;

    if (offset < 0 ||
        (offset + size) >= BF_SPI_EEPROM_SIZE) {
        LOG_ERROR ("invalid parameters\n");
        goto finish;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        LOG_ERROR ("error in malloc\n");
        goto finish;
    }
    fd = fopen (fname, "wb");
    if (!fd) {
        LOG_ERROR ("error opening %s\n", fname);
        goto finish;
    }

    /* This API comes from pkgsrc\bf-drivers\src\lld\lld_spi_if.c.
     * by tsihang, 2022-11-07. */
    if (bfn_spi_eeprom_rd (chip_id, sub_devid, offset, buf,
                          size) == BF_SUCCESS) {
        if (fwrite (buf, 1, size, fd) != (size_t)size) {
            LOG_ERROR ("error writing %s\n", fname);
            goto finish;
        }
    } else {
        LOG_ERROR ("error reading SPI EEPROM\n");
        goto finish;
    }

    /* Finally everything seems fine. */
    err = 0;

finish:
    if (buf) free (buf);
    if (fd)  fclose (fd);
    return err;
}

/* Init function  */
int bf_pltfm_spi_init (void *arg)
{
    (void)arg;
    return 0;
}

