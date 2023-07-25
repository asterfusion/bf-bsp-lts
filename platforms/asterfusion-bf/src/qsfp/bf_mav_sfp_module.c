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

#define MAX_MAV_SFP_I2C_LEN 16

#define ADDR_SFP_I2C         (0x50 << 1)
/* sfp module lock macros */
static bf_sys_mutex_t sfp_page_mutex;
#define MAV_SFP_PAGE_LOCK_INIT bf_sys_mutex_init(&sfp_page_mutex)
#define MAV_SFP_PAGE_LOCK_DEL bf_sys_mutex_del(&sfp_page_mutex)
#define MAV_SFP_PAGE_LOCK bf_sys_mutex_lock(&sfp_page_mutex)
#define MAV_SFP_PAGE_UNLOCK bf_sys_mutex_unlock(&sfp_page_mutex)

/* access is 1 based index, so zeroth index would be unused */
static int
connID_chnlID_to_sfp_ctx_index[BF_PLAT_MAX_QSFP +
                               1][MAX_CHAN_PER_CONNECTOR]
= {{INVALID}};

static struct opt_ctx_t *g_sfp_opt;
static struct sfp_ctx_t *g_xsfp_ctx;
static uint32_t g_xsfp_num;

void bf_pltfm_sfp_init_x532p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num);
void bf_pltfm_sfp_init_x564p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num);
void bf_pltfm_sfp_init_x308p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num);
void bf_pltfm_sfp_init_x312p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num);
void bf_pltfm_sfp_init_hc36y24c (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num);

static void hex_dump (uint8_t *buf, uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            fprintf (stdout, "\n%3d : ", byte);
        }
        fprintf (stdout, "%02x ", buf[byte]);
    }
    fprintf (stdout,  "\n");
}

static inline void sfp_fetch_ctx (void **sfp_ctx)
{
    *sfp_ctx = g_sfp_opt->sfp_ctx;
}

/* search in map to rapid lookup speed. */
static uint32_t sfp_lookup_fast (
    IN uint32_t conn, IN uint32_t chnl)
{
    return (connID_chnlID_to_sfp_ctx_index[conn %
                                           (BF_PLAT_MAX_QSFP + 1)][chnl %
                                                   MAX_CHAN_PER_CONNECTOR]
            != INVALID);
}

EXPORT bool sfp_lookup1 (IN uint32_t
                         module, OUT struct sfp_ctx_t **sfp)
{
    struct sfp_ctx_t *sfp_ctx = NULL;

    sfp_fetch_ctx ((void **)&sfp_ctx);

    if (sfp_ctx &&
        ((int)module <= bf_sfp_get_max_sfp_ports())) {
        *sfp = &sfp_ctx[module - 1];
        return true;
    }

    LOG_ERROR (
        "%s[%d], "
        "sfp.lookup(%02d : %s)"
        "\n",
        __FILE__, __LINE__, module, "Failed to find SFP");

    return false;
}


/* Not that fast, but well-used, it is unused even though exported. */
EXPORT bool sfp_exist (IN uint32_t conn,
                       IN uint32_t chnl)
{
    struct sfp_ctx_t *sfp, *sfp_ctx = NULL;

    sfp_fetch_ctx ((void **)&sfp_ctx);

    if (sfp_ctx) {
        foreach_element (0, bf_sfp_get_max_sfp_ports()) {
            sfp = &sfp_ctx[each_element];
            if (sfp->info.conn == conn &&
                sfp->info.chnl == chnl) {
                return true;
            }
        }
    }

    return false;
}

EXPORT bool is_panel_sfp (uint32_t conn,
                          uint32_t chnl)
{
    return sfp_lookup_fast (conn, chnl);
}

/** set sfp lpmode thru syscpld register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param lpmode
*   true : set lpmode, false : set no-lpmode
*  @return
*   0: success, -1: failure
*/
EXPORT int bf_pltfm_sfp_set_lpmode (
    IN uint32_t module,
    IN bool lpmode)
{
    module = module;
    lpmode = lpmode;
    return 0;
}

