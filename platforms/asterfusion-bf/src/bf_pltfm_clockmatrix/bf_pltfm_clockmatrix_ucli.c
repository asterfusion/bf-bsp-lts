/*!
 * @file bf_pltfm_clockmatrix_ucli.c
 * @date 2026/07/08
 *
 * Hang Tsi (tsihang@asterfusion.com)
 *
 *
 */

#include <pthread.h>
#include <signal.h>
#include <sys/timex.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <lld/bf_ts_if.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_clockmatrix.h>

// Not thread-safe.
static void bf_pltfm_read_devts_bsync() {
    uint64_t devts_cnt_cap= 0;
    uint64_t devts_cnt_set= 0;
    uint64_t devts_off_set= 0;
    uint32_t devts_inc_set= 0;

    uint64_t bsync_cnt_cap= 0;
    uint64_t bsync_cnt_set= 0;
    uint32_t bsync_inc_set= 0;
    uint32_t bsync_inc_f  = 0;
    uint32_t bsync_inc_f_d= 0;

    bool devts_enb, bsync_enb;
    uint32_t rst_cnt_thrs = 0, debounce_cnt = 0;

    static uint64_t devts_cnt_cap_last, bsync_cnt_cap_last;

    bf_ts_global_ts_value_get(0, &devts_cnt_set);
    bf_ts_global_ts_offset_get(0, &devts_off_set);
    bf_ts_global_ts_inc_value_get(0, &devts_inc_set);
    (void)devts_off_set;
    (void)devts_inc_set;

    bf_ts_baresync_reset_value_get(0, &bsync_cnt_set);

    if(platform_type_equal(AFN_X732QT)) {
#if SDE_VERSION_GT(9131)
        bf_tof2_ts_baresync_increment_get(0, &bsync_inc_set, &bsync_inc_f, &bsync_inc_f_d);
#endif
    }

    (void)bsync_inc_f;
    (void)bsync_inc_f_d;

    bf_ts_global_baresync_ts_get(0, &devts_cnt_cap, &bsync_cnt_cap);

    bf_ts_baresync_state_get(0, &rst_cnt_thrs, &debounce_cnt, &bsync_enb);
    bf_ts_global_ts_state_get(0, &devts_enb);

    fprintf(stdout, "\n%16s%16d\n", "bsync_enb", bsync_enb);
    fprintf(stdout, "%16s%16u (F %u : D %u)   (rst %u : dbc %u)\n", "bsync_inc_set", bsync_inc_set, bsync_inc_f, bsync_inc_f_d, rst_cnt_thrs, debounce_cnt);
    fprintf(stdout, "%16s%16lu, cap %16lu, incr %lu\n", "bsync_cnt_set", bsync_cnt_set, bsync_cnt_cap, bsync_cnt_cap - bsync_cnt_cap_last);

    fprintf(stdout, "\n%16s%16d\n", "devts_enb", devts_enb);
    fprintf(stdout, "%16s%16lu, cap %16lu, incr %lu\n", "devts_cnt_set", devts_cnt_set, devts_cnt_cap, devts_cnt_cap - devts_cnt_cap_last);

    devts_cnt_cap_last = devts_cnt_cap;
    bsync_cnt_cap_last = bsync_cnt_cap;
}

