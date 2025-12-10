/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <stdio.h>
#include <string.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_syscpld.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>
#include "bf_mav_qsfp_module.h"
#include <bf_mav_qsfp_i2c_lock.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>
#include <pltfm_types.h>

/* QSFP module lock macros, used for cp2112 based IO.
 * For some platforms, SFP IO is using the same lock.
 * by tsihang, 2021-07-18. */
#define MAV_QSFP_LOCK   \
    if(platform_type_equal(AFN_X312PT)) {\
        MASTER_I2C_LOCK;\
    } else {\
        bf_pltfm_i2c_lock();\
    }
#define MAV_QSFP_UNLOCK \
    if(platform_type_equal(AFN_X312PT)) {\
        MASTER_I2C_UNLOCK;\
    } else {\
        bf_pltfm_i2c_unlock();\
    }

#define DEFAULT_TIMEOUT_MS 500

typedef enum {
    /* The PCA9548 switch selecting for 32 port controllers
    * Note that the address is shifted one to the left to
    * work with the underlying I2C libraries.
    */
    ADDR_SWITCH_32 = 0xe0,
    ADDR_SWITCH_COMM = 0xe8
} mav_mux_pca9548_addr_t;


/* DPU channel num. */
static uint32_t max_vqsfp = 4;
static uint32_t max_vsfp = 6;

/* Global qsfp_ctx specifed by platform.
 * by tsihang, 2021-08-02. */
static struct qsfp_ctx_t *g_qsfp_ctx;
/* DPU channel. */
static struct qsfp_ctx_t *g_vqsfp_ctx;

extern int bf_pltfm_get_sub_module_pres_hc (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
extern int bf_pltfm_get_sub_module_pres_x308p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
extern int bf_pltfm_get_sub_module_pres_x312p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
extern int bf_pltfm_get_sub_module_pres_x532p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
extern int bf_pltfm_get_sub_module_pres_x564p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);
extern int bf_pltfm_get_sub_module_pres_x732q (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h);

void bf_pltfm_qsfp_init_x312p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);
void bf_pltfm_qsfp_init_x308p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);
void bf_pltfm_qsfp_init_x564p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);
void bf_pltfm_qsfp_init_x532p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);
void bf_pltfm_qsfp_init_x732q (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);
void bf_pltfm_qsfp_init_hc36y24c (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum);

/* For those board which has 2 layer PCA9548 to select a port.
 * Not finished yet.
 * by tsihang, 2021-07-15. *
 */
static bool
select_2nd_layer_pca9548 (struct qsfp_ctx_t *qsfp,
                          unsigned char *pca9548, unsigned char *channel)
{
    if (platform_type_equal (AFN_X312PT)) {
        uint8_t l1_pca9548_addr = 0x71;
        uint8_t l1_pca9548_chnl = 1;

        uint8_t l2_pca9548_addr = qsfp->pca9548;
        uint8_t l2_pca9548_chnl = qsfp->channel;
        uint8_t l2_pca9548_unconcerned_addr = ((
                qsfp->rev >> 16) & 0x000000FF);
        /* select 1st pca9548 */
        if (bf_pltfm_master_i2c_write_byte (l1_pca9548_addr,
                                      0x0, 1 << l1_pca9548_chnl)) {
            return false;
        }
        /* unselect unconcerned 2nd pca9548 */
        if (bf_pltfm_master_i2c_write_byte (
                l2_pca9548_unconcerned_addr, 0x0, 0x0)) {
            return false;
        }
        /* select 2nd pca9548 */
        if (bf_pltfm_master_i2c_write_byte (l2_pca9548_addr,
                                      0x0, 1 << l2_pca9548_chnl)) {
            return false;
        }
        /* pass CPLD select offset/value to params pca9548/channel */
        *pca9548 = ((qsfp->rev >> 8) & 0x000000FF);
        *channel = (qsfp->rev & 0x000000FF);
        return true;
    } else {
        return false;
    }
}

