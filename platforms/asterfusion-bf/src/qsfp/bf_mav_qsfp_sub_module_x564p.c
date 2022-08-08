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

static struct qsfp_ctx_t qsfp_ctx_x564p[] = {
    {"C1",   1, (0x70 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (0)}},
    {"C2",   2, (0x70 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (1)}},
    {"C3",   3, (0x70 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (2)}},
    {"C4",   4, (0x70 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (3)}},
    {"C5",   5, (0x71 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (0)}, {BF_MAV_SYSCPLD2, 0x10, BIT (0)}},
    {"C6",   6, (0x71 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (1)}, {BF_MAV_SYSCPLD2, 0x10, BIT (1)}},
    {"C7",   7, (0x71 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (2)}, {BF_MAV_SYSCPLD2, 0x10, BIT (2)}},
    {"C8",   8, (0x71 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (3)}, {BF_MAV_SYSCPLD2, 0x10, BIT (3)}},
    {"C9",   9, (0x72 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (0)}, {BF_MAV_SYSCPLD2, 0x11, BIT (0)}},
    {"C10", 10, (0x72 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (1)}, {BF_MAV_SYSCPLD2, 0x11, BIT (1)}},
    {"C11", 11, (0x72 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (2)}, {BF_MAV_SYSCPLD2, 0x11, BIT (2)}},
    {"C12", 12, (0x72 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (3)}, {BF_MAV_SYSCPLD2, 0x11, BIT (3)}},
    {"C13", 13, (0x73 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (0)}, {BF_MAV_SYSCPLD2, 0x12, BIT (0)}},
    {"C14", 14, (0x73 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (1)}, {BF_MAV_SYSCPLD2, 0x12, BIT (1)}},
    {"C15", 15, (0x73 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 0x0e, BIT (1)}, {BF_MAV_SYSCPLD3, 0x0F, BIT (0)}},
    {"C16", 16, (0x73 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 0x0e, BIT (2)}, {BF_MAV_SYSCPLD2, 0x12, BIT (3)}},
    {"C17", 17, (0x75 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (0)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (0)}},
    {"C18", 18, (0x74 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (1)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (1)}},
    {"C19", 19, (0x74 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (2)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (2)}},
    {"C20", 20, (0x74 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (3)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (3)}},
    {"C21", 21, (0x75 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (0)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (0)}},
    {"C22", 22, (0x75 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (1)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (1)}},
    {"C23", 23, (0x75 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (2)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (2)}},
    {"C24", 24, (0x75 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (3)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (3)}},
    {"C25", 25, (0x77 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (0)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (0)}},
    {"C26", 26, (0x77 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (1)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (1)}},
    {"C27", 27, (0x77 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (2)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (2)}},
    {"C28", 28, (0x77 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (3)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (3)}},
    {"C29", 29, (0x76 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (0)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (0)}},
    {"C30", 30, (0x76 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (1)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (1)}},
    {"C31", 31, (0x76 << 1), 2, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (2)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (2)}},
    {"C32", 32, (0x76 << 1), 3, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (3)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (3)}},
    {"C33", 33, (0x70 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (7)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (7)}},
    {"C34", 34, (0x70 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (6)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (6)}},
    {"C35", 35, (0x70 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (5)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (5)}},
    {"C36", 36, (0x70 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (4)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (4)}},
    {"C37", 37, (0x71 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (7)}, {BF_MAV_SYSCPLD2, 0x10, BIT (7)}},
    {"C38", 38, (0x71 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (6)}, {BF_MAV_SYSCPLD2, 0x10, BIT (6)}},
    {"C39", 39, (0x71 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (5)}, {BF_MAV_SYSCPLD2, 0x10, BIT (5)}},
    {"C40", 40, (0x71 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (4)}, {BF_MAV_SYSCPLD2, 0x10, BIT (4)}},
    {"C41", 41, (0x72 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (7)}, {BF_MAV_SYSCPLD2, 0x11, BIT (7)}},
    {"C42", 42, (0x72 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (6)}, {BF_MAV_SYSCPLD2, 0x11, BIT (6)}},
    {"C43", 43, (0x72 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (5)}, {BF_MAV_SYSCPLD2, 0x11, BIT (5)}},
    {"C44", 44, (0x72 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (4)}, {BF_MAV_SYSCPLD2, 0x11, BIT (4)}},
    {"C45", 45, (0x73 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (7)}, {BF_MAV_SYSCPLD2, 0x12, BIT (7)}},
    {"C46", 46, (0x73 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (6)}, {BF_MAV_SYSCPLD2, 0x12, BIT (6)}},
    {"C47", 47, (0x73 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 0x0e, BIT (3)}, {BF_MAV_SYSCPLD3, 0x0F, BIT (1)}},
    {"C48", 48, (0x73 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (4)}, {BF_MAV_SYSCPLD2, 0x12, BIT (4)}},
    {"C49", 49, (0x74 << 1), 7, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (7)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (7)}},
    {"C50", 50, (0x74 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (6)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (6)}},
    {"C51", 51, (0x74 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (5)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (5)}},
    {"C52", 52, (0x74 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 0x02, BIT (4)}, {BF_MAV_SYSCPLD3, 0x0A, BIT (4)}},
    {"C53", 53, (0x74 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (7)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (7)}},
    {"C54", 54, (0x75 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (6)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (6)}},
    {"C55", 55, (0x75 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (5)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (5)}},
    {"C56", 56, (0x75 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 0x03, BIT (4)}, {BF_MAV_SYSCPLD3, 0x0B, BIT (4)}},
    {"C57", 57, (0x77 << 1), 1, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (7)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (7)}},
    {"C58", 58, (0x77 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (6)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (6)}},
    {"C59", 59, (0x77 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (5)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (5)}},
    {"C60", 60, (0x77 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 0x04, BIT (4)}, {BF_MAV_SYSCPLD3, 0x0C, BIT (4)}},
    {"C61", 61, (0x76 << 1), 0, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (7)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (7)}},
    {"C62", 62, (0x76 << 1), 6, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (6)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (6)}},
    {"C63", 63, (0x76 << 1), 5, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (5)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (5)}},
    {"C64", 64, (0x76 << 1), 4, INVALID, {BF_MAV_SYSCPLD3, 0x05, BIT (4)}, {BF_MAV_SYSCPLD3, 0x0D, BIT (4)}},
};