static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__set_clk (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "set-clk", 1, "set-clk <crystal/ptp>");

    if (!memcmp(uc->pargs->args[0], "crystal", strlen("crystal"))) {
        if (bf_pltfm_set_clk(CLK_CRYSTAL) == 0) {
        }
    } else if (!memcmp(uc->pargs->args[0], "ptp", strlen("ptp"))) {
        if (bf_pltfm_set_clk(CLK_SMU) == 0) {
        }
    } else {
        aim_printf (&uc->pvs,
                    "Usage: set-clk <crystal/ptp>.\n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__tx_rst_pulse (
    ucli_context_t *uc)
{
    uint32_t ctrl;

    UCLI_COMMAND_INFO (uc, "tx-rst-pulse", 1, " <ctrl, 0|1, 0: ignore, 1: Tx RST pulse one time>");

    ctrl = strtoul (uc->pargs->args[0], NULL, 10);

    if(ctrl) {
    	bf_pltfm_tx_rst_pulse();
    }
    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__bsync (
    ucli_context_t *uc)
{
    bool bsync_enb, z;
    uint32_t ctrl = 0;
    uint32_t rst_cnt_thrs = 0, debounce_cnt = 0, x = 0, y = 0;


    UCLI_COMMAND_INFO (uc, "bsync", 3, " <rst_cnt_thrs, 0~127> <debounce_cnt, 0~0xFFFFFF> <enb, 0|1, 0: disable, 1: enable>");

    rst_cnt_thrs = strtoul (uc->pargs->args[0], NULL, 10);
    debounce_cnt = strtoul (uc->pargs->args[1], NULL, 10);
    ctrl = strtoul (uc->pargs->args[2], NULL, 10);

    bf_ts_baresync_state_get(0, &x, &y, &z);

    if(ctrl) {
        bsync_enb = true;
    } else {
        bsync_enb = false;
    }

    if((int32_t)rst_cnt_thrs == -1)
        rst_cnt_thrs = x;
    if((int32_t)debounce_cnt == -1)
        debounce_cnt = y;

    bf_ts_baresync_state_set(0, rst_cnt_thrs, debounce_cnt, bsync_enb);

    aim_printf (&uc->pvs, "bsync    %s\n",
                bsync_enb ?
                "enabled" : "disabled");
    bf_pltfm_read_devts_bsync();
    return 0;
}
// debug only
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__devts (
    ucli_context_t *uc)
{
    uint32_t ctrl = 0;
    bool devts_enb;

    UCLI_COMMAND_INFO (uc, "devts", 1, " <0/1> 0: disable , 1: enable");

    ctrl = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_global_ts_state_get(0, &devts_enb);

    if(ctrl) {
        devts_enb = true;
    } else {
        devts_enb = false;
    }

    bf_ts_global_ts_state_set(0, devts_enb);

    aim_printf (&uc->pvs, "devts    %s\n",
                devts_enb ?
                "enabled" : "disabled");
    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__devts_cnt (
    ucli_context_t *uc)
{
    uint64_t devts_cnt_set = 0;

    UCLI_COMMAND_INFO (uc, "devts-cnt", 1, " <uint64_t>");

    devts_cnt_set = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_global_ts_value_set(0, devts_cnt_set);

    bf_pltfm_read_devts_bsync();
    return 0;
}

// debug only
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__bsync_cnt (
    ucli_context_t *uc)
{
    uint64_t bsync_cnt_set = 0;

    UCLI_COMMAND_INFO (uc, "bsync-cnt", 1, " <uint64_t>");

    bsync_cnt_set = strtoul (uc->pargs->args[0], NULL, 10);

    bf_ts_baresync_reset_value_set(0, bsync_cnt_set);

    bf_pltfm_read_devts_bsync();
    return 0;
}

// For TOF2 PTP, by Hang Tsi, 2025/12/15
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__clkobs_strength_set__(
    ucli_context_t *uc) {

  int dev_id;
  int drive_strength;

  UCLI_COMMAND_INFO(uc,
                    "clkobs_strength_set",
                    2,
                    "Set drive strength "
                    "<dev> <strength: 0-15>");

  dev_id = 0;
  drive_strength = strtol(uc->pargs->args[1], NULL, 0);

  if(drive_strength < 0 || drive_strength > 15)
    return -1;
  (void)dev_id;
  (void)drive_strength;
#if SDE_VERSION_GT(9131)
  if (bf_port_clkobs_drive_strength_set(dev_id, drive_strength) != BF_SUCCESS) {
    return -1;
  }
#endif
  return 0;
}

// debug only
// https://www.renesas.com/en/products/8a34004?tab=documentation
// I2C 2B Mode. See page 5 of REN_8A3xxxx=Family-Prog-Guide-v4p8p7_GDE_20210615.pdf
/* @brief Dump raw hex contents (256 bytes) of a PTP register page.
 *        Page range: 0xC0..0xCF per Renesas 8A34004 paged-I2C protocol. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__page (
    ucli_context_t *uc)
{
    uint8_t buf[BUFSIZ] = { 0 };
    uint16_t byte;
    uint16_t ptp_reg_size = 256;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = 0x58;
    uint8_t page, page_start, page_end;

    UCLI_COMMAND_INFO (uc, "page", -1,
                       "page <page: 0xC0-0xCF>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if(! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
       return 0;
    }

    if (uc->pargs->count >= 1) {
        page_start = strtoul (uc->pargs->args[0], NULL, 0);
        page_end = page_start;
    } else {
        page_start = 0xC0;
        page_end = 0xCF;
    }

    MASTER_I2C_LOCK;
    bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20);

    // List all pages.
    for (page = page_start; page <= page_end; page++) {
        bf_pltfm_cp2112_reg_write_byte(ptp_addr, 0xFD, page);

        for (byte = 0; byte < ptp_reg_size; byte ++) {
            bf_pltfm_cp2112_reg_read_block (ptp_addr, byte, &buf[byte], 1);
        }

        aim_printf (&uc->pvs, "\nPAGE: %02X ", page);
        for (byte = 0; byte < ptp_reg_size; byte++) {
            if ((byte % 16) == 0) {
                aim_printf (&uc->pvs, "\n0x%02X : ", byte);
            }
            aim_printf (&uc->pvs, "%02x ", buf[byte]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00);
    MASTER_I2C_UNLOCK;

    aim_printf (&uc->pvs, "\n");

    return 0;
}

/* @brief Read or write a single PTP register.
 *        Parameter <reg> is the effective address (base + offset),
 *        where base is the module start address and offset is the register within that module.
 *        The 8A34004 paged-I2C maps this to physical address (page, base+offset). */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__reg(ucli_context_t *uc)
{
    uint8_t page, reg, val = 0, regval = 0;
    bool write_op = false;

    UCLI_COMMAND_INFO(uc, "reg", -1,
                      "reg <page> <reg> [<val>]");

    if (!platform_type_equal(AFN_X732QT) ||
        !platform_subtype_equal(V2P0)) {
        aim_printf(&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (!(bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf(&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count == 2) {
        write_op = false;
    } else if (uc->pargs->count == 3) {
        write_op = true;
    } else {
        aim_printf(&uc->pvs, "reg <page> <reg> [<val>]\n");
        return 0;
    }

    page = strtoul(uc->pargs->args[0], NULL, 0);
    reg  = strtoul(uc->pargs->args[1], NULL, 0);

    if (write_op) {
        val = strtoul(uc->pargs->args[2], NULL, 0);
        if (bf_pltfm_read_ptp_reg(page, reg, &regval))
            return 0;
        if (bf_pltfm_write_ptp_reg(page, reg, val))
            return 0;
        if (bf_pltfm_read_ptp_reg(page, reg, &val))
            return 0;
        aim_printf(&uc->pvs, "PAGE=0x%02x REG=0x%02x VAL=0x%02x -> 0x%02x\n",
                   page, reg, regval, val);
    } else {
        if (bf_pltfm_read_ptp_reg(page, reg, &regval))
            return 0;
        aim_printf(&uc->pvs, "PAGE=0x%02x REG=0x%02x VAL=0x%02x\n",
                   page, reg, regval);
    }

    return 0;
}

/* @brief Print a comprehensive summary of the 8A34004 timing plane:
 *        DPLL configuration/state matrix, combo slave matrix, and ToD multi-axis matrix. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__ptp (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "ptp", -1,
                       "Summary the PTP module configuration and status");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot support on this device!\n");
        return 0;
    }

    if(! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
       return 0;
    }

    /* Production ID */
    uint16_t product_id = 0;
    bf_ptp_get_product_id(&product_id);
    aim_printf (&uc->pvs, "GENERAL_STATUS.PRODUCT_ID  : %x\n", product_id);

    /* Release Number */
    uint8_t major = 0, minor = 0, rev = 0;
    bf_ptp_get_release_no(&major, &minor, &rev);
    aim_printf (&uc->pvs, "GENERAL_STATUS.RELEASE     : v%x.%x.%x\n", major, minor, rev);

    /* Configuration of INPUT_0 and INPUT_1 */
    double input_0 = 0.0, input_1 = 0.0;
    bf_ptp_get_input_freq(0, &input_0);
    bf_ptp_get_input_freq(1, &input_1);
    aim_printf (&uc->pvs, "INPUT_0.IN_FREQ(OBSCLK 0)  : %.6f\n", input_0);
    aim_printf (&uc->pvs, "INPUT_1.IN_FREQ(OBSCLK 1)  : %.6f\n", input_1);
    aim_printf (&uc->pvs, "\n");

    /* SCRATCH3 from Asterfusion */
    uint8_t page, reg, regval;
    page = 0xCF;
    reg  = 0x50;
    bf_pltfm_read_ptp_reg(page, reg + 0x0E, &regval);
    aim_printf (&uc->pvs, "SCRATCH.SCRATCH3           : %x\n", regval);
    aim_printf (&uc->pvs, "\n");

    /* System-wide status registers */
    uint32_t boot_status = 0;
    enum dpll_state sys_dpll = 0;
    uint8_t  sys_apll = 0;

    bf_ptp_get_boot_status(&boot_status);
    bf_ptp_get_sys_dpll_state(0, &sys_dpll);
    bf_ptp_get_sys_apll_state(0, &sys_apll);

    aim_printf(&uc->pvs, "BOOT_STATUS                : 0x%08X  %s\n", boot_status,
               (boot_status == 0xA0) ? "READY" : "BUSY");
    aim_printf(&uc->pvs, "SYS_DPLL_STATE             : 0x%02X     %s\n", sys_dpll, dpll_state_str(0, sys_dpll));
    aim_printf(&uc->pvs, "SYS_APLL_STATE             : 0x%02X     %s\n", sys_apll,
               (sys_apll & SYS_APLL_LOSS_LOCK_LIVE_UNLOCKED) ? "UNLOCKED" : "LOCKED");
    aim_printf(&uc->pvs, "\n");

    aim_printf(&uc->pvs, "\n=================================================================================================================\n");
    aim_printf(&uc->pvs, "                          8A34004 TIMING PLANE: DPLL CONFIGURATION & STATE MATRIX                         \n");
    aim_printf(&uc->pvs, "-----------------------------------------------------------------------------------------------------------------\n");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-25s\n",
               "DPLL#", "OPERATIONAL MODE", "LIVE STATE", "REF CLK", "ToD SYNC", "ToD SRC", "PHASE ERROR (ns)");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-25s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "-------------------------");
    for (uint8_t dpll_index = 0; dpll_index < 8; dpll_index++) {
        enum pll_mode op_mode;
        enum dpll_state dpll_state = 0;
        uint8_t dpll_ref = 0;
        bool sync_en = false;
        uint8_t tod_src = 0;
        double phase_ns = 0.0;

        if (bf_ptp_get_dpll_mode(dpll_index, &op_mode) != 0) {
            continue;
        }
        if (bf_ptp_get_dpll_state(dpll_index, &dpll_state) != 0) {
            continue;
        }
        if (bf_ptp_get_dpll_ref_stat(dpll_index, &dpll_ref) != 0) {
            continue;
        }
        if (bf_ptp_get_dpll_tod_sync_cfg(dpll_index, &sync_en, &tod_src) != 0) {
            continue;
        }
        if (bf_ptp_get_dpll_phase_status(dpll_index, &phase_ns) != 0) {
            continue;
        }

        char dpll_idx_str[16];
        char ref_str[16] = "---";
        char sync_en_str[16] = "---";
        char tod_src_str[16] = "---";
        char phase_str[32] = "---";
        const char *op_mode_str = "---";
        const char *live_state_str = "---";

        snprintf(dpll_idx_str, sizeof(dpll_idx_str), "%d", dpll_index);
        if (op_mode != PLL_MODE_DISABLED) {
            op_mode_str = dpll_mode_op_str(op_mode);
            live_state_str = dpll_state_str(dpll_index, dpll_state);
            snprintf(ref_str, sizeof(ref_str), "%s", dpll_refclk_str(dpll_ref));
            snprintf(sync_en_str, sizeof(sync_en_str), "%s", sync_en ? "ENABLED" : "DISABLED");
            snprintf(tod_src_str, sizeof(tod_src_str), "ToD%d", tod_src);
            snprintf(phase_str, sizeof(phase_str), "% .9f ns", phase_ns);
        }

        aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-25s\n",
                   dpll_idx_str, op_mode_str, live_state_str, ref_str, sync_en_str, tod_src_str, phase_str);
    }
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-25s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "-------------------------");


    aim_printf(&uc->pvs, "\n=================================================================================================================\n");
    aim_printf(&uc->pvs, "                          8A34004 TIMING PLANE: DPLL COMBO SLAVE CONFIGURATION MATRIX                         \n");
    aim_printf(&uc->pvs, "-----------------------------------------------------------------------------------------------------------------\n");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s\n",
               "DPLL#", "PRIMARY COMBO", "ENABLED", "FILTERED", "SECONDARY COMBO", "ENABLED", "FILTERED");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "--------");
    for (uint8_t dpll_index = 0; dpll_index < 8; dpll_index++) {
        bool pri_en = false, pri_filt = false;
        uint8_t pri_src = 0;
        bool sec_en = false, sec_filt = false;
        uint8_t sec_src = 0;

        if (bf_ptp_get_dpll_combo_slave_pri_cfg(dpll_index, &pri_en, &pri_filt, &pri_src) != 0) {
            pri_en = false; pri_filt = false; pri_src = 0xFF;
        }
        if (bf_ptp_get_dpll_combo_slave_sec_cfg(dpll_index, &sec_en, &sec_filt, &sec_src) != 0) {
            sec_en = false; sec_filt = false; sec_src = 0xFF;
        }

        char pri_src_str[16];
        char pri_en_str[16];
        char pri_filt_str[16];
        char sec_src_str[16];
        char sec_en_str[16];
        char sec_filt_str[16];

        if (pri_src == 0xFF) {
            snprintf(pri_src_str, sizeof(pri_src_str), "N/A");
        } else if (pri_src == 0x08) {
            snprintf(pri_src_str, sizeof(pri_src_str), "SYSDPLL");
        } else {
            snprintf(pri_src_str, sizeof(pri_src_str), "DPLL%d", pri_src);
        }
        snprintf(pri_en_str, sizeof(pri_en_str), "%s", pri_en ? "TRUE" : "FALSE");
        snprintf(pri_filt_str, sizeof(pri_filt_str), "%s", pri_filt ? "TRUE" : "FALSE");

        if (sec_src == 0xFF) {
            snprintf(sec_src_str, sizeof(sec_src_str), "N/A");
        } else if (sec_src == 0x08) {
            snprintf(sec_src_str, sizeof(sec_src_str), "SYSDPLL");
        } else {
            snprintf(sec_src_str, sizeof(sec_src_str), "DPLL%d", sec_src);
        }
        snprintf(sec_en_str, sizeof(sec_en_str), "%s", sec_en ? "TRUE" : "FALSE");
        snprintf(sec_filt_str, sizeof(sec_filt_str), "%s", sec_filt ? "TRUE" : "FALSE");

        aim_printf(&uc->pvs, " %-5d | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s\n",
                   dpll_index, pri_src_str, pri_en_str, pri_filt_str, sec_src_str, sec_en_str, sec_filt_str);
    }
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "--------");

    aim_printf(&uc->pvs, "\n=================================================================================================================\n");
    aim_printf(&uc->pvs, "                          8A34004 TIMING PLANE: TIME-OF-DAY (ToD) MULTI-AXIS MATRIX                         \n");
    aim_printf(&uc->pvs, "-----------------------------------------------------------------------------------------------------------------\n");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s | %-25s\n",
               "ToD#", "DRIVEN BY", "ENABLED", "PPS MODE", "OUT_SYNC", "WR_CNTR", "RD_CNTR", "CAPTURED WALL-TIME (ToD)");
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s | %-25s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "--------", "-------------------------");
    for (uint8_t idx = 0; idx < 4; idx++) {
        uint8_t clk_src = 0;
        uint8_t write_counter = 0;
        uint8_t read_counter = 0;
        bool enabled = false, even_pps = false, sync_disabled = false;
        uint64_t seconds = 0, nanoseconds = 0;

        /* Step A API: Fetch live driving clock source (DPLL linkage) */
        if (bf_ptp_get_tod_clk_src(idx, &clk_src) != 0) {
            clk_src = 0;
        }

        /* Step B API: Fetch main engine configuration switches */
        if (bf_ptp_get_tod_cfg(idx, &enabled, &even_pps, &sync_disabled) != 0) {
            enabled = false; even_pps = false; sync_disabled = false;
        }

        /* Step C API: Fetch historical write completion metrics from CB block */
        if (bf_ptp_get_tod_write_counter(idx, &write_counter) != 0) {
            write_counter = 0;
        }

        if (bf_ptp_get_tod_read_counter(idx, &read_counter) != 0) {
            read_counter = 0;
        }

        /* Step D API: Execute atomic 10-byte burst snapshot capture across CC block */
        if (bf_ptp_get_tod_time(idx, &seconds, &nanoseconds) != 0) {
            seconds = 0; nanoseconds = 0;
        }

        /* Step E: Stream the fully resolved telemetry metrics row to the CLI */
        /* Determine DPLL driving mode label for all DPLLs (not just 5/6) */
        const char *sync_mode_str = "PLL";
        {
            enum pll_mode op_mode;
            if (bf_ptp_get_dpll_mode(clk_src, &op_mode) == 0) {
                sync_mode_str = dpll_mode_op_str(op_mode);
            }
        }

        char idx_str[16];
        char clk_heart_buf[64];
        char enabled_str[16];
        char pps_mode_str[32];
        char out_sync_str[16];
        char write_cnt_str[16];
        char read_cnt_str[16];
        char tod_time_str[64];

        snprintf(idx_str, sizeof(idx_str), "%d", idx);
        snprintf(clk_heart_buf, sizeof(clk_heart_buf), "DPLL%d (%s)", clk_src, sync_mode_str);
        snprintf(enabled_str, sizeof(enabled_str), "%s", enabled ? "TRUE" : "FALSE");
        snprintf(pps_mode_str, sizeof(pps_mode_str), "%s", even_pps ? "2-Sec EVENT" : "1-Sec STD");
        snprintf(out_sync_str, sizeof(out_sync_str), "%s", sync_disabled ? "DISABLED" : "ENABLED");
        snprintf(write_cnt_str, sizeof(write_cnt_str), "0x%02X", write_counter);
        snprintf(read_cnt_str, sizeof(read_cnt_str), "0x%02X", read_counter);
        snprintf(tod_time_str, sizeof(tod_time_str), "%lu.%09lu sec", seconds, nanoseconds);

        aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s | %-25s\n",
                   idx_str, clk_heart_buf, enabled_str, pps_mode_str, out_sync_str, write_cnt_str, read_cnt_str, tod_time_str);
    }
    aim_printf(&uc->pvs, " %-5s | %-16s | %-16s | %-16s | %-8s | %-8s | %-8s | %-25s\n",
               "-----", "----------------", "----------------", "----------------", "--------", "--------", "--------", "-------------------------");
    aim_printf (&uc->pvs, "\n");

    return 0;
}