static bool
unselect_2nd_layer_pca9548 (struct qsfp_ctx_t
                            *qsfp,
                            unsigned char *pca9548, unsigned char *channel)
{
    if (platform_type_equal (AFN_X312PT)) {
        uint8_t l1_pca9548_addr = 0x71;
        //uint8_t l1_pca9548_chnl = 1;

        uint8_t l2_pca9548_addr = qsfp->pca9548;
        //uint8_t l2_pca9548_chnl = qsfp->channel;
        uint8_t l2_pca9548_unconcerned_addr = ((
                qsfp->rev >> 16) & 0x000000FF);
        /* unselect unconcerned 2nd pca9548 */
        if (bf_pltfm_master_i2c_write_byte (
                l2_pca9548_unconcerned_addr, 0x0, 0x0)) {
            return false;
        }
        /* unselect 2nd pca9548 */
        if (bf_pltfm_master_i2c_write_byte (l2_pca9548_addr,
                                      0x0, 0x0)) {
            return false;
        }
        /* unselect 1st pca9548 */
        if (bf_pltfm_master_i2c_write_byte (l1_pca9548_addr,
                                      0x0, 0x0)) {
            return false;
        }
        /* pass CPLD select offset/value to params pca9548/channel */
        *pca9548 = ((qsfp->rev >> 8) & 0x000000FF);
        *channel = (qsfp->rev & 0x000000FF);
        return true;
    } else {
        return false;
    }
}

static bool
select_1st_layer_pca9548 (int module,
                          unsigned char *pca9548, unsigned char *channel)
{
    struct qsfp_ctx_t *qsfp;

    qsfp = &g_qsfp_ctx[module];

    /* There's no need to select 2nd layer pca9548. */
    if (qsfp->rev == INVALID) {
        *pca9548 = qsfp->pca9548;
        *channel = qsfp->channel;
    } else {
        /* 2nd layer pca9548. */
        select_2nd_layer_pca9548 (qsfp, pca9548, channel);
    }

    return true;
}

static bool
unselect_1st_layer_pca9548 (int module,
                            unsigned char *pca9548, unsigned char *channel)
{
    struct qsfp_ctx_t *qsfp;

    qsfp = &g_qsfp_ctx[module];

    /* There's no need to select 2nd layer pca9548. */
    if (qsfp->rev == INVALID) {
        *pca9548 = qsfp->pca9548;
        *channel = qsfp->channel;
    } else {
        /* 2nd layer pca9548. */
        unselect_2nd_layer_pca9548 (qsfp, pca9548,
                                    channel);
    }

    return true;
}


/* intialize PCA 9548 and PCA 9535 on the qsfp subsystem i2c bus to
 * ensure a consistent state on i2c addressed devices
 */
static int qsfp_init_sub_bus (
    bf_pltfm_cp2112_device_ctx_t *hndl1,
    bf_pltfm_cp2112_device_ctx_t *hndl2)
{
    int i;
    int rc;
    int hndl1_max_pca9548_cnt = 0;
    int hndl2_max_pca9548_cnt = 0;
    bf_pltfm_board_id_t board_id;
    bf_pltfm_chss_mgmt_bd_type_get (&board_id);

    if (platform_type_equal (AFN_X532PT)) {
        hndl1_max_pca9548_cnt = 4;
        hndl2_max_pca9548_cnt = 0;
    } else if (platform_type_equal (AFN_X564PT)) {
        hndl1_max_pca9548_cnt = 8;
        hndl2_max_pca9548_cnt = 0;
    } else if (platform_type_equal (AFN_X308PT)) {
        hndl1_max_pca9548_cnt = 7;
        hndl2_max_pca9548_cnt = 0;
    } else if (platform_type_equal (AFN_X732QT)) {
        if (board_id == AFN_BD_ID_X732QT_V1P0) {
            hndl1_max_pca9548_cnt = 5;
            hndl2_max_pca9548_cnt = 0;
        } else if (board_id == AFN_BD_ID_X732QT_V1P1) {
            hndl1_max_pca9548_cnt = 4;
            hndl2_max_pca9548_cnt = 1;
        } else if (board_id == AFN_BD_ID_X732QT_V2P0) {
            hndl1_max_pca9548_cnt = 4;
            hndl2_max_pca9548_cnt = 1;
        }
    }

    /* Initialize all PCA9548 devices to select channel 0 */
    for (i = 0; i < hndl1_max_pca9548_cnt; i++) {
        rc = bf_pltfm_cp2112_write_byte (
                 hndl1, ADDR_SWITCH_32 + (i * 2), 0,
                 DEFAULT_TIMEOUT_MS);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error in initializing the PCA9548 devices itr %d, "
                       "addr 0x%02x", i, ADDR_SWITCH_32 + (i * 2));
            //return -1;
        }
    }
    for (i = hndl1_max_pca9548_cnt; i < hndl1_max_pca9548_cnt + hndl2_max_pca9548_cnt; i++) {
        rc = bf_pltfm_cp2112_write_byte (
                 hndl2, ADDR_SWITCH_32 + (i * 2), 0,
                 DEFAULT_TIMEOUT_MS);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error in initializing the PCA9548 devices itr %d, "
                       "addr 0x%02x", i, ADDR_SWITCH_32 + (i * 2));
            //return -1;
        }
    }

    return 0;
}

