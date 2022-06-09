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
#define DEFAULT_TIMEOUT_MS 500

#define ADDR_SFP_I2C         (0x50 << 1)
/* sfp module lock macros */
static bf_sys_mutex_t sfp_page_mutex;
#define MAV_SFP_PAGE_LOCK_INIT bf_sys_mutex_init(&sfp_page_mutex)
#define MAV_SFP_PAGE_LOCK_DEL bf_sys_mutex_del(&sfp_page_mutex)
#define MAV_SFP_PAGE_LOCK bf_sys_mutex_lock(&sfp_page_mutex)
#define MAV_SFP_PAGE_UNLOCK bf_sys_mutex_unlock(&sfp_page_mutex)

/* Keep this the same with MAV_QSFP_LOCK and MAV_QSFP_UNLOCK.
 * by tsihang, 2021-07-15. */
#define MAV_SFP_LOCK bf_pltfm_i2c_lock()
#define MAV_SFP_UNLOCK bf_pltfm_i2c_unlock()

/* access is 1 based index, so zeroth index would be unused */
static int
connID_chnlID_to_sfp_ctx_index[BF_PLAT_MAX_QSFP +
                               1][MAX_CHAN_PER_CONNECTOR]
= {{INVALID}};

/* For those platforms which not finished yet, I use this macro as a indicator.
 * Please help me with this fixed.
 * by tsihang, 2021-07-18. */
#define SFP_UNDEFINED_ST_CTX  \
            {BF_MAV_SYSCPLD3, 0x00, BIT (0)}, {BF_MAV_SYSCPLD3, 0x00, BIT (0)}, {BF_MAV_SYSCPLD3, 0x00, BIT (0)}

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x532p[] = {
    {{BF_MAV_SYSCPLD2, 0x0d, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0c, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0a, BIT (0)}},
    {{BF_MAV_SYSCPLD2, 0x0d, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0c, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0a, BIT (1)}}
};

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x564p[] = {
    {{BF_MAV_SYSCPLD2, 0x0d, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0c, BIT (0)}, {BF_MAV_SYSCPLD2, 0x0a, BIT (0)}},
    {{BF_MAV_SYSCPLD2, 0x0d, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0c, BIT (1)}, {BF_MAV_SYSCPLD2, 0x0a, BIT (1)}}
};

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x308p[] = {
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (0)}, {BF_MAV_SYSCPLD1, 0x13, BIT (0)}, {BF_MAV_SYSCPLD1, 0x11, BIT (0)}},   // Y1
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (1)}, {BF_MAV_SYSCPLD1, 0x13, BIT (1)}, {BF_MAV_SYSCPLD1, 0x11, BIT (1)}},   // Y2
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (2)}, {BF_MAV_SYSCPLD1, 0x13, BIT (2)}, {BF_MAV_SYSCPLD1, 0x11, BIT (2)}},   // Y3
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (3)}, {BF_MAV_SYSCPLD1, 0x13, BIT (3)}, {BF_MAV_SYSCPLD1, 0x11, BIT (3)}},   // Y4
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (4)}, {BF_MAV_SYSCPLD1, 0x13, BIT (4)}, {BF_MAV_SYSCPLD1, 0x11, BIT (4)}},   // Y5
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (5)}, {BF_MAV_SYSCPLD1, 0x13, BIT (5)}, {BF_MAV_SYSCPLD1, 0x11, BIT (5)}},   // Y6
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (6)}, {BF_MAV_SYSCPLD1, 0x13, BIT (6)}, {BF_MAV_SYSCPLD1, 0x11, BIT (6)}},   // Y7
    {{BF_MAV_SYSCPLD1, 0x1A, BIT (7)}, {BF_MAV_SYSCPLD1, 0x13, BIT (7)}, {BF_MAV_SYSCPLD1, 0x11, BIT (7)}},   // Y8

    {{BF_MAV_SYSCPLD1, 0x19, BIT (0)}, {BF_MAV_SYSCPLD1, 0x12, BIT (0)}, {BF_MAV_SYSCPLD1, 0x10, BIT (0)}},   // Y9
    {{BF_MAV_SYSCPLD1, 0x19, BIT (1)}, {BF_MAV_SYSCPLD1, 0x12, BIT (1)}, {BF_MAV_SYSCPLD1, 0x10, BIT (1)}},   // Y10
    {{BF_MAV_SYSCPLD1, 0x19, BIT (2)}, {BF_MAV_SYSCPLD1, 0x12, BIT (2)}, {BF_MAV_SYSCPLD1, 0x10, BIT (2)}},   // Y11
    {{BF_MAV_SYSCPLD1, 0x19, BIT (3)}, {BF_MAV_SYSCPLD1, 0x12, BIT (3)}, {BF_MAV_SYSCPLD1, 0x10, BIT (3)}},   // Y12
    {{BF_MAV_SYSCPLD1, 0x19, BIT (4)}, {BF_MAV_SYSCPLD1, 0x12, BIT (4)}, {BF_MAV_SYSCPLD1, 0x10, BIT (4)}},   // Y13
    {{BF_MAV_SYSCPLD1, 0x19, BIT (5)}, {BF_MAV_SYSCPLD1, 0x12, BIT (5)}, {BF_MAV_SYSCPLD1, 0x10, BIT (5)}},   // Y14
    {{BF_MAV_SYSCPLD1, 0x19, BIT (6)}, {BF_MAV_SYSCPLD1, 0x12, BIT (6)}, {BF_MAV_SYSCPLD1, 0x10, BIT (6)}},   // Y15
    {{BF_MAV_SYSCPLD1, 0x19, BIT (7)}, {BF_MAV_SYSCPLD1, 0x12, BIT (7)}, {BF_MAV_SYSCPLD1, 0x10, BIT (7)}},   // Y16

    {{BF_MAV_SYSCPLD1, 0x18, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (0)}},   // Y17
    {{BF_MAV_SYSCPLD1, 0x18, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (1)}},   // Y18
    {{BF_MAV_SYSCPLD1, 0x18, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (2)}},   // Y19
    {{BF_MAV_SYSCPLD1, 0x18, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (3)}},   // Y20
    {{BF_MAV_SYSCPLD1, 0x18, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (4)}},   // Y21
    {{BF_MAV_SYSCPLD1, 0x18, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (5)}},   // Y22
    {{BF_MAV_SYSCPLD1, 0x18, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (6)}},   // Y23
    {{BF_MAV_SYSCPLD1, 0x18, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0F, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0D, BIT (7)}},   // Y24

    {{BF_MAV_SYSCPLD1, 0x17, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (0)}},   // Y25
    {{BF_MAV_SYSCPLD1, 0x17, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (1)}},   // Y26
    {{BF_MAV_SYSCPLD1, 0x17, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (2)}},   // Y27
    {{BF_MAV_SYSCPLD1, 0x17, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (3)}},   // Y28
    {{BF_MAV_SYSCPLD1, 0x17, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (4)}},   // Y29
    {{BF_MAV_SYSCPLD1, 0x17, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (5)}},   // Y30
    {{BF_MAV_SYSCPLD1, 0x17, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (6)}},   // Y31
    {{BF_MAV_SYSCPLD1, 0x17, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0E, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0C, BIT (7)}},   // Y32

    {{BF_MAV_SYSCPLD1, 0x16, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (0)}, {BF_MAV_SYSCPLD1, 0x09, BIT (0)}},   // Y33
    {{BF_MAV_SYSCPLD1, 0x16, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (1)}, {BF_MAV_SYSCPLD1, 0x09, BIT (1)}},   // Y34
    {{BF_MAV_SYSCPLD1, 0x16, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (2)}, {BF_MAV_SYSCPLD1, 0x09, BIT (2)}},   // Y35
    {{BF_MAV_SYSCPLD1, 0x16, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (3)}, {BF_MAV_SYSCPLD1, 0x09, BIT (3)}},   // Y36
    {{BF_MAV_SYSCPLD1, 0x16, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (4)}, {BF_MAV_SYSCPLD1, 0x09, BIT (4)}},   // Y37
    {{BF_MAV_SYSCPLD1, 0x16, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (5)}, {BF_MAV_SYSCPLD1, 0x09, BIT (5)}},   // Y38
    {{BF_MAV_SYSCPLD1, 0x16, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (6)}, {BF_MAV_SYSCPLD1, 0x09, BIT (6)}},   // Y39
    {{BF_MAV_SYSCPLD1, 0x16, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0B, BIT (7)}, {BF_MAV_SYSCPLD1, 0x09, BIT (7)}},   // Y40

    {{BF_MAV_SYSCPLD1, 0x15, BIT (0)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (0)}, {BF_MAV_SYSCPLD1, 0x08, BIT (0)}},   // Y41
    {{BF_MAV_SYSCPLD1, 0x15, BIT (1)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (1)}, {BF_MAV_SYSCPLD1, 0x08, BIT (1)}},   // Y42
    {{BF_MAV_SYSCPLD1, 0x15, BIT (2)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (2)}, {BF_MAV_SYSCPLD1, 0x08, BIT (2)}},   // Y43
    {{BF_MAV_SYSCPLD1, 0x15, BIT (3)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (3)}, {BF_MAV_SYSCPLD1, 0x08, BIT (3)}},   // Y44
    {{BF_MAV_SYSCPLD1, 0x15, BIT (4)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (4)}, {BF_MAV_SYSCPLD1, 0x08, BIT (4)}},   // Y45
    {{BF_MAV_SYSCPLD1, 0x15, BIT (5)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (5)}, {BF_MAV_SYSCPLD1, 0x08, BIT (5)}},   // Y46
    {{BF_MAV_SYSCPLD1, 0x15, BIT (6)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (6)}, {BF_MAV_SYSCPLD1, 0x08, BIT (6)}},   // Y47
    {{BF_MAV_SYSCPLD1, 0x15, BIT (7)}, {BF_MAV_SYSCPLD1, 0x0A, BIT (7)}, {BF_MAV_SYSCPLD1, 0x08, BIT (7)}},   // Y48
};

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x312p[] = {
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (0)}},   // Y1
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (1)}},   // Y2
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (2)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (2)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (2)}},   // Y3
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (3)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (3)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (3)}},   // Y4
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (4)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (4)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (4)}},   // Y5
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (5)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (5)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (5)}},   // Y6
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (6)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (6)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (6)}},   // Y7
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_01_08, BIT (7)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_01_08, BIT (7)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_01_08, BIT (7)}},   // Y8

    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (0)}},   // Y9
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (1)}},   // Y10
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (2)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (2)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (2)}},   // Y11
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (3)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (3)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (3)}},   // Y12
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (4)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (4)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (4)}},   // Y13
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (5)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (5)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (5)}},   // Y14
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_09_15, BIT (6)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_09_15, BIT (6)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_09_15, BIT (6)}},   // Y15

    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (0)}},   // Y16
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (1)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (1)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (1)}},   // Y17
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (2)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (2)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (2)}},   // Y18
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (3)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (3)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (3)}},   // Y19
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (4)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (4)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (4)}},   // Y20
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (5)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (5)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (5)}},   // Y21
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (6)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (6)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (6)}},   // Y22
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_16_23, BIT (7)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_16_23, BIT (7)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_16_23, BIT (7)}},   // Y23

    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (0)}},   // Y24
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (1)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (1)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (1)}},   // Y25
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (2)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (2)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (2)}},   // Y26
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (3)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (3)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (3)}},   // Y27
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (4)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (4)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (4)}},   // Y28
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (5)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (5)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (5)}},   // Y29
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (6)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (6)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (6)}},   // Y30
    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_24_31, BIT (7)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_24_31, BIT (7)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_24_31, BIT (7)}},   // Y31

    {{BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_ENB_32_32, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_PRS_32_32, BIT (0)}, {BF_MON_SYSCPLD4_I2C_ADDR, X312P_SFP_LOS_32_32, BIT (0)}},   // Y32

    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (0)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (0)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (0)}},   // Y33
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (1)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (1)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (1)}},   // Y34
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (2)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (2)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (2)}},   // Y35
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (3)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (3)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (3)}},   // Y36
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (4)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (4)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (4)}},   // Y37
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (5)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (5)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (5)}},   // Y38
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (6)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (6)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (6)}},   // Y39
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_33_40, BIT (7)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_33_40, BIT (7)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_33_40, BIT (7)}},   // Y40

    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (0)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (0)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (0)}},   // Y41
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (1)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (1)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (1)}},   // Y42
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (2)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (2)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (2)}},   // Y43
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (3)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (3)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (3)}},   // Y44
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (4)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (4)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (4)}},   // Y45
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (5)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (5)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (5)}},   // Y46
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (6)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (6)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (6)}},   // Y47
    {{BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_ENB_41_48, BIT (7)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_PRS_41_48, BIT (7)}, {BF_MON_SYSCPLD5_I2C_ADDR, X312P_SFP_LOS_41_48, BIT (7)}},   // Y48

    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_YM_YN, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_YM_YN, BIT (1)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_YM_YN, BIT (1)}},   // Y49
    {{BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_ENB_YM_YN, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_PRS_YM_YN, BIT (0)}, {BF_MON_SYSCPLD3_I2C_ADDR, X312P_SFP_LOS_YM_YN, BIT (0)}},   // Y50
};