/* @brief Detailed DPLL configuration & state matrix.
 *        Expands on the `ptp` summary with SM_MODE and frequency offset (ppb). */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__dpll (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "dpll", -1,
                       "dpll [<dpll_index: 0~7>]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    uint8_t dpll_start = 0, dpll_end = 7;
    if (uc->pargs->count == 1) {
        uint32_t idx = strtoul (uc->pargs->args[0], NULL, 0);
        if (idx > 7) {
            aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid: 0~7)\n", idx);
            return 0;
        }
        dpll_start = dpll_end = (uint8_t)idx;
    } else if (uc->pargs->count > 1) {
        aim_printf (&uc->pvs, "Usage: dpll [<dpll_index: 0~7>]\n");
        return 0;
    }

    aim_printf (&uc->pvs, "\n==========================================================================================================================================================\n");
    aim_printf (&uc->pvs, "                                    8A34004 DPLL CONFIGURATION & STATE MATRIX (DETAILED)                                     \n");
    aim_printf (&uc->pvs, "----------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    aim_printf (&uc->pvs, " %-5s | %-6s | %-14s | %-10s | %-16s | %-16s | %-16s | %-8s | %-8s\n",
                "DPLL#", "OP", "SM_MODE", "LIVE", "REF CLK", "FREQ OFFSET (ppb)", "PHASE ERROR (ns)", "ToD SYNC", "ToD SRC");
    aim_printf (&uc->pvs, " %-5s | %-6s | %-14s | %-10s | %-16s | %-16s | %-16s | %-8s | %-8s\n",
                "-----", "------", "--------------", "----------", "----------------", "----------------", "----------------", "--------", "--------");

    for (uint8_t i = dpll_start; i <= dpll_end; i++) {
        enum pll_mode  op_mode = 0;
        enum pll_state sm_mode = 0;
        enum dpll_state dpll_state = 0;
        bool sync_en = false;
        uint8_t tod_src = 0, dpll_ref = 0;
        double phase_ns = 0.0;
        int64_t fcw = 0;

        if (bf_ptp_get_dpll_mode(i, &op_mode) != 0) continue;
        if (bf_ptp_get_dpll_sm(i, &sm_mode) != 0) continue;
        if (bf_ptp_get_dpll_state(i, &dpll_state) != 0) continue;
        if (bf_ptp_get_dpll_ref_stat(i, &dpll_ref) != 0) continue;
        if (bf_ptp_get_dpll_tod_sync_cfg(i, &sync_en, &tod_src) != 0) continue;
        if (bf_ptp_get_dpll_phase_status(i, &phase_ns) != 0) continue;
        /* Frequency fetch is best-effort (may fail for OFF or uninitialised DPLLs) */
        bf_ptp_get_dpll_freq(i, &fcw);

        if (op_mode == PLL_MODE_DISABLED) continue; /* OFF — skip */

        const char *op_str      = dpll_mode_op_str(op_mode);
        const char *sm_str      = dpll_mode_sm_str(sm_mode);
        const char *live_str    = dpll_state_str(i, dpll_state);
        const char *ref_str     = dpll_refclk_str(dpll_ref);
        const char *sync_str    = sync_en ? "ENABLED" : "DISABLED";

        /* FCW (Q5.3) → ppb:  ppb = fcw * 1e9 / 2^53 */
        char freq_str[32];
        double fcw_ppb = (double)fcw * 1e9 / (double)(1LL << 53);
        if (fcw == 0 && op_mode != PLL_MODE_WRITE_FREQUENCY)
            snprintf(freq_str, sizeof(freq_str), "---");
        else
            snprintf(freq_str, sizeof(freq_str), "% .3f", fcw_ppb);

        aim_printf (&uc->pvs, " %-5d | %-6s | %-14s | %-10s | %-16s | %-16s | % 15.9f | %-8s | ToD%-6d\n",
                    i, op_str, sm_str, live_str, ref_str, freq_str, phase_ns, sync_str, tod_src);
    }
    aim_printf (&uc->pvs, " %-5s | %-6s | %-14s | %-10s | %-16s | %-16s | %-16s | %-8s | %-8s\n",
                "-----", "------", "--------------", "----------", "----------------", "----------------", "----------------", "--------", "--------");
    aim_printf (&uc->pvs, "\n");

    return 0;
}

