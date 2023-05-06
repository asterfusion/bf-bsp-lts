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

static struct qsfp_ctx_t qsfp_ctx_x308p[] = {
    {"C1",   1, (0x76 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (0)}, {BF_MAV_SYSCPLD1, 0x14, BIT (0)}},
    {"C2",   2, (0x76 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (1)}, {BF_MAV_SYSCPLD1, 0x14, BIT (1)}},
    {"C3",   3, (0x76 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (2)}, {BF_MAV_SYSCPLD1, 0x14, BIT (2)}},
    {"C4",   4, (0x76 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (3)}, {BF_MAV_SYSCPLD1, 0x14, BIT (3)}},
    {"C5",   5, (0x76 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (4)}, {BF_MAV_SYSCPLD1, 0x14, BIT (4)}},
    {"C6",   6, (0x76 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (5)}, {BF_MAV_SYSCPLD1, 0x14, BIT (5)}},
    {"C7",   7, (0x76 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (6)}, {BF_MAV_SYSCPLD1, 0x14, BIT (6)}},
    {"C8",   8, (0x76 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x06, BIT (7)}, {BF_MAV_SYSCPLD1, 0x14, BIT (7)}},
};

/* TBD
 * Facing to the panel port, the computing card on the right side is GHCx and the other one is GHCy.
 * And as the vQSFP treated as QSFP by stratum, you can get the map by command bf-sde> qsfp map, or
 * get the dev_port by bf_get_dev_port_by_interface_name.
 * GHCx <- C15/C16, while another 3x 25G are unused.
 * GHCy <- C13/C14, while another 3x 25G are unused.
 * by tsihang, 2022/07/21. */
static struct qsfp_ctx_t vqsfp_ctx_x308p[] = {
    /* GHCy */
    {"C13", 28, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 28*/
    {"C14", 29, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 29*/

    /* GHCx */
    {"C15", 22, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 22*/
    {"C16", 23, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 23*/
};


/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
int bf_pltfm_get_sub_module_pres_x308p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint8_t val = 0;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFF00;
    hndl = hndl;
    uint8_t off = 0x06;
    const uint8_t reg = 0x00;
    uint8_t syscpld = 0;

    syscpld = BF_MAV_SYSCPLD1;

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

    qsfp_pres_l |= val;
    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

end:
    return rc;

}

void bf_pltfm_qsfp_init_x308p (struct qsfp_ctx_t **ctx,
    uint32_t *num,
    struct qsfp_ctx_t **vctx,
    uint32_t *vnum) {
    *ctx = &qsfp_ctx_x308p[0];
    *num = ARRAY_LENGTH(qsfp_ctx_x308p);
    *vctx = &vqsfp_ctx_x308p[0];
    *vnum = ARRAY_LENGTH(vqsfp_ctx_x308p);
}