/* access is 0 based index. order by module dont't change it! */
static struct sfp_ctx_st_t sfp_st_hc[] = {
    {{BF_MAV_SYSCPLD2, 17, BIT (6)}, {BF_MAV_SYSCPLD2, 2, BIT (6)}, {BF_MAV_SYSCPLD2,  7, BIT (6)}}, /* 32/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (6)}, {BF_MAV_SYSCPLD2, 3, BIT (6)}, {BF_MAV_SYSCPLD2,  8, BIT (6)}}, /* 32/1 */
    {{BF_MAV_SYSCPLD2, 21, BIT (3)}, {BF_MAV_SYSCPLD2, 6, BIT (3)}, {BF_MAV_SYSCPLD2, 11, BIT (3)}}, /* 33/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (6)}, {BF_MAV_SYSCPLD2, 4, BIT (6)}, {BF_MAV_SYSCPLD2,  9, BIT (6)}}, /* 32/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (6)}, {BF_MAV_SYSCPLD2, 5, BIT (6)}, {BF_MAV_SYSCPLD2, 10, BIT (6)}}, /* 32/3 */
    {{BF_MAV_SYSCPLD2, 21, BIT (2)}, {BF_MAV_SYSCPLD2, 6, BIT (2)}, {BF_MAV_SYSCPLD2, 11, BIT (2)}}, /* 33/2 */
    {{BF_MAV_SYSCPLD2, 17, BIT (7)}, {BF_MAV_SYSCPLD2, 2, BIT (7)}, {BF_MAV_SYSCPLD2,  7, BIT (7)}}, /* 31/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (7)}, {BF_MAV_SYSCPLD2, 3, BIT (7)}, {BF_MAV_SYSCPLD2,  8, BIT (7)}}, /* 31/1 */
    {{BF_MAV_SYSCPLD2, 21, BIT (1)}, {BF_MAV_SYSCPLD2, 6, BIT (1)}, {BF_MAV_SYSCPLD2, 11, BIT (1)}}, /* 33/1 */
    {{BF_MAV_SYSCPLD2, 19, BIT (7)}, {BF_MAV_SYSCPLD2, 4, BIT (7)}, {BF_MAV_SYSCPLD2,  9, BIT (7)}}, /* 31/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (7)}, {BF_MAV_SYSCPLD2, 5, BIT (7)}, {BF_MAV_SYSCPLD2, 10, BIT (7)}}, /* 31/3 */
    {{BF_MAV_SYSCPLD2, 21, BIT (0)}, {BF_MAV_SYSCPLD2, 6, BIT (0)}, {BF_MAV_SYSCPLD2, 11, BIT (0)}}, /* 33/0 */
    {{BF_MAV_SYSCPLD2, 17, BIT (3)}, {BF_MAV_SYSCPLD2, 2, BIT (3)}, {BF_MAV_SYSCPLD2,  7, BIT (3)}}, /* 29/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (3)}, {BF_MAV_SYSCPLD2, 3, BIT (3)}, {BF_MAV_SYSCPLD2,  8, BIT (3)}}, /* 29/1 */
    {{BF_MAV_SYSCPLD2, 20, BIT (5)}, {BF_MAV_SYSCPLD2, 5, BIT (5)}, {BF_MAV_SYSCPLD2, 10, BIT (5)}}, /* 30/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (3)}, {BF_MAV_SYSCPLD2, 4, BIT (3)}, {BF_MAV_SYSCPLD2,  9, BIT (3)}}, /* 29/2 */
    {{BF_MAV_SYSCPLD2, 20, BIT (3)}, {BF_MAV_SYSCPLD2, 5, BIT (3)}, {BF_MAV_SYSCPLD2, 10, BIT (3)}}, /* 29/3 */
    {{BF_MAV_SYSCPLD2, 19, BIT (5)}, {BF_MAV_SYSCPLD2, 4, BIT (5)}, {BF_MAV_SYSCPLD2,  9, BIT (5)}}, /* 30/2 */
    {{BF_MAV_SYSCPLD2, 17, BIT (4)}, {BF_MAV_SYSCPLD2, 2, BIT (4)}, {BF_MAV_SYSCPLD2,  7, BIT (4)}}, /* 28/0 */
    {{BF_MAV_SYSCPLD2, 18, BIT (4)}, {BF_MAV_SYSCPLD2, 3, BIT (4)}, {BF_MAV_SYSCPLD2,  8, BIT (4)}}, /* 28/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (7)}, {BF_MAV_SYSCPLD3, 9, BIT (7)}, {BF_MAV_SYSCPLD3, 11, BIT (7)}}, /* 30/1 */
    {{BF_MAV_SYSCPLD2, 19, BIT (4)}, {BF_MAV_SYSCPLD3, 9, BIT (4)}, {BF_MAV_SYSCPLD3, 11, BIT (4)}}, /* 28/2 */
    {{BF_MAV_SYSCPLD3, 15, BIT (5)}, {BF_MAV_SYSCPLD2, 5, BIT (4)}, {BF_MAV_SYSCPLD2, 10, BIT (4)}}, /* 28/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (6)}, {BF_MAV_SYSCPLD3, 9, BIT (6)}, {BF_MAV_SYSCPLD3, 11, BIT (6)}}, /* 30/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (0)}, {BF_MAV_SYSCPLD3, 8, BIT (0)}, {BF_MAV_SYSCPLD3, 10, BIT (0)}}, /* 26/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (1)}, {BF_MAV_SYSCPLD3, 8, BIT (1)}, {BF_MAV_SYSCPLD3, 10, BIT (1)}}, /* 26/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (3)}, {BF_MAV_SYSCPLD3, 9, BIT (3)}, {BF_MAV_SYSCPLD3, 11, BIT (3)}}, /* 27/3 */
    {{BF_MAV_SYSCPLD3, 14, BIT (2)}, {BF_MAV_SYSCPLD3, 8, BIT (2)}, {BF_MAV_SYSCPLD3, 10, BIT (2)}}, /* 26/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (3)}, {BF_MAV_SYSCPLD3, 8, BIT (3)}, {BF_MAV_SYSCPLD3, 10, BIT (3)}}, /* 26/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (2)}, {BF_MAV_SYSCPLD3, 9, BIT (2)}, {BF_MAV_SYSCPLD3, 11, BIT (2)}}, /* 27/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (4)}, {BF_MAV_SYSCPLD3, 8, BIT (4)}, {BF_MAV_SYSCPLD3, 10, BIT (4)}}, /* 25/0 */
    {{BF_MAV_SYSCPLD3, 14, BIT (5)}, {BF_MAV_SYSCPLD3, 8, BIT (5)}, {BF_MAV_SYSCPLD3, 10, BIT (5)}}, /* 25/1 */
    {{BF_MAV_SYSCPLD3, 15, BIT (1)}, {BF_MAV_SYSCPLD3, 9, BIT (1)}, {BF_MAV_SYSCPLD3, 11, BIT (1)}}, /* 27/1 */
    {{BF_MAV_SYSCPLD3, 14, BIT (6)}, {BF_MAV_SYSCPLD3, 8, BIT (6)}, {BF_MAV_SYSCPLD3, 10, BIT (6)}}, /* 25/2 */
    {{BF_MAV_SYSCPLD3, 14, BIT (7)}, {BF_MAV_SYSCPLD3, 8, BIT (7)}, {BF_MAV_SYSCPLD3, 10, BIT (7)}}, /* 25/3 */
    {{BF_MAV_SYSCPLD3, 15, BIT (0)}, {BF_MAV_SYSCPLD3, 9, BIT (0)}, {BF_MAV_SYSCPLD3, 11, BIT (0)}}, /* 27/0 */
    {{BF_MAV_SYSCPLD2, 25, BIT (0)}, {BF_MAV_SYSCPLD2, 22, BIT (0)}, {BF_MAV_SYSCPLD2, 23, BIT (0)}}, /* 65/0 */
    {{BF_MAV_SYSCPLD2, 25, BIT (1)}, {BF_MAV_SYSCPLD2, 22, BIT (1)}, {BF_MAV_SYSCPLD2, 23, BIT (1)}}, /* 65/1 */
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_hc36y24c[] = {
    {{"Y1",  32, 0, (1 << 0), (0x77 << 1)}, (void **) &sfp_st_hc[0] },
    {{"Y2",  32, 1, (1 << 1), (0x77 << 1)}, (void **) &sfp_st_hc[1] },
    {{"Y3",  33, 3, (1 << 3), (0x76 << 1)}, (void **) &sfp_st_hc[2] },
    {{"Y4",  32, 2, (1 << 2), (0x77 << 1)}, (void **) &sfp_st_hc[3] },
    {{"Y5",  32, 3, (1 << 3), (0x77 << 1)}, (void **) &sfp_st_hc[4] },
    {{"Y6",  33, 2, (1 << 2), (0x76 << 1)}, (void **) &sfp_st_hc[5] },
    {{"Y7",  31, 0, (1 << 4), (0x77 << 1)}, (void **) &sfp_st_hc[6] },
    {{"Y8",  31, 1, (1 << 5), (0x77 << 1)}, (void **) &sfp_st_hc[7] },
    {{"Y9",  33, 1, (1 << 1), (0x76 << 1)}, (void **) &sfp_st_hc[8] },
    {{"Y10", 31, 2, (1 << 6), (0x77 << 1)}, (void **) &sfp_st_hc[9] },
    {{"Y11", 31, 3, (1 << 7), (0x77 << 1)}, (void **) &sfp_st_hc[10] },
    {{"Y12", 33, 0, (1 << 0), (0x76 << 1)}, (void **) &sfp_st_hc[11] },
    {{"Y13", 29, 0, (1 << 4), (0x74 << 1)}, (void **) &sfp_st_hc[12] },
    {{"Y14", 29, 1, (1 << 5), (0x74 << 1)}, (void **) &sfp_st_hc[13] },
    {{"Y15", 30, 3, (1 << 7), (0x75 << 1)}, (void **) &sfp_st_hc[14] },
    {{"Y16", 29, 2, (1 << 6), (0x74 << 1)}, (void **) &sfp_st_hc[15] },
    {{"Y17", 29, 3, (1 << 7), (0x74 << 1)}, (void **) &sfp_st_hc[16] },
    {{"Y18", 30, 2, (1 << 6), (0x75 << 1)}, (void **) &sfp_st_hc[17] },
    {{"Y19", 28, 0, (1 << 0), (0x75 << 1)}, (void **) &sfp_st_hc[18] },
    {{"Y20", 28, 1, (1 << 1), (0x75 << 1)}, (void **) &sfp_st_hc[19] },
    {{"Y21", 30, 1, (1 << 5), (0x75 << 1)}, (void **) &sfp_st_hc[20] },
    {{"Y22", 28, 2, (1 << 2), (0x75 << 1)}, (void **) &sfp_st_hc[21] },
    {{"Y23", 28, 3, (1 << 3), (0x75 << 1)}, (void **) &sfp_st_hc[22] },
    {{"Y24", 30, 0, (1 << 4), (0x75 << 1)}, (void **) &sfp_st_hc[23] },
    {{"Y25", 26, 0, (1 << 0), (0x73 << 1)}, (void **) &sfp_st_hc[24] },
    {{"Y26", 26, 1, (1 << 1), (0x73 << 1)}, (void **) &sfp_st_hc[25] },
    {{"Y27", 27, 3, (1 << 3), (0x74 << 1)}, (void **) &sfp_st_hc[26] },
    {{"Y28", 26, 2, (1 << 2), (0x73 << 1)}, (void **) &sfp_st_hc[27] },
    {{"Y29", 26, 3, (1 << 3), (0x73 << 1)}, (void **) &sfp_st_hc[28] },
    {{"Y30", 27, 2, (1 << 2), (0x74 << 1)}, (void **) &sfp_st_hc[29] },
    {{"Y31", 25, 0, (1 << 4), (0x73 << 1)}, (void **) &sfp_st_hc[30] },
    {{"Y32", 25, 1, (1 << 5), (0x73 << 1)}, (void **) &sfp_st_hc[31] },
    {{"Y33", 27, 1, (1 << 1), (0x74 << 1)}, (void **) &sfp_st_hc[32] },
    {{"Y34", 25, 2, (1 << 6), (0x73 << 1)}, (void **) &sfp_st_hc[33] },
    {{"Y35", 25, 3, (1 << 7), (0x73 << 1)}, (void **) &sfp_st_hc[34] },
    {{"Y36", 27, 0, (1 << 0), (0x74 << 1)}, (void **) &sfp_st_hc[35] },
    {{"Y37", 65, 0, (1 << 6), (0x76 << 1)}, (void **) &sfp_st_hc[36] },
    {{"Y38", 65, 1, (1 << 7), (0x76 << 1)}, (void **) &sfp_st_hc[37] },
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x532p[] = {
    {{"Y1", 33, 0, 0x10, 1}, (void *) &sfp_st_x532p[0]},
    {{"Y2", 33, 1, 0x20, 2}, (void *) &sfp_st_x532p[1]}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x564p[] = {
    {{"Y1", 65, 0, 0x10, 1}, (void *) &sfp_st_x564p[0]},
    {{"Y2", 65, 1, 0x20, 2}, (void *) &sfp_st_x564p[1]}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x308p[] = {
    {{"Y1",   9, 0, 0x70, 0}, (void *) &sfp_st_x308p[0]},
    {{"Y2",   9, 1, 0x70, 1}, (void *) &sfp_st_x308p[1]},
    {{"Y3",   9, 2, 0x70, 2}, (void *) &sfp_st_x308p[2]},
    {{"Y4",   9, 3, 0x70, 3}, (void *) &sfp_st_x308p[3]},
    {{"Y5",  10, 0, 0x70, 4}, (void *) &sfp_st_x308p[4]},
    {{"Y6",  10, 1, 0x70, 5}, (void *) &sfp_st_x308p[5]},
    {{"Y7",  10, 2, 0x70, 6}, (void *) &sfp_st_x308p[6]},
    {{"Y8",  10, 3, 0x70, 7}, (void *) &sfp_st_x308p[7]},

    {{"Y9",  11, 0, 0x71, 0}, (void *) &sfp_st_x308p[8]},
    {{"Y10", 11, 1, 0x71, 1}, (void *) &sfp_st_x308p[9]},
    {{"Y11", 11, 2, 0x71, 2}, (void *) &sfp_st_x308p[10]},
    {{"Y12", 11, 3, 0x71, 3}, (void *) &sfp_st_x308p[11]},
    {{"Y13", 12, 0, 0x71, 4}, (void *) &sfp_st_x308p[12]},
    {{"Y14", 12, 1, 0x71, 5}, (void *) &sfp_st_x308p[13]},
    {{"Y15", 12, 2, 0x71, 6}, (void *) &sfp_st_x308p[14]},
    {{"Y16", 12, 3, 0x71, 7}, (void *) &sfp_st_x308p[15]},

    {{"Y17", 13, 0, 0x72, 0}, (void *) &sfp_st_x308p[16]},
    {{"Y18", 13, 1, 0x72, 1}, (void *) &sfp_st_x308p[17]},
    {{"Y19", 13, 2, 0x72, 2}, (void *) &sfp_st_x308p[18]},
    {{"Y20", 13, 3, 0x72, 3}, (void *) &sfp_st_x308p[19]},
    {{"Y21", 14, 0, 0x72, 4}, (void *) &sfp_st_x308p[20]},
    {{"Y22", 14, 1, 0x72, 5}, (void *) &sfp_st_x308p[21]},
    {{"Y23", 14, 2, 0x72, 6}, (void *) &sfp_st_x308p[22]},
    {{"Y24", 14, 3, 0x72, 7}, (void *) &sfp_st_x308p[23]},

    {{"Y25", 15, 0, 0x73, 0}, (void *) &sfp_st_x308p[24]},
    {{"Y26", 15, 1, 0x73, 1}, (void *) &sfp_st_x308p[25]},
    {{"Y27", 15, 2, 0x73, 2}, (void *) &sfp_st_x308p[26]},
    {{"Y28", 15, 3, 0x73, 3}, (void *) &sfp_st_x308p[27]},
    {{"Y29", 16, 0, 0x73, 4}, (void *) &sfp_st_x308p[28]},
    {{"Y30", 16, 1, 0x73, 5}, (void *) &sfp_st_x308p[29]},
    {{"Y31", 16, 2, 0x73, 6}, (void *) &sfp_st_x308p[30]},
    {{"Y32", 16, 3, 0x73, 7}, (void *) &sfp_st_x308p[31]},

    {{"Y33", 17, 0, 0x74, 0}, (void *) &sfp_st_x308p[32]},
    {{"Y34", 17, 1, 0x74, 1}, (void *) &sfp_st_x308p[33]},
    {{"Y35", 17, 2, 0x74, 2}, (void *) &sfp_st_x308p[34]},
    {{"Y36", 17, 3, 0x74, 3}, (void *) &sfp_st_x308p[35]},
    {{"Y37", 18, 0, 0x74, 4}, (void *) &sfp_st_x308p[36]},
    {{"Y38", 18, 1, 0x74, 5}, (void *) &sfp_st_x308p[37]},
    {{"Y39", 18, 2, 0x74, 6}, (void *) &sfp_st_x308p[38]},
    {{"Y40", 18, 3, 0x74, 7}, (void *) &sfp_st_x308p[39]},

    {{"Y41", 19, 0, 0x75, 0}, (void *) &sfp_st_x308p[40]},
    {{"Y42", 19, 1, 0x75, 1}, (void *) &sfp_st_x308p[41]},
    {{"Y43", 19, 2, 0x75, 2}, (void *) &sfp_st_x308p[42]},
    {{"Y44", 19, 3, 0x75, 3}, (void *) &sfp_st_x308p[43]},
    {{"Y45", 20, 0, 0x75, 4}, (void *) &sfp_st_x308p[44]},
    {{"Y46", 20, 1, 0x75, 5}, (void *) &sfp_st_x308p[45]},
    {{"Y47", 20, 2, 0x75, 6}, (void *) &sfp_st_x308p[46]},
    {{"Y48", 20, 3, 0x75, 7}, (void *) &sfp_st_x308p[47]},
};

/* 2021/08/12
 * X312P uses two level PCA9548s to read qsfp eeprom,
 * l1_pca9548_chnl is always 0x71
 *
 * So, let's define sfp_ctx_t:
 * sfp_ctx_t.i2c_chnl_addr means l1_pca9548_chnl and l2_pca9548_chnl, the formula is below:
 * i2c_chnl_addr = l1_pca9548_chnl << 4 | l2_pca9548_chnl
 *
 * sfp_ctx_t.rev means l2_pca9548_addr and l2_pca9548_unconcerned_addr, the formula is below:
 * rev = 0x0 | (l2_pca9548_addr << 8) | l2_pca9548_unconcerned_addr
 *
 * Procedure of Select SFP in X312P:
 * 1. Write level1 PCA9548 to select target channel
 * 2. Write unconcerned level2 PCA9548 to select nothing
 * 3. Write level2 PCA9548 to select target channel
 *
 */

/* Following panel order and 0 based index */
static struct sfp_ctx_t sfp28_ctx_x312p[] = {
    {{"Y1",  13, 0, (2 << 4) | 0, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[0]},
    {{"Y2",  13, 1, (2 << 4) | 1, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[1]},
    {{"Y3",  15, 0, (2 << 4) | 2, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[2]},
    {{"Y4",  13, 2, (2 << 4) | 3, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[3]},
    {{"Y5",  13, 3, (2 << 4) | 4, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[4]},
    {{"Y6",  15, 1, (2 << 4) | 5, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[5]},
    {{"Y7",  14, 0, (2 << 4) | 6, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[6]},
    {{"Y8",  14, 1, (2 << 4) | 7, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[7]},
    {{"Y9",  15, 2, (2 << 4) | 0, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[8]},
    {{"Y10", 14, 2, (2 << 4) | 1, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[9]},
    {{"Y11", 14, 3, (2 << 4) | 2, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[10]},
    {{"Y12", 15, 3, (2 << 4) | 3, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[11]},

    {{"Y13", 16, 0, (2 << 4) | 4, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[12]},
    {{"Y14", 16, 1, (2 << 4) | 5, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[13]},
    {{"Y15", 18, 0, (2 << 4) | 6, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[14]},
    {{"Y16", 16, 2, (2 << 4) | 7, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[15]},
    {{"Y17", 16, 3, (3 << 4) | 0, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[16]},
    {{"Y18", 18, 1, (3 << 4) | 1, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[17]},
    {{"Y19", 17, 0, (3 << 4) | 2, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[18]},
    {{"Y20", 17, 1, (3 << 4) | 3, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[19]},
    {{"Y21", 18, 2, (3 << 4) | 4, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[20]},
    {{"Y22", 17, 2, (3 << 4) | 5, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[21]},
    {{"Y23", 17, 3, (3 << 4) | 6, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[22]},
    {{"Y24", 18, 3, (3 << 4) | 7, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[23]},

    {{"Y25", 19, 0, (3 << 4) | 0, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[24]},
    {{"Y26", 19, 1, (3 << 4) | 1, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[25]},
    {{"Y27", 21, 0, (3 << 4) | 2, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[26]},
    {{"Y28", 19, 2, (3 << 4) | 3, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[27]},
    {{"Y29", 19, 3, (3 << 4) | 4, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[28]},
    {{"Y30", 21, 1, (3 << 4) | 5, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[29]},
    {{"Y31", 20, 0, (3 << 4) | 6, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[30]},
    {{"Y32", 20, 1, (3 << 4) | 7, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[31]},
    {{"Y33", 21, 2, (4 << 4) | 0, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[32]},
    {{"Y34", 20, 2, (4 << 4) | 1, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[33]},
    {{"Y35", 20, 3, (4 << 4) | 2, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[34]},
    {{"Y36", 21, 3, (4 << 4) | 3, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[35]},

    {{"Y37", 22, 0, (4 << 4) | 4, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[36]},
    {{"Y38", 22, 1, (4 << 4) | 5, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[37]},
    {{"Y39", 24, 0, (4 << 4) | 6, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[38]},
    {{"Y40", 22, 2, (4 << 4) | 7, (X312P_PCA9548_L2_0x72 << 8) | X312P_PCA9548_L2_0x73 }, (void *) &sfp_st_x312p[39]},
    {{"Y41", 22, 3, (4 << 4) | 0, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[40]},
    {{"Y42", 24, 1, (4 << 4) | 1, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[41]},
    {{"Y43", 23, 0, (4 << 4) | 2, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[42]},
    {{"Y44", 23, 1, (4 << 4) | 3, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[43]},
    {{"Y45", 24, 2, (4 << 4) | 4, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[44]},
    {{"Y46", 23, 2, (4 << 4) | 5, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[45]},
    {{"Y47", 23, 3, (4 << 4) | 6, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[46]},
    {{"Y48", 24, 3, (4 << 4) | 7, (X312P_PCA9548_L2_0x73 << 8) | X312P_PCA9548_L2_0x72 }, (void *) &sfp_st_x312p[47]},

    {{"Y49", 33, 0, (5 << 4) | 1, (X312P_PCA9548_L2_0x72 << 8) | 0x0 }, (void *) &sfp_st_x312p[48]},
    {{"Y50", 33, 1, (5 << 4) | 0, (X312P_PCA9548_L2_0x72 << 8) | 0x0 }, (void *) &sfp_st_x312p[49]}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x564p[] = {
    {{"X1", 65, 2, 0, 0}, NULL},
    {{"X2", 65, 3, 0, 0}, NULL}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x532p[] = {
    {{"X1", 33, 2, 0, 0}, NULL},
    {{"X2", 33, 3, 0, 0}, NULL}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x312p[] = {
    {{"X1", 33, 2, 0, 0}, NULL},
    {{"X2", 33, 3, 0, 0}, NULL}
};

/* Following panel order and 0 based index */
static struct sfp_ctx_t xsfp_ctx_x308p[] = {
    {{"X1", 33, 2, 0, 0}, NULL},
    {{"X2", 33, 3, 0, 0}, NULL}
};

struct opt_ctx_t {
    int (*read) (uint32_t module, uint8_t offset,
                 uint8_t len, uint8_t *buf);
    int (*write) (uint32_t module, uint8_t offset,
                  uint8_t len, uint8_t *buf);
    int (*txdis) (uint32_t module, bool disable,
                  void *st);
    int (*losrd) (uint32_t module, bool *los);

    void (*lock)
    ();      /* internal lock to lock shared resources,
                          * such as master i2c to avoid resource competion with other applications. */
    void (*unlock)
    ();    /* internal lock to lock shared resources,
                          * such as master i2c to avoid resource competion with other applications. */
    int (*select) (uint32_t module);
    int (*unselect) (uint32_t module);

    void *sfp_ctx;  /* You can define your own sfp_ctx here. */
};

struct opt_ctx_t *sfp_opt;
static struct sfp_ctx_t *xsfp_ctx;

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

static void do_lock()
{
    /* Master i2c is used for both X564 and X532P to read SFP.
     * As well as access CPLD/BMC. In order to avoid resource
     * competition with such application, here we lock it up
     * before using it.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_LOCK;
}

static void do_unlock()
{
    /* Master i2c is used for both X564 and X532P to read SFP.
     * As well as access CPLD/BMC. In order to avoid resource
     * competition with such application, here we lock it up
     * before using it.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_UNLOCK;
}

static void do_lock_x308p()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_LOCK;
}

static void do_unlock_x308p()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_UNLOCK;
}

static void do_lock_x3()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_LOCK;
}

static void do_unlock_x3()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_UNLOCK;
}

static void do_lock_hc()
{
    /* CP2112 is used for HC to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_LOCK;
}

static void do_unlock_hc()
{
    /* CP2112 is used for HC to read SFP. And there's no
     * resource competition with other application.
     * So, here we make a stub.
     *
     * by tsihang, 2021-07-16.
     */
    MAV_SFP_UNLOCK;
}

static inline void sfp_fetch_ctx (void **sfp_ctx)
{
    *sfp_ctx = sfp_opt->sfp_ctx;
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

static bool sfp_lookup1 (IN uint32_t
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

static int sfp_select_x3 (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;
    uint8_t l1_ch;
    uint8_t l2_ch;
    uint8_t l1_addr;
    uint8_t l2_addr;
    uint8_t l2_addr_unconcerned;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }
        // get address from sfp28_ctx_x312p
        l1_ch = (sfp->info.i2c_chnl_addr & 0xF0) >> 4;
        l2_ch = (sfp->info.i2c_chnl_addr & 0x0F);
        l1_addr = X312P_PCA9548_L1_0X71;
        l2_addr = (sfp->info.rev >> 8) & 0xFF;
        l2_addr_unconcerned = sfp->info.rev & 0xFF;

        if (platform_subtype_equal (v1dot0) ||
            platform_subtype_equal (v1dot1)) {

            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -1;
                break;
            }

            // select L1
            if (bf_pltfm_cp2112_write_byte (hndl,
                                            l1_addr << 1,  1 << l1_ch,
                                            DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
            // unselect L2_unconcerned
            if (l2_addr_unconcerned) {
                if (bf_pltfm_cp2112_write_byte (hndl,
                                                l2_addr_unconcerned << 1,  0,
                                                DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -4;
                    break;
                }
            }
            // select L2
            if (bf_pltfm_cp2112_write_byte (hndl,
                                            l2_addr << 1,  1 << l2_ch,
                                            DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -5;
                break;
            }
        } else { 
            // select L1
            if (bf_pltfm_master_i2c_write_byte (l1_addr, 0x0,
                                                1 << l1_ch)) {
                rc = -3;
                break;
            }
            // unselect L2_unconcerned
            if (l2_addr_unconcerned) {
                if (bf_pltfm_master_i2c_write_byte (
                        l2_addr_unconcerned, 0x0, 0x0)) {
                    rc = -4;
                    break;
                }
            }
            // select L2
            if (bf_pltfm_master_i2c_write_byte (l2_addr, 0x0,
                                                1 << l2_ch)) {
                rc = -5;
                break;
            }
        }
    } while (0);

    // return
    return rc;
}

static int sfp_unselect_x3 (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;
    uint8_t l1_addr;
    uint8_t l2_addr;
    uint8_t l2_addr_unconcerned;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }
        // get address from sfp28_ctx_x312p
        l1_addr = X312P_PCA9548_L1_0X71;
        l2_addr = (sfp->info.rev >> 8) & 0xFF;
        l2_addr_unconcerned = sfp->info.rev & 0xFF;

        if (platform_subtype_equal (v1dot0) ||
            platform_subtype_equal (v1dot1)) {

            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -1;
                break;
            }
            // unselect L2
            if (bf_pltfm_cp2112_write_byte (hndl,
                                            l2_addr << 1,  0,
                                            DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
            // unselect L2_unconcerned
            if (l2_addr_unconcerned) {
                if (bf_pltfm_cp2112_write_byte (hndl,
                                                l2_addr_unconcerned << 1,  0,
                                                DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -4;
                    break;
                }
            }
            // unselect L1
            if (bf_pltfm_cp2112_write_byte (hndl,
                                            l1_addr << 1,  0,
                                            DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -5;
                break;
            }
        }else {
            // unselect L2
            if (bf_pltfm_master_i2c_write_byte (l2_addr, 0x0,
                                                0x0)) {
                rc = -3;
                break;
            }
            // unselect L2_unconcerned
            if (l2_addr_unconcerned) {
                if (bf_pltfm_master_i2c_write_byte (
                        l2_addr_unconcerned, 0x0, 0x0)) {
                    rc = -4;
                    break;
                }
            }
            // unselect L1
            if (bf_pltfm_master_i2c_write_byte (l1_addr, 0x0,
                                                0x0)) {
                rc = -5;
                break;
            }
        }
    } while (0);

    // return
    return rc;
}


static int sfp_select_hc (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }
        // switch channel
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        (uint8_t)sfp->info.rev,  sfp->info.i2c_chnl_addr,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int sfp_unselect_hc (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }
        // switch channel
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        (uint8_t)sfp->info.rev,  0,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int
sfp_select_x5 (uint32_t module)
{
    int rc = -1;
    struct sfp_ctx_t *sfp = NULL;

    if (sfp_lookup1 (module, &sfp)) {
        rc = bf_pltfm_master_i2c_write_byte (
                 BF_MAV_MASTER_PCA9548_ADDR, 0x00,
                 sfp->info.i2c_chnl_addr);
        if (rc) {
            LOG_ERROR (
                "%s[%d], "
                "i2c_write(%02d : %s)"
                "\n",
                __FILE__, __LINE__, module,
                "Failed to write PCA9548");
        }
    }

    /* Avoid compile error. */
    (void)sfp;
    return rc;
}

static int
sfp_unselect_x5 (uint32_t module)
{
    int rc = -1;

    rc = bf_pltfm_master_i2c_write_byte (
             BF_MAV_MASTER_PCA9548_ADDR, 0x00,
             0x00);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "i2c_write(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to write PCA9548");
    }

    /* Avoid compile error. */
    (void) (module);
    return rc;
}

/* An example, not finished yet.
 * Please help me fix it.
 * by tsihang, 2021-07-18. */
static int
sfp_read_sub_module_x5 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = 0;
    uint8_t addr;
    // judge addr
    if (offset < MAX_SFP_PAGE_SIZE) {
        addr = 0xa0;
    } else {
        addr = 0xa2;
        offset -= MAX_SFP_PAGE_SIZE;
    }

    err = bf_pltfm_master_i2c_read_block (
              addr >> 1, offset, buf, len);

    if (0 && !err) {
        fprintf (stdout, "\n%02x:\n", module);
        hex_dump (buf, len);
    }

    return err;
}

/* An example, not finished yet.
 * Please help me fix it.
 * by tsihang, 2021-07-18. */
static int
sfp_write_sub_module_x5 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = -1;
    uint8_t addr;
    // judge addr
    if (offset < MAX_SFP_PAGE_SIZE) {
        addr = 0xa0;
    } else {
        addr = 0xa2;
        offset -= MAX_SFP_PAGE_SIZE;
    }
    err = bf_pltfm_master_i2c_write_block (
              addr >> 1, offset, buf, len);

    if (0 && !err) {
        fprintf (stdout, "\n%02x:\n", module);
        hex_dump (buf, len);
    }

    return err;
}

static int
sfp_get_module_pres_x5 (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc;
    uint8_t val;
    uint32_t sfp_pres_mask = 0xFFFFFFFF;
    pres_h = pres_h;

    MASTER_I2C_LOCK;
    if (((rc = select_cpld (2)) != 0)) {
        goto end;
    } else {
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, 0x0C, &val);
        MASKBIT (val, 2);
        MASKBIT (val, 3);
        MASKBIT (val, 4);
        MASKBIT (val, 5);
        MASKBIT (val, 6);
        MASKBIT (val, 7);
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;

    if (rc == 0) {
        sfp_pres_mask = ((0xFFFFFF << 8) | val);
    }

    *pres_l = sfp_pres_mask;
    return rc;
}

static int
sfp_module_tx_disable_x5 (
    uint32_t module, bool disable, void *st)
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    /* Convert st to your own st. */
    struct st_ctx_t *dis = & ((struct sfp_ctx_st_t *)
                              st)->tx_dis;

    /* Select PCA9548 channel for a given CPLD. */
    if (((rc = select_cpld (dis->cpld_sel)) != 0)) {
        LOG_ERROR (
            "%s[%d], "
            "select_cpld(%02d : %2d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->cpld_sel,
            "Failed to select CPLD");
        goto end;
    }

    /* read current value from offset of Tx disable control register  */
    rc |= bf_pltfm_master_i2c_read_byte (0x40,
                                         dis->off, &val0);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    val = val0;
    /* dis->off_b is a bit mask in dis->off. */
    if (disable) {
        /* disable Tx control bit is active high */
        val |= (1 << dis->off_b);
    } else {
        /* enable Tx control bit is active low */
        val &= ~ (1 << dis->off_b);
    }

    /* should we still write even though the value is the same in cpld ? */
    /* TODO */

    /* write a new value back to update Tx disable control register. */
    rc |= bf_pltfm_master_i2c_write_byte (0x40,
                                          dis->off, val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "write_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to write CPLD");
        goto end;
    }

    /* read current value from offset of Tx disable control register  */
    rc |= bf_pltfm_master_i2c_read_byte (0x40,
                                         dis->off, &val1);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    fprintf (stdout,
             " SFP: %2d CH: 0x%02x  V: (0x%02x -> 0x%02x) "
             " %sTx\n",
             module,
             dis->cpld_sel, val0,
             val1, disable ? "-" : "+");

end:
    unselect_cpld();
    return rc;
}

static int
sfp_module_los_read_x5 (uint32_t
                        module, bool *los)
{
    // define
    int rc = 1;
    struct sfp_ctx_t *sfp_ctx;
    struct sfp_ctx_t *sfp;
    struct sfp_ctx_st_t *sft_st;
    struct st_ctx_t *rx_los;
    uint8_t val;
    // array check
    if (module > (uint32_t)
        bf_sfp_get_max_sfp_ports()) {
        return rc;
    }
    sfp_ctx = (struct sfp_ctx_t *)sfp_opt->sfp_ctx;
    sfp = &sfp_ctx[module - 1];
    sft_st = sfp->st;
    rx_los = & (sft_st->rx_los);

    // lock and set dis
    MASTER_I2C_LOCK;
    if ((rc = select_cpld (rx_los->cpld_sel)) != 0) {
        goto end;
    } else {
        // read value
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD_I2C_ADDR, rx_los->off, &val);

        if (rc) {
            rc = 1;
            goto end;
        }

        *los = (val & (1 << rx_los->off_b)) == 0 ? false :
               true;
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
}

static int sfp_select_x308p (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }

        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }

        // select PCA9548
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        sfp->info.i2c_chnl_addr << 1,  1 << sfp->info.rev,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int sfp_unselect_x308p (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    do {
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }

        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -2;
            break;
        }

        // unselect PCA9548
        if (bf_pltfm_cp2112_write_byte (hndl,
                                        sfp->info.i2c_chnl_addr << 1,  0,
                                        DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -3;
            break;
        }
    } while (0);

    // return
    return rc;
}

static int
sfp_read_sub_module_x308p (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        if (bf_pltfm_cp2112_write_byte (hndl, addr,
                                        offset, DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -2;
            break;
        }
        if (len > 128) {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, 128,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
            buf += 128;
            len -= 128;
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -4;
                break;
            }
        } else {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -5;
                break;
            }
        }
    } while (0);
    return rc;
}

static int
sfp_write_sub_module_x308p (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }

        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        // write module, CP2112 write bytes should not longer than 61, but we limit it to 60 because we use one byte to mark offset
        uint8_t tmp[61];
        while (len > 60) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * 60);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp, 61,
                                       DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -2;
                break;
            }
            buf += 60;
            offset += 60;
            len -= 60;
        }
        if (len > 0) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * len);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp,
                                       len + 1, DEFAULT_TIMEOUT_MS) !=
                BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
        }
    } while (0);
    return rc;
}

static int
sfp_get_module_pres_x308p (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint32_t sfp_01_32 = 0xFFFFFFFF;
    uint32_t sfp_33_48 = 0xFFFFFFFF;

    uint8_t sfp_01_08 = 0xFF;
    uint8_t sfp_09_16 = 0xFF;
    uint8_t sfp_17_24 = 0xFF;
    uint8_t sfp_25_32 = 0xFF;
    uint8_t sfp_33_40 = 0xFF;
    uint8_t sfp_41_48 = 0xFF;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 19, &sfp_01_08);
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 18, &sfp_09_16);
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 15, &sfp_17_24);
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, &sfp_25_32);
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 11, &sfp_33_40);
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 10, &sfp_41_48);

    MASTER_I2C_UNLOCK;

    if (rc == 0) {
        sfp_01_32 = sfp_01_08 + (sfp_09_16 << 8) + (sfp_17_24 << 16) + (sfp_25_32 << 24);
        sfp_33_48 = sfp_33_40 + (sfp_41_48 << 8) + 0xFFFF0000;
    }

    *pres_l = sfp_01_32;
    *pres_h = sfp_33_48;
    return rc;
}

static int
sfp_module_tx_disable_x308p (
    uint32_t module, bool disable, void *st)
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    /* Convert st to your own st. */
    struct st_ctx_t *dis = & ((struct sfp_ctx_st_t *)
                              st)->tx_dis;

    /* read current value from offset of Tx disable control register  */
    rc |= bf_pltfm_master_i2c_read_byte (0x40,
                                         dis->off, &val0);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    val = val0;
    /* dis->off_b is a bit mask in dis->off. */
    if (disable) {
        /* disable Tx control bit is active high */
        val |= (1 << dis->off_b);
    } else {
        /* enable Tx control bit is active low */
        val &= ~ (1 << dis->off_b);
    }

    /* should we still write even though the value is the same in cpld ? */
    /* TODO */

    /* write a new value back to update Tx disable control register. */
    rc |= bf_pltfm_master_i2c_write_byte (0x40,
                                          dis->off, val);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "write_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to write CPLD");
        goto end;
    }

    /* read current value from offset of Tx disable control register  */
    rc |= bf_pltfm_master_i2c_read_byte (0x40,
                                         dis->off, &val1);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    fprintf (stdout,
             " SFP: %2d CH: 0x%02x  V: (0x%02x -> 0x%02x) "
             " %sTx\n",
             module,
             dis->cpld_sel, val0,
             val1, disable ? "-" : "+");

end:
    return rc;
}

static int
sfp_module_los_read_x308p (uint32_t
                        module, bool *los)
{
    // define
    int rc = 1;
    struct sfp_ctx_t *sfp_ctx;
    struct sfp_ctx_t *sfp;
    struct sfp_ctx_st_t *sft_st;
    struct st_ctx_t *rx_los;
    uint8_t val;
    // array check
    if (module > (uint32_t)
        bf_sfp_get_max_sfp_ports()) {
        return rc;
    }
    sfp_ctx = (struct sfp_ctx_t *)sfp_opt->sfp_ctx;
    sfp = &sfp_ctx[module - 1];
    sft_st = sfp->st;
    rx_los = & (sft_st->rx_los);

    // lock and set dis
    MASTER_I2C_LOCK;
    
    // read value
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD_I2C_ADDR, rx_los->off, &val);

    if (rc) {
        rc = 1;
        goto end;
    }

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
}

static int
sfp_read_sub_module_x3 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }
        if (platform_subtype_equal (v1dot0) ||
            platform_subtype_equal (v1dot1)) {
            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -1;
                break;
            }
            if (bf_pltfm_cp2112_write_byte (hndl, addr,
                                            offset, DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -2;
                break;
            }
            if (len > 128) {
                if (bf_pltfm_cp2112_read (hndl, addr, buf, 128,
                                          DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -3;
                    break;
                }
                buf += 128;
                len -= 128;
                if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                          DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -4;
                    break;
                }
            } else {
                if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                          DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -5;
                    break;
                }
            }
        }else {
            addr = addr >> 1;
            if (bf_pltfm_master_i2c_read_block (addr, offset,
                                                buf, len)) {
                rc = -1;
                break;
            }
        }
    } while (0);
    return rc;
}

static int
sfp_write_sub_module_x3 (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }

        if (platform_subtype_equal (v1dot0) ||
           platform_subtype_equal (v1dot1)) {
            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -1;
                break;
            }
            // write module, CP2112 write bytes should not longer than 61, but we limit it to 60 because we use one byte to mark offset
            uint8_t tmp[61];
            while (len > 60) {
                tmp[0] = offset;
                memcpy (tmp + 1, buf, sizeof (uint8_t) * 60);
                if (bf_pltfm_cp2112_write (hndl, addr, tmp, 61,
                                           DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                    rc = -2;
                    break;
                }
                buf += 60;
                offset += 60;
                len -= 60;
            }
            if (len > 0) {
                tmp[0] = offset;
                memcpy (tmp + 1, buf, sizeof (uint8_t) * len);
                if (bf_pltfm_cp2112_write (hndl, addr, tmp,
                                           len + 1, DEFAULT_TIMEOUT_MS) !=
                    BF_PLTFM_SUCCESS) {
                    rc = -3;
                    break;
                }
            }
        }else {
            addr = addr >> 1;
            if (bf_pltfm_master_i2c_write_block (addr, offset,
                                                 buf, len)) {
                rc = -1;
                break;
            }
        }
    } while (0);
    return rc;
}

/* For X312P and X308P, there're 50 sfp(s) so use 2 mask
 * fetching their present.
 * by tsihang, 2021-07-13. */
static int
sfp_get_module_pres_x3 (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint32_t sfp_01_32 = 0xFFFFFFFF;
    uint32_t sfp_33_64 = 0xFFFFFFFF;
    uint8_t cpld;

    uint8_t sfp_01_08 = 0xFF;
    uint8_t sfp_09_15 = 0xFF;
    uint8_t zsfp_pres = 0xFF;
    uint8_t sfp_16_23 = 0xFF;
    uint8_t sfp_24_31 = 0xFF;
    uint8_t sfp_32_32 = 0xFF;
    uint8_t sfp_33_40 = 0xFF;
    uint8_t sfp_41_48 = 0xFF;

    uint8_t sfp_09_16 = 0xff;
    uint8_t sfp_17_24 = 0xff;
    uint8_t sfp_25_32 = 0xff;

    cpld = BF_MON_SYSCPLD3_I2C_ADDR;
    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_01_08, &sfp_01_08);

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_09_15, &sfp_09_15);
    sfp_09_15 |= 0x80;

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_YM_YN, &zsfp_pres);
    zsfp_pres |= 0xFC;
    switch (zsfp_pres) {
        case 0xFC:
        case 0xFF:
            break;
        case 0xFE:
            zsfp_pres = 0xFD;
            break;
        case 0xFD:
            zsfp_pres = 0xFE;
            break;
        default:
            break;
    }

    cpld = BF_MON_SYSCPLD4_I2C_ADDR;
    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_16_23, &sfp_16_23);

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_24_31, &sfp_24_31);

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_32_32, &sfp_32_32);
    sfp_32_32 |= 0xFE;

    cpld = BF_MON_SYSCPLD5_I2C_ADDR;

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_33_40, &sfp_33_40);

    rc += bf_pltfm_cpld_read_byte (cpld,
                                   X312P_SFP_PRS_41_48, &sfp_41_48);

    if (rc == 0) {
        sfp_09_16 &= sfp_09_15;
        sfp_09_16 &= ((sfp_16_23 & 0x01) ? 0xFF : 0x7F);
        sfp_17_24 &= ((sfp_16_23 >> 1) | 0x80);
        sfp_17_24 &= ((sfp_24_31 & 0x01) ? 0xFF : 0x7F);
        sfp_25_32 &= ((sfp_24_31 >> 1) | 0x80);
        sfp_25_32 &= ((sfp_32_32 & 0x01) ? 0xFF : 0x7F);
        sfp_01_32 = (sfp_01_08) | (sfp_09_16 << 8) |
                    (sfp_17_24 << 16) | (sfp_25_32 << 24);
        sfp_33_64 = (sfp_33_40) | (sfp_41_48 << 8) |
                    (zsfp_pres << 16) | (0xFF << 24);
    }

#if 0
    LOG_ERROR ("sfp_01_08 is %x \n", sfp_01_08);
    LOG_ERROR ("sfp_09_15 is %x \n", sfp_09_15);
    LOG_ERROR ("zsfp_pres is %x \n", zsfp_pres);
    LOG_ERROR ("sfp_16_23 is %x \n", sfp_16_23);
    LOG_ERROR ("sfp_24_31 is %x \n", sfp_24_31);
    LOG_ERROR ("sfp_32_32 is %x \n", sfp_32_32);
    LOG_ERROR ("sfp_33_40 is %x \n", sfp_33_40);
    LOG_ERROR ("sfp_41_48 is %x \n", sfp_41_48);

    LOG_ERROR ("sfp_09_16 is %x \n", sfp_09_16);
    LOG_ERROR ("sfp_17_24 is %x \n", sfp_17_24);
    LOG_ERROR ("sfp_25_32 is %x \n", sfp_25_32);
#endif
    *pres_l = sfp_01_32;
    *pres_h = sfp_33_64;

    return rc;
}

static int
sfp_module_tx_disable_x3 (
    uint32_t module, bool disable, void *st)
{
    // define
    int rc = 0;
    struct st_ctx_t *tx_dis;
    uint8_t val;
    // array check
    if (module > ARRAY_LENGTH (sfp_st_x312p)) {
        return -1;
    }
    tx_dis = &sfp_st_x312p[module - 1].tx_dis;

    // lock and set dis
    MASTER_I2C_LOCK;
    // read origin value
    rc |= bf_pltfm_cpld_read_byte (tx_dis->cpld_sel,
                                   tx_dis->off, &val);

    // set value
    if (disable) {
        val |= (1 << tx_dis->off_b);
    } else {
        val &= (~ (1 << tx_dis->off_b));
    }

    // write back
    rc |= bf_pltfm_cpld_write_byte (tx_dis->cpld_sel,
                                    tx_dis->off, val);

    if (rc) {
        goto end;
    }

end:
    MASTER_I2C_UNLOCK;
    return rc;
}

static int
sfp_module_los_read_x3 (
    uint32_t module, bool *los)
{
    // define
    int rc = 1;
    struct st_ctx_t *rx_los;
    uint8_t val;
    // array check
    if (module > ARRAY_LENGTH (sfp_st_x312p)) {
        return rc;
    }
    rx_los = &sfp_st_x312p[module - 1].rx_los;

    // lock and set dis
    MASTER_I2C_LOCK;
    // read value
    rc |= bf_pltfm_cpld_read_byte (rx_los->cpld_sel,
                                   rx_los->off, &val);

    if (rc) {
        rc = 1;
        goto end;
    }

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;

end:
    MASTER_I2C_UNLOCK;
    return rc;
}

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
static int
sfp_read_sub_module_hc (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        if (bf_pltfm_cp2112_write_byte (hndl, addr,
                                        offset, DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
            rc = -2;
            break;
        }
        if (len > 128) {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, 128,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
            buf += 128;
            len -= 128;
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -4;
                break;
            }
        } else {
            if (bf_pltfm_cp2112_read (hndl, addr, buf, len,
                                      DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -5;
                break;
            }
        }
    } while (0);
    return rc;
}

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
static int
sfp_write_sub_module_hc (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int rc = 0;
    uint8_t addr;
    uint8_t tmp[61];
    do {
        // judge addr
        if (offset < MAX_SFP_PAGE_SIZE) {
            addr = 0xa0;
        } else {
            addr = 0xa2;
            offset -= MAX_SFP_PAGE_SIZE;
        }
        // get cp2112 hndl
        bf_pltfm_cp2112_device_ctx_t *hndl =
            bf_pltfm_cp2112_get_handle (CP2112_ID_1);
        if (!hndl) {
            rc = -1;
            break;
        }
        // write module, CP2112 write bytes should not longer than 61, but we limit it to 60 because we use one byte to mark offset
        while (len > 60) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * 60);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp, 61,
                                       DEFAULT_TIMEOUT_MS) != BF_PLTFM_SUCCESS) {
                rc = -2;
                break;
            }
            buf += 60;
            offset += 60;
            len -= 60;
        }
        if (len > 0) {
            tmp[0] = offset;
            memcpy (tmp + 1, buf, sizeof (uint8_t) * len);
            if (bf_pltfm_cp2112_write (hndl, addr, tmp,
                                       len + 1, DEFAULT_TIMEOUT_MS) !=
                BF_PLTFM_SUCCESS) {
                rc = -3;
                break;
            }
        }
    } while (0);
    return rc;
}

static int
sfp_get_module_pres_hc (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0, i = 0, j = 0;
    uint8_t val[32] = {0xff};
    struct sfp_ctx_st_t *sfp;
    struct st_ctx_t *pres_ctx;

    MASTER_I2C_LOCK;
    if ((rc = select_cpld (2)) != 0) {
        goto end;
    } else {
        /* off=[6:2], off=[22] */
        i = 2;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 0);
        MASKBIT (val[i], 1);
        MASKBIT (val[i], 2);
        MASKBIT (val[i], 5);

        i = 3;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 0);
        MASKBIT (val[i], 1);
        MASKBIT (val[i], 2);
        MASKBIT (val[i], 5);

        i = 4;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 0);
        MASKBIT (val[i], 1);
        MASKBIT (val[i], 2);
        MASKBIT (val[i], 4);
        i = 5;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 0);
        MASKBIT (val[i], 1);
        MASKBIT (val[i], 2);

        i = 6;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 4);
        MASKBIT (val[i], 5);
        MASKBIT (val[i], 6);
        MASKBIT (val[i], 7);

        i = 22;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD2_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 2);
        MASKBIT (val[i], 3);
        MASKBIT (val[i], 4);
        MASKBIT (val[i], 5);
        MASKBIT (val[i], 6);
        MASKBIT (val[i], 7);

        if (rc) {
            goto end;
        }
    }

    if ((rc = select_cpld (3)) != 0) {
        goto end;
    } else {
        /* off=[9:8] */
        i = 8;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, i, &val[i]);
        i = 9;
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD3_I2C_ADDR, i, &val[i]);
        MASKBIT (val[i], 5);

        if (rc) {
            goto end;
        }
    }

    for (i = 0; i < ARRAY_LENGTH (val); i ++) {
        for (j = 0; j < ARRAY_LENGTH (sfp_st_hc); j ++) {
            sfp = &sfp_st_hc[j];
            pres_ctx = &sfp->pres;
            if (i == pres_ctx->off) {
                if (! (val[i] & (1 << pres_ctx->off_b))) {
                    //fprintf(stdout, " SFP %2d detected\n", j + 1);
                    if (j + 1 <= 32) {
                        *pres_l &= ~ (1 << (((j + 1) % 32) - 1));
                    } else {
                        *pres_h &= ~ (1 << (((j + 1) % 32) - 1));
                    }
                }
            }
        }
    }


end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
}

static int
sfp_module_tx_disable_hc (
    uint32_t module, bool disable, void *st)
{
    // define
    int rc = 0;
    struct st_ctx_t *tx_dis;
    uint8_t val;
    // array check
    if (module > ARRAY_LENGTH (sfp_st_hc)) {
        return -1;
    }
    tx_dis = &sfp_st_hc[module - 1].tx_dis;

    // lock and set dis
    MASTER_I2C_LOCK;
    if ((rc = select_cpld (tx_dis->cpld_sel)) != 0) {
        goto end;
    } else {
        // read origin value
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD_I2C_ADDR, tx_dis->off, &val);

        // set value
        if (disable) {
            val |= (1 << tx_dis->off_b);
        } else {
            val &= (~ (1 << tx_dis->off_b));
        }

        // write back
        rc |= bf_pltfm_master_i2c_write_byte (
                  BF_MAV_SYSCPLD_I2C_ADDR, tx_dis->off, val);

        if (rc) {
            goto end;
        }
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
}


// 1 means has loss, 0 means not has loss
static int
sfp_module_los_read_hc (
    uint32_t module, bool *los)
{
    // define
    int rc = 1;
    struct st_ctx_t *rx_los;
    uint8_t val;
    // array check
    if (module > ARRAY_LENGTH (sfp_st_hc)) {
        return rc;
    }
    rx_los = &sfp_st_hc[module - 1].rx_los;

    // lock and set dis
    MASTER_I2C_LOCK;
    if ((rc = select_cpld (rx_los->cpld_sel)) != 0) {
        goto end;
    } else {
        // read value
        rc |= bf_pltfm_master_i2c_read_byte (
                  BF_MAV_SYSCPLD_I2C_ADDR, rx_los->off, &val);

        if (rc) {
            rc = 1;
            goto end;
        }

        *los = (val & (1 << rx_los->off_b)) == 0 ? false :
               true;
    }

end:
    unselect_cpld();
    MASTER_I2C_UNLOCK;
    return rc;
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

/** set sfp lpmode
*
*  @param port
*   port
*  @param lpmode
*   true : set lpmode, false : set no-lpmode
*  @return
*   0: success, -1: failure
*/
EXPORT int bf_pltfm_sfp_set_lpmode (
    uint32_t module,
    bool lpmode)
{
    module = module;
    lpmode = lpmode;
    return 0;
}

/** reset sfp module thru syscpld register
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
EXPORT int bf_pltfm_sfp_module_reset (
    uint32_t module,
    bool reset)
{
    module = module;
    reset = reset;

    return 0;
}

EXPORT int bf_pltfm_get_sfp_module_pres (
    uint32_t *pres_l, uint32_t *pres_h)
{

    /*
     * For hc36y24c, there're 38 sfp(s), for X312P, there're 50 sfp(s),
     * So we use 2 mask fetching their present mask.
     *
     * by tsihang, 2021-07-13.
     */

    /*
     * It's quite a heavy cost to read present bit which specified by st_ctx_t,
     * because of that a byte read out from CPLD once may show we out more than
     * one module. Due to the reason above, I prefer keeping access method below
     * as API call than do it under a function pointer like module IO API.
     *
     * by tsihang, 2021-07-16.
     */

    if (platform_type_equal (X532P) ||
        platform_type_equal (X564P)) {
        /*
         * For X564P and X532P, sfp present mask is the same.
         * by tsihang, 2021-07-13.
         */
        return sfp_get_module_pres_x5 (pres_l,
                                       pres_h);
    } else if (platform_type_equal (X308P)) {
        return sfp_get_module_pres_x308p (pres_l,
                                          pres_h);
    } else if (platform_type_equal (X312P)) {
        return sfp_get_module_pres_x3 (pres_l,
                                       pres_h);
    } else if (platform_type_equal (HC)) {
        /* HCv1, HCv2, HCv3, ... */
        return sfp_get_module_pres_hc (pres_l,
                                       pres_h);
    }

    return -1;
}

EXPORT int bf_pltfm_sfp_get_presence_mask (
    uint32_t *port_1_32_pres,
    uint32_t *port_32_64_pres)
{
    /* initialize to absent */
    uint32_t sfp_pres_mask_l = 0xFFFFFFFF,
             sfp_pres_mask_h = 0xFFFFFFFF;

    bf_pltfm_get_sfp_module_pres (&sfp_pres_mask_l,
                                  &sfp_pres_mask_h);

    *port_1_32_pres  = sfp_pres_mask_l;
    *port_32_64_pres = sfp_pres_mask_h;

    return 0;
}


/** get sfp lpmode status
 *
 *  @param port_1_32_lpmode_
 *   lpmode of lower 32 ports (1-32) 0: no-lpmod 1: lpmode
 *  @param port_32_64_ints
 *   lpmode of upper 32 ports (33-64) 0: no-lpmod 1: lpmode
 */
EXPORT int bf_pltfm_sfp_get_lpmode_mask (
    uint32_t *port_1_32_lpm,
    uint32_t *port_32_64_lpm)
{
    port_1_32_lpm = port_1_32_lpm;
    port_32_64_lpm = port_32_64_lpm;
    return 0;
}

/** get sfp interrupt status
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

EXPORT int bf_pltfm_sfp_module_tx_disable (
    uint32_t module, bool disable)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;

    if (!sfp_lookup1 (module, &sfp)) {
        return -1;
    }

    sfp_opt->lock();
    rc = sfp_opt->txdis (module, disable, sfp->st);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.txdis(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to disable Tx");
    }
    sfp_opt->unlock();

    return rc;
}

/* ture means has los, false means no los */
/* rc means get los succesfull or not */
EXPORT int bf_pltfm_sfp_los_read (IN uint32_t
                                  module,
                                  OUT bool *los)
{
    int rc = 1;
    struct sfp_ctx_t *sfp;

    if (!sfp_lookup1 (module, &sfp)) {
        return 1;
    }

    sfp_opt->lock();
    rc = sfp_opt->losrd (module, los);
    sfp_opt->unlock();

    return rc;
}


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

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
EXPORT int bf_pltfm_sfp_read_module (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = 0;

    MAV_SFP_PAGE_LOCK;
    sfp_opt->lock();

    if (sfp_opt->select (module)) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.select(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to select SFP");
        goto end;
    }

    sfp_opt->read (module, offset, len, buf);

    if (sfp_opt->unselect (module)) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.unselect(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to dis-select SFP");
        goto end;
    }

end:
    sfp_opt->unlock();
    MAV_SFP_PAGE_UNLOCK;

    return err;
}

/*
 * to avoid changing code structure, we assume offset 0-127 is at 0xA0, offset 128-255 is at 0xA2
 */
EXPORT int bf_pltfm_sfp_write_module (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf)
{
    int err = 0;

    /* Do not remove this lock. */
    MAV_SFP_PAGE_LOCK;
    sfp_opt->lock();

    if (sfp_opt->select (module)) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.select(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to select SFP");
        goto end;
    }

    sfp_opt->write (module, offset, len, buf);

    if (sfp_opt->unselect (module)) {
        LOG_ERROR (
            "%s[%d], "
            "sfp.unselect(%02d : %s)"
            "\n",
            __FILE__, __LINE__, module,
            "Failed to dis-select SFP");
        goto end;
    }

end:
    sfp_opt->unlock();
    MAV_SFP_PAGE_UNLOCK;
    return err;
}

/*
 * To avoid changing code structure,
 * we assume offset 0-127 is at 0xA0,
 * offset 128-255 is at 0xA2
 */
EXPORT int bf_pltfm_sfp_read_reg (
    uint32_t module,
    uint8_t page,
    int offset,
    uint8_t *val)
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

/*
 * To avoid changing code structure,
 * we assume offset 0-127 is at 0xA0,
 * offset 128-255 is at 0xA2
 */
EXPORT int bf_pltfm_sfp_write_reg (
    uint32_t module,
    uint8_t page,
    int offset,
    uint8_t val)
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
    if (!xsfp_ctx)
        return -1;

    *sfp_ctx = xsfp_ctx;
    return 0;
}

struct opt_ctx_t sfp28_opt_x532p = {
    .read = sfp_read_sub_module_x5,
    .write = sfp_write_sub_module_x5,
    .txdis = sfp_module_tx_disable_x5,
    .losrd = sfp_module_los_read_x5,

    .lock = do_lock,
    .unlock = do_unlock,
    .select = sfp_select_x5,
    .unselect = sfp_unselect_x5,
    .sfp_ctx = &sfp28_ctx_x532p,
};

struct opt_ctx_t sfp28_opt_x564p = {
    .read = sfp_read_sub_module_x5,
    .write = sfp_write_sub_module_x5,
    .txdis = sfp_module_tx_disable_x5,
    .losrd = sfp_module_los_read_x5,

    .lock = do_lock,
    .unlock = do_unlock,
    .select = sfp_select_x5,
    .unselect = sfp_unselect_x5,
    .sfp_ctx = &sfp28_ctx_x564p,
};

struct opt_ctx_t sfp28_opt_x308p = {
    .read = sfp_read_sub_module_x308p,
    .write = sfp_write_sub_module_x308p,
    .txdis = sfp_module_tx_disable_x308p,
    .losrd = sfp_module_los_read_x308p,

    .lock = do_lock_x308p,
    .unlock = do_unlock_x308p,
    .select = sfp_select_x308p,
    .unselect = sfp_unselect_x308p,
    .sfp_ctx = &sfp28_ctx_x308p,
};

struct opt_ctx_t sfp28_opt_x312p = {
    .read = sfp_read_sub_module_x3,
    .write = sfp_write_sub_module_x3,
    .txdis = sfp_module_tx_disable_x3,
    .losrd = sfp_module_los_read_x3,

    .lock = do_lock_x3,
    .unlock = do_unlock_x3,
    .select = sfp_select_x3,
    .unselect = sfp_unselect_x3,
    .sfp_ctx = &sfp28_ctx_x312p,
};

struct opt_ctx_t sfp28_opt_hc36y24c = {
    .read = sfp_read_sub_module_hc,
    .write = sfp_write_sub_module_hc,
    .txdis = sfp_module_tx_disable_hc,
    .losrd = sfp_module_los_read_hc,

    .lock = do_lock_hc,
    .unlock = do_unlock_hc,
    .select = sfp_select_hc,
    .unselect = sfp_unselect_hc,
    .sfp_ctx = &sfp28_ctx_hc36y24c,
};

int bf_pltfm_sfp_init (void *arg)
{
    struct sfp_ctx_t *sfp;
    (void)arg;

    fprintf (stdout,
             "\n\n================== SFPs INIT ==================\n");

    if (platform_type_equal (X532P)) {
        xsfp_ctx = &xsfp_ctx_x532p[0];
        sfp_opt = &sfp28_opt_x532p;
        bf_sfp_set_num (ARRAY_LENGTH (sfp28_ctx_x532p));
    } else if (platform_type_equal (X564P)) {
        xsfp_ctx = &xsfp_ctx_x564p[0];
        sfp_opt = &sfp28_opt_x564p;
        bf_sfp_set_num (ARRAY_LENGTH (sfp28_ctx_x564p));
    } else if (platform_type_equal (X308P)) {
        xsfp_ctx = &xsfp_ctx_x308p[0];
        sfp_opt = &sfp28_opt_x308p;
        bf_sfp_set_num (ARRAY_LENGTH (sfp28_ctx_x308p));
    } else if (platform_type_equal (X312P)) {
        xsfp_ctx = &xsfp_ctx_x312p[0];
        sfp_opt = &sfp28_opt_x312p;
        bf_sfp_set_num (ARRAY_LENGTH (sfp28_ctx_x312p));
    } else if (platform_type_equal (HC)) {
        xsfp_ctx = NULL;
        sfp_opt = &sfp28_opt_hc36y24c;
        bf_sfp_set_num (ARRAY_LENGTH (sfp28_ctx_hc36y24c));
    }
    BUG_ON (sfp_opt == NULL);

    fprintf (stdout, "num of SFPs : %d\n",
             bf_sfp_get_max_sfp_ports());

    /* init SFP map */
    uint32_t module;
    foreach_element (0, bf_sfp_get_max_sfp_ports()) {
        struct sfp_ctx_t *sfp_ctx = (struct sfp_ctx_t *)
                                    sfp_opt->sfp_ctx;
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
                                    sfp_opt->sfp_ctx;
        module = (each_element + 1);
        sfp = &sfp_ctx[each_element];
        fprintf (stdout, "Y%02d   <->   %02d/%d\n",
                 module, sfp->info.conn, sfp->info.chnl);
    }

    /* lock init */
    MAV_SFP_PAGE_LOCK_INIT;
    return 0;
}

/* NOT finished yet. is_panel_sfp is a commonly used API.
 * by tsihang, 2021-07-18. */
EXPORT bool is_panel_qsfp_module (
    IN unsigned int module)
{
    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= 32)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= 64)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        if ((module >= 1) && (module <= 8)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= 12)) {
            return true;
        }
    } else if (platform_type_equal (HC)) {
        if ((module >= 1) && (module <= 24)) {
            return true;
        }
    }
    return false;
}

