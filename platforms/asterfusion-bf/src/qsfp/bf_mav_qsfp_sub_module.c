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
    if(platform_type_equal(X312P)) {\
        MASTER_I2C_LOCK;\
    } else {\
        bf_pltfm_i2c_lock();\
    }
#define MAV_QSFP_UNLOCK \
    if(platform_type_equal(X312P)) {\
        MASTER_I2C_UNLOCK;\
    } else {\
        bf_pltfm_i2c_unlock();\
    }

#define DEFAULT_TIMEOUT_MS 500
#define INVALIDBIT(b) (-1)

/* GHC channel num. */
static int max_vqsfp = 4;
static int max_vsfp = 6;

typedef enum {
    /* The PCA9548 switch selecting for 32 port controllers
    * Note that the address is shifted one to the left to
    * work with the underlying I2C libraries.
    */
    ADDR_SWITCH_32 = 0xe0,
    ADDR_SWITCH_COMM = 0xe8
} mav_mux_pca9548_addr_t;

/* For those platforms which not finished yet, I use this macro as a indicator.
 * Please help me with this fixed.
 * by tsihang, 2021-07-18. */
#define QSFP_UNDEFINED_ST_CTX  \
     {BF_MAV_SYSCPLD3, 0x00, BIT (0)}, {BF_MAV_SYSCPLD3, 0x00, BIT (0)}

/* access is 0 based index. */
static struct qsfp_ctx_t qsfp_ctx_x532p[] = {
    {"C1",   1, (0x70 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x02, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (0)}},
    {"C2",   2, (0x70 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x02, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (1)}},
    {"C3",   3, (0x70 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x02, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (2)}},
    {"C4",   4, (0x70 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x02, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (3)}},
    {"C5",   5, (0x70 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x03, BIT (0)}, {BF_MAV_SYSCPLD1, 0x10, BIT (0)}},
    {"C6",   6, (0x70 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x03, BIT (1)}, {BF_MAV_SYSCPLD1, 0x10, BIT (1)}},
    {"C7",   7, (0x70 << 1), 6, INVALID, {BF_MAV_SYSCPLD1, 0x03, BIT (2)}, {BF_MAV_SYSCPLD1, 0x10, BIT (2)}},
    {"C8",   8, (0x70 << 1), 7, INVALID, {BF_MAV_SYSCPLD1, 0x03, BIT (3)}, {BF_MAV_SYSCPLD1, 0x10, BIT (3)}},
    {"C9",   9, (0x71 << 1), 0, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (0)}, {BF_MAV_SYSCPLD1, 0x11, BIT (0)}},
    {"C10", 10, (0x71 << 1), 1, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (1)}, {BF_MAV_SYSCPLD1, 0x11, BIT (1)}},
    {"C11", 11, (0x71 << 1), 2, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (2)}, {BF_MAV_SYSCPLD1, 0x11, BIT (2)}},
    {"C12", 12, (0x71 << 1), 3, INVALID, {BF_MAV_SYSCPLD1, 0x04, BIT (3)}, {BF_MAV_SYSCPLD1, 0x11, BIT (3)}},
    {"C13", 13, (0x71 << 1), 4, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (0)}, {BF_MAV_SYSCPLD1, 0x12, BIT (0)}},
    {"C14", 14, (0x71 << 1), 5, INVALID, {BF_MAV_SYSCPLD1, 0x05, BIT (1)}, {BF_MAV_SYSCPLD1, 0x12, BIT (1)}},
    {"C15", 15, (0x71 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x0e, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0F, BIT (0)}},
    {"C16", 16, (0x71 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x0e, BIT (2)}, {BF_MAV_SYSCPLD1, 0x12, BIT (3)}},
    {"C17", 17, (0x72 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0A, BIT (0)}},
    {"C18", 18, (0x72 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0A, BIT (1)}},
    {"C19", 19, (0x72 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0A, BIT (2)}},
    {"C20", 20, (0x72 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x02, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0A, BIT (3)}},
    {"C21", 21, (0x72 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0B, BIT (0)}},
    {"C22", 22, (0x72 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0B, BIT (1)}},
    {"C23", 23, (0x72 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0B, BIT (2)}},
    {"C24", 24, (0x72 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x03, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0B, BIT (3)}},
    {"C25", 25, (0x73 << 1), 0, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0C, BIT (0)}},
    {"C26", 26, (0x73 << 1), 1, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0C, BIT (1)}},
    {"C27", 27, (0x73 << 1), 2, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0C, BIT (2)}},
    {"C28", 28, (0x73 << 1), 3, INVALID, {BF_MAV_SYSCPLD2, 0x04, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0C, BIT (3)}},
    {"C29", 29, (0x73 << 1), 4, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0D, BIT (0)}},
    {"C30", 30, (0x73 << 1), 5, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0D, BIT (1)}},
    {"C31", 31, (0x73 << 1), 6, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (2)}, {BF_MAV_SYSCPLD2, 0x0D, BIT (2)}},
    {"C32", 32, (0x73 << 1), 7, INVALID, {BF_MAV_SYSCPLD2, 0x05, BIT (3)}, {BF_MAV_SYSCPLD2, 0x0D, BIT (3)}},
};

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