/** reset sfp module thru syscpld register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param reset
*   true : reset the transceiver, false : de-reset the transceiver
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_module_reset (
    IN uint32_t module,
    IN bool reset)
{
    module = module;
    reset = reset;
    return 0;
}

/** enable/disable sfp module Tx thru syscpld register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param disable
*   true : disable the Tx of the transceiver, false : enable the Tx of the transceiver.
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_module_tx_disable (
    IN uint32_t module,
    IN bool disable)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    if (!sfp_lookup1 (module, &sfp)) {
        return -1;
    }

    g_sfp_opt->lock();
    rc = g_sfp_opt->txdis (module, disable, sfp->st);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.txdis(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to disable Tx", rc);
    }
    g_sfp_opt->unlock();

    return rc;
}

/** get los of sfp module thru syscpld register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param los
*   true : los detected, false : los not detected
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_los_read (
    IN uint32_t module,
    OUT bool *los)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    if (!sfp_lookup1 (module, &sfp)) {
        return -1;
    }

    g_sfp_opt->lock();
    rc = g_sfp_opt->losrd (module, los, sfp->st);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.losrd(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to read LOS", rc);
    }
    g_sfp_opt->unlock();

    return rc;
}

/** get sfp module presmask thru syscpld register
*
*  @param *port_1_32_pres
*   presmask of lower 32 ports (1-32).
*  @param port_32_64_pres
*   presmask of higher 32 ports (32-64).
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_get_presence_mask (
    uint32_t *port_1_32_pres,
    uint32_t *port_32_64_pres)
{
    int rc = 0;

    g_sfp_opt->lock();
    /*
    * It's quite a heavy cost to read present bit which specified by st_ctx_t,
    * because of that a byte read out from CPLD once may show we out more than
    * one module. Due to the reason above, I prefer keeping access method below
    * as API call than do it under a function pointer like module IO API.
    *
    * by tsihang, 2021-07-16.
    */
    rc = g_sfp_opt->presrd (port_1_32_pres, port_32_64_pres);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.presrd(%s : %d)"
            "\n",
            __FILE__, __LINE__,
            "Failed to get present mask", rc);
    }
    g_sfp_opt->unlock();

    return rc;
}

/** get sfp lpmode status thru syscpld register
 *
 *  @param port_1_32_lpmode
 *   lpmode of lower 32 ports (1-32) 0: no-lpmod 1: lpmode
 *  @param port_1_32_lpmode
 *   lpmode of upper 32 ports (33-64) 0: no-lpmod 1: lpmode
 *  @return
 *   0 on success and -1 in error
 */
EXPORT int bf_pltfm_sfp_get_lpmode_mask (
    uint32_t *port_1_32_lpm,
    uint32_t *port_32_64_lpm)
{
    port_1_32_lpm = port_1_32_lpm;
    port_32_64_lpm = port_32_64_lpm;
    return 0;
}

/** get sfp interrupt status thru syscpld register
 *
 *  @param port_1_32_ints
 *   interrupt from lower 32 ports (1-32) 0: int-present, 1:not-present
 *  @param port_32_64_ints
 *   interrupt from upper 32 ports (33-64) 0: int-present, 1:not-present
 */
EXPORT void bf_pltfm_sfp_get_int_mask (
    uint32_t *port_1_32_lpm,
    uint32_t *port_32_64_lpm)
{
    port_1_32_lpm = port_1_32_lpm;
    port_32_64_lpm = port_32_64_lpm;
}

/** Convert a transceiver index to SDE index (connid/chnlid)
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param *conn
*   conn_id of a port in SDE
*  @param *chnl
*   chnl_id of a port in SDE
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_lookup_by_module (
    IN uint32_t module,
    OUT uint32_t *conn, OUT uint32_t *chnl)
{
    struct sfp_ctx_t *sfp;

    if (!sfp_lookup1 (module, &sfp)) {
        return -1;
    }

    *conn = sfp->info.conn;
    *chnl = sfp->info.chnl;

    return 0;
}

/** Convert SDE index (connid/chnlid) to transceiver index
*
*  @param conn
*   conn_id of a port in SDE
*  @param chnl
*   chnl_id of a port in SDE
*  @param module
*   index of sfp transceiver (1 based)
*  @return
*   0 on success and -1 in error
*/
EXPORT int bf_pltfm_sfp_lookup_by_conn (
    IN uint32_t conn,
    IN uint32_t chnl, OUT uint32_t *module)
{
    struct sfp_ctx_t *sfp;
    struct sfp_ctx_t *sfp_ctx = NULL;

    sfp_fetch_ctx ((void **)&sfp_ctx);

    if (sfp_ctx) {
        foreach_element (0, bf_sfp_get_max_sfp_ports()) {
            sfp = &sfp_ctx[each_element];
            if (sfp->info.conn == conn &&
                sfp->info.chnl == chnl) {
                /* SFP module is 1 based. */
                *module = (each_element + 1);
                return 0;
            }
        }
    }

    return -1;
}

