/*!
 * @file bf_pltfm_clockmatrix.c
 * @date 2026/07/08
 *
 * Hang Tsi (tsihang@asterfusion.com)
 *
 *
 */

#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_clockmatrix.h>

int bf_pltfm_read_ptp_reg(uint8_t page, uint8_t reg, uint8_t* value) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = CLOCKMATRIX_ADDR;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_read_block (ptp_addr, reg, value, 1)) {
        fprintf (stdout, "\nFailed to read PTP reg.\n");
        rc = -3;
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_write_ptp_reg(uint8_t page, uint8_t reg, uint8_t value) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = CLOCKMATRIX_ADDR;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, reg, value)) {
        fprintf (stdout, "\nFailed to write PTP reg.\n");
        rc = -3;
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_write_ptp_reg_burst(uint8_t page, uint8_t reg, uint8_t *value, size_t l) {
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = CLOCKMATRIX_ADDR;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }
#if 0
    for (size_t i = 0; i < l; i++) {
        if (bf_pltfm_cp2112_reg_write_byte(ptp_addr, reg + i, value[i]) < 0) {
            fprintf (stdout, "\nFailed to write PTP reg.\n");
            return -3;
        }
    }
#else
    if (bf_pltfm_cp2112_reg_write_block(ptp_addr, reg, value, l)) {
        rc = -3;
    }
#endif
finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_read_ptp_reg_burst(uint8_t page, uint8_t reg, uint8_t *value, size_t l)
{
    int rc = 0;
    uint8_t pca9548_addr = BF_MAV_MASTER_PCA9548_ADDR74;
    uint8_t ptp_addr = CLOCKMATRIX_ADDR;

    MASTER_I2C_LOCK;
    // Select channel 5 to access PTP module
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x20)) {
        fprintf (stdout, "\nFailed to select PTP.\n");
        rc = -1;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_write_byte (ptp_addr, 0xFD, page)) {
        fprintf (stdout, "\nFailed to select PTP page.\n");
        rc = -2;
        goto finish;
    }

    if (bf_pltfm_cp2112_reg_read_block (ptp_addr, reg, value, l)) {
        fprintf (stdout, "\nFailed to read PTP reg.\n");
        rc = -3;
    }

finish:
    if (bf_pltfm_cp2112_reg_write_byte (pca9548_addr, 0x00, 0x00)) {
        fprintf (stdout, "\nFailed to de-select PTP.\n");
        rc = -4;
    }

    MASTER_I2C_UNLOCK;
    return rc;
}

// for X732Q-T-V2.0 only
int bf_pltfm_set_clk(bf_pltfm_clk_source clk_source)
{
    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0))
       goto finish;

    if (clk_source == CLK_SMU) {
        // absent = ptp absent ? see 0x18
        // source selected ? see 0x19
        if(bf_pltfm_mgr_ctx()->flags & AF_PLAT_MNTR_PTPX_INSTALLED) {
            if (bf_pltfm_cpld_write_byte (1, 0x19, 0x01)) {
                fprintf (stdout, "\nFailed to write CPLD.\n");
                return -1;
            }
            bf_pltfm_mgr_ctx()->flags |= AF_PLAT_CTRL_CLK_SMU;
        }
    } else {
        // source selected ? see 0x19
        if (bf_pltfm_cpld_write_byte (1, 0x19, 0x02)) {
            fprintf (stdout, "\nFailed to write CPLD.\n");
            return -1;
        }
        bf_pltfm_mgr_ctx()->flags &= ~AF_PLAT_CTRL_CLK_SMU;
    }

finish:
    fprintf (stdout, "\nCLK=%s\n\n",
            (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CLK_SMU) ? "CLK_SMU" : "CLK_CRYSTAL");
    return 0;
}

int bf_pltfm_tx_rst_pulse()
{
    uint8_t bsync_ctrl, val = 0;

    if(!(bf_pltfm_mgr_ctx()->flags &
            AF_PLAT_CTRL_CLK_SMU)) {
        return 0;
    }

    // 0x1A[1] is not neccessary if bsync_tx_rst_pulse is triggered by software.

    // bsync_tx_rst_pulse, see 0x1A[0]
    bsync_ctrl = 0x03;
    if (bf_pltfm_cpld_write_byte (1, 0x1A, bsync_ctrl)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bf_pltfm_cpld_read_byte (1, 0x1A, &val)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bsync_ctrl != val) {
        fprintf(stdout, "%d %x != %x\n", __LINE__, bsync_ctrl, val);
    }

    //bf_sys_usleep (500);

    bsync_ctrl = 0x02;
    if (bf_pltfm_cpld_write_byte (1, 0x1A, bsync_ctrl)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bf_pltfm_cpld_read_byte (1, 0x1A, &val)) {
        fprintf (stdout, "\nFailed to write CPLD.\n");
        return -1;
    }
    if (bsync_ctrl != val) {
        fprintf(stdout, "%d %x != %x\n", __LINE__, bsync_ctrl, val);
    }

    return 0;
}

/**
 * @brief Reads the global System APLL Loss-of-Lock Live status register.
 *
 *        STATUS.SYS_APLL_STATUS (0xC03C / 0x0021) bit 0:
 *          0 = SYS_APLL_LOSS_LOCK_LIVE_LOCKED (all APLLs locked)
 *          1 = SYS_APLL_LOSS_LOCK_LIVE_UNLOCKED
 *
 * @param sys_dpll_index  Reserved for future per-DPLL filtering; unused.
 * @param state           Output pointer receiving the raw 8-bit register value.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_sys_apll_state(uint8_t sys_dpll_index, uint8_t *state)
{
    (void)sys_dpll_index;  /* global register, index reserved */
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;

    if (!state) {
        return -1;
    }

    if (bf_ptp_lookup_register_general("STATUS.SYS_APLL_STATUS",
            &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *state = (reg_byte & SYS_APLL_STATE_MASK);

    return 0;
}

/**
 * @brief Reads the global System DPLL State status register.
 *
 *        STATUS.SYS_DPLL_STATUS (0xC03C / 0x0020) bits [3:0]:
 *          0 = FREERUN, 1 = LOCKACQ, 2 = LOCKREC,
 *          3 = LOCKED, 4 = HOLDOVER, 5 = OPEN_LOOP
 *
 * @param sys_apll_index  Reserved for future per-APLL filtering; unused.
 * @param state           Output pointer receiving the extracted DPLL_SYS_STATE field [3:0] (enum dpll_state).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_sys_dpll_state(uint8_t sys_apll_index, enum dpll_state *state)
{
    (void)sys_apll_index;  /* global register, index reserved */
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;

    if (!state) {
        return -1;
    }

    if (bf_ptp_lookup_register_general("STATUS.SYS_DPLL_STATUS",
            &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *state = (reg_byte & DPLL_SYS_STATE_MASK);

    return 0;
}

/**
 * @brief Clears the BOOT_STATUS register (4 bytes) to prepare for
 *        post-reset polling.
 *
 *        GENERAL_STATUS.BOOT_STATUS at page 0xC0, base 0x14, offset 0x00.
 *
 * @return int 0 on success; negative on error.
 */
int bf_ptp_clr_boot_status(void)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[4] = {0};

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.BOOT_STATUS",
            &regdesc)) {
        return -1;
    }

    /* Write 4 zero bytes in one burst to clear the 32-bit boot status word */
    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc),
            REGADDR(regdesc), buf, 4) < 0) {
        return -2;
    }

    return 0;
}