/* Fixed me */
static struct qsfp_ctx_t vqsfp_ctx_x308p[] = {
    /* vQSFP: C9,C10,C11,C12, treated as QSFP by stratum. */
    {"C9",  24, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 9*/
    {"C10", 22, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 10*/
    {"C11", 26, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 11*/
    {"C12", 14, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 12*/

    /* GHC0 in the future. */
    {"C13", 18, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 18*/
    {"C14", 23, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 23*/

    /* GHC1 in the future. */
    {"C15", 16, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 16*/
    {"C26", 25, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                  /* QSFP 25*/
};

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
    {"C1",   1, 0x73, 2, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfb), {BF_MON_SYSCPLD1_I2C_ADDR, 0x03, BIT (2)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x01, BIT (2)} }, /* QSFP 30*/
    {"C2",   2, 0x73, 3, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xf7), {BF_MON_SYSCPLD1_I2C_ADDR, 0x03, BIT (3)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x01, BIT (3)} }, /* QSFP 32*/
    {"C3",   3, 0x73, 0, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfe), {BF_MON_SYSCPLD1_I2C_ADDR, 0x03, BIT (0)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x01, BIT (0)} }, /* QSFP  6*/
    {"C4",   4, 0x73, 1, (0x0 | (0x74 << 16) | (0x09 << 8) | 0xfd), {BF_MON_SYSCPLD1_I2C_ADDR, 0x03, BIT (1)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x01, BIT (1)} }, /* QSFP  2*/
    {"C5",   5, 0x74, 0, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfe), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (0)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (0)} }, /* QSFP  4*/
    {"C6",   6, 0x74, 1, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfd), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (1)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (1)} }, /* QSFP  5*/
    {"C7",   7, 0x74, 2, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xfb), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (2)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (2)} }, /* QSFP  3*/
    {"C8",   8, 0x74, 3, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xf7), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (3)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (3)} }, /* QSFP  1*/
    {"C9",   9, 0x74, 4, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xef), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (4)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (4)} }, /* QSFP 31*/
    {"C10", 10, 0x74, 5, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xdf), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (5)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (5)} }, /* QSFP 29*/
    {"C11", 11, 0x74, 6, (0x0 | (0x73 << 16) | (0x08 << 8) | 0xbf), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (6)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (6)} }, /* QSFP 27*/
    {"C12", 12, 0x74, 7, (0x0 | (0x73 << 16) | (0x08 << 8) | 0x7f), {BF_MON_SYSCPLD1_I2C_ADDR, 0x02, BIT (7)}, {BF_MON_SYSCPLD1_I2C_ADDR, 0x00, BIT (7)} }, /* QSFP 28*/
};

