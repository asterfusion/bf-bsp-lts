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

#define DEFAULT_TIMEOUT_MS 500

extern bool sfp_lookup1 (IN uint32_t
                         module, OUT struct sfp_ctx_t **sfp);

/* access is 0 based index. */
static struct sfp_ctx_st_t sfp_st_x312p[] = {
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (0)}},   // Y1
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (1)}},   // Y2
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (2)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (2)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (2)}},   // Y3
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (3)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (3)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (3)}},   // Y4
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (4)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (4)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (4)}},   // Y5
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (5)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (5)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (5)}},   // Y6
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (6)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (6)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (6)}},   // Y7
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_01_08, BIT (7)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_01_08, BIT (7)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_01_08, BIT (7)}},   // Y8

    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (0)}},   // Y9
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (1)}},   // Y10
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (2)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (2)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (2)}},   // Y11
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (3)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (3)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (3)}},   // Y12
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (4)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (4)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (4)}},   // Y13
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (5)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (5)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (5)}},   // Y14
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_09_15, BIT (6)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_09_15, BIT (6)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_09_15, BIT (6)}},   // Y15

    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (0)}},   // Y16
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (1)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (1)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (1)}},   // Y17
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (2)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (2)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (2)}},   // Y18
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (3)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (3)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (3)}},   // Y19
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (4)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (4)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (4)}},   // Y20
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (5)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (5)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (5)}},   // Y21
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (6)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (6)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (6)}},   // Y22
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_16_23, BIT (7)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_16_23, BIT (7)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_16_23, BIT (7)}},   // Y23

    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (0)}},   // Y24
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (1)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (1)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (1)}},   // Y25
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (2)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (2)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (2)}},   // Y26
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (3)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (3)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (3)}},   // Y27
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (4)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (4)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (4)}},   // Y28
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (5)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (5)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (5)}},   // Y29
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (6)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (6)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (6)}},   // Y30
    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_24_31, BIT (7)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_24_31, BIT (7)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_24_31, BIT (7)}},   // Y31

    {{BF_MAV_SYSCPLD4, X312P_SFP_ENB_32_32, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_PRS_32_32, BIT (0)}, {BF_MAV_SYSCPLD4, X312P_SFP_LOS_32_32, BIT (0)}},   // Y32

    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (0)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (0)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (0)}},   // Y33
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (1)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (1)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (1)}},   // Y34
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (2)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (2)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (2)}},   // Y35
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (3)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (3)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (3)}},   // Y36
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (4)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (4)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (4)}},   // Y37
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (5)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (5)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (5)}},   // Y38
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (6)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (6)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (6)}},   // Y39
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_33_40, BIT (7)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_33_40, BIT (7)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_33_40, BIT (7)}},   // Y40

    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (0)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (0)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (0)}},   // Y41
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (1)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (1)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (1)}},   // Y42
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (2)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (2)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (2)}},   // Y43
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (3)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (3)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (3)}},   // Y44
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (4)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (4)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (4)}},   // Y45
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (5)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (5)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (5)}},   // Y46
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (6)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (6)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (6)}},   // Y47
    {{BF_MAV_SYSCPLD5, X312P_SFP_ENB_41_48, BIT (7)}, {BF_MAV_SYSCPLD5, X312P_SFP_PRS_41_48, BIT (7)}, {BF_MAV_SYSCPLD5, X312P_SFP_LOS_41_48, BIT (7)}},   // Y48

    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_YM_YN, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_YM_YN, BIT (1)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_YM_YN, BIT (1)}},   // Y49
    {{BF_MAV_SYSCPLD3, X312P_SFP_ENB_YM_YN, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_PRS_YM_YN, BIT (0)}, {BF_MAV_SYSCPLD3, X312P_SFP_LOS_YM_YN, BIT (0)}},   // Y50
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
static struct sfp_ctx_t xsfp_ctx_x312p[] = {
    {{"X1", 33, 2, 0, 0}, NULL},
    {{"X2", 33, 3, 0, 0}, NULL}
};

static void
do_lock_x312p()
{
    /* SuperIO/CP2112 is used for x312p to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_LOCK;
}

static void
do_unlock_x312p()
{
    /* SuperIO/CP2112 is used for x312p to read SFP. Even though
     * there's no resource competition with other application,
     * here we still hold the lock in case of risk.
     *
     * by tsihang, 2021-07-16.
     */
    MASTER_I2C_UNLOCK;
}

