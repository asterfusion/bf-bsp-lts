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

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x308p[] = {
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (0)}, {BF_MAV_SYSCPLD1, 0x13, BIT (0)}, {BF_MAV_SYSCPLD1, 0x11, BIT (0)}},   // Y1
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (1)}, {BF_MAV_SYSCPLD1, 0x13, BIT (1)}, {BF_MAV_SYSCPLD1, 0x11, BIT (1)}},   // Y2
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (2)}, {BF_MAV_SYSCPLD1, 0x13, BIT (2)}, {BF_MAV_SYSCPLD1, 0x11, BIT (2)}},   // Y3
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (3)}, {BF_MAV_SYSCPLD1, 0x13, BIT (3)}, {BF_MAV_SYSCPLD1, 0x11, BIT (3)}},   // Y4
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (4)}, {BF_MAV_SYSCPLD1, 0x13, BIT (4)}, {BF_MAV_SYSCPLD1, 0x11, BIT (4)}},   // Y5
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (5)}, {BF_MAV_SYSCPLD1, 0x13, BIT (5)}, {BF_MAV_SYSCPLD1, 0x11, BIT (5)}},   // Y6
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (6)}, {BF_MAV_SYSCPLD1, 0x13, BIT (6)}, {BF_MAV_SYSCPLD1, 0x11, BIT (6)}},   // Y7
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (7)}, {BF_MAV_SYSCPLD1, 0x13, BIT (7)}, {BF_MAV_SYSCPLD1, 0x11, BIT (7)}},   // Y8

    {{BF_MAV_SYSCPLD1, 0x19, BIT (0)}, {BF_MAV_SYSCPLD1, 0x12, BIT (0)}, {BF_MAV_SYSCPLD1, 0x10, BIT (0)}},   // Y9
    {{BF_MAV_SYSCPLD1, 0x19, BIT (1)}, {BF_MAV_SYSCPLD1, 0x12, BIT (1)}, {BF_MAV_SYSCPLD1, 0x10, BIT (1)}},   // Y10
    {{BF_MAV_SYSCPLD1, 0x19, BIT (2)}, {BF_MAV_SYSCPLD1, 0x12, BIT (2)}, {BF_MAV_SYSCPLD1, 0x10, BIT (2)}},   // Y11
    {{BF_MAV_SYSCPLD1, 0x19, BIT (3)}, {BF_MAV_SYSCPLD1, 0x12, BIT (3)}, {BF_MAV_SYSCPLD1, 0x10, BIT (3)}},   // Y12
    {{BF_MAV_SYSCPLD1, 0x19, BIT (4)}, {BF_MAV_SYSCPLD1, 0x12, BIT (4)}, {BF_MAV_SYSCPLD1, 0x10, BIT (4)}},   // Y13
    {{BF_MAV_SYSCPLD1, 0x19, BIT (5)}, {BF_MAV_SYSCPLD1, 0x12, BIT (5)}, {BF_MAV_SYSCPLD1, 0x10, BIT (5)}},   // Y14
    {{BF_MAV_SYSCPLD1, 0x19, BIT (6)}, {BF_MAV_SYSCPLD1, 0x12, BIT (6)}, {BF_MAV_SYSCPLD1, 0x10, BIT (6)}},   // Y15
    {{BF_MAV_SYSCPLD1, 0x19, BIT (7)}, {BF_MAV_SYSCPLD1, 0x12, BIT (7)}, {BF_MAV_SYSCPLD1, 0x10, BIT (7)}},   // Y16

    {{BF_MAV_SYSCPLD1, 0x18, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (0)}},   // Y17
    {{BF_MAV_SYSCPLD1, 0x18, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (1)}},   // Y18
    {{BF_MAV_SYSCPLD1, 0x18, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (2)}},   // Y19
    {{BF_MAV_SYSCPLD1, 0x18, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (3)}},   // Y20
    {{BF_MAV_SYSCPLD1, 0x18, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (4)}},   // Y21
    {{BF_MAV_SYSCPLD1, 0x18, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (5)}},   // Y22
    {{BF_MAV_SYSCPLD1, 0x18, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (6)}},   // Y23
    {{BF_MAV_SYSCPLD1, 0x18, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (7)}},   // Y24

    {{BF_MAV_SYSCPLD1, 0x17, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (0)}},   // Y25
    {{BF_MAV_SYSCPLD1, 0x17, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (1)}},   // Y26
    {{BF_MAV_SYSCPLD1, 0x17, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (2)}},   // Y27
    {{BF_MAV_SYSCPLD1, 0x17, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (3)}},   // Y28
    {{BF_MAV_SYSCPLD1, 0x17, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (4)}},   // Y29
    {{BF_MAV_SYSCPLD1, 0x17, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (5)}},   // Y30
    {{BF_MAV_SYSCPLD1, 0x17, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (6)}},   // Y31
    {{BF_MAV_SYSCPLD1, 0x17, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (7)}},   // Y32

    {{BF_MAV_SYSCPLD1, 0x16, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (0)}, {BF_MAV_SYSCPLD1, 0x09, BIT (0)}},   // Y33
    {{BF_MAV_SYSCPLD1, 0x16, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (1)}, {BF_MAV_SYSCPLD1, 0x09, BIT (1)}},   // Y34
    {{BF_MAV_SYSCPLD1, 0x16, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (2)}, {BF_MAV_SYSCPLD1, 0x09, BIT (2)}},   // Y35
    {{BF_MAV_SYSCPLD1, 0x16, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (3)}, {BF_MAV_SYSCPLD1, 0x09, BIT (3)}},   // Y36
    {{BF_MAV_SYSCPLD1, 0x16, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (4)}, {BF_MAV_SYSCPLD1, 0x09, BIT (4)}},   // Y37
    {{BF_MAV_SYSCPLD1, 0x16, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (5)}, {BF_MAV_SYSCPLD1, 0x09, BIT (5)}},   // Y38
    {{BF_MAV_SYSCPLD1, 0x16, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (6)}, {BF_MAV_SYSCPLD1, 0x09, BIT (6)}},   // Y39
    {{BF_MAV_SYSCPLD1, 0x16, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (7)}, {BF_MAV_SYSCPLD1, 0x09, BIT (7)}},   // Y40

    {{BF_MAV_SYSCPLD1, 0x15, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (0)}, {BF_MAV_SYSCPLD1, 0x08, BIT (0)}},   // Y41
    {{BF_MAV_SYSCPLD1, 0x15, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (1)}, {BF_MAV_SYSCPLD1, 0x08, BIT (1)}},   // Y42
    {{BF_MAV_SYSCPLD1, 0x15, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (2)}, {BF_MAV_SYSCPLD1, 0x08, BIT (2)}},   // Y43
    {{BF_MAV_SYSCPLD1, 0x15, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (3)}, {BF_MAV_SYSCPLD1, 0x08, BIT (3)}},   // Y44
    {{BF_MAV_SYSCPLD1, 0x15, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (4)}, {BF_MAV_SYSCPLD1, 0x08, BIT (4)}},   // Y45
    {{BF_MAV_SYSCPLD1, 0x15, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (5)}, {BF_MAV_SYSCPLD1, 0x08, BIT (5)}},   // Y46
    {{BF_MAV_SYSCPLD1, 0x15, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (6)}, {BF_MAV_SYSCPLD1, 0x08, BIT (6)}},   // Y47
    {{BF_MAV_SYSCPLD1, 0x15, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (7)}, {BF_MAV_SYSCPLD1, 0x08, BIT (7)}},   // Y48
};