/* vQSFP: C13,C14,C15,C16, treated as QSFP by stratum. */
static struct qsfp_ctx_t vqsfp_ctx_x312p[] = {
    /* GHC0 */
    {"C13", 24, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 24*/
    {"C14", 22, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 22*/

    /* GHC1 */
    {"C15", 26, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 26*/
    {"C16", 14, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 14*/

    /* GHC0 in the future. */
    {"C17", 18, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 18*/
    {"C18", 23, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 23*/

    /* GHC1 in the future. */
    {"C19", 16, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 16*/
    {"C20", 25, 0x0, 0, 0, {0, 0x0, BIT (0)}, {0, 0x0, BIT (0)}},                                                                                   /* QSFP 25*/

};

/* Global qsfp_ctx specifed by platform.
 * by tsihang, 2021-08-02. */
static struct qsfp_ctx_t *g_qsfp_ctx;
/* GHC channel. */
static struct qsfp_ctx_t *g_vqsfp_ctx;

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

/* Some QSFP and its logic ID in CPLD are alternant.
 * by tsihang, 2021/06/29. */
static int qsfp_pres_map_hc[] = {
    2, 3, 0, 1, 4, 5, 8, 9, 6, 7, 10, 11, 14, 15,
    12, 13, 16, 17, 20, 21, 18, 19, 22, 23
};

/* magic array for X312P */
static int qsfp_pres_map_x312p[] = {
    4, 5, 6, 7, 8, 9, 10, 11, 2, 3, 0, 1
};

/* For those board which has 2 layer PCA9548 to select a port.
 * Not finished yet.
 * by tsihang, 2021-07-15. *
 */
static bool
select_2nd_layer_pca9548 (struct qsfp_ctx_t *qsfp,
                          unsigned char *pca9548, unsigned char *channel)
{
    if (platform_type_equal ( X312P)) {
        uint8_t l1_pca9548_addr = 0x71;
        uint8_t l1_pca9548_chnl = 1;

        uint8_t l2_pca9548_addr = qsfp->pca9548;
        uint8_t l2_pca9548_chnl = qsfp->channel;
        uint8_t l2_pca9548_unconcerned_addr = ((
                qsfp->rev >> 16) & 0x000000FF);
        /* select 1nd pca9548 */
        if (bf_pltfm_cpld_write_byte (l1_pca9548_addr,
                                      0x0, 1 << l1_pca9548_chnl)) {
            return false;
        }
        /* unselect unconcerned 2nd pca9548 */
        if (bf_pltfm_cpld_write_byte (
                l2_pca9548_unconcerned_addr, 0x0, 0x0)) {
            return false;
        }
        /* select 2nd pca9548 */
        if (bf_pltfm_cpld_write_byte (l2_pca9548_addr,
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
    if (platform_type_equal ( X312P)) {
        uint8_t l1_pca9548_addr = 0x71;
        //uint8_t l1_pca9548_chnl = 1;

        uint8_t l2_pca9548_addr = qsfp->pca9548;
        //uint8_t l2_pca9548_chnl = qsfp->channel;
        uint8_t l2_pca9548_unconcerned_addr = ((
                qsfp->rev >> 16) & 0x000000FF);
        /* unselect unconcerned 2nd pca9548 */
        if (bf_pltfm_cpld_write_byte (
                l2_pca9548_unconcerned_addr, 0x0, 0x0)) {
            return false;
        }
        /* unselect 2nd pca9548 */
        if (bf_pltfm_cpld_write_byte (l2_pca9548_addr,
                                      0x0, 0x0)) {
            return false;
        }
        /* unselect 1nd pca9548 */
        if (bf_pltfm_cpld_write_byte (l1_pca9548_addr,
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


/* intialize PCA 9548 and PCA 9535 on the qsfp subsystem i2c bus to
 * ensure a consistent state on i2c addreesed devices
 */
static int qsfp_init_sub_bus (
    bf_pltfm_cp2112_device_ctx_t *hndl)
{
    int i;
    int rc;
    int max_pca9548_cnt = 0;

    if (platform_type_equal (X532P)) {
        max_pca9548_cnt = 4;
    } else if (platform_type_equal (X564P)) {
        max_pca9548_cnt = 8;
    } else if (platform_type_equal (X308P)) {
        max_pca9548_cnt = 7;
    }

    /* Initialize all PCA9548 devices to select channel 0 */
    for (i = 0; i < max_pca9548_cnt; i++) {
        rc = bf_pltfm_cp2112_write_byte (
                 hndl, ADDR_SWITCH_32 + (i * 2), 0,
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
        if (qsfp_init_sub_bus (hndl1)) {
            return -1;
        }
    }
#if 0
    /* For those platform which access qsfp/sfp with 2-way CP2112.
     * Closed due to only one CP2112 on duty to access qsfp/sfp
     * for all X-T platforms.
     * by tsihang, 2021-07-07. */
    if (hndl2 != NULL) {
        if (qsfp_init_sub_bus (hndl2)) {
            return -1;
        }
    }
#endif

    if (platform_type_equal (X532P)) {
        g_vqsfp_ctx = NULL;
        max_vqsfp = 0;
        g_qsfp_ctx = &qsfp_ctx_x532p[0];
        bf_qsfp_set_num (ARRAY_LENGTH (qsfp_ctx_x532p));
    } else if (platform_type_equal (X564P)) {
        g_vqsfp_ctx = NULL;
        max_vqsfp = 0;
        g_qsfp_ctx = &qsfp_ctx_x564p[0];
        bf_qsfp_set_num (ARRAY_LENGTH (qsfp_ctx_x564p));
    } else if (platform_type_equal (X308P)) {
        g_vqsfp_ctx = &vqsfp_ctx_x308p[0];
        max_vqsfp = 4;
        g_qsfp_ctx = &qsfp_ctx_x308p[0];
        bf_qsfp_set_num (ARRAY_LENGTH (qsfp_ctx_x308p));
    } else if (platform_type_equal (X312P)) {
        g_vqsfp_ctx = &vqsfp_ctx_x312p[0];
        //max_vqsfp = ARRAY_LENGTH(vqsfp_ctx_x312p);
        max_vqsfp = 4;
        g_qsfp_ctx = &qsfp_ctx_x312p[0];
        bf_qsfp_set_num (ARRAY_LENGTH (qsfp_ctx_x312p));
    } else if (platform_type_equal (HC)) {
        g_vqsfp_ctx = NULL;
        max_vqsfp = 0;
        g_qsfp_ctx = &qsfp_ctx_hc[0];
        bf_qsfp_set_num (ARRAY_LENGTH (qsfp_ctx_hc));
    }
    BUG_ON (g_qsfp_ctx == NULL);

    return 0;
}

bool is_internal_port_need_autonegotiate (
    uint32_t conn_id, uint32_t chnl_id)
{
    if (platform_type_equal (X564P)) {
        if (conn_id == 65) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (X532P)) {
        if (conn_id == 33) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (X308P)) {
        if (conn_id == 33) {
            if (chnl_id <= 3) {
                return true;
            }
        }
    } else if (platform_type_equal (X312P)) {
        if (conn_id == 33) {
            if ((chnl_id == 2) || (chnl_id == 3)) {
                return true;
            }
        }
    } else if (platform_type_equal (HC)) {
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
    uint8_t chnl = 0;
    uint8_t i2c_addr = 0;

    if (!unselect_1st_layer_pca9548 (sub_port,
                                     &i2c_addr,
                                     &chnl)) {
        return -1;
    }

    if (platform_type_equal (X312P)) {
        /* select QSFP in CPLD1, i2c_addr is offset, chnl is value */
        rc = bf_pltfm_cpld_write_byte (
                 BF_MON_SYSCPLD1_I2C_ADDR, i2c_addr, 0xff);
    } else {
        rc = bf_pltfm_cp2112_write_byte (hndl,
                                         i2c_addr, 0, DEFAULT_TIMEOUT_MS);
    }
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_DEBUG ("QSFP : %2d , PCA9548 : 0x%02x error\n",
                   sub_port, i2c_addr);
        return -2;
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

    if (platform_type_equal (X312P)) {
        /* select QSFP in CPLD1, i2c_addr is offset, chnl is value */
        rc = bf_pltfm_cpld_write_byte (
                 BF_MON_SYSCPLD1_I2C_ADDR, i2c_addr, chnl);
    } else {
        rc = bf_pltfm_cp2112_write_byte (hndl,
                                         i2c_addr, (1 << chnl), DEFAULT_TIMEOUT_MS);
    }
    if (rc != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("QSFP : %2d , PCA9548 : 0x%02x CHNL : 0x%02x\n",
                   sub_port, i2c_addr, chnl);
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
    if (!platform_type_equal (X312P)) {
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
               module);
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

    if (!platform_type_equal (X312P)) {
        out_buf[0] = offset;
        memcpy (out_buf + 1, buf, len);

        rc = bf_pltfm_cp2112_write (
                 hndl, i2c_addr, out_buf, len + 1,
                 DEFAULT_TIMEOUT_MS);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error in qsfp write port <%d>\n",
                       module + 1);
            unselect_qsfp (hndl, module);
            MAV_QSFP_UNLOCK;
            return BF_PLTFM_COMM_FAILED;
        }
    } else {
        rc = bf_pltfm_master_i2c_write_block (
                 i2c_addr >> 1, offset, buf, len);
        if (rc != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error in qsfp write port <%d>\n",
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
    uint8_t cpld_addr;
    struct qsfp_ctx_t *qsfp;
    int usec = 1000;

    qsfp = &g_qsfp_ctx[module];

    if (platform_type_equal (X312P)) {
        cpld_addr = BF_MON_SYSCPLD1_I2C_ADDR;
        if (!reset) {
            /* X312P, there's no need to de-reset! It will auto de-reset after 200ms, so just return */
            fprintf (stdout,
                     "QSFP    %2d : -RST : auto de-reset\n",
                     module + 1);
            LOG_DEBUG (
                "QSFP    %2d : -RST : auto de-reset\n",
                module + 1);
            return 0;
        }
        /* for X312P, 0 means reset, 1 means de-reset, so reverse reset */
        reset = !reset;
    } else {
        cpld_addr = qsfp->rst.cpld_sel;
    }

    MASTER_I2C_LOCK;
    /* Select PCA9548 channel for a given CPLD. */
    if (((rc = select_cpld (qsfp->rst.cpld_sel)) !=
         0)) {
        fprintf (stdout, "0 : write offset : %2d\n",
                 qsfp->rst.cpld_sel);
        goto end;
    }
    bf_sys_usleep (usec);

    rc |= bf_pltfm_cpld_read_byte (cpld_addr,
                                   qsfp->rst.off, &val0);
    if (rc != 0) {
        fprintf (stdout, "1 : read offset : %2d\n",
                 qsfp->rst.off);
        goto end;
    }

    val = val0;

    if (reset) {
        /* reset control bit is active high */
        val |= (1 << (qsfp->rst.off_b));
    } else {
        /* de-reset control bit is active low */
        val &= ~ (1 << (qsfp->rst.off_b));
    }

    /* Write it back */
    rc |= bf_pltfm_cpld_write_byte (cpld_addr,
                                    qsfp->rst.off, val);
    if (rc != 0) {
        fprintf (stdout, "2 : write offset : %2d\n",
                 qsfp->rst.off);
        goto end;
    }
    bf_sys_usleep (usec);

    rc |= bf_pltfm_cpld_read_byte (cpld_addr,
                                   qsfp->rst.off, &val1);
    if (rc != 0) {
        fprintf (stdout, "3 : read offset : %2d\n",
                 qsfp->rst.off);
        goto end;
    }

    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "QSFP    %2d : %sRST : (0x%02x -> 0x%02x)\n",
             (module + 1), original_reset ? "+" : "-", val0,
             val1);
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
static int bf_pltfm_get_sub_module_pres_x312p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint8_t addr;
    uint8_t off;
    uint8_t val = 0xff;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;

    MASTER_I2C_LOCK;

    addr = BF_MON_SYSCPLD1_I2C_ADDR;
    off = X312P_QSFP_PRS_1;
    rc = bf_pltfm_cpld_read_byte (addr, off, &val);
    if (rc) {
        goto end;
    }
    bf_pltfm_qsfp_bitmap_x312p (&qsfp_pres_l,
                                &qsfp_pres_h, off, val);


    off = X312P_QSFP_PRS_2;
    rc = bf_pltfm_cpld_read_byte (addr, off, &val);
    if (rc) {
        goto end;
    }
    val = 0xF0 | val;
    bf_pltfm_qsfp_bitmap_x312p (&qsfp_pres_l,
                                &qsfp_pres_h, off, val);

end:
    MASTER_I2C_UNLOCK;

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;
}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
static int bf_pltfm_get_sub_module_pres_hc (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int off = 0;
    int rc = 0;
    uint8_t val = 0;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;

    MASTER_I2C_LOCK;
    if (((rc = select_cpld (3)) != 0)) {
        goto end;
    } else {
        /* qsfp[7:0] */
        off = 0;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, 0x02 + off, &val);
        if (rc) {
            goto end;
        }
        bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                                 &qsfp_pres_h, off, val);

        /* qsfp[15:8] */
        off = 1;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, 0x02 + off, &val);
        if (rc) {
            goto end;
        }
        bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                                 &qsfp_pres_h, off, val);

        /* qsfp[23:16] */
        off = 2;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, 0x02 + off, &val);
        if (rc) {
            goto end;
        }
        bf_pltfm_qsfp_bitmap_hc (&qsfp_pres_l,
                                 &qsfp_pres_h, off, val);

    }
end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;
}


/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
static int bf_pltfm_get_sub_module_pres_x532p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    int off;
    uint8_t val[5] = {0};
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    hndl = hndl;
    const uint8_t reg = 0x02;

    MASTER_I2C_LOCK;
    if (((rc = select_cpld (1)) != 0)) {
        goto end;
    } else {
        /* MISC */
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD1_I2C_ADDR, 0x05, &val[0]);
        MASKBIT (val[0], 2);
        MASKBIT (val[0], 3);
        MASKBIT (val[0], 4);
        MASKBIT (val[0], 5);
        MASKBIT (val[0], 6);
        MASKBIT (val[0], 7);
        bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                    &qsfp_pres_h, 0, val[0]);
    }

    if (((rc = select_cpld (2)) != 0)) {
        goto end;
    } else {
        off = 0;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[1]);
        bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                    &qsfp_pres_h, 1, val[1]);

        off = 1;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[2]);
        bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                    &qsfp_pres_h, 2, val[2]);

        off = 2;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[3]);
        bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                    &qsfp_pres_h, 3, val[3]);

        off = 3;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[4]);
        MASKBIT (val[4], 6);
        MASKBIT (val[4], 7);
        bf_pltfm_qsfp_bitmap_x532p (&qsfp_pres_l,
                                    &qsfp_pres_h, 4, val[4]);
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;

}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
static int bf_pltfm_get_sub_module_pres_x564p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    int off;
    uint8_t val[9] = {0};
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFFFF;
    hndl = hndl;
    const uint8_t reg = 0x02;

    MASTER_I2C_LOCK;
    if (((rc = select_cpld (2)) != 0)) {
        goto end;
    } else {
        off = 0;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[0]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 0, val[0]);

        off = 1;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[1]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 1, val[1]);