/* @brief Enable or disable ToD synchronization on a specified DPLL block.
 *        Preserves the current ToD source index so only the enable bit is toggled. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__dpll_tod (
    ucli_context_t *uc)
{
    uint32_t dpll_index = 0;
    uint32_t enable_val = 0;
    bool current_enabled = false;
    uint8_t tod_source = 1; /* Default to ToD1 as fallback */

    UCLI_COMMAND_INFO (uc, "dpll-tod", -1,
                       "dpll-tod <dpll_index: 0~7> <0|1>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 2) {
        aim_printf (&uc->pvs, "Usage: dpll-tod <dpll_index: 0~7> <0|1>\n");
        return 0;
    }

    dpll_index = strtoul (uc->pargs->args[0], NULL, 0);
    enable_val = strtoul (uc->pargs->args[1], NULL, 0);

    if (dpll_index > 7) {
        aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid range: 0~7)\n", dpll_index);
        return 0;
    }

    /* Retrieve current ToD source configuration from the chip first */
    if (bf_ptp_get_dpll_tod_sync_cfg((uint8_t)dpll_index, &current_enabled, &tod_source) != 0) {
        aim_printf (&uc->pvs, "Warning: Failed to retrieve DPLL%d ToD configuration, defaulting to ToD%d.\n", dpll_index, dpll_index == 6 ? 1 : 0);
        tod_source = (dpll_index == 6) ? 1 : 0;
    }

    bool enable_sync = (enable_val != 0);

    int rc = bf_ptp_set_dpll_tod_sync_cfg((uint8_t)dpll_index, enable_sync, tod_source);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to configure DPLL%d ToD synchronization (error code: %d)\n", dpll_index, rc);
    } else {
        aim_printf (&uc->pvs, "Successfully %s ToD synchronization for DPLL%d (Source Preserved: ToD%d)\n",
                    enable_sync ? "ENABLED" : "DISABLED", dpll_index, tod_source);
    }

    return 0;
}

/* @brief Configure the primary or secondary combo slave source for a DPLL.
 *        Controls enable, filtered mode, and source DPLL index (0-8, where 8 = SYSDPLL). */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__dpll_combo (
    ucli_context_t *uc)
{
    uint32_t dpll_index = 0;
    const char *source_type = NULL;
    uint32_t enable_val = 0;
    uint32_t filtered_val = 0;
    uint32_t src_dpll_idx = 0;

    UCLI_COMMAND_INFO (uc, "dpll-combo", -1,
                       "dpll-combo <dpll_index: 0~7> <pri|sec> <enable: 0|1> <filtered: 0|1> <src_dpll_idx: 0~8>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 5) {
        aim_printf (&uc->pvs, "Usage: dpll-combo <dpll_index: 0~7> <pri|sec> <enable: 0|1> <filtered: 0|1> <src_dpll_idx: 0~8>\n");
        return 0;
    }

    dpll_index = strtoul (uc->pargs->args[0], NULL, 0);
    source_type = uc->pargs->args[1];
    enable_val = strtoul (uc->pargs->args[2], NULL, 0);
    filtered_val = strtoul (uc->pargs->args[3], NULL, 0);
    src_dpll_idx = strtoul (uc->pargs->args[4], NULL, 0);

    if (dpll_index > 7) {
        aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid range: 0~7)\n", dpll_index);
        return 0;
    }

    if (strcmp(source_type, "pri") != 0 && strcmp(source_type, "sec") != 0) {
        aim_printf (&uc->pvs, "Invalid source type: '%s' (Must be 'pri' or 'sec')\n", source_type);
        return 0;
    }

    if (src_dpll_idx > 8) {
        aim_printf (&uc->pvs, "Invalid src_dpll_idx: %d (Valid range: 0~8)\n", src_dpll_idx);
        return 0;
    }

    bool enable_sync = (enable_val != 0);
    bool filtered = (filtered_val != 0);
    int rc = 0;

    if (strcmp(source_type, "pri") == 0) {
        rc = bf_ptp_set_dpll_combo_slave_pri_cfg((uint8_t)dpll_index, enable_sync, filtered, (uint8_t)src_dpll_idx);
    } else {
        rc = bf_ptp_set_dpll_combo_slave_sec_cfg((uint8_t)dpll_index, enable_sync, filtered, (uint8_t)src_dpll_idx);
    }

    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to configure DPLL%d combo %s source (error code: %d)\n", dpll_index, source_type, rc);
    } else {
        aim_printf (&uc->pvs, "Successfully configured DPLL%d combo %s source (Enabled: %s, Filtered: %s, Source: DPLL%d)\n",
                    dpll_index, source_type,
                    enable_sync ? "TRUE" : "FALSE",
                    filtered ? "TRUE" : "FALSE",
                    src_dpll_idx);
    }

    return 0;
}