/**
 * @brief Reads the 32-bit BOOT_STATUS register and packs it into
 *        a little-endian uint32_t.
 *
 *        After SM_RESET, the chip firmware loads from EEPROM and
 *        transitions through BOOT_STATUS; readiness is confirmed
 *        when the value reaches 0x000000A0.
 *
 * @param status Output pointer receiving the 32-bit boot status value.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_boot_status(uint32_t *status)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[4] = {0};

    if (!status) {
        return -1;
    }

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.BOOT_STATUS",
            &regdesc)) {
        return -2;
    }

    /* BOOT_STATUS is 4 bytes wide; read in one burst, LE order */
    if (bf_pltfm_read_ptp_reg_burst(REGPAGE(regdesc),
            REGADDR(regdesc), buf, 4) < 0) {
        return -3;
    }

    *status = ((uint32_t)buf[3] << 24) | ((uint32_t)buf[2] << 16)
            | ((uint32_t)buf[1] <<  8) |  (uint32_t)buf[0];

    return 0;
}

/**
 * @brief Retrieves the 16-bit Product ID from the GENERAL_STATUS registers.
 * 
 * @param product_id Output pointer populated with the 16-bit hardware Product ID.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_product_id(uint16_t *product_id)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.PRODUCT_ID_L",
            &regdesc)) {
        return -1;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -2;
    }
    *product_id = reg_byte;

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.PRODUCT_ID_H",
            &regdesc)) {
        return -3;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -4;
    }

    *product_id |= reg_byte << 8;
    return 0;
}

/**
 * @brief Retrieves the firmware Release Number (Major, Minor, and Revision).
 * 
 * @param major Output pointer populated with the Major release version.
 * @param minor Output pointer populated with the Minor release version.
 * @param rev Output pointer populated with the Revision version.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_release_no(uint8_t *major, uint8_t *minor, uint8_t *rev)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t major_reg_byte = 0;
    uint8_t minor_reg_byte = 0;

    if (!major || !minor || !rev) {
        return -1;
    }

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.RELEASE_MAJOR",
            &regdesc)) {
        return -2;
    }
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &major_reg_byte) < 0) {
        return -3;
    }

    if (bf_ptp_lookup_register_general("GENERAL_STATUS.RELEASE_MINOR",
            &regdesc)) {
        return -4;
    }
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &minor_reg_byte) < 0) {
        return -5;
    }

    *major = (major_reg_byte >> 1);
    *rev   = (major_reg_byte & 0x01);
    *minor = minor_reg_byte;

    return 0;
}

/**
 * @brief Triggers a hard state-machine reset of all DPLL and ToD state engines
 *        via the RESET_CTRL.SM_RESET register, forcing them back through
 *        the FULL acquisition sequence (FREE_RUN → LOCKACQ → LOCKED).
 *
 *        This is a write-only fire-and-forget command per the Renesas H/W API:
 *        writes 0x5A (SM_RESET_CMD) to SM_RESET at offset 0x12 (pre-V520).
 *        The register auto-clears upon completion.
 *
 * @note  FW v4.8.1 uses SM_RESET offset 0x12. V520+ uses 0x13.
 *
 * @return int 0 on success; negative on error.
 */
int bf_ptp_reset_sm(void)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;

    if (bf_ptp_lookup_register_general("RESET_CTRL.SM_RESET",
            &regdesc)) {
        return -1;
    }

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x5A) < 0) {
        return -2;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Retrieves the input clock frequency for physical clock inputs (INPUT_0 or INPUT_1).
 * 
 * @param input_index Target input channel index (Valid range: 0 or 1).
 * @param freq Output pointer populated with the calculated input frequency in Hz.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_input_freq(int input_index, double *freq)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (input_index != 0 && input_index != 1) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "INPUT_%d.IN_FREQ", input_index);
    if (bf_ptp_lookup_register_general(name_buf, &regdesc)) {
        return -2;
    }

    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset ++) {
        if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset,
                &burst_buf[offset]) < 0) {
            return -3;
        }
    }
    *freq = decode_obsclk_freq(input_index, burst_buf);

    return 0;
}

/**
 * @brief Sets the 32-bit signed phase increment for DPLL_n.
 *        Performs sequential per-byte writes over the 4-byte phase offset region
 *        with a 200 µs micro-sequencer latch settlement stall.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param phase Signed 32-bit phase target value.
 * @return int 0 on success; negative on hardware fault.
 */
int bf_ptp_set_dpll_phase(uint8_t dpll_index, int32_t phase)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[4] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    dpll_phase_le32_pack(phase, buf);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            buf, 4) < 0) {
        return -3;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Gets the 32-bit signed phase increment currently cached or running in DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_phase Output pointer populated with the running 32-bit signed phase.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_phase(uint8_t dpll_index, int32_t *out_phase)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[4] = { 0 };
    char name_buf[64] = { 0 };

    if ((dpll_index != 5 && dpll_index != 6) || !out_phase) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            buf, 4) < 0) {
        return -3;
    }

    *out_phase = dpll_phase_le32_unpack(buf);

    return 0;
}

/**
 * @brief Atomically extracts and decodes the 40-bit residual sub-ns phase offset error of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param phase_ns Output pointer populated with the decoded absolute phase error in nanoseconds.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_phase_status(uint8_t dpll_index, double *phase_ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !phase_ns) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE_STATUS", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Enforce continuous stride alignment via the registered burst width (5 Bytes) */
    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset++) {
        if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset,
                &burst_buf[offset])) {
            return -3;
        }
    }
    *phase_ns = dpll_phase_status(dpll_index, burst_buf);

    return 0;
}

