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

/* access is 0 based index. */
static struct qsfp_ctx_t qsfp_ctx_x532p[] = {
    {"C1",   1, (0x70 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (0)}},
    {"C2",   2, (0x70 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (1)}},
    {"C3",   3, (0x70 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (2)}},
    {"C4",   4, (0x70 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (3)}},
    {"C5",   5, (0x70 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (4)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (4)}},
    {"C6",   6, (0x70 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (5)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (5)}},
    {"C7",   7, (0x70 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (6)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (6)}},
    {"C8",   8, (0x70 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (7)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (7)}},
    {"C9",   9, (0x71 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (0)}, {BF_MAV_SYSCPLD2, 0x10, BIT (0)}},
    {"C10", 10, (0x71 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (1)}, {BF_MAV_SYSCPLD2, 0x10, BIT (1)}},
    {"C11", 11, (0x71 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (2)}, {BF_MAV_SYSCPLD2, 0x10, BIT (2)}},
    {"C12", 12, (0x71 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (3)}, {BF_MAV_SYSCPLD2, 0x10, BIT (3)}},
    {"C13", 13, (0x71 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (4)}, {BF_MAV_SYSCPLD2, 0x10, BIT (4)}},
    {"C14", 14, (0x71 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (5)}, {BF_MAV_SYSCPLD2, 0x10, BIT (5)}},
    {"C15", 15, (0x71 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (6)}, {BF_MAV_SYSCPLD2, 0x10, BIT (6)}},
    {"C16", 16, (0x71 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (7)}, {BF_MAV_SYSCPLD2, 0x10, BIT (7)}},
    {"C17", 17, (0x72 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (0)}, {BF_MAV_SYSCPLD2, 0x11, BIT (0)}},
    {"C18", 18, (0x72 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (1)}, {BF_MAV_SYSCPLD2, 0x11, BIT (1)}},
    {"C19", 19, (0x72 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (2)}, {BF_MAV_SYSCPLD2, 0x11, BIT (2)}},
    {"C20", 20, (0x72 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (3)}, {BF_MAV_SYSCPLD2, 0x11, BIT (3)}},
    {"C21", 21, (0x72 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (4)}, {BF_MAV_SYSCPLD2, 0x11, BIT (4)}},
    {"C22", 22, (0x72 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (5)}, {BF_MAV_SYSCPLD2, 0x11, BIT (5)}},
    {"C23", 23, (0x72 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (6)}, {BF_MAV_SYSCPLD2, 0x11, BIT (6)}},
    {"C24", 24, (0x72 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (7)}, {BF_MAV_SYSCPLD2, 0x11, BIT (7)}},
    {"C25", 25, (0x73 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (0)}, {BF_MAV_SYSCPLD2, 0x12, BIT (0)}},
    {"C26", 26, (0x73 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (1)}, {BF_MAV_SYSCPLD2, 0x12, BIT (1)}},
    {"C27", 27, (0x73 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (2)}, {BF_MAV_SYSCPLD2, 0x12, BIT (2)}},
    {"C28", 28, (0x73 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (3)}, {BF_MAV_SYSCPLD2, 0x12, BIT (3)}},
    {"C29", 29, (0x73 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (4)}, {BF_MAV_SYSCPLD2, 0x12, BIT (4)}},
    {"C30", 30, (0x73 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (5)}, {BF_MAV_SYSCPLD2, 0x12, BIT (5)}},
    {"C31", 31, (0x73 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (0)}, {BF_MAV_SYSCPLD1, 0x02, BIT (0)}},
    {"C32", 32, (0x73 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (1)}, {BF_MAV_SYSCPLD1, 0x02, BIT (1)}},
};

/* Some QSFP and its logic ID in CPLD are alternant.
 * by tsihang, 2021/06/29. */
static int qsfp_pres_map_x532p[] = {

    /* CPLD1 */
    30, 31, INVALIDBIT (2),  INVALIDBIT (3), INVALIDBIT (4), INVALIDBIT (5), INVALIDBIT (6), INVALIDBIT (7),

    /* CPLD2 */
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23,
    24, 25, 26, 27, 28, 29, INVALIDBIT (6), INVALIDBIT (7)
};


static inline void bf_pltfm_qsfp_bitmap_x532p (
    uint32_t *maskl, uint32_t *maskh,
    int off, uint8_t pres)
{
    int i;
    maskh = maskh;

    for (i = 0; i < 8; i ++) {
        if (off != 0) {
            if (pres & (1 << i)) {
                continue;
            }
            *maskl &= ~ (1 << qsfp_pres_map_x532p[i + 8 *
                                                    off]);
        } else {
            if (pres & (1 << i)) {
                continue;
            }
            if (i == 0) {
                /* C31 */
                *maskl &= ~ (1 << qsfp_pres_map_x532p[i + 8 *
                                                        off]);
            }
            if (i == 1) {
                /* C32 */
                *maskl &= ~ (1 << qsfp_pres_map_x532p[i + 8 *
                                                        off]);
            }
        }
    }
}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_x532p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    uint8_t off;
    uint8_t val[5] = {0};
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    hndl = hndl;
    const uint8_t reg = 0x02;
    uint8_t syscpld = 0;


    syscpld = BF_MAV_SYSCPLD1;

    /* MISC */
    off = 0x03;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[0]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    MASKBIT (val[0], 2);
    MASKBIT (val[0], 3);
    MASKBIT (val[0], 4);
    MASKBIT (val[0], 5);
    MASKBIT (val[0], 6);
    MASKBIT (val[0], 7);
    bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                &qsfp_pres_h, 0, val[0]);

    syscpld = BF_MAV_SYSCPLD2;

    off = 0;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[1]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                &qsfp_pres_h, 1, val[1]);


    off = 1;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[2]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                &qsfp_pres_h, 2, val[2]);

    off = 2;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[3]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                &qsfp_pres_h, 3, val[3]);


    /* MISC */
    off = 3;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[4]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    MASKBIT (val[4], 6);
    MASKBIT (val[4], 7);
    bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                &qsfp_pres_h, 4, val[4]);

end:
    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;
}

void bf_pltfm_qsfp_init_x532p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_x532p[0];
    *num = ARRAY_LENGTH(qsfp_ctx_x532p);
    *vctx = NULL;
    *vnum = 0;
}

