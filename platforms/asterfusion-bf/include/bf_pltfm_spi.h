/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PLTFM_SPI_H
#define _BF_PLTFM_SPI_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

int bf_pltfm_spi_wr (int chip_id,
                     const char *fname, int offset, int size);
int bf_pltfm_spi_rd (int chip_id,
                     const char *fname, int offset, int size);
int bf_pltfm_spi_cmp (const char *, const char *);

int bf_pltfm_spi_init (void *arg);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_SPI_H */