/**
 * @brief Sets the 42-bit signed frequency micro-adjustment (FFO) to DPLL_n.
 *        Enforces Read-Modify-Write on byte D5 to safeguard [7:2] Reserved bits,
 *        then performs sequential per-byte writes over the 6-byte frequency offset region
 *        with a 200 µs micro-sequencer latch settlement stall.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param freq_ffo_q53 Signed 42-bit frequency offset in units of 2^-53 (Q53 format).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_freq(uint8_t dpll_index, int64_t freq_ffo_q53)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };
    uint8_t original_d5 = 0;

    if (dpll_index > 7) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.FREQ", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    int64_t cleaned_freq = freq_ffo_q53 & 0x3FFFFFFFFFFLL;

    burst_buf[0] = (uint8_t)(cleaned_freq & 0xFF);
    burst_buf[1] = (uint8_t)((cleaned_freq >> 8) & 0xFF);
    burst_buf[2] = (uint8_t)((cleaned_freq >> 16) & 0xFF);
    burst_buf[3] = (uint8_t)((cleaned_freq >> 24) & 0xFF);
    burst_buf[4] = (uint8_t)((cleaned_freq >> 32) & 0xFF);

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + 5,
            &original_d5) < 0) {
        return -3;
    }

    burst_buf[5] = (original_d5 & 0xFC) | (uint8_t)((cleaned_freq >> 40) & 0x03);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 6) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Gets the 42-bit signed frequency offset currently configured in DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_freq_ffo_q53 Output pointer populated with the signed 42-bit frequency offset in Q53 format.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_freq(uint8_t dpll_index, int64_t *out_freq_ffo_q53)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_freq_ffo_q53) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.FREQ", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    for (int i = 0; i < 6; i++) {
        if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + i,
                &burst_buf[i]) < 0) {
            return -3;
        }
    }

    int64_t raw_val = ((int64_t)(burst_buf[5] & 0x03) << 40) | ((int64_t)burst_buf[4] << 32) |
                      ((int64_t)burst_buf[3] << 24)       | ((int64_t)burst_buf[2] << 16) |
                      ((int64_t)burst_buf[1] << 8)        | ((int64_t)burst_buf[0]);

    if (raw_val & 0x0000002000000000LL) { // 42-bit sign extension
        raw_val |= 0xFFFFFC0000000000LL;
    }
    *out_freq_ffo_q53 = raw_val;
    return 0;
}

/**
 * @brief Retrieves the current operational configuration mode byte of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param op_mode Output pointer populated with the raw operational mode control byte.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_mode(uint8_t dpll_index, enum pll_mode *op_mode)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !op_mode) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.MODE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }
    *op_mode = (enum pll_mode)((reg_byte >> PLL_MODE_SHIFT) & PLL_MODE_MASK);
    return 0;
}

/**
 * @brief Configures the operational mode and state machine mode of DPLL_n.
 *        Enforces Read-Modify-Write to protect RESERVED[7:6] bits.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param op_mode Operational mode (0=PLL, 1=PHASE, 2=FREQ, 3=GPIO, 4=SYNTH, 5=PMEAS, 6=OFF).
 * @param sm_mode State machine mode (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_mode(uint8_t dpll_index, enum pll_mode op_mode)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t dpll_mode = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || op_mode < PLL_MODE_MIN || op_mode > PLL_MODE_MAX) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.MODE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Read-Modify-Write to protect RESERVED[7:6] bits */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &dpll_mode) < 0) {
        return -3;
    }

	dpll_mode &= ~(PLL_MODE_MASK << PLL_MODE_SHIFT);
	dpll_mode |= (op_mode << PLL_MODE_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            dpll_mode) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Retrieves the state-machine mode from the DPLL_MODE register of DPLL_n.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param sm_mode Output pointer receiving the SM_MODE field [2:0]
 *                (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_sm(uint8_t dpll_index, enum pll_state *sm_mode)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !sm_mode) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.MODE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }
    *sm_mode = (reg_byte >> STATE_MODE_SHIFT) & STATE_MODE_MASK;
    return 0;
}

/**
 * @brief Sets the state-machine mode of DPLL_n via RMW, preserving OP_MODE and reserved bits.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param sm_mode State machine mode (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_sm(uint8_t dpll_index, enum pll_state sm_mode)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t dpll_mode = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || sm_mode < PLL_STATE_MIN || sm_mode > PLL_STATE_MAX) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.MODE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Read-Modify-Write to protect RESERVED[7:6] and PLL_MODE[5:3] bits */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &dpll_mode) < 0) {
        return -3;
    }

	dpll_mode &= ~(STATE_MODE_MASK << STATE_MODE_SHIFT);
	dpll_mode |= (sm_mode << STATE_MODE_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            dpll_mode) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Retrieves the live tracking reference status of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param ref_stat Output pointer populated with the upstream physical CLK channel index.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_ref_stat(uint8_t dpll_index, uint8_t *ref_stat)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !ref_stat) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.REF_STAT", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }
    *ref_stat = reg_byte;
    return 0;
}

/**
 * @brief Retrieves the core state machine lock configuration of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param state Output pointer receiving the extracted DPLL_STATE field [3:0] (enum dpll_state).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_state(uint8_t dpll_index, enum dpll_state *state)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !state) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.STATUS", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *state = (reg_byte & DPLL_STATE_MASK);

    return 0;
}

/**
 * @brief Retrieves the TOD synchronization configuration for DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_enabled Output pointer populated with the ToD sync enable status.
 * @param out_tod_source Output pointer populated with the ToD source index (0 to 3).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_tod_sync_cfg(uint8_t dpll_index,
        bool *out_enabled, uint8_t *out_tod_source)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_enabled || !out_tod_source) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.TOD_SYNC_CFG", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *out_enabled = (reg_byte & 0x01) ? true : false;
    *out_tod_source = (reg_byte >> 1) & 0x03;

    return 0;
}

/**
 * @brief Modifies the TOD synchronization configuration for DPLL_n.
 *        Enforces Read-Modify-Write to protect RESERVED[7:3] bits.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param enable_sync Enables ToD synchronization.
 * @param tod_source The ToD source index to synchronize to (0 to 3).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_tod_sync_cfg(uint8_t dpll_index,
        bool enable_sync, uint8_t tod_source)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || tod_source > 3) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.TOD_SYNC_CFG", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Read current configuration byte to isolate RESERVED[7:3] */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &original_val) < 0) {
        return -3;
    }

    /* Modify: keep [7:3] RESERVED bits untouched */
    updated_val = original_val & 0xF8;
    updated_val |= (tod_source & 0x03) << 1;
    if (enable_sync) {
        updated_val |= 0x01;
    }

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            updated_val) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Retrieves the Combo Mode Slave Primary Source configuration for a given DPLL.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_pri_en Output pointer populated with the primary source enablement state.
 * @param out_pri_filt Output pointer populated with the filtered DCO configuration state.
 * @param out_pri_src_id Output pointer populated with the primary combo source DPLL index (0 to 8, where 8 is SYSDPLL).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_combo_slave_pri_cfg(uint8_t dpll_index,
        bool *out_pri_en, bool *out_pri_filt, uint8_t *out_pri_src_id)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_pri_en || !out_pri_filt || !out_pri_src_id) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.COMBO_SLAVE_CFG_0", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *out_pri_en = (reg_byte & 0x20) ? true : false;
    *out_pri_filt = (reg_byte & 0x10) ? true : false;
    *out_pri_src_id = reg_byte & 0x0F;

    return 0;
}