/* initialize various devices on qsfp cp2112 subsystem i2c bus */
int bf_pltfm_init_cp2112_qsfp_bus (
    bf_pltfm_cp2112_device_ctx_t *hndl1,
    bf_pltfm_cp2112_device_ctx_t *hndl2)
{
    if (hndl1 != NULL) {
        if (qsfp_init_sub_bus (hndl1, hndl2)) {
            return -1;
        }
    }
#if 0
    /* Make sure that the cp2112 hndl in CP2112_ID_1 for those platforms
     * which has two CP2112 but only one on duty to access QSFP.
     * See differentiate_cp2112_devices for reference.
     * by tsihang, 2021-07-14. */
    if (hndl2 != NULL) {
        if (qsfp_init_sub_bus (hndl2)) {
            return -1;
        }
    }
#endif

    uint32_t qsfp28_num = 0, vqsfp_num = 0;
    if (platform_type_equal (AFN_X532PT)) {
        bf_pltfm_qsfp_init_x532p((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    } else if (platform_type_equal (AFN_X564PT)) {
        bf_pltfm_qsfp_init_x564p((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    } else if (platform_type_equal (AFN_X308PT)) {
        bf_pltfm_qsfp_init_x308p((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    } else if (platform_type_equal (AFN_X312PT)) {
        bf_pltfm_qsfp_init_x312p((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    } else if (platform_type_equal (AFN_X732QT)) {
        bf_pltfm_qsfp_init_x732q((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        bf_pltfm_qsfp_init_hc36y24c((struct qsfp_ctx_t **)&g_qsfp_ctx,
            &qsfp28_num, (struct qsfp_ctx_t **)&g_vqsfp_ctx, &vqsfp_num);
    }
    BUG_ON (g_qsfp_ctx == NULL);
    bf_qsfp_set_num (qsfp28_num);
    max_vqsfp = vqsfp_num;

    fprintf (stdout, "QSFPs/vQSFPs : %2d/%2d\n",
             bf_qsfp_get_max_qsfp_ports(), bf_pltfm_get_max_vqsfp_ports());

    return 0;
}

bool is_internal_port_need_autonegotiate (
    uint32_t conn_id, uint32_t chnl_id)
{
    if (platform_type_equal (AFN_X564PT)) {
        if (conn_id == 65) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (AFN_X532PT)) {
        if (conn_id == 33) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (AFN_X308PT)) {
        if (conn_id == 33) {
            if (chnl_id <= 3) {
                return true;
            }
        }
    } else if (platform_type_equal (AFN_X312PT)) {
        if (conn_id == 33) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (AFN_X732QT)) {
        if (conn_id == 33) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        if (conn_id == 65) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
        /* fabric card port */
        if ((conn_id >= 40) && (conn_id <= 55)) {
            return true;
        }
    }
    return false;
}

/* disable the channel of PCA 9548  connected to the CPU QSFP */
int unselect_cpu_qsfp (
    bf_pltfm_cp2112_device_ctx_t *hndl)
{
    int rc;
    uint8_t i2c_addr;

    i2c_addr = ADDR_SWITCH_COMM;
    rc = bf_pltfm_cp2112_write_byte (hndl, i2c_addr,
                                     0, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in %s\n", __func__);
        return -1;
    }
    return 0;
}

/* enable the channel of PCA 9548  connected to the CPU QSFP */
int select_cpu_qsfp (bf_pltfm_cp2112_device_ctx_t
                     *hndl)
{
    int rc;
    uint8_t bit;
    uint8_t i2c_addr;

    bit = (1 << MAV_PCA9548_CPU_PORT);
    i2c_addr = ADDR_SWITCH_COMM;
    rc = bf_pltfm_cp2112_write_byte (hndl, i2c_addr,
                                     bit, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error in %s\n", __func__);
        return -1;
    }
    return 0;
}

/* disable the channel of PCA9548 connected to the QSFP (sub_port)  */
/* sub_port is zero based */
static int unselect_qsfp (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int sub_port)
{
    int rc;
    int retry_times = 0;
    uint8_t pca9548_value = 0xFF;
    uint8_t chnl = 0;
    uint8_t i2c_addr = 0;

    if (!unselect_1st_layer_pca9548 (sub_port,
                                     &i2c_addr,
                                     &chnl)) {
        return -1;
    }

    if (platform_type_equal (AFN_X312PT)) {
        /* select QSFP in CPLD1, i2c_addr is offset, chnl is value */
        rc = bf_pltfm_cpld_write_byte (
                 BF_MAV_SYSCPLD1, i2c_addr, 0xff);
    } else {
        rc = -2;
        for (retry_times = 0; retry_times < 10; retry_times ++) {
            // unselect PCA9548
            if (bf_pltfm_cp2112_write_byte (hndl, i2c_addr, 0,
                                            DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }

            bf_sys_usleep (5000);

            if (bf_pltfm_cp2112_read (hndl, i2c_addr, &pca9548_value, 1,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -4;
                break;
            }

            if (pca9548_value == 0) {
                rc = BF_PLTFM_SUCCESS;
                break;
            }

            bf_sys_usleep (5000);
        }
    }
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "%s[%d], "
            "qsfp.disselect(%02d : %s : %d : %02x : %02x)"
            "\n",
            __FILE__, __LINE__, sub_port,
            "Failed to diselect QSFP", rc, i2c_addr, chnl);
        return -5;
    }

    return 0;
}

/* enable the channel of PCA9548 connected to the QSFP (sub_port)  */
/* sub_port is zero based */
static int select_qsfp (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int sub_port)
{
    int rc;
    uint8_t chnl = 0;
    uint8_t i2c_addr = 0;

    if (!select_1st_layer_pca9548 (sub_port,
                                   &i2c_addr,
                                   &chnl)) {
        return -1;
    }

    if (platform_type_equal (AFN_X312PT)) {
        /* select QSFP in CPLD1, i2c_addr is offset, chnl is value */
        rc = bf_pltfm_cpld_write_byte (
                 BF_MAV_SYSCPLD1, i2c_addr, chnl);
    } else {
        rc = bf_pltfm_cp2112_write_byte (hndl,
                                         i2c_addr, (1 << chnl), DEFAULT_TIMEOUT_MS);
    }
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "%s[%d], "
            "qsfp.select(%02d : %s : %d : %02x : %02x)"
            "\n",
            __FILE__, __LINE__, sub_port,
            "Failed to select QSFP", rc, i2c_addr, chnl);
        return -2;
    }

    return 0;
}

/* disable the channel of PCA 9548  connected to the CPU QSFP
 * and release i2c lock
 */
int bf_mav_unselect_unlock_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl)
{
    int rc;

    rc = unselect_cpu_qsfp (hndl);
    MAV_QSFP_UNLOCK;
    return rc;
}

/* get the i2c lock and
 * enable the channel of PCA 9548  connected to the CPU port PCA9535
 */
int bf_mav_lock_select_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl)
{
    int rc;
    uint8_t bit;
    uint8_t i2c_addr;

    MAV_QSFP_LOCK;
    bit = (1 << MAV_PCA9535_MISC);
    i2c_addr = ADDR_SWITCH_COMM;
    rc = bf_pltfm_cp2112_write_byte (hndl, i2c_addr,
                                     bit, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        MAV_QSFP_UNLOCK;
        LOG_ERROR ("Error in %s\n", __func__);
        return -1;
    }
    return 0;
}

/* get the i2c lock and
 * disable the channel of PCA 9548  connected to the CPU port PCA9535
 */
int bf_mav_lock_unselect_misc_chn (
    bf_pltfm_cp2112_device_ctx_t *hndl)
{
    int rc;
    uint8_t bit;
    uint8_t i2c_addr;

    MAV_QSFP_LOCK;
    bit = 0;
    i2c_addr = ADDR_SWITCH_COMM;
    rc = bf_pltfm_cp2112_write_byte (hndl, i2c_addr,
                                     bit, DEFAULT_TIMEOUT_MS);
    if (rc != BF_PLTFM_SUCCESS) {
        MAV_QSFP_UNLOCK;
        LOG_ERROR ("Error in %s\n", __func__);
        return -1;
    }
    return 0;
}
/** read a qsfp module within a single cp2112 domain
 *
 *  @param module
 *   module  0 based ; cpu qsfp = 32
 *  @param i2c_addr
 *   i2c_addr of qsfp module (left bit shitef by 1)
 *  @param offset
 *   offset in qsfp memory
 *  @param len
 *   num of bytes to read
 *  @param buf
 *   buf to read into
 *  @return bf_pltfm_status_t
 *   BF_PLTFM_SUCCESS on success and other codes on error
 */
bf_pltfm_status_t bf_mav_qsfp_sub_module_read (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    uint8_t i2c_addr,
    unsigned int offset,
    int len,
    uint8_t *buf)
{
    int rc;

    if (((offset + len) > (MAX_QSFP_PAGE_SIZE * 2))
        /*|| module > BF_MAV_SUB_PORT_CNT */) {
        return BF_PLTFM_INVALID_ARG;
    }

    MAV_QSFP_LOCK;
    if (select_qsfp (hndl, module)) {
        unselect_qsfp (hndl,
                       module); /* unselect: selection might have happened */
        MAV_QSFP_UNLOCK;
        return BF_PLTFM_COMM_FAILED;
    }
    if (!platform_type_equal (AFN_X312PT)) {
        rc = bf_pltfm_cp2112_write_byte (hndl, i2c_addr,
                                         offset, DEFAULT_TIMEOUT_MS);
        if (rc != BF_PLTFM_SUCCESS) {
            goto i2c_error_end;
        }
        if (len > 128) {
            rc = bf_pltfm_cp2112_read (hndl, i2c_addr, buf,
                                       128, DEFAULT_TIMEOUT_MS);
            if (rc != BF_PLTFM_SUCCESS) {
                goto i2c_error_end;
            }
            buf += 128;
            len -= 128;
            rc = bf_pltfm_cp2112_read (hndl, i2c_addr, buf,
                                       len, DEFAULT_TIMEOUT_MS);
            if (rc != BF_PLTFM_SUCCESS) {
                goto i2c_error_end;
            }

        } else {
            rc = bf_pltfm_cp2112_read (hndl, i2c_addr, buf,
                                       len, DEFAULT_TIMEOUT_MS);
            if (rc != BF_PLTFM_SUCCESS) {
                goto i2c_error_end;
            }
        }
    } else {
        rc = bf_pltfm_master_i2c_read_block (
                 i2c_addr >> 1, offset, buf, len);
        if (rc != BF_PLTFM_SUCCESS) {
            goto i2c_error_end;
        }
    }

    unselect_qsfp (hndl, module);
    MAV_QSFP_UNLOCK;
    return BF_PLTFM_SUCCESS;

i2c_error_end:
    /*
     * Temporary change to remove the clutter on console. This is
     * a genuine error is the module was present
     */
    LOG_DEBUG ("Error in qsfp read port %d\n",
               module + 1);
    unselect_qsfp (hndl, module);
    MAV_QSFP_UNLOCK;
    return BF_PLTFM_COMM_FAILED;
}

/** write a qsfp module within a single cp2112 domain
 *
 *  @param module
 *   module  0 based ; cpu qsfp = 32
 *  @param i2c_addr
 *   i2c_addr of qsfp module (left bit shitef by 1)
 *  @param offset
 *   offset in qsfp memory
 *  @param len
 *   num of bytes to write
 *  @param buf
 *   buf to write from
 *  @return bf_pltfm_status_t
 *   BF_PLTFM_SUCCESS on success and other codes on error
 */
bf_pltfm_status_t bf_mav_qsfp_sub_module_write (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    uint8_t i2c_addr,
    unsigned int offset,
    int len,
    uint8_t *buf)
{
    int rc;
    uint8_t out_buf[61] = {0};

    /* CP2112 can write only 61 bytes at a time, and we use up one bit for the
     * offset */
    if ((len > 60) ||
        ((offset + len) >= (MAX_QSFP_PAGE_SIZE * 2))
        /*|| module > BF_MAV_SUB_PORT_CNT */) {
        return BF_PLTFM_INVALID_ARG;
    }

    MAV_QSFP_LOCK;
    if (select_qsfp (hndl, module)) {
        unselect_qsfp (hndl,
                       module); /* unselect: selection might have happened */
        MAV_QSFP_UNLOCK;
        return BF_PLTFM_COMM_FAILED;
    }

    if (!platform_type_equal (AFN_X312PT)) {
        out_buf[0] = offset;
        memcpy (out_buf + 1, buf, len);

        rc = bf_pltfm_cp2112_write (
                 hndl, i2c_addr, out_buf, len + 1,
                 DEFAULT_TIMEOUT_MS);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_WARNING ("Error in qsfp write port <%d>\n",
                       module + 1);
            unselect_qsfp (hndl, module);
            MAV_QSFP_UNLOCK;
            return BF_PLTFM_COMM_FAILED;
        }
    } else {
        rc = bf_pltfm_master_i2c_write_block (
                 i2c_addr >> 1, offset, buf, len);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_WARNING ("Error in qsfp write port <%d>\n",
                       module + 1);
            unselect_qsfp (hndl, module);
            MAV_QSFP_UNLOCK;
            return BF_PLTFM_COMM_FAILED;
        }
    }

    unselect_qsfp (hndl, module);
    MAV_QSFP_UNLOCK;
    return BF_PLTFM_SUCCESS;
}

int bf_pltfm_sub_module_reset (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module, bool reset)
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    bool original_reset = reset;
    struct qsfp_ctx_t *qsfp;
    struct st_ctx_t *st;
    int usec = 1000;

    qsfp = &g_qsfp_ctx[module];
    st = &qsfp->rst;

    /* for X312P/X732Q, 0 means reset, 1 means de-reset, so reverse reset */
    if (platform_type_equal (AFN_X312PT)) {
        if (!reset) {
            /* There's no need to de-reset! It will auto de-reset after 200ms, so just return */
            return 0;
        }
        reset = !reset;
    } else if (platform_type_equal (AFN_X732QT)) {
        reset = !reset;
    }

    rc = bf_pltfm_cpld_read_byte (st->cpld_sel, st->off, &val0);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, (module + 1), st->off,
            "Failed to read CPLD");
        goto end;
    }

    val = val0;
    if (reset) {
        /* reset control bit is active high */
        val |= (1 << (st->off_b));
    } else {
        /* de-reset control bit is active low */
        val &= ~ (1 << (st->off_b));
    }

    /* Write it back */
    rc = bf_pltfm_cpld_write_byte (st->cpld_sel, st->off, val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "write_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, (module + 1), st->off,
            "Failed to write CPLD");
        goto end;
    }
    bf_sys_usleep (usec);

    rc = bf_pltfm_cpld_read_byte (st->cpld_sel, st->off, &val1);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, (module + 1), st->off,
            "Failed to read CPLD");
        goto end;
    }

    LOG_DEBUG (
        "QSFP    %2d : %sRST : (0x%02x -> 0x%02x)  ",
        (module + 1), original_reset ? "+" : "-", val0,
        val1);

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    if (platform_type_equal (AFN_X532PT)) {
        return bf_pltfm_get_sub_module_pres_x532p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (AFN_X564PT)) {
        return bf_pltfm_get_sub_module_pres_x564p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (AFN_X308PT)) {
        return bf_pltfm_get_sub_module_pres_x308p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (AFN_X312PT)) {
        return bf_pltfm_get_sub_module_pres_x312p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (AFN_X732QT)) {
        return bf_pltfm_get_sub_module_pres_x732q (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (AFN_HC36Y24C)) {
        /* HCv1, HCv2, HCv3, ... */
        return bf_pltfm_get_sub_module_pres_hc (hndl,
                                                pres_l, pres_h);
    }

    return -1;
}