/** read sfp transceiver eeprom
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param offset
*   offset to eeprom
*  @param len
*   number of bytes read from the offset of eeprom
*  @param buf
*   a buffer where the data store to.
*  @return
*   0: success, -1: failure
*   To avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
*/
EXPORT int bf_pltfm_sfp_read_module (
    IN uint32_t module,
    IN uint8_t offset,
    IN uint8_t len,
    OUT uint8_t *buf)
{
    int err = 0;

    MAV_SFP_PAGE_LOCK;
    g_sfp_opt->lock();

    err = g_sfp_opt->select (module);
    if (err) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.select(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to select SFP", err);
        goto end;
    }

    err = g_sfp_opt->read (module, offset, len, buf);
    if (err) {
        /* Must de-select even error occured while reading. */
        LOG_ERROR (
            "%s[%d], "
            "sfp.read(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to read SFP", err);
    }

    err = g_sfp_opt->unselect (module);
    if (err) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.unselect(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to dis-select SFP", err);
        goto end;
    }

    if (0 && !err) {
        fprintf (stdout, "\n%02x:\n", module);
        hex_dump (buf, len);
    }

end:
    g_sfp_opt->unlock();
    MAV_SFP_PAGE_UNLOCK;

    return err;
}

/** write sfp transceiver eeprom
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param offset
*   offset to eeprom
*  @param len
*   number of bytes write to the offset of eeprom
*  @param buf
*   a buffer where the data comes from.
*  @return
*   0: success, -1: failure
*   To avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
*/
EXPORT int bf_pltfm_sfp_write_module (
    IN uint32_t module,
    IN uint8_t offset,
    IN uint8_t len,
    IN uint8_t *buf)
{
    int err = 0;

    /* Do not remove this lock. */
    MAV_SFP_PAGE_LOCK;
    g_sfp_opt->lock();

    err = g_sfp_opt->select (module);
    if (err) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.select(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to select SFP", err);
        goto end;
    }

    err = g_sfp_opt->write (module, offset, len, buf);
    if (err) {
        /* Must de-select even error occured while writing. */
        LOG_ERROR (
            "%s[%d], "
            "sfp.write(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to write SFP", err);
    }

    err = g_sfp_opt->unselect (module);
    if (err) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.unselect(%02d : %s : %d)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to dis-select SFP", err);
        goto end;
    }

    if (0 && !err) {
        fprintf (stdout, "\n%02x:\n", module);
        hex_dump (buf, len);
    }

end:
    g_sfp_opt->unlock();
    MAV_SFP_PAGE_UNLOCK;
    return err;
}

/** read sfp transceiver's register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param page
*   page index
*  @param offset
*   number of bytes write to the offset of eeprom
*  @param val
*   data from the register.
*  @return
*   0: success, -1: failure
*   To avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
*/
EXPORT int bf_pltfm_sfp_read_reg (
    IN uint32_t module,
    IN uint8_t page,
    IN int offset,
    OUT uint8_t *val)
{
    int err = 0;

    /* safety check */
    if (!val) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.read_reg(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Invalid param");
        return -1;
    }

    if (offset >= (2 * MAX_SFP_PAGE_SIZE)) {
        return -3;
    }

    MAV_SFP_PAGE_LOCK;
    /* set the page if no flat memory and offset > MAX_SFP_PAGE_SIZE */
    if (bf_pltfm_sfp_write_module (module,
                                   127, 1, &page)) {
        goto end;
    }

    bf_sys_usleep (50000);

    /* Read one byte from the register which pointed by offset. */
    if (bf_pltfm_sfp_read_module (module,
                                  offset, 1, val)) {
        goto end;
    }

    /* reset the page if paged */
    page = 0;
    if (bf_pltfm_sfp_write_module (module,
                                   127, 1, &page)) {
        goto end;
    }

end:
    MAV_SFP_PAGE_UNLOCK;
    return err;
}