/**
 * @brief Configures the Combo Mode Slave Primary Source for a given DPLL.
 *        Enforces Read-Modify-Write to protect RESERVED[7:6] bits.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param pri_en Enable the primary combo source.
 * @param pri_filt Select filtered configuration for the primary combo source.
 * @param pri_src_id Primary combo source DPLL index (0 to 8, where 8 is SYSDPLL).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_combo_slave_pri_cfg(uint8_t dpll_index,
        bool pri_en, bool pri_filt, uint8_t pri_src_id)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || pri_src_id > 8) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.COMBO_SLAVE_CFG_0", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &original_val) < 0) {
        return -3;
    }

    /* Modify: keep [7:6] RESERVED bits untouched */
    updated_val = original_val & 0xC0;
    if (pri_en) {
        updated_val |= 0x20;
    }
    if (pri_filt) {
        updated_val |= 0x10;
    }
    updated_val |= (pri_src_id & 0x0F);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            updated_val) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Retrieves the Combo Mode Slave Secondary Source configuration for a given DPLL.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_sec_en Output pointer populated with the secondary source enablement state.
 * @param out_sec_filt Output pointer populated with the filtered configuration state.
 * @param out_sec_src_id Output pointer populated with the secondary combo source DPLL index (0 to 8, where 8 is SYSDPLL).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_combo_slave_sec_cfg(uint8_t dpll_index,
        bool *out_sec_en, bool *out_sec_filt, uint8_t *out_sec_src_id)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_sec_en || !out_sec_filt || !out_sec_src_id) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.COMBO_SLAVE_CFG_1", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *out_sec_en = (reg_byte & 0x20) ? true : false;
    *out_sec_filt = (reg_byte & 0x10) ? true : false;
    *out_sec_src_id = reg_byte & 0x0F;

    return 0;
}

/**
 * @brief Configures the Combo Mode Slave Secondary Source for a given DPLL.
 *        Enforces Read-Modify-Write to protect RESERVED[7:6] bits.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param sec_en Enable the secondary combo source.
 * @param sec_filt Select filtered configuration for the secondary combo source.
 * @param sec_src_id Secondary combo source DPLL index (0 to 8, where 8 is SYSDPLL).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_combo_slave_sec_cfg(uint8_t dpll_index,
        bool sec_en, bool sec_filt, uint8_t sec_src_id)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || sec_src_id > 8) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL%d.COMBO_SLAVE_CFG_1", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &original_val) < 0) {
        return -3;
    }

    /* Modify: keep [7:6] RESERVED bits untouched */
    updated_val = original_val & 0xC0;
    if (sec_en) {
        updated_val |= 0x20;
    }
    if (sec_filt) {
        updated_val |= 0x10;
    }
    updated_val |= (sec_src_id & 0x0F);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            updated_val) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Executes the mandatory hardware commit trigger to enforce phase pull-in / frequency step alignment.
 *        Maps perfectly to DPLL_PHASE_PULL_IN_n and FREQ alignment triggers.
 * 
 * @param trigger_type 0 for Phase Pull-In (0x0F), 1 for Frequency Commit (0x3F).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_commit_dpll_trigger(uint8_t trigger_type)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    const char *name_buf = (trigger_type == 0) ? 
            "DPLL_PHASE.EXEC_TRIGGER" : "DPLL_FREQ.EXEC_TRIGGER";

    if (bf_ptp_lookup_register_general(name_buf, &regdesc)) {
        return -1;
    }

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -2;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Writes the target phase offset for the FW phase pull-in state machine.
 *
 *        DPLL_n.PULL_IN_OFFSET is a signed 32-bit register at 50 ps/LSB,
 *        identical in format to DPLL_n.PHASE.
 *
 *        The pull-in slews the phase error *toward zero*: write the negated
 *        correction here so the FW drives the DCO to cancel the measured error.
 *
 * @param dpll_index    Target DPLL block (0-7).
 * @param offset_50ps   Signed 50 ps phase offset (negate the desired correction).
 * @return int          0 on success; negative on error.
 */
int bf_ptp_set_phase_pull_in_offset(uint8_t dpll_index, int32_t offset_50ps)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[4] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE_PULL_IN", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    dpll_phase_le32_pack(offset_50ps, buf);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            buf, 4) < 0) {
        return -3;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Caps the maximum frequency offset during the FW phase pull-in slew.
 *
 *        DPLL_n.PULL_IN_SLOPE_LIMIT is an unsigned 24-bit register in ppb.
 *        A value of 0 lets the firmware use its internal default.
 *
 * @param dpll_index    Target DPLL block (0-7).
 * @param max_ffo_ppb   Maximum fractional frequency offset in ppb (0 = firmware default).
 * @return int          0 on success; negative on error.
 */
