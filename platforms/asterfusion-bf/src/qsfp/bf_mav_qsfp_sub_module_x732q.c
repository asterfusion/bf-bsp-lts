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

/* access is 0 based index.
 * Please also take attention in bf_pltfm_onlp_mntr_transceiver if you want to change the desc for every qsfp_ctx_t entry. */
static struct qsfp_ctx_t qsfp_ctx_x732q[] = {
    {"QC1",   1, (0x70 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (0)}, {BF_MAV_SYSCPLD1, 0x08, BIT (0)}},
    {"QC2",   2, (0x70 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (1)}, {BF_MAV_SYSCPLD1, 0x08, BIT (1)}},
    {"QC3",   3, (0x70 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (2)}, {BF_MAV_SYSCPLD1, 0x08, BIT (2)}},
    {"QC4",   4, (0x70 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (3)}, {BF_MAV_SYSCPLD1, 0x08, BIT (3)}},
    {"QC5",   5, (0x70 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (4)}, {BF_MAV_SYSCPLD1, 0x08, BIT (4)}},
    {"QC6",   6, (0x70 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (5)}, {BF_MAV_SYSCPLD1, 0x08, BIT (5)}},
    {"QC7",   7, (0x70 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (6)}, {BF_MAV_SYSCPLD1, 0x08, BIT (6)}},
    {"QC8",   8, (0x70 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (7)}, {BF_MAV_SYSCPLD1, 0x08, BIT (7)}},
    {"QC9",   9, (0x71 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (0)}, {BF_MAV_SYSCPLD1, 0x09, BIT (0)}},
    {"QC10", 10, (0x71 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (1)}, {BF_MAV_SYSCPLD1, 0x09, BIT (1)}},
    {"QC11", 11, (0x71 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (2)}, {BF_MAV_SYSCPLD1, 0x09, BIT (2)}},
    {"QC12", 12, (0x71 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (3)}, {BF_MAV_SYSCPLD1, 0x09, BIT (3)}},
    {"QC13", 13, (0x71 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (4)}, {BF_MAV_SYSCPLD1, 0x09, BIT (4)}},
    {"QC14", 14, (0x71 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (5)}, {BF_MAV_SYSCPLD1, 0x09, BIT (5)}},
    {"QC15", 15, (0x71 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (6)}, {BF_MAV_SYSCPLD1, 0x09, BIT (6)}},
    {"QC16", 16, (0x71 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (7)}, {BF_MAV_SYSCPLD1, 0x09, BIT (7)}},
    {"QC17", 17, (0x72 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (0)}},
    {"QC18", 18, (0x72 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (1)}},
    {"QC19", 19, (0x72 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (2)}},
    {"QC20", 20, (0x72 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (3)}},
    {"QC21", 21, (0x72 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (4)}},
    {"QC22", 22, (0x72 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (5)}},
    {"QC23", 23, (0x72 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (6)}},
    {"QC24", 24, (0x72 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (7)}},
    {"QC25", 25, (0x73 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (0)}, {BF_MAV_SYSCPLD1, 0x10, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (0)}},
    {"QC26", 26, (0x73 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (1)}, {BF_MAV_SYSCPLD1, 0x10, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (1)}},
    {"QC27", 27, (0x73 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (2)}, {BF_MAV_SYSCPLD1, 0x10, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (2)}},
    {"QC28", 28, (0x73 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (3)}, {BF_MAV_SYSCPLD1, 0x10, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (3)}},
    {"QC29", 29, (0x73 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (4)}, {BF_MAV_SYSCPLD1, 0x10, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (4)}},
    {"QC30", 30, (0x73 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (5)}, {BF_MAV_SYSCPLD1, 0x10, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (5)}},
    {"QC31", 31, (0x73 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (6)}, {BF_MAV_SYSCPLD1, 0x10, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (6)}},
    {"QC32", 32, (0x73 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x07, BIT (7)}, {BF_MAV_SYSCPLD1, 0x10, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (7)}},
};

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_x732q (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    uint8_t off;
    uint8_t val;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0x00000000;
    hndl = hndl;
    const uint8_t reg = 0x00;
    uint8_t syscpld = BF_MAV_SYSCPLD1;

    /* MISC */
    /* QC8 - QC1 */
    off = 0x04;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    qsfp_pres_l |= (val << 0);

    /* QC16 - QC9 */
    off = 0x05;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    qsfp_pres_l |= (val << 8);

    /* QC24 - QC17 */
    off = 0x06;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    qsfp_pres_l |= (val << 16);

    /* QC32 - QC25 */
    off = 0x07;
    rc = bf_pltfm_cpld_read_byte (syscpld, reg + off, &val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, syscpld, (reg + off),
            "Failed to read CPLD");
        goto end;
    }
    qsfp_pres_l |= (val << 24);

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;
end:
    return rc;
}

void bf_pltfm_qsfp_init_x732q (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_x732q[0];
    *num = ARRAY_LENGTH(qsfp_ctx_x732q);
    *vctx = NULL;
    *vnum = 0;
}