/** write sfp transceiver's register
*
*  @param module
*   index of sfp transceiver (1 based)
*  @param page
*   page index
*  @param offset
*   number of bytes write to the offset of eeprom
*  @param val
*   data to be written to the register.
*  @return
*   0: success, -1: failure
*   To avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
*/
EXPORT int bf_pltfm_sfp_write_reg (
    IN uint32_t module,
    IN uint8_t page,
    IN int offset,
    IN uint8_t val)
{
    int err = 0;

    /* safety check */
    if (!val) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.write_reg(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Invalid param");
        return -1;
    }

    if (offset >= (2 * MAX_SFP_PAGE_SIZE)) {
        return -3;
    }

    MAV_SFP_PAGE_LOCK;
    /* set the page */
    if (bf_pltfm_sfp_write_module (module,
                                   MAX_SFP_PAGE_SIZE + 127, 1, &page)) {
        goto end;
    }

    bf_sys_usleep (50000);

    /* Write one byte to the register which pointed by offset. */
    if (bf_pltfm_sfp_write_module (module,
                                   offset, 1, &val)) {
        goto end;
    }

    /* reset the page */
    page = 0;
    if (bf_pltfm_sfp_write_module (module,
                                   MAX_SFP_PAGE_SIZE + 127, 1, &page)) {
        goto end;
    }

end:
    MAV_SFP_PAGE_UNLOCK;
    return err;
}

/** detect a sfp module by attempting to read its memory offset zero
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if present and false if not present
 */
EXPORT bool bf_pltfm_detect_sfp (uint32_t module)
{
    int rc;
    uint8_t val;

    rc = bf_pltfm_sfp_read_module (module, 0, 1,
                                   &val);
    if (rc != BF_PLTFM_SUCCESS) {
        return false;
    } else {
        return true;
    }
}

EXPORT int bf_pltfm_get_sfp_ctx (struct sfp_ctx_t
                                 **sfp_ctx)
{
    sfp_fetch_ctx ((void **)sfp_ctx);
    return 0;
}

EXPORT int bf_pltfm_get_xsfp_ctx (struct sfp_ctx_t
                              **sfp_ctx)
{
    if (!g_xsfp_ctx)
        return -1;

    *sfp_ctx = g_xsfp_ctx;
    return 0;
}

int bf_pltfm_sfp_init (void *arg)
{
    struct sfp_ctx_t *sfp;
    (void)arg;
    uint32_t sfp28_num = 0, xsfp_num = 0;

    fprintf (stdout,
             "\n\n================== SFPs INIT ==================\n");

    if (platform_type_equal (X532P)) {
        bf_pltfm_sfp_init_x532p ((struct opt_ctx_t **)&g_sfp_opt, &sfp28_num, &xsfp_num);
    } else if (platform_type_equal (X564P)) {
        bf_pltfm_sfp_init_x564p ((struct opt_ctx_t **)&g_sfp_opt, &sfp28_num, &xsfp_num);
    } else if (platform_type_equal (X308P)) {
        bf_pltfm_sfp_init_x308p ((struct opt_ctx_t **)&g_sfp_opt, &sfp28_num, &xsfp_num);
    } else if (platform_type_equal (X312P)) {
        bf_pltfm_sfp_init_x312p ((struct opt_ctx_t **)&g_sfp_opt, &sfp28_num, &xsfp_num);
    } else if (platform_type_equal (HC)) {
        bf_pltfm_sfp_init_hc36y24c ((struct opt_ctx_t **)&g_sfp_opt, &sfp28_num, &xsfp_num);
    }
    BUG_ON (g_sfp_opt == NULL);
    g_xsfp_ctx = g_sfp_opt->xsfp_ctx;
    g_xsfp_num = xsfp_num;
    bf_sfp_set_num (sfp28_num);

    fprintf (stdout, "num of SFPs : %d : num of xSFPs : %d\n",
             bf_sfp_get_max_sfp_ports(), g_xsfp_num);

    /* init SFP map */
    uint32_t module;
    foreach_element (0, bf_sfp_get_max_sfp_ports()) {
        struct sfp_ctx_t *sfp_ctx = (struct sfp_ctx_t *)
                                    g_sfp_opt->sfp_ctx;
        sfp = &sfp_ctx[each_element];
        /* SFP module is 1 based. */
        connID_chnlID_to_sfp_ctx_index[sfp->info.conn][sfp->info.chnl]
            = (each_element + 1);
    }

    foreach_element (0, bf_sfp_get_max_sfp_ports()) {
        module = (each_element + 1);
        bf_pltfm_sfp_module_tx_disable (module, false);
    }

    /* Dump the map of SFP <-> QSFP/CH. */
    foreach_element (0, bf_sfp_get_max_sfp_ports()) {
        struct sfp_ctx_t *sfp_ctx = (struct sfp_ctx_t *)
                                    g_sfp_opt->sfp_ctx;
        module = (each_element + 1);
        sfp = &sfp_ctx[each_element];
        LOG_DEBUG (
                 "Y%02d   <->   %02d/%d",
                 module, sfp->info.conn, sfp->info.chnl);
    }

    /* lock init */
    MAV_SFP_PAGE_LOCK_INIT;
    return 0;
}

