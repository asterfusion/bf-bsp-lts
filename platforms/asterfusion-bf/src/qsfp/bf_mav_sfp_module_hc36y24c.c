/*!
 * @file bf_pltfm_sfp_module.c
 * @date 2020/05/31
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>	/* for NAME_MAX */
#include <sys/ioctl.h>
#include <string.h>
#include <strings.h>	/* for strcasecmp() */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_types/bf_types.h>
#include <tofino/bf_pal/bf_pal_types.h>
#include <bf_pm/bf_pm_intf.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_mav_qsfp_i2c_lock.h>
#include <bf_pltfm_syscpld.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#include <bf_pltfm_qsfp.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <pltfm_types.h>
#include "bf_mav_sfp_module.h"

/* Keep this the same with MAV_QSFP_LOCK and MAV_QSFP_UNLOCK.
 * by tsihang, 2021-07-15. */
#define MAV_SFP_LOCK bf_pltfm_i2c_lock()
#define MAV_SFP_UNLOCK bf_pltfm_i2c_unlock()

#define DEFAULT_TIMEOUT_MS 500

extern bool sfp_lookup1 (IN uint32_t
                         module, OUT struct sfp_ctx_t **sfp);


/* access is 0 based index. order by module dont't change it! */
static struct sfp_ctx_st_t sfp_st_hc[] = {
    {{BF_MAV_SYSCPLD2, 17, BIT (6)}, {BF_MAV_SYSCPLD2, 2, BIT (6)}, {BF_MAV_SYSCPLD2,  7, BIT (6)}}, /* 32/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (6)}, {BF_MAV_SYSCPLD2, 3, BIT (6)}, {BF_MAV_SYSCPLD2,  8, BIT (6)}}, /* 32/1 */
    {{BF_MAV_SYSCPLD2, 21, BIT (3)}, {BF_MAV_SYSCPLD2, 6, BIT (3)}, {BF_MAV_SYSCPLD2, 11, BIT (3)}}, /* 33/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (6)}, {BF_MAV_SYSCPLD2, 4, BIT (6)}, {BF_MAV_SYSCPLD2,  9, BIT (6)}}, /* 32/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (6)}, {BF_MAV_SYSCPLD2, 5, BIT (6)}, {BF_MAV_SYSCPLD2, 10, BIT (6)}}, /* 32/3 */
    {{BF_MAV_SYSCPLD2, 21, BIT (2)}, {BF_MAV_SYSCPLD2, 6, BIT (2)}, {BF_MAV_SYSCPLD2, 11, BIT (2)}}, /* 33/2 */
    {{BF_MAV_SYSCPLD2, 17, BIT (7)}, {BF_MAV_SYSCPLD2, 2, BIT (7)}, {BF_MAV_SYSCPLD2,  7, BIT (7)}}, /* 31/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (7)}, {BF_MAV_SYSCPLD2, 3, BIT (7)}, {BF_MAV_SYSCPLD2,  8, BIT (7)}}, /* 31/1 */
    {{BF_MAV_SYSCPLD2, 21, BIT (1)}, {BF_MAV_SYSCPLD2, 6, BIT (1)}, {BF_MAV_SYSCPLD2, 11, BIT (1)}}, /* 33/1 */
    {{BF_MAV_SYSCPLD2, 19, BIT (7)}, {BF_MAV_SYSCPLD2, 4, BIT (7)}, {BF_MAV_SYSCPLD2,  9, BIT (7)}}, /* 31/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (7)}, {BF_MAV_SYSCPLD2, 5, BIT (7)}, {BF_MAV_SYSCPLD2, 10, BIT (7)}}, /* 31/3 */
    {{BF_MAV_SYSCPLD2, 21, BIT (0)}, {BF_MAV_SYSCPLD2, 6, BIT (0)}, {BF_MAV_SYSCPLD2, 11, BIT (0)}}, /* 33/0 */
    {{BF_MAV_SYSCPLD2, 17, BIT (3)}, {BF_MAV_SYSCPLD2, 2, BIT (3)}, {BF_MAV_SYSCPLD2,  7, BIT (3)}}, /* 29/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (3)}, {BF_MAV_SYSCPLD2, 3, BIT (3)}, {BF_MAV_SYSCPLD2,  8, BIT (3)}}, /* 29/1 */
    {{BF_MAV_SYSCPLD2, 20, BIT (5)}, {BF_MAV_SYSCPLD2, 5, BIT (5)}, {BF_MAV_SYSCPLD2, 10, BIT (5)}}, /* 30/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (3)}, {BF_MAV_SYSCPLD2, 4, BIT (3)}, {BF_MAV_SYSCPLD2,  9, BIT (3)}}, /* 29/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (3)}, {BF_MAV_SYSCPLD2, 5, BIT (3)}, {BF_MAV_SYSCPLD2, 10, BIT (3)}}, /* 29/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (5)}, {BF_MAV_SYSCPLD2, 4, BIT (5)}, {BF_MAV_SYSCPLD2,  9, BIT (5)}}, /* 30/2 */
    {{BF_MAV_SYSCPLD2, 17, BIT (4)}, {BF_MAV_SYSCPLD2, 2, BIT (4)}, {BF_MAV_SYSCPLD2,  7, BIT (4)}}, /* 28/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (4)}, {BF_MAV_SYSCPLD2, 3, BIT (4)}, {BF_MAV_SYSCPLD2,  8, BIT (4)}}, /* 28/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (7)}, {BF_MAV_SYSCPLD3, 9, BIT (7)}, {BF_MAV_SYSCPLD3, 11, BIT (7)}}, /* 30/1 */
    {{BF_MAV_SYSCPLD2, 19, BIT (4)}, {BF_MAV_SYSCPLD3, 9, BIT (4)}, {BF_MAV_SYSCPLD3, 11, BIT (4)}}, /* 28/2 */
    {{BF_MAV_SYSCPLD3, 15, BIT (5)}, {BF_MAV_SYSCPLD2, 5, BIT (4)}, {BF_MAV_SYSCPLD2, 10, BIT (4)}}, /* 28/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (6)}, {BF_MAV_SYSCPLD3, 9, BIT (6)}, {BF_MAV_SYSCPLD3, 11, BIT (6)}}, /* 30/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (0)}, {BF_MAV_SYSCPLD3, 8, BIT (0)}, {BF_MAV_SYSCPLD3, 10, BIT (0)}}, /* 26/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (1)}, {BF_MAV_SYSCPLD3, 8, BIT (1)}, {BF_MAV_SYSCPLD3, 10, BIT (1)}}, /* 26/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (3)}, {BF_MAV_SYSCPLD3, 9, BIT (3)}, {BF_MAV_SYSCPLD3, 11, BIT (3)}}, /* 27/3 */
    {{BF_MAV_SYSCPLD3, 14, BIT (2)}, {BF_MAV_SYSCPLD3, 8, BIT (2)}, {BF_MAV_SYSCPLD3, 10, BIT (2)}}, /* 26/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (3)}, {BF_MAV_SYSCPLD3, 8, BIT (3)}, {BF_MAV_SYSCPLD3, 10, BIT (3)}}, /* 26/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (2)}, {BF_MAV_SYSCPLD3, 9, BIT (2)}, {BF_MAV_SYSCPLD3, 11, BIT (2)}}, /* 27/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (4)}, {BF_MAV_SYSCPLD3, 8, BIT (4)}, {BF_MAV_SYSCPLD3, 10, BIT (4)}}, /* 25/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (5)}, {BF_MAV_SYSCPLD3, 8, BIT (5)}, {BF_MAV_SYSCPLD3, 10, BIT (5)}}, /* 25/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (1)}, {BF_MAV_SYSCPLD3, 9, BIT (1)}, {BF_MAV_SYSCPLD3, 11, BIT (1)}}, /* 27/1 */
    {{BF_MAV_SYSCPLD3, 14, BIT (6)}, {BF_MAV_SYSCPLD3, 8, BIT (6)}, {BF_MAV_SYSCPLD3, 10, BIT (6)}}, /* 25/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (7)}, {BF_MAV_SYSCPLD3, 8, BIT (7)}, {BF_MAV_SYSCPLD3, 10, BIT (7)}}, /* 25/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (0)}, {BF_MAV_SYSCPLD3, 9, BIT (0)}, {BF_MAV_SYSCPLD3, 11, BIT (0)}}, /* 27/0 */
    {{BF_MAV_SYSCPLD2, 25, BIT (0)}, {BF_MAV_SYSCPLD2, 22, BIT (0)}, {BF_MAV_SYSCPLD2, 23, BIT (0)}}, /* 65/0 */
    {{BF_MAV_SYSCPLD2, 25, BIT (1)}, {BF_MAV_SYSCPLD2, 22, BIT (1)}, {BF_MAV_SYSCPLD2, 23, BIT (1)}}, /* 65/1 */
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_hc36y24c[] = {
    {{"Y1",  32, 0, (1 << 0), (0x77 << 1)}, (void **) &sfp_st_hc[0] },
    {{"Y2",  32, 1, (1 << 1), (0x77 << 1)}, (void **) &sfp_st_hc[1] },
    {{"Y3",  33, 3, (1 << 3), (0x76 << 1)}, (void **) &sfp_st_hc[2] },
    {{"Y4",  32, 2, (1 << 2), (0x77 << 1)}, (void **) &sfp_st_hc[3] },
    {{"Y5",  32, 3, (1 << 3), (0x77 << 1)}, (void **) &sfp_st_hc[4] },
    {{"Y6",  33, 2, (1 << 2), (0x76 << 1)}, (void **) &sfp_st_hc[5] },
    {{"Y7",  31, 0, (1 << 4), (0x77 << 1)}, (void **) &sfp_st_hc[6] },
    {{"Y8",  31, 1, (1 << 5), (0x77 << 1)}, (void **) &sfp_st_hc[7] },
    {{"Y9",  33, 1, (1 << 1), (0x76 << 1)}, (void **) &sfp_st_hc[8] },
    {{"Y10", 31, 2, (1 << 6), (0x77 << 1)}, (void **) &sfp_st_hc[9] },
    {{"Y11", 31, 3, (1 << 7), (0x77 << 1)}, (void **) &sfp_st_hc[10] },
    {{"Y12", 33, 0, (1 << 0), (0x76 << 1)}, (void **) &sfp_st_hc[11] },
    {{"Y13", 29, 0, (1 << 4), (0x74 << 1)}, (void **) &sfp_st_hc[12] },
    {{"Y14", 29, 1, (1 << 5), (0x74 << 1)}, (void **) &sfp_st_hc[13] },
    {{"Y15", 30, 3, (1 << 7), (0x75 << 1)}, (void **) &sfp_st_hc[14] },
    {{"Y16", 29, 2, (1 << 6), (0x74 << 1)}, (void **) &sfp_st_hc[15] },
    {{"Y17", 29, 3, (1 << 7), (0x74 << 1)}, (void **) &sfp_st_hc[16] },
    {{"Y18", 30, 2, (1 << 6), (0x75 << 1)}, (void **) &sfp_st_hc[17] },
    {{"Y19", 28, 0, (1 << 0), (0x75 << 1)}, (void **) &sfp_st_hc[18] },
    {{"Y20", 28, 1, (1 << 1), (0x75 << 1)}, (void **) &sfp_st_hc[19] },
    {{"Y21", 30, 1, (1 << 5), (0x75 << 1)}, (void **) &sfp_st_hc[20] },
    {{"Y22", 28, 2, (1 << 2), (0x75 << 1)}, (void **) &sfp_st_hc[21] },
    {{"Y23", 28, 3, (1 << 3), (0x75 << 1)}, (void **) &sfp_st_hc[22] },
    {{"Y24", 30, 0, (1 << 4), (0x75 << 1)}, (void **) &sfp_st_hc[23] },
    {{"Y25", 26, 0, (1 << 0), (0x73 << 1)}, (void **) &sfp_st_hc[24] },
    {{"Y26", 26, 1, (1 << 1), (0x73 << 1)}, (void **) &sfp_st_hc[25] },
    {{"Y27", 27, 3, (1 << 3), (0x74 << 1)}, (void **) &sfp_st_hc[26] },
    {{"Y28", 26, 2, (1 << 2), (0x73 << 1)}, (void **) &sfp_st_hc[27] },
    {{"Y29", 26, 3, (1 << 3), (0x73 << 1)}, (void **) &sfp_st_hc[28] },
    {{"Y30", 27, 2, (1 << 2), (0x74 << 1)}, (void **) &sfp_st_hc[29] },
    {{"Y31", 25, 0, (1 << 4), (0x73 << 1)}, (void **) &sfp_st_hc[30] },
    {{"Y32", 25, 1, (1 << 5), (0x73 << 1)}, (void **) &sfp_st_hc[31] },
    {{"Y33", 27, 1, (1 << 1), (0x74 << 1)}, (void **) &sfp_st_hc[32] },
    {{"Y34", 25, 2, (1 << 6), (0x73 << 1)}, (void **) &sfp_st_hc[33] },
    {{"Y35", 25, 3, (1 << 7), (0x73 << 1)}, (void **) &sfp_st_hc[34] },
    {{"Y36", 27, 0, (1 << 0), (0x74 << 1)}, (void **) &sfp_st_hc[35] },
    {{"Y37", 65, 0, (1 << 6), (0x76 << 1)}, (void **) &sfp_st_hc[36] },
    {{"Y38", 65, 1, (1 << 7), (0x76 << 1)}, (void **) &sfp_st_hc[37] },
};

static void do_lock_hc()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_LOCK;
}