/* Panel <-> ASIC. */
EXPORT bool is_panel_sfp_module (
    IN unsigned int module)
{
    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        if ((module >= 1) && (module <= 48)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= 50)) {
            return true;
        }
    } else if (platform_type_equal (HC)) {
        if ((module >= 1) && (module <= 38)) {
            return true;
        }
    }
    return false;
}

/* COMe <-> ASIC : X1, X2 */
EXPORT bool is_xsfp_module (
    IN unsigned int module)
{
    if (platform_type_equal (X532P)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (X564P)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (X312P)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (HC)) {
        if ((module >= 1) && (module <= 2)) {
            return true;
        }
    } else if (platform_type_equal (X308P)) {
        /* X308P-T has no this module. */
    }
    return false;
}

/* COMe channel, always be 2x 10G. */
EXPORT int bf_pltfm_xsfp_lookup_by_module (
    IN  int module,
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

/* The interface only export to caller, such as stratum, etc.
 * and the @name as input param is readable and comes from panel.
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
        return -1;
    }

    if (name[0] == 'C') {
        port_hdl.chnl_id = 0;
        if (is_panel_qsfp_module (module)) {
            err = bf_pltfm_qsfp_lookup_by_module (module,
                                            &port_hdl.conn_id);
            if (err) {
                return -1;
            }
            goto fetch_dp;
        } else {
            /* GHC channel ? */
            err = bf_pltfm_vqsfp_lookup_by_module (module,
                                                &port_hdl.conn_id);
            if (err) {
                return -1;
            }
        }
    } else if (name[0] == 'Y') {
        if (is_panel_sfp_module (module)) {
            err = bf_pltfm_sfp_lookup_by_module (module,
                                                &port_hdl.conn_id, &port_hdl.chnl_id);
            if (err) {
                return -1;
            }
            goto fetch_dp;
        } else {
            return -1;
        }
    } else if (name[0] == 'X') {
        if (is_xsfp_module (module)) {
            err = bf_pltfm_xsfp_lookup_by_module (module,
                                                &port_hdl.conn_id, &port_hdl.chnl_id);
            if (err) {
                return -1;
            }
            goto fetch_dp;
        } else {
            return -1;
        }
    } else {
        return -1;
    }

fetch_dp:
    FP2DP (dev_id, &port_hdl, &d_p);
    return d_p;
}