static int
sfp_select_x312p (uint32_t module)
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
            rc = -1;
            break;
        }
        // get address from sfp28_ctx_x312p
        l1_ch = (sfp->info.i2c_chnl_addr & 0xF0) >> 4;
        l2_ch = (sfp->info.i2c_chnl_addr & 0x0F);
        l1_addr = X312P_PCA9548_L1_0x71;
        l2_addr = (sfp->info.rev >> 8) & 0xFF;
        l2_addr_unconcerned = sfp->info.rev & 0xFF;

        if (platform_subtype_equal (v1dot0)) {

            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -2;
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
            /* X312 V3 dev use suio to set cpld qsfp/sfp status, and cp2112 read module info,
             * so need to read once to ensure that the value is written correctly, otherwise
             * reading and writing qsfp/sfp module information will report an error, because it
             * may not be really written.
             */
            uint8_t ret_value = 0;
            ret_value = ret_value;

            // select L1. write level_1 PCA9548.
            if (bf_pltfm_master_i2c_write_byte (l1_addr, 0x0,
                                                1 << l1_ch)) {
                rc = -2;
                break;
            } else {
                //nothing to do.
            }

            // unselect L2_unconcerned. write level_2 unconcerned PCA9548
            if (l2_addr_unconcerned) {
                if (bf_pltfm_master_i2c_write_byte (
                        l2_addr_unconcerned, 0x0, 0x0)) {
                    rc = -3;
                    break;
                } else {
                    rc = bf_pltfm_master_i2c_read_byte(l2_addr_unconcerned, 0x0, &ret_value);
                    if (rc || (ret_value != 0x0)) {
                        LOG_ERROR("%s:%d >>> Get SFP : %2d , PCA9548 : 0x%02x , value : 0x%02x, Try again ...\n",
                                    __func__, __LINE__, module, l2_addr_unconcerned, ret_value);
                        rc = bf_pltfm_master_i2c_read_byte(l2_addr_unconcerned, 0x0, &ret_value);
                        if (rc || (ret_value != 0x0)) {
                            LOG_ERROR("%s:%d >>> Get SFP : %2d , PCA9548 : 0x%02x , value : 0x%02x, exit ...\n",
                                    __func__, __LINE__, module, l2_addr_unconcerned, ret_value);
                            rc = -400;
                            break;
                        }
                    }
                }
            }
            // select L2. write level_2 PCA9548.
            if (bf_pltfm_master_i2c_write_byte (l2_addr, 0x0,
                                                1 << l2_ch)) {
                rc = -5;
                break;
            } else {
                //nothing to do.
            }
        }
    } while (0);

    // return
    return rc;
}

static int
sfp_unselect_x312p (uint32_t module)
{
    int rc = 0;
    struct sfp_ctx_t *sfp;
    uint8_t l1_addr;
    uint8_t l2_addr;
    uint8_t l2_addr_unconcerned;

    do {
        // get sfp
        if (!sfp_lookup1 (module, &sfp)) {
            rc = -1;
            break;
        }
        // get address from sfp28_ctx_x312p
        l1_addr = X312P_PCA9548_L1_0x71;
        l2_addr = (sfp->info.rev >> 8) & 0xFF;
        l2_addr_unconcerned = sfp->info.rev & 0xFF;

        if (platform_subtype_equal (v1dot0)) {

            // get cp2112 hndl
            bf_pltfm_cp2112_device_ctx_t *hndl =
                bf_pltfm_cp2112_get_handle (CP2112_ID_1);
            if (!hndl) {
                rc = -2;
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
                rc = -2;
                break;
            }
            // unselect L2_unconcerned
            if (l2_addr_unconcerned) {
                if (bf_pltfm_master_i2c_write_byte (
                        l2_addr_unconcerned, 0x0, 0x0)) {
                    rc = -3;
                    break;
                }
            }
            // unselect L1
            if (bf_pltfm_master_i2c_write_byte (l1_addr, 0x0,
                                                0x0)) {
                rc = -4;
                break;
            }
        }
    } while (0);

    // return
    return rc;
}