static void do_unlock_hc()
{
    /* CP2112 is used for HC to read SFP. And there's no
     * resource competition with other application.
     * So, here we make a stub.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_UNLOCK;
}

static int sfp_select_hc (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }

        // switch channel
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        (uint8_t)sfp->info.rev,  sfp->info.i2c_chnl_addr,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int sfp_unselect_hc (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }

        // switch channel
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        (uint8_t)sfp->info.rev,  0,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
static int
sfp_read_sub_module_hc (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        if (bf_pltfm_cp2112_write_byte (hndl, addr,
                                        offset, DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -2;
            break;
        }
        if (len > 128) {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, 128,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
            buf += 128;
            len -= 128;
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -4;
                break;
            }
        } else {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -5;
                break;
            }
        }
    } while (0);
    return rc;
}

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
static int
sfp_write_sub_module_hc (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    uint8_t tmp[61];
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        // write module, CP2112 write bytes should not longer than 61, but we limit it to 60 because we use one byte to mark offset
        while (len > 60) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * 60);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp, 61,
                                       DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -2;
                break;
            }
            buf += 60;
            offset += 60;
            len -= 60;
        }
        if (len > 0) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * len);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp,
                                       len + 1, DEFAULT_TIMEOUT_MS) !=
                BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
        }
    } while (0);
    return rc;
}

static int
sfp_get_module_pres_hc (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0, off = 0, i = 0, j = 0;
    uint8_t val[32] = {0xff};
    struct sfp_ctx_st_t *sfp;
    struct st_ctx_t *pres_ctx;

    /* off=[6:2], off=[22] */
    off = 2;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 0);
    MASKBIT (val[off], 1);
    MASKBIT (val[off], 2);
    MASKBIT (val[off], 5);

    off = 3;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 0);
    MASKBIT (val[off], 1);
    MASKBIT (val[off], 2);
    MASKBIT (val[off], 5);

    off = 4;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 0);
    MASKBIT (val[off], 1);
    MASKBIT (val[off], 2);
    MASKBIT (val[off], 4);

    off = 5;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 0);
    MASKBIT (val[off], 1);
    MASKBIT (val[off], 2);

    off = 6;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 4);
    MASKBIT (val[off], 5);
    MASKBIT (val[off], 6);
    MASKBIT (val[off], 7);

    off = 22;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2, off, &val[off]);
    if (rc) {
        goto end;
    }
    MASKBIT (val[off], 2);
    MASKBIT (val[off], 3);
    MASKBIT (val[off], 4);
    MASKBIT (val[off], 5);
    MASKBIT (val[off], 6);
    MASKBIT (val[off], 7);

    /* off=[9:8] */
    off = 8;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD3, off, &val[off]);
    if (rc) {
        goto end;
    }

    off = 9;
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD3, off, &val[off]);
    if (rc) {
        goto end;
    }

    MASKBIT (val[off], 5);

    for (i = 0; i < ARRAY_LENGTH (val); i ++) {
        for (j = 0; j < ARRAY_LENGTH (sfp_st_hc); j ++) {
            sfp = &sfp_st_hc[j];
            pres_ctx = &sfp->pres;
            if (i == pres_ctx->off) {
                if (! (val[i] & (1 << pres_ctx->off_b))) {
                    //fprintf(stdout, " SFP %2d detected\n", j + 1);
                    if (j + 1 <= 32) {
                        *pres_l &= ~ (1 << (((j + 1) % 32) - 1));
                    } else {
                        *pres_h &= ~ (1 << (((j + 1) % 32) - 1));
                    }
                }
            }
        }
    }


