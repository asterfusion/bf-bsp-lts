/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_syscpld.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_bd_cfg.h>
#include <bf_pltfm_qsfp.h>
#include "bf_mav_qsfp_module.h"
#include <bf_mav_qsfp_i2c_lock.h>

#define ADDR_QSFP_I2C (0x50 << 1)
/* qsfp module lock macros */
static bf_sys_mutex_t qsfp_page_mutex;
#define MAV_QSFP_PAGE_LOCK_INIT bf_sys_mutex_init(&qsfp_page_mutex)
#define MAV_QSFP_PAGE_LOCK_DEL bf_sys_mutex_del(&qsfp_page_mutex)
#define MAV_QSFP_PAGE_LOCK bf_sys_mutex_lock(&qsfp_page_mutex)
#define MAV_QSFP_PAGE_UNLOCK bf_sys_mutex_unlock(&qsfp_page_mutex)

/* limit the maximum QSFP i2c burst size */
#define MAX_MAV_QSFP_I2C_LEN 60 /* to fit into one cp2112 report ID */

static bf_pltfm_cp2112_device_ctx_t
*qsfp_hndl[CP2112_ID_MAX] = {NULL, NULL};

/** platform qsfp subsystem initialization
 *
 *  @param arg
 *   arg __unused__
 *  @return
 *   0 on success and -1 on error
 */
int bf_pltfm_qsfp_init (void *arg)
{
    bf_pltfm_board_id_t board_id;
    (void)arg;

    fprintf (stdout,
             "\n\n================== QSFPs INIT ==================\n");

    MAV_QSFP_PAGE_LOCK_INIT;
    bf_pltfm_chss_mgmt_bd_type_get (&board_id);
    qsfp_hndl[CP2112_ID_1] =
        bf_pltfm_cp2112_get_handle (CP2112_ID_1);
    if (!qsfp_hndl[CP2112_ID_1]) {
        LOG_ERROR ("error getting upper cp2112 handle in %s\n",
                   __func__);
        return -1;
    }
    if ((board_id == BF_PLTFM_BD_ID_X532PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X532PT_V1DOT1) ||
        (board_id == BF_PLTFM_BD_ID_X532PT_V2DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT1) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT2) ||
        (board_id == BF_PLTFM_BD_ID_X308PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_HC36Y24C_V1DOT0)) {
        qsfp_hndl[CP2112_ID_2] =
            bf_pltfm_cp2112_get_handle (CP2112_ID_2);
        if (!qsfp_hndl[CP2112_ID_2]) {
            LOG_ERROR ("error getting lower cp2112 handle in %s\n",
                       __func__);
            return -1;
        }
    }
    if (bf_pltfm_init_cp2112_qsfp_bus (
            qsfp_hndl[CP2112_ID_1],
            qsfp_hndl[CP2112_ID_2])) {
        LOG_ERROR ("Error initing QSFP CP2112 sub-bus\n");
        return -1;
    }
    fprintf (stdout, "num of QSFPs/MACs : %2d/%2d\n",
             bf_qsfp_get_max_qsfp_ports(), platform_num_ports_get());

    return 0;
}

/* helper function that returns cp2112 subsystem and qsfp-id relative to the
 *  subsystem. e.g., there are two cp2112 subsystems on mavericks.
 *  <module> is 1 based.
 */
static int mav_qsfp_param_get (unsigned int
                               module,
                               unsigned int *sub_module,
                               bf_pltfm_cp2112_device_ctx_t **handle)
{
    bf_pltfm_board_id_t board_id;
    bf_pltfm_chss_mgmt_bd_type_get (&board_id);

    if ((board_id == BF_PLTFM_BD_ID_X312PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X312PT_V1DOT1) ||
        (board_id == BF_PLTFM_BD_ID_X312PT_V2DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X312PT_V3DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X312PT_V4DOT0)) {
        if (module < 1 ||
            module > (BF_MAV_SUB_PORT_CNT + 1)) {
            return -1;
        }
        /* make module zero based */
        module -= 1;
        if (module <
            BF_MAV_SUB_PORT_CNT) { /* upper cp2112 subsystem */
            *handle = qsfp_hndl[CP2112_ID_1];
            *sub_module = module;
        } else { /* this is the cpu qsf port, upper cp2112 */
            *handle = qsfp_hndl[CP2112_ID_1];
            *sub_module = BF_MAV_SUB_PORT_CNT;
        }
    } else if ((board_id == BF_PLTFM_BD_ID_X532PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X532PT_V1DOT1) ||
        (board_id == BF_PLTFM_BD_ID_X532PT_V2DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT1) ||
        (board_id == BF_PLTFM_BD_ID_X564PT_V1DOT2) ||
        (board_id == BF_PLTFM_BD_ID_X308PT_V1DOT0) ||
        (board_id == BF_PLTFM_BD_ID_HC36Y24C_V1DOT0)) {
        if (module < 1 ||
            module > (BF_MAV_SUB_PORT_CNT * 2 + 1)) {
            return -1;
        }
        /* make module zero based */
        module -= 1;
        if (module <
            BF_MAV_SUB_PORT_CNT) { /* upper cp2112 subsystem */
            *handle = qsfp_hndl[CP2112_ID_1];
            *sub_module = module;
        } else if (module < (BF_MAV_SUB_PORT_CNT *
                             2)) { /* lower cp2112 */
            *handle = qsfp_hndl[CP2112_ID_2];
            *sub_module = module - BF_MAV_SUB_PORT_CNT;
        } else { /* this is the cpu qsf port, upper cp2112 */
            *handle = qsfp_hndl[CP2112_ID_1];
            *sub_module = BF_MAV_SUB_PORT_CNT;
        }
    } else {
        return -1;
    }
    return 0;
}