/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x308p[] = {
    {{"Y1",   9, 0, 0x70, 0}, (void *) &sfp_st_x308p[0]},
    {{"Y2",   9, 1, 0x70, 1}, (void *) &sfp_st_x308p[1]},
    {{"Y3",   9, 2, 0x70, 2}, (void *) &sfp_st_x308p[2]},
    {{"Y4",   9, 3, 0x70, 3}, (void *) &sfp_st_x308p[3]},
    {{"Y5",  10, 0, 0x70, 4}, (void *) &sfp_st_x308p[4]},
    {{"Y6",  10, 1, 0x70, 5}, (void *) &sfp_st_x308p[5]},
    {{"Y7",  10, 2, 0x70, 6}, (void *) &sfp_st_x308p[6]},
    {{"Y8",  10, 3, 0x70, 7}, (void *) &sfp_st_x308p[7]},

    {{"Y9",  11, 0, 0x71, 0}, (void *) &sfp_st_x308p[8]},
    {{"Y10", 11, 1, 0x71, 1}, (void *) &sfp_st_x308p[9]},
    {{"Y11", 11, 2, 0x71, 2}, (void *) &sfp_st_x308p[10]},
    {{"Y12", 11, 3, 0x71, 3}, (void *) &sfp_st_x308p[11]},
    {{"Y13", 12, 0, 0x71, 4}, (void *) &sfp_st_x308p[12]},
    {{"Y14", 12, 1, 0x71, 5}, (void *) &sfp_st_x308p[13]},
    {{"Y15", 12, 2, 0x71, 6}, (void *) &sfp_st_x308p[14]},
    {{"Y16", 12, 3, 0x71, 7}, (void *) &sfp_st_x308p[15]},

    {{"Y17", 13, 0, 0x72, 0}, (void *) &sfp_st_x308p[16]},
    {{"Y18", 13, 1, 0x72, 1}, (void *) &sfp_st_x308p[17]},
    {{"Y19", 13, 2, 0x72, 2}, (void *) &sfp_st_x308p[18]},
    {{"Y20", 13, 3, 0x72, 3}, (void *) &sfp_st_x308p[19]},
    {{"Y21", 14, 0, 0x72, 4}, (void *) &sfp_st_x308p[20]},
    {{"Y22", 14, 1, 0x72, 5}, (void *) &sfp_st_x308p[21]},
    {{"Y23", 14, 2, 0x72, 6}, (void *) &sfp_st_x308p[22]},
    {{"Y24", 14, 3, 0x72, 7}, (void *) &sfp_st_x308p[23]},

    {{"Y25", 15, 0, 0x73, 0}, (void *) &sfp_st_x308p[24]},
    {{"Y26", 15, 1, 0x73, 1}, (void *) &sfp_st_x308p[25]},
    {{"Y27", 15, 2, 0x73, 2}, (void *) &sfp_st_x308p[26]},
    {{"Y28", 15, 3, 0x73, 3}, (void *) &sfp_st_x308p[27]},
    {{"Y29", 16, 0, 0x73, 4}, (void *) &sfp_st_x308p[28]},
    {{"Y30", 16, 1, 0x73, 5}, (void *) &sfp_st_x308p[29]},
    {{"Y31", 16, 2, 0x73, 6}, (void *) &sfp_st_x308p[30]},
    {{"Y32", 16, 3, 0x73, 7}, (void *) &sfp_st_x308p[31]},

    {{"Y33", 17, 0, 0x74, 0}, (void *) &sfp_st_x308p[32]},
    {{"Y34", 17, 1, 0x74, 1}, (void *) &sfp_st_x308p[33]},
    {{"Y35", 17, 2, 0x74, 2}, (void *) &sfp_st_x308p[34]},
    {{"Y36", 17, 3, 0x74, 3}, (void *) &sfp_st_x308p[35]},
    {{"Y37", 18, 0, 0x74, 4}, (void *) &sfp_st_x308p[36]},
    {{"Y38", 18, 1, 0x74, 5}, (void *) &sfp_st_x308p[37]},
    {{"Y39", 18, 2, 0x74, 6}, (void *) &sfp_st_x308p[38]},
    {{"Y40", 18, 3, 0x74, 7}, (void *) &sfp_st_x308p[39]},

    {{"Y41", 19, 0, 0x75, 0}, (void *) &sfp_st_x308p[40]},
    {{"Y42", 19, 1, 0x75, 1}, (void *) &sfp_st_x308p[41]},
    {{"Y43", 19, 2, 0x75, 2}, (void *) &sfp_st_x308p[42]},
    {{"Y44", 19, 3, 0x75, 3}, (void *) &sfp_st_x308p[43]},
    {{"Y45", 20, 0, 0x75, 4}, (void *) &sfp_st_x308p[44]},
    {{"Y46", 20, 1, 0x75, 5}, (void *) &sfp_st_x308p[45]},
    {{"Y47", 20, 2, 0x75, 6}, (void *) &sfp_st_x308p[46]},
    {{"Y48", 20, 3, 0x75, 7}, (void *) &sfp_st_x308p[47]},
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x308p[] = {
    {{"X1", 33, 2, 0, 0}, NULL},    /* X86 KR, keep the order same with X5-T.   */
    {{"X2", 33, 3, 0, 0}, NULL},    /* X86 KR, keep the order same with X5-T.   */
    {{"X3", 33, 0, 0, 0}, NULL},    /* Not used unless current hw supports PTP. */
    {{"X4", 33, 1, 0, 0}, NULL},    /* Not used unless current hw supports PTP. */
};

static void do_lock_x308p()
{
    /* CP2112 is used for x308p to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_LOCK;
}

static void do_unlock_x308p()
{
    /* CP2112 is used for x308p to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_UNLOCK;
}

static int sfp_select_x308p (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -1;
            break;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -2;
            break;
        }

        // select PCA9548
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        sfp->info.i2c_chnl_addr << 1,  1 << sfp->info.rev,
                                        DEFAULT_TIMEOUT_MS * 2) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int sfp_unselect_x308p (uint32_t module)
{
    int rc = -1, last_wr_rc = 0, last_rd_rc = 0;
    int retry_times = 0;
    uint8_t pca9548_value = 0xFF;
    struct sfp_ctx_t *sfp;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -1;
            break;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -2;
            break;
        }

        rc = -3;
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if ((rc = bf_pltfm_cp2112_write_byte (hndl,
                                                  sfp->info.i2c_chnl_addr << 1,  0,
                                                  DEFAULT_TIMEOUT_MS * 2)) != BF_PLTFM_SUCCESS) {
                last_wr_rc = (rc - 1000);
            }

            /* Please refer to PCA9548 Spec to set a valid delay between the op of WR and RD.
             * Here we give an empirical value. */
            bf_sys_usleep (5000);

            // readback PCA9548 to ensure PCA9548 is closed
            if ((rc = bf_pltfm_cp2112_read (hndl,
                                            sfp->info.i2c_chnl_addr << 1,  &pca9548_value, 1,
                                            DEFAULT_TIMEOUT_MS)) != BF_PLTFM_SUCCESS) {
                last_rd_rc = (rc - 2000);
            }

            if (pca9548_value == 0) {
                rc = 0;
                break;
            }

            bf_sys_usleep (5000);
        }
    } while (0);

    /* Anyway, an error occured during PCA9548 selection. */
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "i2c_write(%02d : %s) "
            "last_rc = %d : retry_times = %d"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to write PCA9548",
            last_wr_rc ? last_wr_rc : (last_rd_rc ? last_rd_rc : -1000),
            retry_times);
    }

    /* Avoid compile error. */
    (void) (module);
    return rc;
}

