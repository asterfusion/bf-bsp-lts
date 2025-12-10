/*!
 * @file bf_pltfm_sfp_module.c
 * @date 2023/08/21
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

extern bool sfp_lookup1 (IN uint32_t
                         module, OUT struct sfp_ctx_t **sfp);

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x732q[] = {
    {{BF_MAV_SYSCPLD1, 0x11, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0c, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0c, BIT (3)}},
    {{BF_MAV_SYSCPLD1, 0x11, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0c, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0c, BIT (2)}}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x732q[] = {
    {{"Y1", 33, 0, 0x04, INVALID}, (void *) &sfp_st_x732q[0]},
    {{"Y2", 33, 1, 0x02, INVALID}, (void *) &sfp_st_x732q[1]}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x732q[] = {
    {{"X1", 33, 2, 0, 0}, NULL},
    {{"X2", 33, 3, 0, 0}, NULL}
};

static void do_lock()
{
    MAV_SFP_LOCK;
}

static void do_unlock()
{
    MAV_SFP_UNLOCK;
}

static int
sfp_select_x7 (uint32_t module)
{
    int rc = -1;
    struct sfp_ctx_t *sfp = NULL;

    if (sfp_lookup1 (module, &sfp)) {
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
            rc = bf_pltfm_cp2112_reg_write_byte (
                    0x74, 0x00, sfp->info.i2c_chnl_addr);
        } else {
            rc = bf_pltfm_master_i2c_write_byte (
                    0x74, 0x00, sfp->info.i2c_chnl_addr);
        }

        if (rc) {
            LOG_ERROR (
                "%s[%d], "
                "i2c_write(%02d : %s)"
                "\n",
                __FILE__, __LINE__, module,
                "Failed to write PCA9548");
        }
    }

    /* Avoid compile error. */
    (void)sfp;
    return rc;
}

static int
sfp_unselect_x7 (uint32_t module)
{
    int rc = -1, last_wr_rc = 0, last_rd_rc = 0;
    int retry_times = 0;
    uint8_t pca9548_value = 0xFF;

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if ((rc = bf_pltfm_cp2112_reg_write_byte (
                          0x74, 0x00, 0x00)) != BF_PLTFM_SUCCESS) {
                last_wr_rc = (rc - 1000);
            }

            bf_sys_usleep (5000);

            // readback PCA9548 to ensure PCA9548 is closed
            if ((rc = bf_pltfm_cp2112_reg_read_block (
                          0x74, 0x00, &pca9548_value, 0x01)) != BF_PLTFM_SUCCESS) {
                last_rd_rc = (rc - 2000);
            }

            if (pca9548_value == 0) {
                rc = 0;
                break;
            }

            bf_sys_usleep (5000);
        }
    } else {
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if ((rc = bf_pltfm_master_i2c_write_byte (
                          0x74, 0x00, 0x00)) != BF_PLTFM_SUCCESS) {
                last_wr_rc = (rc - 1000);
            }

            bf_sys_usleep (5000);

            // readback PCA9548 to ensure PCA9548 is closed
            if ((rc = bf_pltfm_master_i2c_read_byte (
                          0x74, 0x00, &pca9548_value)) != BF_PLTFM_SUCCESS) {
                last_rd_rc = (rc - 2000);
            }

            if (pca9548_value == 0) {
                rc = 0;
                break;
            }

            bf_sys_usleep (5000);
        }
    }

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

/* An example, not finished yet.
 * Please help me fix it.
 * by tsihang, 2021-07-18. */
static int
sfp_read_sub_module_x7 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = 0;
    uint8_t addr;

    // judge addr
    if (offset < MAX_SFP_PAGE_SIZE) {
        addr = 0xa0;
    } else {
        addr = 0xa2;
        offset -= MAX_SFP_PAGE_SIZE;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        err = bf_pltfm_cp2112_reg_read_block (
                    addr >> 1, offset, buf, len);
    } else {
        err = bf_pltfm_master_i2c_read_block (
                    addr >> 1, offset, buf, len);
    }

    return err;
}

/* An example, not finished yet.
 * Please help me fix it.
 * by tsihang, 2021-07-18. */
static int
sfp_write_sub_module_x7 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = -1;
    uint8_t addr;

    // judge addr
    if (offset < MAX_SFP_PAGE_SIZE) {
        addr = 0xa0;
    } else {
        addr = 0xa2;
        offset -= MAX_SFP_PAGE_SIZE;
    }

    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        err = bf_pltfm_cp2112_reg_write_block (
                    addr >> 1, offset, buf, len);
    } else {
        err = bf_pltfm_master_i2c_write_block (
                    addr >> 1, offset, buf, len);
    }

    return err;
}

static int
sfp_get_module_pres_x7 (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    uint8_t val;
    uint32_t sfp_pres_mask = 0xFFFFFFFF;

    /* MISC */
    rc = bf_pltfm_cpld_read_byte (
              BF_MAV_SYSCPLD1, 0x0C, &val);
    if (rc) {
        goto end;
    }
    /* For X732Q-T sfp pres,
       bit 0 stands for Y1 and bit 1 stands for Y2
       by SunZheng, 2024-01-24.*/
    val = ((val << 1) & 2) | ((val >> 1) & 1);
    MASKBIT (val, 2);
    MASKBIT (val, 3);
    MASKBIT (val, 4);
    MASKBIT (val, 5);
    MASKBIT (val, 6);
    MASKBIT (val, 7);

    sfp_pres_mask = ((0xFFFFFF << 8) | val);
    *pres_l = sfp_pres_mask;
    *pres_h = *pres_h;

end:
    return rc;
}

static int
sfp_module_tx_disable_x7 (
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
            "write_cpld(%02d : %d : %s)"
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
sfp_module_los_read_x7 (uint32_t
                        module, bool *los, void *st)
{
    // define
    int rc = 0;
    uint8_t val;
    struct st_ctx_t *rx_los = & ((struct sfp_ctx_st_t *)
                                  st)->rx_los;

    /* read current value from offset of LOS status register  */
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
    val = ((val << 1) & 8) | ((val >> 1) & 4);

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;

end:
    return rc;
}

static struct opt_ctx_t sfp28_opt_x732q = {
    .read = sfp_read_sub_module_x7,
    .write = sfp_write_sub_module_x7,
    .txdis = sfp_module_tx_disable_x7,
    .losrd = sfp_module_los_read_x7,
    .presrd = sfp_get_module_pres_x7,

    .lock = do_lock,
    .unlock = do_unlock,
    .select = sfp_select_x7,
    .unselect = sfp_unselect_x7,
    .sfp_ctx = &sfp28_ctx_x732q,
    .xsfp_ctx = &xsfp_ctx_x732q,
};

void bf_pltfm_sfp_init_x732q (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num) {
    *opt = &sfp28_opt_x732q;
    *num = ARRAY_LENGTH(sfp28_ctx_x732q);
    *xsfp_num = ARRAY_LENGTH(xsfp_ctx_x732q);
}