/** detect a qsfp module by attempting to read its memory offste zero
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if present and false if not present
 */
bool bf_pltfm_detect_qsfp (unsigned int module)
{
    int rc;
    uint8_t val;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    rc = mav_qsfp_param_get (module, &sub_module,
                             &hndl);
    if (rc) {
        return false;
    }
    rc = bf_mav_qsfp_sub_module_read (hndl,
                                      sub_module, ADDR_QSFP_I2C, 0, 1, &val);
    if (rc != BF_PLTFM_SUCCESS) {
        return false;
    } else {
        return true;
    }
}

/** read a qsfp module memory
 *
 *  @param module
 *   module (1 based)
 *  @param offset
 *   offset into qsfp memory space
 *  @param len
 *   len num of bytes to read
 *  @param buf
 *   buf to read into
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_read_module (unsigned int
                               module,
                               int offset,
                               int len,
                               uint8_t *buf)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    rc = mav_qsfp_param_get (module, &sub_module,
                             &hndl);
    if (rc) {
        return -1;
    }
    MAV_QSFP_PAGE_LOCK;
    rc = 0;
    while (len >= MAX_MAV_QSFP_I2C_LEN) {
        rc |= bf_mav_qsfp_sub_module_read (
                  hndl, sub_module, ADDR_QSFP_I2C, offset,
                  MAX_MAV_QSFP_I2C_LEN, buf);
        buf += MAX_MAV_QSFP_I2C_LEN;
        offset += MAX_MAV_QSFP_I2C_LEN;
        len -= MAX_MAV_QSFP_I2C_LEN;
    }
    if (len) {
        rc |= bf_mav_qsfp_sub_module_read (
                  hndl, sub_module, ADDR_QSFP_I2C, offset, len,
                  buf);
    }
    MAV_QSFP_PAGE_UNLOCK;
    if (rc != BF_PLTFM_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}

/** write a qsfp module memory
 *
 *  @param module
 *   module (1 based)
 *  @param offset
 *   offset into qsfp memory space
 *  @param len
 *   len num of bytes to write
 *  @param buf
 *   buf to read from
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_write_module (unsigned int
                                module,
                                int offset,
                                int len,
                                uint8_t *buf)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    rc = mav_qsfp_param_get (module, &sub_module,
                             &hndl);
    if (rc) {
        return -1;
    }
    MAV_QSFP_PAGE_LOCK;
    rc = 0;
    while (len >= MAX_MAV_QSFP_I2C_LEN) {
        rc |= bf_mav_qsfp_sub_module_write (
                  hndl, sub_module, ADDR_QSFP_I2C, offset,
                  MAX_MAV_QSFP_I2C_LEN, buf);
        buf += MAX_MAV_QSFP_I2C_LEN;
        offset += MAX_MAV_QSFP_I2C_LEN;
        len -= MAX_MAV_QSFP_I2C_LEN;
    }
    if (len) {
        rc |= bf_mav_qsfp_sub_module_write (
                  hndl, sub_module, ADDR_QSFP_I2C, offset, len,
                  buf);
    }
    MAV_QSFP_PAGE_UNLOCK;
    if (rc != BF_PLTFM_SUCCESS) {
        return -1;
    } else {
        return 0;
    }
}

int bf_pltfm_qsfp_read_reg (unsigned int module,
                            uint8_t page,
                            int offset,
                            uint8_t *val)
{
    int err = 0;

    if (page != 0 && !bf_qsfp_has_pages (module)) {
        return -1;
    }

    if (offset >= (2 * MAX_QSFP_PAGE_SIZE)) {
        return -1;
    }

    MAV_QSFP_PAGE_LOCK;
    /* set the page */
    if (bf_pltfm_qsfp_write_module (module, 127, 1,
                                    &page)) {
        err = -1;
        goto read_reg_end;
    }
    /* read the register */
    if (bf_pltfm_qsfp_read_module (module, offset, 1,
                                   val)) {
        err = -1;
        goto read_reg_end;
    }
    /* reset the page */
    page = 0;
    if (bf_pltfm_qsfp_write_module (module, 127, 1,
                                    &page)) {
        err = -1;
        goto read_reg_end;
    }