static int
sfp_read_sub_module_x308p (
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

static int
sfp_write_sub_module_x308p (
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
        // write module, CP2112 write bytes should not longer than 61, but we limit it to 60 because we use one byte to mark offset
        uint8_t tmp[61];
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
sfp_get_module_pres_x308p (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint32_t sfp_01_32 = 0xFFFFFFFF;
    uint32_t sfp_33_48 = 0xFFFFFFFF;

    uint8_t sfp_01_08 = 0xFF;
    uint8_t sfp_09_16 = 0xFF;
    uint8_t sfp_17_24 = 0xFF;
    uint8_t sfp_25_32 = 0xFF;
    uint8_t sfp_33_40 = 0xFF;
    uint8_t sfp_41_48 = 0xFF;

    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 19, &sfp_01_08);
    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 18, &sfp_09_16);
    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 15, &sfp_17_24);
    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 14, &sfp_25_32);
    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 11, &sfp_33_40);
    rc |= bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 10, &sfp_41_48);

    if (rc == 0) {
        sfp_01_32 = sfp_01_08 + (sfp_09_16 << 8) + (sfp_17_24 << 16) + (sfp_25_32 << 24);
        sfp_33_48 = sfp_33_40 + (sfp_41_48 << 8) + 0xFFFF0000;
        *pres_l = sfp_01_32;
        *pres_h = sfp_33_48;
    }

    return rc;
}