int bf_ptp_set_phase_pull_in_slope_limit(uint8_t dpll_index, uint32_t max_ffo_ppb)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[3] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7) {
        return -1;
    }
    if (max_ffo_ppb & 0xFF000000) {   /* clamp to 24-bit */
        max_ffo_ppb = 0;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE_PULL_IN", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    buf[0] = (uint8_t)(max_ffo_ppb & 0xFF);
    buf[1] = (uint8_t)((max_ffo_ppb >> 8) & 0xFF);
    buf[2] = (uint8_t)((max_ffo_ppb >> 16) & 0xFF);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc) + 4,
            buf, 3) < 0) {
        return -3;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Kicks off the firmware autonomous phase pull-in state machine.
 *
 *        Writes 0x01 to DPLL_n.PULL_IN_CTRL.  The firmware then autonomously
 *        slews the DCO output phase toward the programmed PULL_IN_OFFSET,
 *        respecting PULL_IN_SLOPE_LIMIT.
 *
 *        Returns negative if a pull-in is already in progress on this DPLL.
 *
 * @param dpll_index    Target DPLL block (0-7).
 * @return int          0 on success; negative if already active or on error.
 */
int bf_ptp_start_phase_pull_in(uint8_t dpll_index)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t ctrl = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.PHASE_PULL_IN", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + 7,
            &ctrl) < 0) {
        return -3;
    }

    if (ctrl != 0) {
        return -4;   /* pull-in already in progress */
    }

    ctrl = 0x01;
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + 7,
            ctrl) < 0) {
        return -5;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Combined FW phase pull-in convenience: offset + slope + start.
 *
 *        Programs DPLL_n.PULL_IN_OFFSET (negated delta), PULL_IN_SLOPE_LIMIT,
 *        then writes 0x01 to PULL_IN_CTRL to kick off the firmware autonomous
 *        phase-slewing state machine.
 *
 *        This is the preferred path for small ToD corrections (< 15 µs) —
 *        the ToD counter slews continuously without a backward jump.
 *
 * @param dpll_index    Target DPLL block (0-7).
 * @param delta_ns      Desired phase correction in nanoseconds (signed).
 * @param max_ffo_ppb   Max frequency offset during pull-in (0 = default).
 * @return int          0 on success; negative on error.
 */
int bf_ptp_phase_pull_in(uint8_t dpll_index, int32_t delta_ns, uint32_t max_ffo_ppb)
{
    int rc;

    /* PULL_IN_OFFSET expects the target offset (negated correction).
     * Convert ns -> 50 ps LSBs: phase_50ps = -delta_ns * 20 */
    int64_t offset_50ps = (int64_t)(-delta_ns) * 20;
    if (offset_50ps >  INT32_MAX) offset_50ps =  INT32_MAX;
    if (offset_50ps < -INT32_MAX) offset_50ps = -INT32_MAX;

    rc = bf_ptp_set_phase_pull_in_offset(dpll_index, (int32_t)offset_50ps);
    if (rc < 0) return rc;

    rc = bf_ptp_set_phase_pull_in_slope_limit(dpll_index, max_ffo_ppb);
    if (rc < 0) return rc;

    return bf_ptp_start_phase_pull_in(dpll_index);
}

/**
 * @brief Retrieves the live driving clock source (DPLL linkage index) of a specified TOD instance.
 *        Queries DPLL_n.DPLL_TOD_SYNC_CFG registers to locate which DPLL is actively synchronized
 *        with this ToD index.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_dpll_src Output pointer populated with the driving DPLL index (Range: 0 to 7).
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_clk_src(uint8_t tod_index, uint8_t *out_dpll_src)
{
    bool enabled = false;
    uint8_t tod_source = 0;

    if (tod_index > 3 || !out_dpll_src) {
        return -1;
    }

    /* Scan DPLLs to locate DPLL_n actively synchronizing to this TOD index */
    for (uint8_t dpll_index = 0; dpll_index < 8; dpll_index++) {
        if (bf_ptp_get_dpll_tod_sync_cfg(dpll_index, &enabled, &tod_source) == 0) {
            if (enabled && (tod_source == tod_index)) {
                *out_dpll_src = dpll_index;
                //fprintf(stdout, "Find default conf for ToD%d - DPLL%d ...\n", tod_index, dpll_index);
                return 0;
            }
        }
    }

    return -1;
}

/**
 * @brief Retrieves the historical write completion metrics from the specified TOD_WRITE_n module.
 *        Corresponds to StepC logic.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_write_counter Output pointer populated with the raw hardware write count byte.
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_write_counter(uint8_t tod_index, uint8_t *out_write_counter)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_write_counter) {
        return -1;
    }

    /* Dynamic string construction matching "TOD_WRITE_n.COUNTER" within Page 0xCB */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.WRITE.COUNTER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Execute paged read targeting 0xCB0C + (tod_index * 0x10) */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *out_write_counter = reg_byte;

    return 0;
}

/**
 * @brief Retrieves the historical read snapshot completion metrics from the specified TOD_READ_PRIMARY_n module.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_read_counter Output pointer populated with the raw hardware read count byte.
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_read_counter(uint8_t tod_index, uint8_t *out_read_counter)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_read_counter) {
        return -1;
    }

    /* Dynamic string construction matching "TODn.READ_PRIMARY.COUNTER" within Page 0xCC */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.READ_PRIMARY.COUNTER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Execute paged read targeting 0xCC4B/0xCC5B/0xCC6B/0xCC8B */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *out_read_counter = reg_byte;

    return 0;
}

/**
 * @brief Retrieves and decodes the current configuration bits of the specified TOD_n.TOD_CFG register.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_enabled Output pointer populated with the TOD_ENABLE[0] status.
 * @param out_even_pps Output pointer populated with the TOD_EVEN_PPS_MODE[2] status.
 * @param out_sync_disabled Output pointer populated with the TOD_OUT_SYNC_DISABLE[1] status.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_tod_cfg(uint8_t tod_index,
        bool *out_enabled, bool *out_even_pps, bool *out_sync_disabled)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_enabled || !out_even_pps || !out_sync_disabled) {
        return -1;
    }

    /* 1. Dynamic string construction matching "TOD_n.TOD_CFG" */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.TOD_CFG", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    /* 2. Execute low-level 1B paged read over the I2C physical bus (e.g., 0xCB10 for TOD_1) */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    /* 3. Extract bitfields based on Table 356 specification */
    *out_enabled       = (reg_byte & (1 << 0)) ? true : false; /* Bit 0: TOD_ENABLE */
    *out_sync_disabled = (reg_byte & (1 << 1)) ? true : false; /* Bit 1: TOD_OUT_SYNC_DISABLE */
    *out_even_pps      = (reg_byte & (1 << 2)) ? true : false; /* Bit 2: TOD_EVEN_PPS_MODE */

    return 0;
}