read_reg_end:
    MAV_QSFP_PAGE_UNLOCK;
    return err;
}

int bf_pltfm_qsfp_write_reg (unsigned int module,
                             uint8_t page,
                             int offset,
                             uint8_t val)
{
    int err = 0;

    if (page != 0 && !bf_qsfp_has_pages (module)) {
        return -1;
    }

    if (offset >= (2 * MAX_QSFP_PAGE_SIZE)) {
        return -1;
    }

    MAV_QSFP_PAGE_LOCK;
    /* set the page */
    if (bf_pltfm_qsfp_write_module (module, 127, 1,
                                    &page)) {
        err = -1;
        goto write_reg_end;
    }
    /* read the register */
    if (bf_pltfm_qsfp_write_module (module, offset, 1,
                                    &val)) {
        err = -1;
        goto write_reg_end;
    }
    /* reset the page */
    page = 0;
    if (bf_pltfm_qsfp_write_module (module, 127, 1,
                                    &page)) {
        err = -1;
        goto write_reg_end;
    }
write_reg_end:
    MAV_QSFP_PAGE_UNLOCK;
    return err;
}

/** read a syscpld register
 *
 *  @param module
 *   module (1 based)
 *  @param reg
 *   syscpld register to read
 *  @param val
 *   value to read into
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_syscpld_read (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    int reg,
    uint8_t *val)
{
    uint8_t wr_val;

    wr_val = reg;
    return (bf_pltfm_cp2112_write_read_unsafe (
                hndl, BF_MAV_SYSCPLD_I2C_ADDR << 1, &wr_val, val,
                1, 1, 100));
}

/** write a syscpld register
 *
 *  @param module
 *   module (1 based)
 *  @param reg
 *   syscpld register to write
 *  @param val
 *   value to write
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_syscpld_write (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    int reg,
    uint8_t val)
{
    uint8_t wr_val[2];

    wr_val[0] = reg;
    wr_val[1] = val;
    return (bf_pltfm_cp2112_write (hndl,
                                   BF_MAV_SYSCPLD_I2C_ADDR << 1, wr_val, 2, 100));
}

/** reset qsfp module thru syscpld register
 *
 *  @param module
 *   module (1 based)
 *  @param reg
 *   syscpld register to write
 *  @param val
 *   value to write
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_module_reset (unsigned int
                                module,
                                bool reset)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    rc = mav_qsfp_param_get (module, &sub_module,
                             &hndl);
    if (rc) {
        return -2;
    }
    return bf_pltfm_sub_module_reset (hndl,
                                      sub_module, reset);
}

int bf_pltfm_qsfp_get_presence_mask (
    uint32_t *port_1_32_pres,
    uint32_t *port_32_64_pres,
    uint32_t *port_cpu_pres)
{
    bf_pltfm_cp2112_device_ctx_t *hndl = NULL;
    /* initialize to absent */
    uint32_t qsfp_pres_mask_l = 0xFFFFFFFF,
             qsfp_pres_mask_h = 0xFFFFFFFF;

    bf_pltfm_get_sub_module_pres (hndl,
                                  &qsfp_pres_mask_l, &qsfp_pres_mask_h);

    *port_1_32_pres  = qsfp_pres_mask_l;
    *port_32_64_pres = qsfp_pres_mask_h;
    /* CPU mask */
    *port_cpu_pres   = *port_cpu_pres;

    return 0;
}

/** get qsfp interrupt status
 *
 *  @param port_1_32_ints
 *   interrupt from lower 32 ports (1-32) 0: int-present, 1:not-present
 *  @param port_32_64_ints
 *   interrupt from upper 32 ports (33-64) 0: int-present, 1:not-present
 *  @param port_cpu_ints
 *   mask for cpu port interrupt
 */