/* @brief Set the operational mode and state machine mode of a DPLL.
 *        op_mode: pll|phase|freq|gpio|synth|pmeas|off;  sm_mode: auto|force_lock|force_frun|force_hldover. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__dpll_mode (
    ucli_context_t *uc)
{
    uint32_t dpll_index = 0;
    const char *op_mode_str = NULL;
    const char *sm_mode_str = NULL;
    enum pll_mode  op_mode = 0;
    enum pll_state sm_mode = 0;

    UCLI_COMMAND_INFO (uc, "dpll-mode", -1,
                       "dpll-mode <dpll_index: 0~7> <op_mode: pll|phase|freq|gpio|synth|pmeas|off> <sm_mode: auto|force_lock|force_frun|force_hldover>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 3) {
        aim_printf (&uc->pvs, "Usage: dpll-mode <dpll_index: 0~7> <op_mode: pll|phase|freq|gpio|synth|pmeas|off> <sm_mode: auto|force_lock|force_frun|force_hldover>\n");
        return 0;
    }

    dpll_index = strtoul (uc->pargs->args[0], NULL, 0);
    op_mode_str = uc->pargs->args[1];
    sm_mode_str = uc->pargs->args[2];

    if (dpll_index > 7) {
        aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid range: 0~7)\n", dpll_index);
        return 0;
    }

    /* Parse operational mode string */
    if (strcmp(op_mode_str, "pll") == 0)       op_mode = PLL_MODE_PLL;
    else if (strcmp(op_mode_str, "phase") == 0) op_mode = PLL_MODE_WRITE_PHASE;
    else if (strcmp(op_mode_str, "freq") == 0)  op_mode = PLL_MODE_WRITE_FREQUENCY;
    else if (strcmp(op_mode_str, "gpio") == 0)  op_mode = PLL_MODE_GPIO_INC_DEC;
    else if (strcmp(op_mode_str, "synth") == 0) op_mode = PLL_MODE_SYNTHESIS;
    else if (strcmp(op_mode_str, "pmeas") == 0) op_mode = PLL_MODE_PHASE_MEASUREMENT;
    else if (strcmp(op_mode_str, "off") == 0)   op_mode = PLL_MODE_DISABLED;
    else {
        aim_printf (&uc->pvs, "Invalid op_mode: '%s' (Valid: pll|phase|freq|gpio|synth|pmeas|off)\n", op_mode_str);
        return 0;
    }

    /* Parse state machine mode string */
    if (strcmp(sm_mode_str, "auto") == 0)          sm_mode = PLL_STATE_AUTO;
    else if (strcmp(sm_mode_str, "force_lock") == 0)   sm_mode = PLL_STATE_FORCE_LOCK;
    else if (strcmp(sm_mode_str, "force_frun") == 0)   sm_mode = PLL_STATE_FORCE_FRUN;
    else if (strcmp(sm_mode_str, "force_hldover") == 0) sm_mode = PLL_STATE_FORCE_HOLDOVER;
    else {
        aim_printf (&uc->pvs, "Invalid sm_mode: '%s' (Valid: auto|force_lock|force_frun|force_hldover)\n", sm_mode_str);
        return 0;
    }

    int rc = bf_ptp_set_dpll_mode((uint8_t)dpll_index, op_mode);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to set DPLL%d op_mode (error code: %d)\n", dpll_index, rc);
    } else {
        rc = bf_ptp_set_dpll_sm((uint8_t)dpll_index, sm_mode);
        if (rc != 0) {
            aim_printf (&uc->pvs, "Failed to set DPLL%d sm_mode (error code: %d)\n", dpll_index, rc);
        } else {
            /* Build the full mode byte to feed back through the existing decoder helpers */
            aim_printf (&uc->pvs, "Successfully set DPPL%d mode: OP=%s, SM=%s\n",
                        dpll_index,
                        dpll_mode_op_str(op_mode),
                        dpll_mode_sm_str(sm_mode));
        }
    }

    return 0;
}

/* @brief Write the DPLL frequency control word (FCW) in FFO Q5.3 format.
 *        A value of 0 represents the nominal frequency (zero-ppm offset). */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__dpll_freq (
    ucli_context_t *uc)
{
    uint32_t dpll_index = 0;
    int64_t fcw_val = 0;

    UCLI_COMMAND_INFO (uc, "dpll-freq", -1,
                       "dpll-freq <dpll_index: 0~7> <fcw_ffo_q53>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 2) {
        aim_printf (&uc->pvs, "Usage: dpll-freq <dpll_index: 0~7> <fcw_ffo_q53>\n");
        return 0;
    }

    dpll_index = strtoul (uc->pargs->args[0], NULL, 0);
    fcw_val = strtoll (uc->pargs->args[1], NULL, 0);

    if (dpll_index > 7) {
        aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid range: 0~7)\n", dpll_index);
        return 0;
    }

    int rc = bf_ptp_set_dpll_freq((uint8_t)dpll_index, fcw_val);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to set DPLL%d frequency (error code: %d)\n", dpll_index, rc);
    } else {
        aim_printf (&uc->pvs, "Successfully set DPLL%d FCW to %lld (0x%llX)\n", dpll_index,
                    (long long)fcw_val, (long long)fcw_val);
    }

    return 0;
}