/**
 * @brief Modifies the specified TOD_n.TOD_CFG register to activate or freeze the clock core.
 *        Enforces strict Read-Modify-Write to isolate and protect the RESERVED[7:3] high bits.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param enable_tod Sets the TOD_ENABLE[0] bit (True = run, False = stop).
 * @param even_pps_mode Sets the TOD_EVEN_PPS_MODE[2] bit (True = 2-second pulse, False = 1-second pulse).
 * @param disable_out_sync Sets the TOD_OUT_SYNC_DISABLE[1] bit (True = disable, False = enable sync).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_tod_cfg(uint8_t tod_index,
        bool enable_tod, bool even_pps_mode, bool disable_out_sync)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3) {
        return -1;
    }

    /* 1. Dynamic string construction matching "TOD_n.TOD_CFG" */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.TOD_CFG", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    /* 2. READ Phase: Fetch current configuration byte to isolate RESERVED[7:3] */
    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &original_val) < 0) {
        return -3;
    }

    /* 3. MODIFY Phase: Clear lower 3 bits and inject the new control configuration flags */
    updated_val = original_val & 0xF8; /* Keep [7:3] RESERVED bits completely untouched */

    if (even_pps_mode)    updated_val |= (1 << 2); /* Bit 2: Even pulse mode selector */
    if (disable_out_sync) updated_val |= (1 << 1); /* Bit 1: Reverse logic output synchronization barrier */
    if (enable_tod)       updated_val |= (1 << 0); /* Bit 0: Main core counter throttle clock engine */

    /* 
     * 4. WRITE Phase: Commit the packed byte back to the physical silicon.
     * Crucial: Writing to this address acts as the hard trigger to execute all preloaded time bytes.
     */
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            updated_val) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief Software-triggered ToD read via the PRIMARY snapshot buffer.
 *
 * Writes an IMMEDIATE trigger to TODn.READ_PRIMARY.TRIGGER, polls for
 * the hardware latch to settle, then burst-reads the 11-byte time array.
 * The snapshot is software-gated — jitter depends on I2C bus + CPU scheduling.
 *
 * For hardware-gated (low-jitter) reads, use the SECONDARY path:
 *   bf_ptp_arm_tod_read_trigger_refclk() + bf_ptp_get_tod_time_triggered().
 *
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_sec   Output pointer populated with the decoded Unix Epoch seconds.
 * @param out_ns    Output pointer populated with the residual sub-second nanoseconds.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_tod_time(uint8_t tod_index, uint32_t *out_sec, uint32_t *out_ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_sec || !out_ns) {
        return -1;
    }

    /* 1. HARDWARE SNAPSHOT LATCH: Instantly freeze the live running clock counters into shadow cache */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.READ_PRIMARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Write 0x01 to execute the hardware snapshot freeze */
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -2;
    }
    usleep(10); /* Brief physical bus flush stall */

    /* 2. Reset coordinates back to the start of the locked 11-byte time array */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.READ_PRIMARY.SUB_NS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -3;
    }
    uint8_t page = REGPAGE(regdesc);
    uint8_t addr = REGADDR(regdesc);

    /* 3. CONTINUOUS STRIDE BURST READ: Extract all 11 bytes sequentially to prevent phase tearing */
    for (int offset = 0; offset < 11; offset++) {
        if (bf_pltfm_read_ptp_reg(page, addr + offset,
                &burst_buf[offset]) < 0) {
            return -4;
        }
    }

    /* 4. DECODE NANOSECONDS (Little-Endian assembly across offsets 001h to 004h) */
    *out_ns = ((uint32_t)burst_buf[4] << 24) |
              ((uint32_t)burst_buf[3] << 16) |
              ((uint32_t)burst_buf[2] << 8)  |
              ((uint32_t)burst_buf[1]);

    /* 5. DECODE SECONDS (Little-Endian assembly across offsets 005h to 008h) */
    *out_sec = ((uint32_t)burst_buf[8] << 24) |
               ((uint32_t)burst_buf[7] << 16) |
               ((uint32_t)burst_buf[6] << 8)  |
               ((uint32_t)burst_buf[5]);

    return 0; /* Coherent ToD fetched successfully */
}

/**
 * @brief Preloads and commits a 64-bit absolute Time-of-Day (ToD)
 *        to any specified hardware TOD instance (0 to 3).
 *        Writes sub-nanoseconds, nanoseconds, and seconds, then triggers the commit.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param sec Unix Epoch absolute seconds to set.
 * @param ns Residual sub-second nanoseconds to set.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_tod_time(uint8_t tod_index, uint32_t sec, uint32_t ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (tod_index > 3) {
        return -1;
    }

    /* 1. Preload sub-nanoseconds (set to 0) */
    burst_buf[0] = 0x00;

    /* 2. Preload nanoseconds (Little-Endian assembly) */
    burst_buf[1] = (uint8_t)(ns & 0xFF);
    burst_buf[2] = (uint8_t)((ns >> 8) & 0xFF);
    burst_buf[3] = (uint8_t)((ns >> 16) & 0xFF);
    burst_buf[4] = (uint8_t)((ns >> 24) & 0xFF);

    /* 3. Preload seconds (Little-Endian assembly) */
    burst_buf[5] = (uint8_t)(sec & 0xFF);
    burst_buf[6] = (uint8_t)((sec >> 8) & 0xFF);
    burst_buf[7] = (uint8_t)((sec >> 16) & 0xFF);
    burst_buf[8] = (uint8_t)((sec >> 24) & 0xFF);
    burst_buf[9] = 0x00;
    burst_buf[10] = 0x00;

    /* 4. Write all 11 bytes starting from SUB_NS using burst write */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.WRITE.SUB_NS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 11) < 0) {
        return -3;
    }

    /* 5. HARDWARE WRITE COMMIT TRIGGER: Commit the preloaded values into the active counters */
    snprintf(name_buf, sizeof(name_buf), "TOD%d.WRITE.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -4;
    }

    /* Write 0x01 (Absolute TOD, Immediate Trigger) */
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -5;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

/**
 * @brief SECONDARY path — reads a hardware-latched ToD snapshot.
 *
 * Unlike bf_ptp_get_tod_time() (PRIMARY, software-gated trigger), this
 * reads the pre-latched snapshot from the SECONDARY buffer — the trigger
 * was fired by an external hardware event, not by software.
 *
 * Checks the TRIGGER register is not already pending, then reads the
 * 11-byte snapshot from the secondary base and decodes seconds + nanoseconds.
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param out_sec    Output pointer populated with the latched seconds.
 * @param out_ns     Output pointer populated with the latched nanoseconds.
 * @return int       0 on success; -4 if trigger still pending (EBUSY).
 */
int bf_ptp_get_tod_time_triggered(uint8_t tod_index,
                                   uint32_t *out_sec, uint32_t *out_ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };
    uint8_t trigger = 0;

    if (tod_index > 3 || !out_sec || !out_ns)
        return -1;

    /* 1. Check that a snapshot is ready (trigger bit cleared → latched) */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -2;

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &trigger) < 0)
        return -3;

    if (trigger & TOD_READ_TRIGGER_MASK)
        return -4; /* EBUSY: previous trigger still pending */

    /* 2. BURST-READ 11-byte snapshot array from SECONDARY BASE */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.SUB_NS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -5;

    uint8_t page = REGPAGE(regdesc);
    uint8_t addr = REGADDR(regdesc);

    for (int offset = 0; offset < 11; offset++) {
        if (bf_pltfm_read_ptp_reg(page, addr + offset,
                &burst_buf[offset]) < 0)
            return -6;
    }

    /* 3. DECODE NANOSECONDS (Little-Endian across offsets 001h-004h) */
    *out_ns = ((uint32_t)burst_buf[4] << 24) |
              ((uint32_t)burst_buf[3] << 16) |
              ((uint32_t)burst_buf[2] <<  8) |
              ((uint32_t)burst_buf[1]);

    /* 4. DECODE SECONDS (Little-Endian across offsets 005h-008h) */
    *out_sec = ((uint32_t)burst_buf[8] << 24) |
               ((uint32_t)burst_buf[7] << 16) |
               ((uint32_t)burst_buf[6] <<  8) |
               ((uint32_t)burst_buf[5]);

    return 0;
}

