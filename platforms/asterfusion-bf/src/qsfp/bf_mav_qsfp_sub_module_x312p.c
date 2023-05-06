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

#define INVALIDBIT(b) (-1)


/* 2021/08/06
 * X312P uses two level PCA9548s to read qsfp eeprom,
 * Address of level1 PCA9548 is always 0x71,
 * For QSFP, channel of level1 PCA9548 is always CH1
 *
 * So, let's define qsfp_ctx_t:
 * qsfp_ctx_t.pca9548 means level2 PCA9548 address
 * qsfp_ctx_t.channel means level2 PCA9548 channel
 * qsfp_ctx_t.rev means l2_pca9548_unconcerned, qsfp_select_addr and qsfp_select_value, the formula is below:
 * rev = 0x0 | (l2_pca9548_unconcerned << 16) | (qsfp_select_addr << 8) | qsfp_select_value
 *
 * Procedure of Select QSFP in X312P:
 * 1. Write level1 PCA9548 to select target channel
 * 2. Write unconcerned level2 PCA9548 to select nothing
 * 3. Write level2 PCA9548 to select target channel
 * 4. Write CPLD1 to Select QSFP finally
 *
 */
static struct qsfp_ctx_t qsfp_ctx_x312p[] = {
    {"C1",   1, 0x73, 2, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfb), {BF_MAV_SYSCPLD1, 0x03, BIT (2)}, {BF_MAV_SYSCPLD1, 0x01, BIT (2)} }, /* QSFP 1*/
    {"C2",   2, 0x73, 3, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xf7), {BF_MAV_SYSCPLD1, 0x03, BIT (3)}, {BF_MAV_SYSCPLD1, 0x01, BIT (3)} }, /* QSFP 2*/
    {"C3",   3, 0x73, 0, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfe), {BF_MAV_SYSCPLD1, 0x03, BIT (0)}, {BF_MAV_SYSCPLD1, 0x01, BIT (0)} }, /* QSFP 3*/
    {"C4",   4, 0x73, 1, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfd), {BF_MAV_SYSCPLD1, 0x03, BIT (1)}, {BF_MAV_SYSCPLD1, 0x01, BIT (1)} }, /* QSFP 4*/
    {"C5",   5, 0x74, 0, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfe), {BF_MAV_SYSCPLD1, 0x02, BIT (0)}, {BF_MAV_SYSCPLD1, 0x00, BIT (0)} }, /* QSFP 5*/
    {"C6",   6, 0x74, 1, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfd), {BF_MAV_SYSCPLD1, 0x02, BIT (1)}, {BF_MAV_SYSCPLD1, 0x00, BIT (1)} }, /* QSFP 6*/
    {"C7",   7, 0x74, 2, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfb), {BF_MAV_SYSCPLD1, 0x02, BIT (2)}, {BF_MAV_SYSCPLD1, 0x00, BIT (2)} }, /* QSFP 7*/
    {"C8",   8, 0x74, 3, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xf7), {BF_MAV_SYSCPLD1, 0x02, BIT (3)}, {BF_MAV_SYSCPLD1, 0x00, BIT (3)} }, /* QSFP 8*/
    {"C9",   9, 0x74, 4, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xef), {BF_MAV_SYSCPLD1, 0x02, BIT (4)}, {BF_MAV_SYSCPLD1, 0x00, BIT (4)} }, /* QSFP 9*/
    {"C10", 10, 0x74, 5, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xdf), {BF_MAV_SYSCPLD1, 0x02, BIT (5)}, {BF_MAV_SYSCPLD1, 0x00, BIT (5)} }, /* QSFP 10*/
    {"C11", 11, 0x74, 6, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xbf), {BF_MAV_SYSCPLD1, 0x02, BIT (6)}, {BF_MAV_SYSCPLD1, 0x00, BIT (6)} }, /* QSFP 11*/
    {"C12", 12, 0x74, 7, (0x0 | (0x73 << 16) | (0x08 << 8) | 0x7f), {BF_MAV_SYSCPLD1, 0x02, BIT (7)}, {BF_MAV_SYSCPLD1, 0x00, BIT (7)} }, /* QSFP 12*/
};

/* TBD
 * Facing to the panel port, the computing card on the right side is GHCx and the other one is GHCy.
 * And as the vQSFP treated as QSFP by stratum, you can get the map by command bf-sde> qsfp map, or
 * get the dev_port by bf_get_dev_port_by_interface_name.
 * GHCx <- C15/C16, while another 3x 25G are unused.
 * GHCy <- C13/C14, while another 3x 25G are unused.
 * by tsihang, 2022/07/21. */
static struct qsfp_ctx_t vqsfp_ctx_x312p[] = {
    /* GHCy */
    {"C13", 27, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},     /* QSFP 31*/
    {"C14", 28, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},     /* QSFP 32*/

    /* GHCx */
    {"C15", 31, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},     /* QSFP 27*/
    {"C16", 32, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},     /* QSFP 28*/
};

/* magic array for X312P */
static int qsfp_pres_map_x312p[] = {
    4, 5, 6, 7, 8, 9, 10, 11, 2, 3, 0, 1
};


static inline void bf_pltfm_qsfp_bitmap_x312p (
    uint32_t *maskl, uint32_t *maskh,
    int off, uint8_t pres)
{
    int i;
    maskh = maskh;
    /* off = 2 or 3 */
    off -= 2;

    for (i = 0; i < 8; i ++) {
        if (pres & (1 << i)) {
            continue;
        }
        *maskl &= ~ (1 << qsfp_pres_map_x312p[i + 8 *
                                                off]);
    }
}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_x312p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint8_t off;
    uint8_t val = 0xff;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    uint8_t syscpld = 0;
    const uint8_t reg = 0x00;

    syscpld = BF_MAV_SYSCPLD1;

    off = X312P_QSFP_PRS_1;
    rc = bf_pltfm_cpld_read_byte (syscpld,
                              reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
         goto end;
    }
    bf_pltfm_qsfp_bitmap_x312p (&qsfp_pres_l,
                                &qsfp_pres_h, off, val);

    off = X312P_QSFP_PRS_2;
    rc = bf_pltfm_cpld_read_byte (syscpld,
                              reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
         goto end;
    }
    val = 0xF0 | val;
    bf_pltfm_qsfp_bitmap_x312p (&qsfp_pres_l,
                                &qsfp_pres_h, off, val);
    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

end:
    return rc;
}

void bf_pltfm_qsfp_init_x312p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_x312p[0];
    *num = ARRAY_LENGTH(qsfp_ctx_x312p);
    *vctx = &vqsfp_ctx_x312p[0];
    *vnum = ARRAY_LENGTH(vqsfp_ctx_x312p);
}

