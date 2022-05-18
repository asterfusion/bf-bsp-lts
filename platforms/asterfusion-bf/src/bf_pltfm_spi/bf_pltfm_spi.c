/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <dvm/bf_drv_intf.h>
#include <lld/lld_spi_if.h>
#include <bf_pltfm_spi.h>

#if 0
#define SPI_DEBUG 0 /* Make this 1 for debugging */

#define LOG_ERROR printf
#define LOG_DEBUG \
  if (SPI_DEBUG) printf

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
    int err;
    FILE *fd;
    uint8_t *buf;

    if (offset < 0 ||
        (offset + size) >= BF_SPI_EEPROM_SIZE) {
        LOG_ERROR ("invalid parameters\n");
        return -1;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        LOG_ERROR ("error in malloc\n");
        return -1;
    }
    fd = fopen (fname, "rb");
    if (!fd) {
        LOG_ERROR ("error opening %s\n", fname);
        free (buf);
        return -1;
    }
    if (fread (buf, 1, size, fd) != (size_t)size) {
        LOG_ERROR ("error reading %s\n", fname);
        free (buf);
        fclose (fd);
        return -1;
    }
    err = 0;
    if (bf_spi_eeprom_wr (chip_id, offset, buf,
                          size) != BF_SUCCESS) {
        LOG_ERROR ("error writing to SPI EEPROM\n");
        err = -1;
    }
    free (buf);
    fclose (fd);
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
    int err;
    FILE *fd;
    uint8_t *buf;

    if (offset < 0 ||
        (offset + size) >= BF_SPI_EEPROM_SIZE) {
        LOG_ERROR ("invalid parameters\n");
        return -1;
    }
    buf = malloc (BF_SPI_EEPROM_SIZE);
    if (!buf) {
        LOG_ERROR ("error in malloc\n");
        return -1;
    }
    fd = fopen (fname, "wb");
    if (!fd) {
        LOG_ERROR ("error opening %s\n", fname);
        free (buf);
        return -1;
    }
    err = 0;
    if (bf_spi_eeprom_rd (chip_id, offset, buf,
                          size) == BF_SUCCESS) {
        if (fwrite (buf, 1, size, fd) != (size_t)size) {
            LOG_ERROR ("error writing %s\n", fname);
            err = -1;
        }
    } else {
        LOG_ERROR ("error reading SPI EEPROM\n");
        err = -1;
    }
    free (buf);
    fclose (fd);
    return err;
}

static ucli_status_t bf_pltfm_ucli_ucli__spi_wr_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    char fname[64];
    int offset;
    int size;

    UCLI_COMMAND_INFO (
        uc, "spi-wr", 4,
        "spi-wr <dev> <file name> <offset> <size>");

    dev = atoi (uc->pargs->args[0]);
    strncpy (fname, uc->pargs->args[1],
             sizeof (fname) - 1);
    fname[sizeof (fname) - 1] = '\0';
    offset = atoi (uc->pargs->args[2]);
    size = atoi (uc->pargs->args[3]);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_spi: spi-wr <dev=%d> <file name=%s> <offset=%d> <size=%d>\n",
        dev,
        fname,
        offset,
        size);

    if (bf_pltfm_spi_wr (dev, fname, offset, size)) {
        aim_printf (&uc->pvs,
                    "error writing to SPI eeprom\n");
    } else {
        aim_printf (&uc->pvs, "SPI eeprom write OK\n");
    }
    return 0;
}

static ucli_status_t bf_pltfm_ucli_ucli__spi_rd_
(ucli_context_t *uc)
{
    bf_dev_id_t dev = 0;
    char fname[64];
    int offset;
    int size;

    UCLI_COMMAND_INFO (
        uc, "spi-rd", 4,
        "spi-rd <dev> <file name> <offset> <size>");

    dev = atoi (uc->pargs->args[0]);
    strncpy (fname, uc->pargs->args[1],
             sizeof (fname) - 1);
    fname[sizeof (fname) - 1] = '\0';
    offset = atoi (uc->pargs->args[2]);
    size = atoi (uc->pargs->args[3]);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_spi: spi-rd <dev=%d> <file name=%s> <offset=%d> <size=%d>\n",
        dev,
        fname,
        offset,
        size);

    if (bf_pltfm_spi_rd (dev, fname, offset, size)) {
        aim_printf (&uc->pvs,
                    "error reading from SPI eeprom\n");
    } else {
        aim_printf (&uc->pvs, "SPI eeprom read OK\n");
    }
    return 0;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_spi_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__spi_wr_, bf_pltfm_ucli_ucli__spi_rd_, NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t bf_pltfm_spi_ucli_module__
= {
    "spi_ucli", NULL, bf_pltfm_spi_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_spi_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_spi_ucli_module__);
    n = ucli_node_create ("spi", m,
                          &bf_pltfm_spi_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("spi"));
    return n;
}

/* Init function  */
int bf_pltfm_spi_init (void *arg)
{
    (void)arg;
    return 0;
}
#endif