end:
    return rc;
}

static int
sfp_module_tx_disable_hc (
    uint32_t module, bool disable, void *st)
{
    // define
    int rc = 0;
    uint8_t val;
    /* Convert st to your own st. */
    struct st_ctx_t *dis = & ((struct sfp_ctx_st_t *)
                              st)->tx_dis;

    rc = bf_pltfm_cpld_read_byte (dis->cpld_sel, dis->off, &val);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    // set value
    if (disable) {
        val |= (1 << dis->off_b);
    } else {
        val &= (~ (1 << dis->off_b));
    }

    rc = bf_pltfm_cpld_write_byte (dis->cpld_sel, dis->off, val);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "write_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to write CPLD");
        goto end;
    }

end:
    return rc;
}


// 1 means has loss, 0 means not has loss
static int
sfp_module_los_read_hc (
            uint32_t module, bool *los, void *st)
{
    // define
    int rc = 0;
    uint8_t val;
    struct st_ctx_t *rx_los = & ((struct sfp_ctx_st_t *)
                                  st)->rx_los;

    rc = bf_pltfm_cpld_read_byte (rx_los->cpld_sel,
                                        rx_los->off, &val);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "write_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, rx_los->off,
            "Failed to write CPLD");
        goto end;
    }

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;
end:
    return rc;
}

static struct opt_ctx_t sfp28_opt_hc36y24c = {
    .read = sfp_read_sub_module_hc,
    .write = sfp_write_sub_module_hc,
    .txdis = sfp_module_tx_disable_hc,
    .losrd = sfp_module_los_read_hc,
    .presrd = sfp_get_module_pres_hc,

    .lock = do_lock_hc,
    .unlock = do_unlock_hc,
    .select = sfp_select_hc,
    .unselect = sfp_unselect_hc,
    .sfp_ctx = &sfp28_ctx_hc36y24c,
    .xsfp_ctx = NULL, /* Not implemented yet. */
};

void bf_pltfm_sfp_init_hc36y24c (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num) {
    *opt = &sfp28_opt_hc36y24c;
    *num = ARRAY_LENGTH(sfp28_ctx_hc36y24c);
    *xsfp_num = 0;
}