/** Is it a panel QSFP28 for a given index of module.
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if it is and false if it is not
 */
EXPORT bool is_panel_qsfp_module (
    IN unsigned int module)
{
    const unsigned int qsfp28_num = (unsigned int)bf_qsfp_get_max_qsfp_ports ();

    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= qsfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= qsfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        if ((module >= 1) && (module <= qsfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= qsfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (HC)) {
        if ((module >= 1) && (module <= qsfp28_num)) {
            return true;
        }
    }
    return false;
}

/** Is it a panel SFP28 for a given index of module.
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if it is and false if it is not
 */
EXPORT bool is_panel_sfp_module (
    IN unsigned int module)
{
    const unsigned int sfp28_num = (unsigned int)bf_sfp_get_max_sfp_ports ();

    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= sfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= sfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        if ((module >= 1) && (module <= sfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= sfp28_num)) {
            return true;
        }
    } else if (platform_type_equal (HC)) {
        if ((module >= 1) && (module <= sfp28_num)) {
            return true;
        }
    }
    return false;
}

/** Is it a xSFP for a given index of module.
 *  COMe <-> ASIC : X1, X2
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if it is and false if it is not
 */
EXPORT bool is_xsfp_module (
    IN unsigned int module)
{
    /* Do not remove this valid check until HC36Y24C ready. */
    if (!g_xsfp_num) {
        return false;
    }

    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= g_xsfp_num)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= g_xsfp_num)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= g_xsfp_num)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        if ((module >= 1) && (module <= g_xsfp_num)) {
            return true;
        }
    }
    return false;
}

/* COMe channel, always be 2x 10G. */
EXPORT int bf_pltfm_xsfp_lookup_by_module (
    IN  unsigned int module,
    OUT uint32_t *conn_id,
    OUT uint32_t *chnl_id
)
{
    struct sfp_ctx_t *sfp, *sfp_ctx;
    if (bf_pltfm_get_xsfp_ctx (&sfp_ctx)) {
        LOG_ERROR (
            "Current platform has no xSFP %2d, exiting ...", module);
        return -1;
    }

    sfp = &sfp_ctx[(module - 1) %
        2];

    *conn_id = sfp->info.conn;
    *chnl_id = sfp->info.chnl;

    return 0;
}

/* The interface only export to caller, such as stratum, etc, and the @name as input param
 * is readable and comes from panel. The interface will return the real D_P after all ports have
 * been added by command "pm port-add".
 * by tsihang, 2022-05-11. */
EXPORT bf_dev_port_t bf_get_dev_port_by_interface_name (
    IN char *name)
{
    /* safety check */
    if (!name) {
        return -1;
    }
    bf_dev_id_t dev_id = 0;
    bf_dev_port_t d_p = 0;
    int module, err = 0;
    bf_pal_front_port_handle_t port_hdl;

    module = atoi (&name[1]);
    if (module == 0) {
        return -2;
    }

    if (name[0] == 'C') {
        port_hdl.chnl_id = 0;
        if (is_panel_qsfp_module (module)) {
            err = bf_pltfm_qsfp_lookup_by_module (module,
                                            &port_hdl.conn_id);
            if (err) {
                return -3;
            }
            goto fetch_dp;
        } else {
            /* DPU channel ? */
            err = bf_pltfm_vqsfp_lookup_by_module (module,
                                                &port_hdl.conn_id);
            if (err) {
                return -4;
            }
        }
    } else if (name[0] == 'Y') {
        if (is_panel_sfp_module (module)) {
            err = bf_pltfm_sfp_lookup_by_module (module,
                                                &port_hdl.conn_id, &port_hdl.chnl_id);
            if (err) {
                return -5;
            }
            goto fetch_dp;
        } else {
            return -6;
        }
    } else if (name[0] == 'X') {
        if (is_xsfp_module (module)) {
            err = bf_pltfm_xsfp_lookup_by_module (module,
                                                &port_hdl.conn_id, &port_hdl.chnl_id);
            if (err) {
                return -7;
            }
            goto fetch_dp;
        } else {
            return -8;
        }
    } else {
        return -9;
    }

fetch_dp:
    FP2DP (dev_id, &port_hdl, &d_p);
    /* D_P may start from zero. */
    return d_p;
}