static int
sfp_read_sub_module_x312p (
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
        if (platform_subtype_equal (v1dot0)) {
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
sfp_write_sub_module_x312p (
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

        if (platform_subtype_equal (v1dot0)) {
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
sfp_get_module_pres_x312p (
    uint32_t *pres_l, uint32_t *pres_h)
{
    int rc = 0;
    uint32_t sfp_01_32 = 0xFFFFFFFF;
    uint32_t sfp_33_64 = 0xFFFFFFFF;
    uint8_t syscpld = 0, off = 0;

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

    syscpld = BF_MAV_SYSCPLD3;

    off = X312P_SFP_PRS_01_08;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_01_08);

    off = X312P_SFP_PRS_09_15;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_09_15);
    sfp_09_15 |= 0x80;

    off = X312P_SFP_PRS_YM_YN;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &zsfp_pres);
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

    syscpld = BF_MAV_SYSCPLD4;

    off = X312P_SFP_PRS_16_23;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_16_23);

    off = X312P_SFP_PRS_24_31;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_24_31);

    off = X312P_SFP_PRS_32_32;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_32_32);
    sfp_32_32 |= 0xFE;


    syscpld = BF_MAV_SYSCPLD5;

    off = X312P_SFP_PRS_33_40;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_33_40);

    off = X312P_SFP_PRS_41_48;
    rc += bf_pltfm_cpld_read_byte (syscpld,
                                   off, &sfp_41_48);

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
        *pres_l = sfp_01_32;
        *pres_h = sfp_33_64;
    }
    return rc;
}

static int
sfp_module_tx_disable_x321p (
    uint32_t module, bool disable, void *st)
{
    // define
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    struct st_ctx_t *dis = & ((struct sfp_ctx_st_t *)
                                  st)->tx_dis;

    // lock and set dis
    // read origin value
    rc = bf_pltfm_cpld_read_byte (dis->cpld_sel,
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
    // set value
    if (disable) {
        val |= (1 << dis->off_b);
    } else {
        val &= (~ (1 << dis->off_b));
    }

    // write back
    rc = bf_pltfm_cpld_write_byte (dis->cpld_sel,
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
    rc = bf_pltfm_cpld_read_byte (
            dis->cpld_sel, dis->off, &val1);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "read_cpld(%02d : %d : %s)"
            "\n",
            __FILE__, __LINE__, module, dis->off,
            "Failed to read CPLD");
        goto end;
    }

    LOG_DEBUG (
             " SFP    %2d : CH : 0x%02x  V : (0x%02x -> 0x%02x) "
             " %sTx",
             module,
             dis->cpld_sel, val0,
             val1, disable ? "-" : "+");

end:
    return rc;
}

static int
sfp_module_los_read_x312p (
                uint32_t module, bool *los, void *st)
{
    // define
    int rc = 0;
    uint8_t val;
    struct st_ctx_t *rx_los = & ((struct sfp_ctx_st_t *)
                                  st)->rx_los;
    // read value
    rc = bf_pltfm_cpld_read_byte (rx_los->cpld_sel,
                                    rx_los->off, &val);
    if (rc) {
        LOG_ERROR (
             "%s[%d], "
              "read_cpld(%02d : %d : %s)"
              "\n",
            __FILE__, __LINE__, module, rx_los->off,
            "Failed to read CPLD");
        goto end;
    }

    *los = (val & (1 << rx_los->off_b)) == 0 ? false :
           true;

end:
    return rc;
}

static struct opt_ctx_t sfp28_opt_x312p = {
    .read = sfp_read_sub_module_x312p,
    .write = sfp_write_sub_module_x312p,
    .txdis = sfp_module_tx_disable_x321p,
    .losrd = sfp_module_los_read_x312p,
    .presrd = sfp_get_module_pres_x312p,

    .lock = do_lock_x312p,
    .unlock = do_unlock_x312p,
    .select = sfp_select_x312p,
    .unselect = sfp_unselect_x312p,
    .sfp_ctx = &sfp28_ctx_x312p,
    .xsfp_ctx = &xsfp_ctx_x312p,
};

void bf_pltfm_sfp_init_x312p (struct opt_ctx_t **opt,
    uint32_t *num, uint32_t *xsfp_num) {
    *opt = &sfp28_opt_x312p;
    *num = ARRAY_LENGTH(sfp28_ctx_x312p);
    *xsfp_num = ARRAY_LENGTH(xsfp_ctx_x312p);
}