/* get the qsfp interrupt status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module interrupt active, 1: interrupt not-active
 */
int bf_pltfm_get_sub_module_int (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *intr)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    *intr = 0xFFFFFFFF;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}


/* get the qsfp present mask status for the cpu port
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_cpu_module_pres (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    *pres = 0xFFFFFFFF;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}


/* get the qsfp interrupt status for the cpu port
 * bit 0: module interrupt active, 1: interrupt not-active
 */
int bf_pltfm_get_cpu_module_int (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *intr)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    *intr = 0xFFFFFFFF;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}


/* get the qsfp lpmode status for the cpu port
 * bit 0: no-lpmode 1: lpmode
 */
int bf_pltfm_get_cpu_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *lpmode)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    *lpmode = 0xFFFFFFFF;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}


/* set the qsfp reset for the cpu port
 */
int bf_pltfm_set_cpu_module_reset (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    bool reset)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    reset = reset;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}


/* set the qsfp lpmode for the cpu port
 */
int bf_pltfm_set_cpu_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    bool lpmode)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    lpmode = lpmode;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}

/* get the qsfp lpmode as set by hardware pins
 */
int bf_pltfm_get_sub_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *lpmode)
{
    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    *lpmode = 0xFFFFFFFF;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}

/* set the qsfp lpmode as set by hardware pins
 */
