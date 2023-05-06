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

/* Not finished yet, please help this work.
 * by tsihang, 2021-07-18. */
static struct qsfp_ctx_t qsfp_ctx_hc[] = {
    {"C1", 22, (0x70 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (0)}, {BF_MAV_SYSCPLD3, 0x10, BIT (0)}}, /* QSFP 22 */
    {"C2", 21, (0x70 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (1)}, {BF_MAV_SYSCPLD3, 0x10, BIT (1)}}, /* QSFP 21 */
    {"C3", 24, (0x70 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (2)}, {BF_MAV_SYSCPLD3, 0x10, BIT (2)}}, /* QSFP 24 */
    {"C4", 20, (0x70 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (3)}, {BF_MAV_SYSCPLD3, 0x10, BIT (3)}}, /* QSFP 20 */
    {"C5", 19, (0x70 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (4)}, {BF_MAV_SYSCPLD3, 0x10, BIT (4)}}, /* QSFP 19 */
    {"C6", 23, (0x70 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (5)}, {BF_MAV_SYSCPLD3, 0x10, BIT (5)}}, /* QSFP 23 */
    {"C7", 16, (0x71 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (6)}, {BF_MAV_SYSCPLD3, 0x10, BIT (6)}}, /* QSFP 16 */
    {"C8", 15, (0x71 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 2, BIT (7)}, {BF_MAV_SYSCPLD3, 0x10, BIT (7)}}, /* QSFP 15 */
    {"C9", 18, (0x70 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (0)}, {BF_MAV_SYSCPLD3, 0x11, BIT (0)}}, /* QSFP 18 */
    {"C10", 14, (0x70 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (1)}, {BF_MAV_SYSCPLD3, 0x11, BIT (1)}}, /* QSFP 14 */
    {"C11", 13, (0x71 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (2)}, {BF_MAV_SYSCPLD3, 0x11, BIT (2)}}, /* QSFP 13 */
    {"C12", 17, (0x71 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (3)}, {BF_MAV_SYSCPLD3, 0x11, BIT (3)}}, /* QSFP 17 */
    {"C13", 10, (0x71 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (4)}, {BF_MAV_SYSCPLD3, 0x11, BIT (4)}}, /* QSFP 10 */
    {"C14", 9, (0x71 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (5)}, {BF_MAV_SYSCPLD3, 0x11, BIT (5)}}, /* QSFP  9 */
    {"C15", 12, (0x71 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (6)}, {BF_MAV_SYSCPLD3, 0x11, BIT (6)}}, /* QSFP 12 */
    {"C16", 8, (0x71 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 3, BIT (7)}, {BF_MAV_SYSCPLD3, 0x11, BIT (7)}}, /* QSFP  8 */
    {"C17", 7, (0x72 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (0)}, {BF_MAV_SYSCPLD3, 0x12, BIT (0)}}, /* QSFP  7 */
    {"C18", 11, (0x72 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (1)}, {BF_MAV_SYSCPLD3, 0x12, BIT (1)}}, /* QSFP 11 */
    {"C19", 4, (0x72 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (2)}, {BF_MAV_SYSCPLD3, 0x12, BIT (2)}}, /* QSFP  4 */
    {"C20", 3, (0x72 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (3)}, {BF_MAV_SYSCPLD3, 0x12, BIT (3)}}, /* QSFP  3 */
    {"C21", 6, (0x72 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (4)}, {BF_MAV_SYSCPLD3, 0x12, BIT (4)}}, /* QSFP  6 */
    {"C22", 2, (0x72 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (5)}, {BF_MAV_SYSCPLD3, 0x12, BIT (5)}}, /* QSFP  2 */
    {"C23", 1, (0x72 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (6)}, {BF_MAV_SYSCPLD3, 0x12, BIT (6)}}, /* QSFP  1 */
    {"C24", 5, (0x72 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 4, BIT (7)}, {BF_MAV_SYSCPLD3, 0x12, BIT (7)}}, /* QSFP  5 */
};

/* Some QSFP and its logic ID in CPLD are alternant.
 * by tsihang, 2021/06/29. */
static int qsfp_pres_map_hc[] = {
    2, 3, 0, 1, 4, 5, 8, 9, 6, 7, 10, 11, 14, 15,
    12, 13, 16, 17, 20, 21, 18, 19, 22, 23
};

static inline void bf_pltfm_qsfp_bitmap_hc (
    uint32_t *maskl, uint32_t *maskh,
    int off, uint8_t pres)
{
    int i;
    maskh = maskh;

    for (i = 0; i < 8; i ++) {
        if (pres & (1 << i)) {
            continue;
        }
        *maskl &= ~ (1 << qsfp_pres_map_hc[i + 8 * off]);
    }
}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_hc (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    uint8_t off = 0;
    int rc = 0;
    uint8_t val = 0;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    const uint8_t reg = 0x02;
    uint8_t syscpld = 0;

    syscpld = BF_MAV_SYSCPLD3;

    /* qsfp[7:0] */
    off = 0;
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
    bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                             &qsfp_pres_h, off, val);


    /* qsfp[15:8] */
    off = 1;
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
    bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                             &qsfp_pres_h, off, val);


    /* qsfp[23:16] */
    off = 2;
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
    bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                             &qsfp_pres_h, off, val);

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;
end:
    return rc;
}

void bf_pltfm_qsfp_init_hc36y24c (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_hc[0];
    *num = ARRAY_LENGTH(qsfp_ctx_hc);
    *vctx = NULL;
    *vnum = 0;
}