/* @brief Focused Time-of-Day (ToD) multi-axis status with PRIMARY + SECONDARY snapshots. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__tod (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "tod", -1,
                       "tod [<tod_index: 0~3>]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    uint8_t tod_start = 0, tod_end = 3;
    if (uc->pargs->count == 1) {
        uint32_t idx = strtoul (uc->pargs->args[0], NULL, 0);
        if (idx > 3) {
            aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid: 0~3)\n", idx);
            return 0;
        }
        tod_start = tod_end = (uint8_t)idx;
    } else if (uc->pargs->count > 1) {
        aim_printf (&uc->pvs, "Usage: tod [<tod_index: 0~3>]\n");
        return 0;
    }

    aim_printf (&uc->pvs, "\n======================================================================================================================================================================\n");
    aim_printf (&uc->pvs, "                                    8A34004 TIME-OF-DAY (ToD) MULTI-AXIS STATUS (DETAILED)                                     \n");
    aim_printf (&uc->pvs, "----------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    aim_printf (&uc->pvs, " %-5s | %-22s | %-8s | %-12s | %-8s | %-8s | %-8s | %-25s | %-25s\n",
                "ToD#", "DRIVEN BY", "ENABLED", "PPS MODE", "OUT_SYNC", "WR_CNTR", "RD_CNTR", "PRIMARY WALL-TIME", "SECONDARY WALL-TIME");
    aim_printf (&uc->pvs, " %-5s | %-22s | %-8s | %-12s | %-8s | %-8s | %-8s | %-25s | %-25s\n",
                "-----", "----------------------", "--------", "------------", "--------", "--------", "--------", "-------------------------", "-------------------------");

    for (uint8_t idx = tod_start; idx <= tod_end; idx++) {
        uint8_t clk_src = 0, write_counter = 0, read_counter = 0;
        bool enabled = false, even_pps = false, sync_disabled = false;
        uint64_t pri_sec = 0, pri_ns = 0, sec_sec = 0, sec_ns = 0;

        /* Clock source */
        if (bf_ptp_get_tod_clk_src(idx, &clk_src) != 0) clk_src = 0;

        /* Configuration switches */
        if (bf_ptp_get_tod_cfg(idx, &enabled, &even_pps, &sync_disabled) != 0) {
            enabled = false; even_pps = false; sync_disabled = false;
        }

        /* Write/read counters */
        if (bf_ptp_get_tod_write_counter(idx, &write_counter) != 0) write_counter = 0;
        if (bf_ptp_get_tod_read_counter(idx, &read_counter) != 0) read_counter = 0;

        /* PRIMARY snapshot */
        if (bf_ptp_get_tod_time(idx, &pri_sec, &pri_ns) != 0) { pri_sec = 0; pri_ns = 0; }

        /* SECONDARY snapshot (best-effort — may be empty/invalid if not armed) */
        int sec_rc = bf_ptp_get_tod_time_triggered(idx, &sec_sec, &sec_ns);

        /* Driving DPLL mode label */
        const char *sync_mode_str = "PLL";
        enum pll_mode op_mode;
        if (bf_ptp_get_dpll_mode(clk_src, &op_mode) == 0)
            sync_mode_str = dpll_mode_op_str(op_mode);

        char clk_buf[64], pri_buf[64], sec_buf[64];
        snprintf(clk_buf, sizeof(clk_buf), "DPLL%d (%s)", clk_src, sync_mode_str);
        snprintf(pri_buf, sizeof(pri_buf), "%lu.%09lu sec", pri_sec, pri_ns);
        if (sec_rc == 0)
            snprintf(sec_buf, sizeof(sec_buf), "%lu.%09lu sec", sec_sec, sec_ns);
        else
            snprintf(sec_buf, sizeof(sec_buf), "--- (not armed)");

        aim_printf (&uc->pvs, " %-5d | %-22s | %-8s | %-12s | %-8s | 0x%02X    | 0x%02X    | %-25s | %-25s\n",
                    idx, clk_buf,
                    enabled ? "TRUE" : "FALSE",
                    even_pps ? "2-Sec EVENT" : "1-Sec STD",
                    sync_disabled ? "DISABLED" : "ENABLED",
                    write_counter, read_counter,
                    pri_buf, sec_buf);
    }
    aim_printf (&uc->pvs, " %-5s | %-22s | %-8s | %-12s | %-8s | %-8s | %-8s | %-25s | %-25s\n",
                "-----", "----------------------", "--------", "------------", "--------", "--------", "--------", "-------------------------", "-------------------------");
    aim_printf (&uc->pvs, "\n");

    return 0;
}

/* @brief Enable or disable a specified ToD instance.
 *        Preserves the current PPS mode and output-sync configuration. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__tod_en (
    ucli_context_t *uc)
{
    uint32_t tod_index = 0;
    uint32_t enable_val = 0;
    bool current_enabled = false;
    bool current_even_pps = false;
    bool current_sync_disabled = false;

    UCLI_COMMAND_INFO (uc, "tod-en", -1,
                       "tod-en <tod_index: 0~3> <0|1>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 2) {
        aim_printf (&uc->pvs, "Usage: tod-en <tod_index: 0~3> <0|1>\n");
        return 0;
    }

    tod_index = strtoul (uc->pargs->args[0], NULL, 0);
    enable_val = strtoul (uc->pargs->args[1], NULL, 0);

    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    /* Retrieve current ToD configuration from the chip first to preserve other fields */
    if (bf_ptp_get_tod_cfg((uint8_t)tod_index, &current_enabled, &current_even_pps, &current_sync_disabled) != 0) {
        aim_printf (&uc->pvs, "Failed to retrieve current ToD%d configuration from the chip.\n", tod_index);
        return 0;
    }

    bool enable_tod = (enable_val != 0);

    int rc = bf_ptp_set_tod_cfg((uint8_t)tod_index, enable_tod, current_even_pps, current_sync_disabled);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to configure ToD%d state (error code: %d)\n", tod_index, rc);
    } else {
        aim_printf (&uc->pvs, "Successfully %s ToD%d (PPS Mode: %s, Out Sync: %s)\n",
                    enable_tod ? "ENABLED" : "DISABLED",
                    tod_index,
                    current_even_pps ? "2-Sec even" : "1-Sec std",
                    current_sync_disabled ? "DISABLE" : "ENABLE");
    }

    return 0;
}

/* @brief Set the absolute wall-clock time (seconds + nanoseconds) of a ToD instance.
 *        Preloads sub-ns/ns/sec into the WRITE shadow registers then triggers the hardware commit. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__tod_set (
    ucli_context_t *uc)
{
    uint32_t tod_index = 0;
    uint64_t seconds = 0;
    uint64_t nanoseconds = 0;

    UCLI_COMMAND_INFO (uc, "tod-set", -1,
                       "tod-set <tod_index: 0~3> <seconds> <nanoseconds>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }

    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count != 3) {
        aim_printf (&uc->pvs, "Usage: tod-set <tod_index: 0~3> <seconds> <nanoseconds>\n");
        return 0;
    }

    tod_index = strtoul (uc->pargs->args[0], NULL, 0);
    seconds = strtoul (uc->pargs->args[1], NULL, 0);
    nanoseconds = strtoul (uc->pargs->args[2], NULL, 0);

    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    int rc = bf_ptp_set_tod_time((uint8_t)tod_index, seconds, nanoseconds);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to set ToD%d time (error code: %d)\n", tod_index, rc);
    } else {
        aim_printf (&uc->pvs, "Successfully set ToD%d to %lu.%09lu sec\n", tod_index, seconds, nanoseconds);
    }

    return 0;
}

/* @brief FW phase pull-in via DPLL PULL_IN registers (wraps bf_ptp_phase_pull_in).
 *        The 8A34004 firmware autonomously slews the DCO phase toward the
 *        target offset without a backward time jump.
 *        Preferred path for |delta| < 15 µs ToD corrections. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__phase_pull_in (
    ucli_context_t *uc)
{
    uint32_t dpll_index = 0;
    int32_t  delta_ns   = 0;
    uint32_t max_ppb    = 0;

    UCLI_COMMAND_INFO (uc, "phase-pull-in", -1,
                       "phase-pull-in <dpll_index: 0~7> <delta_ns> [max_ppb]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }
    if (uc->pargs->count < 2 || uc->pargs->count > 3) {
        aim_printf (&uc->pvs, "Usage: phase-pull-in <dpll_index: 0~7> <delta_ns> [max_ppb]\n");
        return 0;
    }

    dpll_index = strtoul (uc->pargs->args[0], NULL, 0);
    delta_ns   = (int32_t)strtol (uc->pargs->args[1], NULL, 0);
    if (uc->pargs->count == 3) {
        max_ppb = strtoul (uc->pargs->args[2], NULL, 0);
    }

    if (dpll_index > 7) {
        aim_printf (&uc->pvs, "Invalid dpll_index: %d (Valid range: 0~7)\n", dpll_index);
        return 0;
    }

    if (delta_ns > 107374182 || delta_ns < -107374182) {
        aim_printf (&uc->pvs, "delta_ns out of range: %d (Valid: -107374182~107374182 ns, i.e. ±107 ms, HW phase limit)\n",
                    delta_ns);
        return 0;
    }

    int rc = bf_ptp_phase_pull_in((uint8_t)dpll_index, delta_ns, max_ppb);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to start phase pull-in on DPLL%d (error code: %d)\n", dpll_index, rc);
    } else {
        aim_printf (&uc->pvs, "DPLL%d phase pull-in started: delta=%d ns, max_ppb=%u\n",
                    dpll_index, delta_ns, max_ppb);
    }
    return 0;
}

/* @brief Reset the 8A34004 state machine via SM_RESET command (0x5A).
 *        Firmware reinitializes over ~3 seconds; check boot_status for readiness. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__reset (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "reset", -1,
                       "Reset the ClockMatrix's state machine");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    int rc = bf_clock_reset();
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed to reset state machine (error code: %d)\n", rc);
    } else {
        aim_printf (&uc->pvs, "SM_RESET sent (0x5A). Firmware reinitializing (~3 sec)...\n");
    }

    return 0;
}

/* ── POSIX-style clock API wrappers ─────────────────────────────
 * These test bf_clock_gettime / bf_clock_settime / bf_clock_getres /
 * bf_clock_adjtime which are the target interfaces for linuxptp4. */