void bf_pltfm_qsfp_get_int_mask (uint32_t
                                 *port_1_32_ints,
                                 uint32_t *port_32_64_ints,
                                 uint32_t *port_cpu_ints)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    /* get the cp2112 for lower numbered ports */
    rc = mav_qsfp_param_get (1, &sub_module, &hndl);
    if (rc) {
        return;
    }
    bf_pltfm_get_sub_module_int (hndl,
                                 port_1_32_ints);
    bf_pltfm_get_cpu_module_int (hndl, port_cpu_ints);

    /* get the cp2112 for upper numbered ports */
    rc = mav_qsfp_param_get (33, &sub_module, &hndl);
    if (rc) {
        *port_cpu_ints = 0xFFFFFFFFUL;
        return;
    }
    bf_pltfm_get_sub_module_int (hndl,
                                 port_32_64_ints);
}

/** get qsfp lpmode status
 *
 *  @param port_1_32_lpmode_
 *   lpmode of lower 32 ports (1-32) 0: no-lpmod 1: lpmode
 *  @param port_32_64_ints
 *   lpmode of upper 32 ports (33-64) 0: no-lpmod 1: lpmode
 *  @param port_cpu_ints
 *   lpmode of cpu port
 */
int bf_pltfm_qsfp_get_lpmode_mask (
    uint32_t *port_1_32_lpm,
    uint32_t *port_32_64_lpm,
    uint32_t *port_cpu_lpm)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    /* get the cp2112 for lower numbered ports */
    rc = mav_qsfp_param_get (1, &sub_module, &hndl);
    if (rc) {
        return -1;
    }
    bf_pltfm_get_sub_module_lpmode (hndl,
                                    port_1_32_lpm);
    bf_pltfm_get_cpu_module_lpmode (hndl,
                                    port_cpu_lpm);

    /* get the cp2112 for upper numbered ports */
    rc = mav_qsfp_param_get (33, &sub_module, &hndl);
    if (rc) {
        *port_cpu_lpm = 0x0;
        return -1;
    }
    rc = bf_pltfm_get_sub_module_lpmode (hndl,
                                         port_32_64_lpm);
    return rc;
}

/** set qsfp lpmode
 *
 *  @param module
 *   port
 *  @param lpmode
 *   true : set lpmode, false : set no-lpmode
 *  @return
 *   0: success, -1: failure
 */
int bf_pltfm_qsfp_set_lpmode (unsigned int module,
                              bool lpmode)
{
    int rc;
    unsigned int sub_module;
    bf_pltfm_cp2112_device_ctx_t *hndl;

    /* get the cp2112 for lower numbered ports */
    rc = mav_qsfp_param_get (module, &sub_module,
                             &hndl);
    if (rc) {
        return -1;
    }
    return (bf_pltfm_set_sub_module_lpmode (hndl,
                                            sub_module, lpmode));
}

/* set the i2c mux to enable the MISC channel of PCA9548. many miscellaneous
 * devices are present on this channel. Get the QSFP-lock before enabling.
 * Calling function must call the function
 * bf_pltfm_qsfp_unselect_unlock_misc_chn()
 * subbsequently.
 * this is on CP2112_ID_1.
 */
int bf_pltfm_i2c_lock_select_misc_chn()
{
    return (bf_mav_lock_select_misc_chn (
                qsfp_hndl[CP2112_ID_1]));
}

/* set the i2c mux to disable the MISC channel of PCA9548. many miscellaneous
 * devices are present on this channel. release the QSFP-lock after this.
 * this is on CP2112_ID_1.
 */
int bf_pltfm_unselect_i2c_unlock_misc_chn()
{
    return (bf_mav_unselect_unlock_misc_chn (
                qsfp_hndl[CP2112_ID_1]));
}

/* disable all channel of PCA9548 on CP2112_ID_2. many miscellaneous
 * devices are present on this channel. Get the QSFP-lock before disabling.
 * Calling function must call the function
 * bf_pltfm_qsfp_unselect_unlock_rtmr_i2c_bus()
 * subbsequently.
 * this is on CP2112_ID_2.
 */
int bf_pltfm_rtmr_i2c_lock()
{
    /* for now, we want to just lock the i2c resouce. QSFP sub system
     * is assumed to leave all channels of all PCA9548s disabled when it
     * releases the i2c lock */
    bf_pltfm_i2c_lock();
    return 0;
    //  return (bf_mav_lock_unselect_misc_chn(qsfp_hndl[CP2112_ID_2]));
}

/* set the i2c mux to disable all channels of PCA9548. many miscellaneous
 * devices are present on this channel. release the QSFP-lock after this.
 * this is on CP2112_ID_2.
 */
int bf_pltfm_rtmr_i2c_unlock()
{
    /* for now, we want to just unlock the i2c resouce. QSFP sub system
     * is assumed to leave all channels of all PCA9548s disabled when it
     * releases the i2c lock */
    bf_pltfm_i2c_unlock();
    return 0;
    //  return (bf_mav_unselect_unlock_misc_chn(qsfp_hndl[CP2112_ID_2]));
}