/* Some QSFP and its logic ID in CPLD are alternant.
 * by tsihang, 2021/06/29. */
static int qsfp_pres_map_x564p[] = {

    /* CPLD2 */
    0, 1, 2, 3, 35, 34, 33, 32,
    4, 5, 6, 7, 39, 38, 37, 36,
    8, 9, 10, 11, 43, 42, 41, 40,
    12, 13, INVALIDBIT (2), INVALIDBIT (3), 47, INVALIDBIT (5), 45, 44,

    /* CPLD3 */
    16, 17, 18, 19, 51, 50, 49, 48,
    20, 21, 22, 23, 55, 54, 53, 52,
    24, 25, 26, 27, 59, 58, 57, 56,
    28, 29, 30, 31, 63, 62, 61, 60,

    /* MISC */
    INVALIDBIT (0), 14, 15, 46, INVALIDBIT (4), INVALIDBIT (5), INVALIDBIT (6), INVALIDBIT (7)
};


static inline void bf_pltfm_qsfp_bitmap_x564p (
    uint32_t *maskl, uint32_t *maskh,
    int off, uint8_t pres)
{
    int i;
    maskh = maskh;

    for (i = 0; i < 8; i ++) {
        if (off != 8) {
            if (pres & (1 << i)) {
                continue;
            }
            if (i <= 3) {
                *maskl &= ~ (1 << qsfp_pres_map_x564p[i + 8 *
                                                        off]);
            } else {
                *maskh &= ~ (1 << ((qsfp_pres_map_x564p[i + 8 *
                                                          off]) % 32));
            }
        } else {
            if (pres & (1 << i)) {
                continue;
            }
            if (i == 1) {
                /* C15 */
                *maskl &= ~ (1 << qsfp_pres_map_x564p[i + 8 *
                                                        off]);
            }
            if (i == 2) {
                /* C16 */
                *maskl &= ~ (1 << qsfp_pres_map_x564p[i + 8 *
                                                        off]);
            }
            if (i == 3) {
                /* C47 */
                *maskh &= ~ (1 << ((qsfp_pres_map_x564p[i + 8 *
                                                          off]) % 32));
            }
        }
    }
}


/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_x564p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    uint8_t off;
    uint8_t val[9] = {0};
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    hndl = hndl;
    uint8_t syscpld = 0;
    const uint8_t reg = 0x02;

    syscpld = BF_MAV_SYSCPLD2;

    off = 0;
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
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 0, val[0]);


    off = 1;
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
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 1, val[1]);

    off = 2;
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
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 2, val[2]);


    /* MISC */
    off = 3;
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
    MASKBIT (val[3], 2);
    MASKBIT (val[3], 3);
    MASKBIT (val[3], 5);
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 3, val[3]);


    syscpld = BF_MAV_SYSCPLD3;

    off = 0;
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
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 4, val[4]);


    off = 1;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[5]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 5, val[5]);


    off = 2;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[6]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 6, val[6]);


    off = 3;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val[7]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 7, val[7]);


    /* MISC */
    off = 0x0E;
    rc = bf_pltfm_cpld_read_byte (syscpld, off, &val[8]);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    MASKBIT (val[8], 0);
    MASKBIT (val[8], 4);
    MASKBIT (val[8], 5);
    MASKBIT (val[8], 6);
    MASKBIT (val[8], 7);
    bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                &qsfp_pres_h, 8, val[8]);

end:

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;

}

void bf_pltfm_qsfp_init_x564p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_x564p[0];
    *num = ARRAY_LENGTH(qsfp_ctx_x564p);
    *vctx = NULL;
    *vnum = 0;
}