/* @brief Read ToD clock via bf_clock_gettime (POSIX-style). */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__gettime (
    ucli_context_t *uc)
{
    uint32_t tod_index = 2;
    struct timespec tp = { 0, 0 };

    UCLI_COMMAND_INFO (uc, "gettime", -1,
                       "gettime [<tod_index: 0~3>]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }
    if (uc->pargs->count >= 1) {
        tod_index = strtoul (uc->pargs->args[0], NULL, 0);
    }
    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    int rc = bf_clock_gettime((clockid_t)tod_index, &tp);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed (error: %d)\n", rc);
    } else {
        aim_printf (&uc->pvs, "ToD%d: %lu.%09lu sec\n", tod_index, tp.tv_sec, tp.tv_nsec);
    }
    return 0;
}

/* @brief Set ToD clock via bf_clock_settime (POSIX-style).
 *        Up to 48-bit seconds (hardware max).
 *        Defaults to tod_index=2 if omitted. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__settime (
    ucli_context_t *uc)
{
    uint32_t tod_index = 2;
    int arg_off = 0;
    struct timespec tp = { 0, 0 };

    UCLI_COMMAND_INFO (uc, "settime", -1,
                       "settime [<tod_index: 0~3>] <seconds> <nanoseconds>");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }
    if (uc->pargs->count < 2 || uc->pargs->count > 3) {
        aim_printf (&uc->pvs, "Usage: settime [<tod_index: 0~3>] <seconds> <nanoseconds>\n");
        return 0;
    }

    if (uc->pargs->count == 3) {
        tod_index = strtoul (uc->pargs->args[0], NULL, 0);
        arg_off = 1;
    }

    tp.tv_sec  = strtoll (uc->pargs->args[arg_off + 0], NULL, 0);
    tp.tv_nsec = strtol  (uc->pargs->args[arg_off + 1], NULL, 0);

    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    int rc = bf_clock_settime((clockid_t)tod_index, &tp);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed (error: %d)\n", rc);
    } else {
        aim_printf (&uc->pvs, "ToD%d set to %lu.%09lu sec\n", tod_index, tp.tv_sec, tp.tv_nsec);
    }
    return 0;
}

/* @brief Read ToD clock resolution via bf_clock_getres.
 *        Defaults to tod_index=2 if omitted. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__getres (
    ucli_context_t *uc)
{
    uint32_t tod_index = 2;
    struct timespec res = { 0, 0 };

    UCLI_COMMAND_INFO (uc, "getres", -1,
                       "getres [<tod_index: 0~3>]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }
    if (uc->pargs->count > 1) {
        aim_printf (&uc->pvs, "Usage: getres [<tod_index: 0~3>]\n");
        return 0;
    }

    if (uc->pargs->count == 1) {
        tod_index = strtoul (uc->pargs->args[0], NULL, 0);
    }
    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    int rc = bf_clock_getres((clockid_t)tod_index, &res);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed (error: %d)\n", rc);
    } else {
        aim_printf (&uc->pvs, "ToD%d resolution: %lu.%09lu sec\n", tod_index, res.tv_sec, res.tv_nsec);
    }
    return 0;
}

/* @brief Adjust clock via bf_clock_adjtime (POSIX-style).
 *        Defaults to tod_index=2 if args[0] is not a bare 0-3.
 *
 *        Sub-commands:
 *          setoffset <sec> <usec>       ADJ_SETOFFSET step
 *          frequency <scaled_ppm>       ADJ_FREQUENCY rate
 *          offset    <ns>               ADJ_OFFSET | ADJ_NANO phase
 *          read                          read-back (modes == 0) */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__adjtime (
    ucli_context_t *uc)
{
    uint32_t tod_index = 2;
    int arg_off = 0;
    struct timex tx;

    UCLI_COMMAND_INFO (uc, "adjtime", -1,
                       "adjtime [<tod_index: 0~3>] setoffset <sec> <usec> | frequency <scaled_ppm> | offset <ns> | read");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }
    if (uc->pargs->count < 1) {
        aim_printf (&uc->pvs, "Usage: adjtime [<tod_index: 0~3>] setoffset <sec> <usec> | frequency <scaled_ppm> | offset <ns> | read\n");
        return 0;
    }

    /* Detect: is args[0] a bare 0-3 (tod index) or a subcommand? */
    if (uc->pargs->count >= 2) {
        char *endptr = NULL;
        unsigned long val = strtoul (uc->pargs->args[0], &endptr, 0);
        if (endptr && *endptr == '\0' && val <= 3) {
            tod_index = (uint32_t)val;
            arg_off = 1;
        }
    }
    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    memset(&tx, 0, sizeof(tx));

    const char *subcmd = uc->pargs->args[arg_off];
    int nargs = uc->pargs->count - arg_off;

    if (!strcasecmp(subcmd, "setoffset")) {
        if (nargs != 3) {
            aim_printf (&uc->pvs, "Usage: ... setoffset <sec> <usec>\n");
            return 0;
        }
        tx.modes = ADJ_SETOFFSET;
        tx.time.tv_sec  = strtol (uc->pargs->args[arg_off + 1], NULL, 0);
        tx.time.tv_usec = strtol (uc->pargs->args[arg_off + 2], NULL, 0);

    } else if (!strcasecmp(subcmd, "frequency")) {
        if (nargs != 2) {
            aim_printf (&uc->pvs, "Usage: ... frequency <scaled_ppm>\n");
            return 0;
        }
        tx.modes = ADJ_FREQUENCY;
        tx.freq = strtoll (uc->pargs->args[arg_off + 1], NULL, 0);

    } else if (!strcasecmp(subcmd, "offset")) {
        if (nargs != 2) {
            aim_printf (&uc->pvs, "Usage: ... offset <ns>\n");
            return 0;
        }
        tx.modes = ADJ_OFFSET | ADJ_NANO;
        tx.offset = strtoll (uc->pargs->args[arg_off + 1], NULL, 0);

    } else if (!strcasecmp(subcmd, "read")) {
        tx.modes = 0;

    } else {
        aim_printf (&uc->pvs, "Unknown sub-command: %s\n", subcmd);
        return 0;
    }

    int rc = bf_clock_adjtime((clockid_t)tod_index, &tx);
    if (rc != 0) {
        aim_printf (&uc->pvs, "Failed (error: %d)\n", rc);
    } else {
        aim_printf (&uc->pvs, "ok\n");
    }
    return 0;
}

#define TEST(label, expr) do {                                      \
    aim_printf (&uc->pvs, "  %-36s ", label);                       \
    int _rc = (expr);                                                \
    if (_rc == 0) { aim_printf (&uc->pvs, "[PASS]\n"); pass++; }     \
    else { aim_printf (&uc->pvs, "[FAIL] rc=%d\n", _rc); fail++; }   \
} while(0)

/* @brief Automated smoke test for bf_clock_adjtime covering all sub-modes
 *        exercised by ptp4l (ADJ_SETOFFSET, ADJ_FREQUENCY, ADJ_OFFSET, read).
 *        Seeds ToD with a known epoch, runs each mode, reports PASS/FAIL,
 *        then restores the original time. */