int bf_pltfm_set_sub_module_lpmode (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    unsigned int module,
    bool lpmode)
{
    /* Set lpmode via hardware pin is NOT supported for ALL X-T platforms */

    int rc;
    /* If the THIS source is indicated by CPLD and read by master i2c,
     * please using MASTER_I2C_LOCK.
     * by tsihang, 2021-07-18. */
    MAV_QSFP_LOCK;
    hndl = hndl;
    lpmode = lpmode;
    rc = 0;
    MAV_QSFP_UNLOCK;
    return rc;
}

EXPORT int bf_pltfm_get_qsfp_ctx (struct
                                  qsfp_ctx_t
                                  **qsfp_ctx)
{
    *qsfp_ctx = g_qsfp_ctx;
    return 0;
}

EXPORT int bf_pltfm_get_vqsfp_ctx (struct
                                qsfp_ctx_t
                                **qsfp_ctx)
{
    *qsfp_ctx = g_vqsfp_ctx;
    return 0;
}

EXPORT int bf_pltfm_get_max_vqsfp_ports (void)
{
    return max_vqsfp;
}

/* Panel QSFP28. */
EXPORT int bf_pltfm_qsfp_lookup_by_module (
    IN  int module,
    OUT uint32_t *conn_id
)
{
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    if (bf_pltfm_get_qsfp_ctx (&qsfp_ctx)) {
        return -1;
    }

    qsfp = &qsfp_ctx[(module - 1) %
        bf_qsfp_get_max_qsfp_ports()];
    *conn_id = qsfp->conn_id;

    return 0;
}