/**
 * @brief SECONDARY path — arms a hardware-gated ToD snapshot on a refclk edge.
 *
 * Unlike bf_ptp_get_tod_time() (PRIMARY, software-triggered), this configures
 * the SECONDARY read buffer to auto-latch the time on an external reference
 * clock edge, eliminating software jitter from the trigger path.
 *
 * Writes the reference clock index to SEL_CFG_0 and sets the trigger mode
 * to REFCLK in the TRIGGER (CMD) register. The hardware will auto-latch a
 * snapshot on the next selected refclk edge.
 *
 * After the edge fires, call bf_ptp_get_tod_time_triggered() to retrieve
 * the latched snapshot.
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param ref        Reference clock index (0-15).
 * @return int       0 on success; negative on error.
 */
int bf_ptp_arm_tod_read_trigger_refclk(uint8_t tod_index, uint8_t ref)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    char name_buf[64] = { 0 };
    uint8_t reg_byte = 0;

    if (tod_index > 3 || ref > 15)
        return -1;

    /* Write selected reference clock index into SEL_CFG_0 */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.SEL_CFG_0", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

	reg_byte &= ~(WR_REF_INDEX_MASK << WR_REF_INDEX_SHIFT);
	reg_byte |= (ref << WR_REF_INDEX_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0)
        return -3;

    /* Arm trigger: REFCLK edge detect */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -4;

    reg_byte = 0 | (SCSR_TOD_READ_TRIG_SEL_REFCLK << TOD_READ_TRIGGER_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0)
        return -5;

    return 0;
}

/**
 * @brief SECONDARY path — arms a one-shot immediate trigger for quick test/debug.
 *
 * Writes TOD_READ_TRIG_SEL_IMMEDIATE to the SECONDARY TRIGGER register.
 * Unlike bf_ptp_get_tod_time() which uses PRIMARY, this routes through the
 * SECONDARY buffer — useful for testing the secondary path without external
 * hardware events.  After calling, poll bf_ptp_get_tod_time_triggered() until
 * it returns 0 (the trigger bit auto-clears once the latch completes).
 *
 * For production 1PPS/EXTTS timestamping, use
 * bf_ptp_arm_tod_read_trigger_refclk() instead.
 *
 * @param tod_index  Target TOD instance (0-3).
 * @return int       0 on success; negative on error.
 */
int bf_ptp_arm_tod_read_trigger_immediate(uint8_t tod_index)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    char name_buf[64] = { 0 };
    uint8_t reg_byte = 0;

    if (tod_index > 3)
        return -1;

    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -2;

    reg_byte = (uint8_t)SCSR_TOD_READ_TRIG_SEL_IMMEDIATE;
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0)
        return -3;

    return 0;
}

/*
 *  PTP Adjustment API Family
 *
 *  ----------------------------------------------------------------------------
 *  │ API            | Target               | Effect                           │
 *  -----------------|----------------------|-----------------------------------
 *  | bf_ptp_adjtime | ToD counter (wall-   | Instant step on the time-of-     │
 *  | (below)        | clock).  RMW via     | day.  Reads current PRIMARY      |
 *  |                | PRIMARY+WRITE paths. | snapshot, adds delta_ns,         |
 *  |                |                      | commits via WRITE shadow.        |
 *  -----------------|----------------------|-----------------------------------
 *  | bf_ptp_adjphase| DPLL output phase.   | Gradual slew of the DCO output   |
 *  |                | PCW -> DPLL_n.PHASE  | waveform.  Auto-switches DPLL    |
 *  |                | (50 ps / LSB).       | to WRITE_PHASE op_mode.          |
 *  -----------------|----------------------|-----------------------------------
 *  | bf_ptp_adjfine | DPLL DCO frequency.  | Persistent rate offset on the    |
 *  |                | FCW -> DPLL_n.FREQ   | oscillator.  Auto-switches DPLL  |
 *  |                | (Q5.3 format).       | to WRITE_FREQUENCY op_mode.      |
 *  ----------------------------------------------------------------------------
 *
 *  PTP servo mapping:
 *    SERVO_JUMP   -> bf_ptp_adjtime()    brute-force correction on ToD
 *    SERVO_LOCKED -> bf_ptp_adjfine()    continuous frequency discipline
 *    SERVO_LOCKED -> bf_ptp_adjphase()   fine-grained phase alignment
 */

/**
 * @brief Adjusts the Time-of-Day by a signed nanosecond delta.
 *
 *        Dual-path correction strategy (matches upstream idtcm_adjtime):
 *
 *        - |delta| <  PHASE_PULL_IN_THRESHOLD_NS (15 µs):
 *          FW phase pull-in via the driving DPLL — the DCO slews the
 *          ToD counter gradually without a backward time jump.
 *
 *        - |delta| >= PHASE_PULL_IN_THRESHOLD_NS:
 *          Absolute read-modify-write step via the PRIMARY path
 *          (bf_ptp_get_tod_time + bf_ptp_set_tod_time).
 *
 *        For stand-alone DPLL phase adjustment, see bf_ptp_adjphase().
 *        For continuous frequency offset, see bf_ptp_adjfine().
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param delta_ns   Signed nanosecond offset (positive = advance).
 * @return int       0 on success; negative on error.
 */
int bf_ptp_adjtime(uint8_t tod_index, int64_t delta_ns)
{
    uint32_t sec = 0, ns = 0;

    if (tod_index > 3)
        return -1;

    /* Small deltas: use FW phase pull-in via the driving DPLL.
     * This avoids an instant ToD counter step that would confuse
     * userspace clock_gettime consumers. */
    if ((delta_ns > -PHASE_PULL_IN_THRESHOLD_NS) &&
        (delta_ns <  PHASE_PULL_IN_THRESHOLD_NS)) {

        uint8_t dpll_src = 0;
        if (bf_ptp_get_tod_clk_src(tod_index, &dpll_src) < 0)
            goto fallback_step;

        return bf_ptp_phase_pull_in(dpll_src, (int32_t)delta_ns, 0);
    }

fallback_step:
    if (bf_ptp_get_tod_time(tod_index, &sec, &ns) < 0)
        return -2;

    int64_t total_ns = (int64_t)sec * 1000000000LL + (int64_t)ns + delta_ns;

    if (total_ns < 0) {
        sec = 0;
        ns  = 0;
    } else {
        sec = (uint32_t)(total_ns / 1000000000LL);
        ns  = (uint32_t)(total_ns % 1000000000LL);
    }

    return bf_ptp_set_tod_time(tod_index, sec, ns);
}