        off = 2;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[2]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 2, val[2]);

        off = 3;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR,
                  reg + off, &val[3]);
        MASKBIT (val[3], 2);
        MASKBIT (val[3], 3);
        MASKBIT (val[3], 5);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 3, val[3]);
    }

    if (((rc = select_cpld (3)) != 0)) {
        goto end;
    } else {
        off = 0;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR,
                  reg + off, &val[4]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 4, val[4]);

        off = 1;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR,
                  reg + off, &val[5]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 5, val[5]);

        off = 2;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR,
                  reg + off, &val[6]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 6, val[6]);

        off = 3;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR,
                  reg + off, &val[7]);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 7, val[7]);

        /* MISC */
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, 0x0E, &val[8]);
        MASKBIT (val[8], 0);
        MASKBIT (val[8], 4);
        MASKBIT (val[8], 5);
        MASKBIT (val[8], 6);
        MASKBIT (val[8], 7);
        bf_pltfm_qsfp_bitmap_x564p (&qsfp_pres_l,
                                    &qsfp_pres_h, 8, val[8]);
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

    return rc;

}

/*
 * get the qsfp present mask status for the group of qsfps that are
 * within a single cp2112 domain
 * bit 0: module present, 1: not-present
 */
static int bf_pltfm_get_sub_module_pres_x308p (
    bf_pltfm_cp2112_device_ctx_t *hndl,
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint8_t val = 0;
    uint32_t qsfp_pres_h = 0xFFFFFFFF;
    uint32_t qsfp_pres_l = 0xFFFFFF00;
    hndl = hndl;

    MASTER_I2C_LOCK;
    /* MISC */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 0x06, &val);

    qsfp_pres_l |= val;

    MASTER_I2C_UNLOCK;

    *pres_l = qsfp_pres_l;
    *pres_h = qsfp_pres_h;

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
    if (platform_type_equal (X532P)) {
        return bf_pltfm_get_sub_module_pres_x532p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (X564P)) {
        return bf_pltfm_get_sub_module_pres_x564p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (X308P)) {
        return bf_pltfm_get_sub_module_pres_x308p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (X312P)) {
        return bf_pltfm_get_sub_module_pres_x312p (hndl,
                pres_l, pres_h);
    } else if (platform_type_equal (HC)) {
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

/* vQSFP: GHC channel. */
EXPORT int bf_pltfm_vqsfp_lookup_by_module (
    IN  int module,
    OUT uint32_t *conn_id
)
{
    int base = 0;
    struct qsfp_ctx_t *qsfp, *qsfp_ctx;
    if (bf_pltfm_get_vqsfp_ctx (&qsfp_ctx)) {
        return -1;
    }

    if (platform_type_equal (X308P)) {
        base = 8;
    } else if (platform_type_equal (X312P)) {
        base = 12;
    } else if (platform_type_equal (HC)) {
        base = 24;
    } else {
        //LOG_ERROR (
        //    "Current platform has no vQSFP %2d, exiting ...", module);
        return -1;
    }

    qsfp = &qsfp_ctx[(module - base - 1) %
        max_vqsfp];
    *conn_id = qsfp->conn_id;

    max_vsfp = max_vsfp;
    return 0;
}