/* vQSFP: DPU channel. */
EXPORT int bf_pltfm_vqsfp_lookup_by_module (
    IN  int module,
    OUT uint32_t *conn_id
)
{
    int alias_num;
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    if (bf_pltfm_get_vqsfp_ctx (&qsfp_ctx)) {
        return -1;
    }

    for (int i = 0; i < bf_pltfm_get_max_vqsfp_ports(); i ++) {
        qsfp = &qsfp_ctx[i];
        alias_num = atoi (&qsfp->desc[1]);
        if (alias_num == module) {
            // found in vqsfp
            *conn_id = qsfp->conn_id;
            return 0;
        }
    }

    max_vsfp = max_vsfp;
    // not found
    return -1;
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
    if ((module >= 1) &&
        (module <= (unsigned int)bf_qsfp_get_max_qsfp_ports ())) {
        return true;
    }

    return false;
}

EXPORT int bf_pltfm_vqsfp_lookup_by_index (
    IN  int index,
    OUT char *alias,
    OUT uint32_t *conn_id
)
{
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    if (bf_pltfm_get_vqsfp_ctx (&qsfp_ctx)) {
        return -1;
    }

    qsfp = &qsfp_ctx[index %
        bf_pltfm_get_max_vqsfp_ports()];
    *conn_id = qsfp->conn_id;
    strcpy(alias, qsfp->desc);

    return 0;
}

EXPORT int bf_pltfm_qsfp_is_vqsfp (
    IN  uint32_t conn_id
)
{
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    if (bf_pltfm_get_vqsfp_ctx (&qsfp_ctx)) {
        return -1;
    }

    for (int i = 0; i < bf_pltfm_get_max_vqsfp_ports(); i ++) {
        qsfp = &qsfp_ctx[i];
        if (conn_id == qsfp->conn_id) {
            // found in vqsfp
            return 1;
        }
    }

    // not found
    return 0;
}