static int
sfp_module_tx_disable_x308p (
    uint32_t module, bool disable, void *st)
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    /* Convert st to your own st. */
    struct st_ctx_t *dis = & ((struct sfp_ctx_st_t *)
                              st)->tx_dis;

    /* read current value from offset of Tx disable control register  */
    rc = bf_pltfm_cpld_read_byte (
                dis->cpld_sel, dis->off, &val0);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    val = val0;
    /* dis->off_b is a bit mask in dis->off. */
    if (disable) {
        /* disable Tx control bit is active high */
        val |= (1 << dis->off_b);
    } else {
        /* enable Tx control bit is active low */
        val &= ~ (1 << dis->off_b);
    }

    /* should we still write even though the value is the same in cpld ? */
    /* TODO */

    /* write a new value back to update Tx disable control register. */
    rc = bf_pltfm_cpld_write_byte (
                dis->cpld_sel, dis->off, val);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to write CPLD");
        goto end;
    }

    /* read current value from offset of Tx disable control register  */
    rc = bf_pltfm_cpld_read_byte (
                dis->cpld_sel, dis->off, &val1);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    LOG_DEBUG (
             " SFP    %2d : CH : 0x%02x  V : (0x%02x -> 0x%02x) "
             " %sTx",
             module,
             dis->cpld_sel, val0,
             val1, disable ? "-" : "+");

end:
    return rc;
}

static int
sfp_module_los_read_x308p (uint32_t
                        module, bool *los, void *st)
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
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, rx_los->off,
            "Failed to read CPLD");
        goto end;
    }

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;

end:
    return rc;
}

static struct opt_ctx_t sfp28_opt_x308p = {
    .read = sfp_read_sub_module_x308p,
    .write = sfp_write_sub_module_x308p,
    .txdis = sfp_module_tx_disable_x308p,
    .losrd = sfp_module_los_read_x308p,
    .presrd = sfp_get_module_pres_x308p,

    .lock = do_lock_x308p,
    .unlock = do_unlock_x308p,
    .select = sfp_select_x308p,
    .unselect = sfp_unselect_x308p,
    .sfp_ctx = &sfp28_ctx_x308p,
    .xsfp_ctx = &xsfp_ctx_x308p,
};

void bf_pltfm_sfp_init_x308p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num) {
    uint32_t n = 2;
    if (platform_subtype_equal (v3dot0))
        n = ARRAY_LENGTH(xsfp_ctx_x308p);
    *opt = &sfp28_opt_x308p;
    *num = ARRAY_LENGTH(sfp28_ctx_x308p);
    *xsfp_num = n;
}