/**
 * @brief Adjusts DPLL phase by a signed nanosecond delta (gradual slew).
 *
 * Converts delta_ns -> picoseconds, divides by 50 ps/step, writes LE
 * to DPLL_n.PHASE.  Auto-switches the DPLL to WRITE_PHASE op_mode.
 *
 * This is a gradual phase slew — not an instant time-step.
 * For time-step correction, see bf_ptp_adjtime().
 * For frequency offset, see bf_ptp_adjfine().
 *
 * @param dpll_index  Target DPLL block (0-7).
 * @param delta_ns    Signed nanosecond phase offset (resolution: 50 ps).
 * @return int        0 on success; negative on error.
 */
int bf_ptp_adjphase(uint8_t dpll_index, int32_t delta_ns)
{
    enum pll_mode mode;

    if (dpll_index > 7)
        return -1;

    /* Ensure DPLL is in WRITE_PHASE (preserves SM_MODE via RMW) */
    if (bf_ptp_get_dpll_mode(dpll_index, &mode) < 0)
        return -2;

    if (mode != PLL_MODE_WRITE_PHASE) {
        if (bf_ptp_set_dpll_mode(dpll_index, PLL_MODE_WRITE_PHASE) < 0)
            return -3;
    }

    /*
     * Phase register resolution: 50 ps / LSB.
     *           phase_50ps = div_s64(offset_ps, 50)
     *           offset_ps = (s64)delta_ns * 1000
     * So: phase_raw = delta_ns * 1000 / 50 = delta_ns * 20
     */
    int64_t phase_raw = (int64_t)delta_ns * 20;

    /* Clamp to 32-bit signed range (HW register is int32) */
    if (phase_raw >  INT32_MAX) phase_raw =  INT32_MAX;
    if (phase_raw < -INT32_MAX) phase_raw = -INT32_MAX;

    return bf_ptp_set_dpll_phase(dpll_index, (int32_t)phase_raw);
}

/**
 * @brief Adjusts DPLL frequency using a scaled PPM offset (persistent rate).
 *
 * Converts scaled_ppm (2^-16 ppm) to raw FCW via
 *   FCW = scaled_ppm * 5^12 / (111 * 2^4) = scaled_ppm * 244140625 / 1776,
 * writes LE to DPLL_n.FREQ in Q5.3 format.  Auto-switches the DPLL to
 * WRITE_FREQUENCY op_mode.
 *
 * This is a persistent frequency offset — not a time-step or phase slew.
 * For time-step correction, see bf_ptp_adjtime().
 * For gradual phase slew, see bf_ptp_adjphase().
 *
 * @param dpll_index  Target DPLL block (0-7).
 * @param scaled_ppm  Signed frequency offset in 2^-16 ppm units.
 * @return int        0 on success; negative on error.
 */
int bf_ptp_adjfine(uint8_t dpll_index, int64_t scaled_ppm)
{
    enum pll_mode mode;

    if (dpll_index > 7)
        return -1;

    /* Ensure DPLL is in WRITE_FREQUENCY (preserves SM_MODE via RMW) */
    if (bf_ptp_get_dpll_mode(dpll_index, &mode) < 0)
        return -2;

    if (mode != PLL_MODE_WRITE_FREQUENCY) {
        if (bf_ptp_set_dpll_mode(dpll_index, PLL_MODE_WRITE_FREQUENCY) < 0)
            return -3;
    }

    /*
     * FCW conversion:
     *   FCW = scaled_ppm * 5^12 / (111 * 2^4)
     *       = scaled_ppm * 244140625 / 1776
     */
    int64_t fcw = (scaled_ppm * 244140625LL) / 1776;

    return bf_ptp_set_dpll_freq(dpll_index, fcw);
}

/**
 * @brief PRIMARY path — reads the current ToD with trigger-bit polling.
 *
 * Matches upstream _idtcm_gettime_immediate + _idtcm_gettime:
 * writes IMMEDIATE to PRIMARY.TRIGGER, polls until the trigger bit
 * clears (up to 10 iterations), then burst-reads the 11-byte snapshot.
 * The polling loop is more robust than a fixed usleep against I2C
 * bus latency variance.
 *
 * For hardware-gated (low-jitter) reads, use the SECONDARY path:
 *   bf_ptp_arm_tod_read_trigger_refclk() + bf_ptp_get_tod_time_triggered().
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param out_sec    Output pointer populated with Unix Epoch seconds.
 * @param out_ns     Output pointer populated with sub-second nanoseconds.
 * @return int       0 on success; negative on error.
 */
int bf_ptp_gettime(uint8_t tod_index, uint32_t *out_sec, uint32_t *out_ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };
    uint8_t trigger = 0;
    int timeout = 10;

    if (tod_index > 3 || !out_sec || !out_ns)
        return -1;

    /* 1. Fire IMMEDIATE trigger on PRIMARY */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_PRIMARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -2;

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             (uint8_t)SCSR_TOD_READ_TRIG_SEL_IMMEDIATE) < 0)
        return -3;

    /* 2. Poll TRIGGER register until the latch settles */
    do {
        if (timeout-- == 0)
            return -4; /* EIO: timeout waiting for latch */

        if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
                &trigger) < 0)
            return -5;
    } while (trigger & TOD_READ_TRIGGER_MASK);

    /* 3. Burst-read the 11-byte snapshot from PRIMARY base */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_PRIMARY.SUB_NS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc))
        return -6;

    uint8_t page = REGPAGE(regdesc);
    uint8_t addr = REGADDR(regdesc);

    for (int offset = 0; offset < 11; offset++) {
        if (bf_pltfm_read_ptp_reg(page, addr + offset,
                &burst_buf[offset]) < 0)
            return -7;
    }

    /* 4. Decode nanoseconds (LE across offsets 001h-004h) */
    *out_ns = ((uint32_t)burst_buf[4] << 24) |
              ((uint32_t)burst_buf[3] << 16) |
              ((uint32_t)burst_buf[2] <<  8) |
              ((uint32_t)burst_buf[1]);

    /* 5. Decode seconds (LE across offsets 005h-008h) */
    *out_sec = ((uint32_t)burst_buf[8] << 24) |
               ((uint32_t)burst_buf[7] << 16) |
               ((uint32_t)burst_buf[6] <<  8) |
               ((uint32_t)burst_buf[5]);

    return 0;
}

/**
 * @brief PRIMARY path — writes an absolute time to the ToD counter.
 *
 * Matches upstream _idtcm_settime with SCSR_TOD_WR_TYPE_SEL_ABSOLUTE:
 * preloads sub-ns=0, ns, and seconds into the WRITE registers, then
 * fires the ABSOLUTE + IMMEDIATE commit trigger (0x01).
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param sec        Unix Epoch absolute seconds to set.
 * @param ns         Residual sub-second nanoseconds to set.
 * @return int       0 on success; negative on error.
 */
int bf_ptp_settime(uint8_t tod_index, uint32_t sec, uint32_t ns)
{
    return bf_ptp_set_tod_time(tod_index, sec, ns);
}