static ucli_status_t
bf_pltfm_clockmatrix_ucli_ucli__adjtest (
    ucli_context_t *uc)
{
    uint32_t tod_index = 2;
    struct timespec tp, saved;
    struct timex tx;
    int pass = 0, fail = 0;

    UCLI_COMMAND_INFO (uc, "adjtest", -1,
                       "adjtest [<tod_index: 0~3>]");

    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        aim_printf (&uc->pvs, "\nNot supported on this device!\n");
        return 0;
    }
    if (! (bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED)) {
        aim_printf (&uc->pvs, "\nPTP board not installed!\n");
        return 0;
    }

    if (uc->pargs->count >= 1)
        tod_index = strtoul (uc->pargs->args[0], NULL, 0);
    if (tod_index > 3) {
        aim_printf (&uc->pvs, "Invalid tod_index: %d (Valid range: 0~3)\n", tod_index);
        return 0;
    }

    /* Save the current time so we can restore it at the end. */
    if (bf_clock_gettime((clockid_t)tod_index, &saved) < 0) {
        aim_printf (&uc->pvs, "FATAL: cannot read ToD%d\n", tod_index);
        return 0;
    }

    aim_printf (&uc->pvs, "\n=== bf_clock_adjtime smoke test (ToD%d) ===\n", tod_index);
    aim_printf (&uc->pvs, "Saved: %lu.%09lu\n\n", saved.tv_sec, saved.tv_nsec);

    /* ── 1. ADJ_SETOFFSET (bf_clockadj_step path) ─────────────── */
    aim_printf (&uc->pvs, "-- ADJ_SETOFFSET --\n");

    tp.tv_sec = 1752441600; tp.tv_nsec = 0;
    bf_clock_settime((clockid_t)tod_index, &tp);

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_SETOFFSET;
    tx.time.tv_sec = 1; tx.time.tv_usec = 0;
    TEST("setoffset +1s", bf_clock_adjtime((clockid_t)tod_index, &tx));

    bf_clock_gettime((clockid_t)tod_index, &tp);
    aim_printf (&uc->pvs, "           -> %lu.%09lu (expect %lu)\n",
                tp.tv_sec, tp.tv_nsec, 1752441601UL);

    tp.tv_sec = 1752441600; tp.tv_nsec = 0;
    bf_clock_settime((clockid_t)tod_index, &tp);

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_SETOFFSET;
    tx.time.tv_sec = -1; tx.time.tv_usec = 500000;
    TEST("setoffset -1s +500ms (-500ms)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tp.tv_sec = 1752441600; tp.tv_nsec = 0;
    bf_clock_settime((clockid_t)tod_index, &tp);

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_SETOFFSET;
    tx.time.tv_sec = 1000; tx.time.tv_usec = 0;
    TEST("setoffset +1000s", bf_clock_adjtime((clockid_t)tod_index, &tx));

    /* ── 2. ADJ_FREQUENCY (PI servo tracking path) ────────────── */
    aim_printf (&uc->pvs, "\n-- ADJ_FREQUENCY --\n");

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_FREQUENCY;
    tx.freq = 65536;
    TEST("frequency +1 ppm", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.freq = -32768;
    TEST("frequency -0.5 ppm", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.freq = 6553600;
    TEST("frequency +100 ppm", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.freq = 65536000;
    TEST("frequency +1000 ppm (max)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.freq = -65536000;
    TEST("frequency -1000 ppm (min)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.freq = 65536001;
    TEST("frequency +1001 ppm (expect fail)",
         bf_clock_adjtime((clockid_t)tod_index, &tx) != 0 ? 0 : -1);

    /* ── 3. ADJ_OFFSET (phase pull-in / ToD step) ─────────────── */
    aim_printf (&uc->pvs, "\n-- ADJ_OFFSET | ADJ_NANO --\n");

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_OFFSET | ADJ_NANO;
    tx.offset = 500;
    TEST("offset +500 ns (FW pull-in)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.offset = -2000;
    TEST("offset -2 us (FW pull-in)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.offset = 1000000;
    TEST("offset +1 ms (ToD step)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    tx.offset = 0;
    TEST("offset 0 (no-op)", bf_clock_adjtime((clockid_t)tod_index, &tx));

    /* ── 4. read (modes == 0) ─────────────────────────────────── */
    aim_printf (&uc->pvs, "\n-- READ (modes == 0) --\n");

    memset(&tx, 0, sizeof(tx));
    tx.modes = 0;
    TEST("read", bf_clock_adjtime((clockid_t)tod_index, &tx));

    /* ── 5. Epoch guardrail ───────────────────────────────────── */
    aim_printf (&uc->pvs, "\n-- Epoch guardrail --\n");

    tp.tv_sec = 5; tp.tv_nsec = 0;
    bf_clock_settime((clockid_t)tod_index, &tp);

    memset(&tx, 0, sizeof(tx));
    tx.modes = ADJ_SETOFFSET;
    tx.time.tv_sec = -10; tx.time.tv_usec = 0;
    TEST("setoffset -10s @ T=5s (expect fail)",
         bf_clock_adjtime((clockid_t)tod_index, &tx) != 0 ? 0 : -1);

    /* ── Restore ──────────────────────────────────────────────── */
    bf_clock_settime((clockid_t)tod_index, &saved);
    aim_printf (&uc->pvs, "\nRestored: %lu.%09lu\n", saved.tv_sec, saved.tv_nsec);
    aim_printf (&uc->pvs, "=== %d pass, %d fail ===\n\n", pass, fail);

#undef TEST
    return 0;
}

static ucli_command_handler_f
bf_pltfm_clockmatrix_ucli_ucli_handlers__[] = {
    bf_pltfm_clockmatrix_ucli_ucli__set_clk,
    bf_pltfm_clockmatrix_ucli_ucli__bsync,
    bf_pltfm_clockmatrix_ucli_ucli__devts,
    bf_pltfm_clockmatrix_ucli_ucli__devts_cnt,
    bf_pltfm_clockmatrix_ucli_ucli__bsync_cnt,
    bf_pltfm_clockmatrix_ucli_ucli__tx_rst_pulse,
    bf_pltfm_clockmatrix_ucli_ucli__clkobs_strength_set__,

    bf_pltfm_clockmatrix_ucli_ucli__page,
    bf_pltfm_clockmatrix_ucli_ucli__reg,
    bf_pltfm_clockmatrix_ucli_ucli__ptp,

    bf_pltfm_clockmatrix_ucli_ucli__dpll,
    bf_pltfm_clockmatrix_ucli_ucli__dpll_tod,
    bf_pltfm_clockmatrix_ucli_ucli__dpll_combo,
    bf_pltfm_clockmatrix_ucli_ucli__dpll_mode,
    bf_pltfm_clockmatrix_ucli_ucli__dpll_freq,

    bf_pltfm_clockmatrix_ucli_ucli__tod,
    bf_pltfm_clockmatrix_ucli_ucli__tod_en,
    bf_pltfm_clockmatrix_ucli_ucli__tod_set,

    bf_pltfm_clockmatrix_ucli_ucli__reset,
    bf_pltfm_clockmatrix_ucli_ucli__gettime,
    bf_pltfm_clockmatrix_ucli_ucli__settime,
    bf_pltfm_clockmatrix_ucli_ucli__getres,
    bf_pltfm_clockmatrix_ucli_ucli__adjtime,
    bf_pltfm_clockmatrix_ucli_ucli__adjtest,

    bf_pltfm_clockmatrix_ucli_ucli__phase_pull_in,

    NULL
};

static ucli_module_t bf_pltfm_clockmatrix_ucli_module__
= {
    "bf_pltfm_clockmatrix_ucli",
    NULL,
    bf_pltfm_clockmatrix_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_pltfm_clockmatrix_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_clockmatrix_ucli_module__);
    n = ucli_node_create ("clockmatrix", m,
                          &bf_pltfm_clockmatrix_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("clockmatrix"));
    return n;
}

