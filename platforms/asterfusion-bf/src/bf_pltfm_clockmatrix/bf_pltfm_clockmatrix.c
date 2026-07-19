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

struct clockmatrix_t clockmatrix;

static inline
void do_lock()
{
	struct clockmatrix_t *matrix = &clockmatrix;
    pthread_mutex_lock(&matrix->lock);
}

static inline
void do_unlock()
{
	struct clockmatrix_t *matrix = &clockmatrix;
    pthread_mutex_unlock(&matrix->lock);
}


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

    if (bf_pltfm_cp2112_reg_write_block(ptp_addr, reg, value, l)) {
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

int bf_ptp_set_dpll_freq(uint8_t dpll_index, int64_t freq_ffo_q53)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };

    if (dpll_index > 7) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL%d.FREQ", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    /* Write full 48-bit FCW (matches upstream ptp_clockmatrix.c).
     * The 8A34004 DPLL_WR_FREQ is a 6-byte register — masking to
     * 42 bits was truncating the upper 6 bits and causing FCW
     * overflow at ±1000 ppm. */
    int64_t cleaned_freq = freq_ffo_q53 & 0xFFFFFFFFFFFFLL;

    burst_buf[0] = (uint8_t)(cleaned_freq & 0xFF);
    burst_buf[1] = (uint8_t)((cleaned_freq >> 8) & 0xFF);
    burst_buf[2] = (uint8_t)((cleaned_freq >> 16) & 0xFF);
    burst_buf[3] = (uint8_t)((cleaned_freq >> 24) & 0xFF);
    burst_buf[4] = (uint8_t)((cleaned_freq >> 32) & 0xFF);
    burst_buf[5] = (uint8_t)((cleaned_freq >> 40) & 0xFF);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 6) < 0) {
        return -4;
    }

    usleep(CLKMATRIX_DELAY_US);
    return 0;
}

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

int bf_ptp_phase_pull_in(uint8_t dpll_index, int32_t delta, uint32_t max_ffo_ppb)
{
    int rc;

    /* PULL_IN_OFFSET expects the target offset (negated correction).
     * Convert ns -> 50 ps LSBs: phase_50ps = -delta * 20 */
    int64_t offset_50ps = (int64_t)(-delta) * 20;
    if (offset_50ps >  INT32_MAX) offset_50ps =  INT32_MAX;
    if (offset_50ps < -INT32_MAX) offset_50ps = -INT32_MAX;

    rc = bf_ptp_set_phase_pull_in_offset(dpll_index, (int32_t)offset_50ps);
    if (rc < 0) return rc;

    rc = bf_ptp_set_phase_pull_in_slope_limit(dpll_index, max_ffo_ppb);
    if (rc < 0) return rc;

    return bf_ptp_start_phase_pull_in(dpll_index);
}

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

int bf_ptp_get_tod_time(uint8_t tod_index, uint64_t *out_sec, uint64_t *out_ns)
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

    /* 3. CONTINUOUS STRIDE BURST READ: Extract all 11 bytes sequentially to prevent phase tearing */
	if (bf_pltfm_read_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 11) < 0) {
		return -4;
	}

    /* 4. DECODE NANOSECONDS (Little-Endian assembly across offsets 001h to 004h) */
    *out_ns = ((uint64_t)burst_buf[4] << 24) |
              ((uint64_t)burst_buf[3] << 16) |
              ((uint64_t)burst_buf[2] << 8)  |
              ((uint64_t)burst_buf[1]);

    /* 5. DECODE SECONDS (48-bit Little-Endian across offsets 005h to 00Ah) */
    *out_sec = ((uint64_t)burst_buf[10] << 40) |
               ((uint64_t)burst_buf[9]  << 32) |
               ((uint64_t)burst_buf[8]  << 24) |
               ((uint64_t)burst_buf[7]  << 16) |
               ((uint64_t)burst_buf[6]  << 8)  |
               ((uint64_t)burst_buf[5]);

    return 0; /* Coherent ToD fetched successfully */
}

int bf_ptp_set_tod_time(uint8_t tod_index, uint64_t sec, uint64_t ns)
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

    /* 3. Preload seconds (48-bit Little-Endian, offsets 005h-00Ah) */
    burst_buf[5]  = (uint8_t)(sec & 0xFF);
    burst_buf[6]  = (uint8_t)((sec >> 8)  & 0xFF);
    burst_buf[7]  = (uint8_t)((sec >> 16) & 0xFF);
    burst_buf[8]  = (uint8_t)((sec >> 24) & 0xFF);
    burst_buf[9]  = (uint8_t)((sec >> 32) & 0xFF);
    burst_buf[10] = (uint8_t)((sec >> 40) & 0xFF);

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

int bf_ptp_get_tod_time_triggered(uint8_t tod_index,
                                   uint64_t *out_sec, uint64_t *out_ns)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[64] = { 0 };
    char name_buf[64] = { 0 };
    uint8_t trigger = 0;

    if (tod_index > 3 || !out_sec || !out_ns) {
        return -1;
    }

    /* 1. Check that a snapshot is ready (trigger bit cleared → latched) */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &trigger) < 0) {
        return -3;
    }

    if (trigger & TOD_READ_TRIGGER_MASK) {
	    /* EBUSY: previous trigger still pending */
        return -4;
    }

    /* 2. BURST-READ 11-byte snapshot array from SECONDARY BASE */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.SUB_NS", tod_index);

    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -5;
    }

	if (bf_pltfm_read_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 11) < 0) {
		return -6;
	}

    /* 3. DECODE NANOSECONDS (Little-Endian across offsets 001h-004h) */
    *out_ns = ((uint64_t)burst_buf[4] << 24) |
              ((uint64_t)burst_buf[3] << 16) |
              ((uint64_t)burst_buf[2] <<  8) |
              ((uint64_t)burst_buf[1]);

    /* 4. DECODE SECONDS (48-bit Little-Endian across offsets 005h-00Ah) */
    *out_sec = ((uint64_t)burst_buf[10] << 40) |
               ((uint64_t)burst_buf[9]  << 32) |
               ((uint64_t)burst_buf[8]  << 24) |
               ((uint64_t)burst_buf[7]  << 16) |
               ((uint64_t)burst_buf[6]  <<  8) |
               ((uint64_t)burst_buf[5]);

    return 0;
}

int bf_ptp_arm_tod_read_trigger_refclk(uint8_t tod_index, uint8_t ref)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    char name_buf[64] = { 0 };
    uint8_t reg_byte = 0;

    if (tod_index > 3 || ref > 15) {
        return -1;
    }

    /* Write selected reference clock index into SEL_CFG_0 */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.SEL_CFG_0", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

	reg_byte &= ~(WR_REF_INDEX_MASK << WR_REF_INDEX_SHIFT);
	reg_byte |= (ref << WR_REF_INDEX_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0) {
        return -3;
    }

    /* Arm trigger: REFCLK edge detect */
    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -4;
    }

    reg_byte = 0 | (SCSR_TOD_READ_TRIG_SEL_REFCLK << TOD_READ_TRIGGER_SHIFT);

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0) {
        return -5;
    }

    return 0;
}

int bf_ptp_arm_tod_read_trigger_immediate(uint8_t tod_index)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    char name_buf[64] = { 0 };
    uint8_t reg_byte = 0;

    if (tod_index > 3) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf),
             "TOD%d.READ_SECONDARY.TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    reg_byte = (uint8_t)SCSR_TOD_READ_TRIG_SEL_IMMEDIATE;
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
             reg_byte) < 0) {
        return -3;
    }

    return 0;
}

int bf_ptp_adjtime(uint8_t tod_index, int64_t delta)
{
    uint64_t sec = 0, ns = 0;
	uint8_t dpll_index = 0;
    int err = 0;

    if (tod_index > 3) {
        return -1;
    }

	do_lock();

	/* Small deltas: use FW phase pull-in via the driving DPLL.
     * This avoids an instant ToD counter step that would confuse
     * userspace clock_gettime consumers. */
    if ((delta > -PHASE_PULL_IN_THRESHOLD_NS) &&
        (delta <  PHASE_PULL_IN_THRESHOLD_NS)) {

        if (bf_ptp_get_tod_clk_src(tod_index, &dpll_index) < 0)
            goto fallback_step;

        err = bf_ptp_phase_pull_in(dpll_index, (int32_t)delta, 0);
        goto finish;
    }

fallback_step:
    err = bf_ptp_get_tod_time(tod_index, &sec, &ns);
    if (err < 0) {
        goto finish;
    }

    int64_t total_ns = (int64_t)sec * NSEC_PER_SEC + (int64_t)ns + delta;

    /* Validate: ToD + offset must stay >= Unix epoch (0) */
    if (total_ns < 0) {
        fprintf (stdout,
                "Epoch guardrail: resulting time would be %lld ns before epoch\n",
                (long long)total_ns);
        err = -ERANGE;
        goto finish;
    }

    sec = (total_ns / NSEC_PER_SEC);
    ns  = (total_ns % NSEC_PER_SEC);

    err = bf_ptp_set_tod_time(tod_index, sec, ns);

finish:
    do_unlock();
    return err;
}

int bf_ptp_adjphase(uint8_t tod_index, int32_t delta)
{
    enum pll_mode mode;
	uint8_t dpll_index = 0;
    int err;

    if (tod_index > 3) {
        return -1;
    }

	do_lock();
    err = bf_ptp_get_tod_clk_src(tod_index, &dpll_index);
    if (err < 0) {
	    goto finish;
    }

    /* Ensure DPLL is in WRITE_PHASE (preserves SM_MODE via RMW) */
    err = bf_ptp_get_dpll_mode(dpll_index, &mode);
    if (err < 0) {
        goto finish;
    }

    if (mode != PLL_MODE_WRITE_PHASE) {
        err = bf_ptp_set_dpll_mode(dpll_index, PLL_MODE_WRITE_PHASE);
        if (err < 0) {
            goto finish;
        }
    }

    /*
     * Phase register resolution: 50 ps / LSB.
     *           offset_ps = (s64)delta * 1000
     *           phase_50ps = div_s64(offset_ps, 50)
     * So: phase_50ps = delta * 1000 / 50 = delta * 20
     */
    int64_t phase_50ps = (int64_t)delta * DPLL_PHASE_PER_NS;

    /* Clamp to 32-bit signed range (HW register is int32) */
    if (phase_50ps >  INT32_MAX) phase_50ps =  INT32_MAX;
    if (phase_50ps < -INT32_MAX) phase_50ps = -INT32_MAX;

    err = bf_ptp_set_dpll_phase(dpll_index, (int32_t)phase_50ps);

finish:
    do_unlock();
    return err;
}

int bf_ptp_adjfine(uint8_t tod_index, int64_t scaled_ppm)
{
    enum pll_mode mode;
	uint8_t dpll_index = 0;
    int err;

    if (tod_index > 3) {
        return -1;
    }

	do_lock();

    err = bf_ptp_get_tod_clk_src(tod_index, &dpll_index);
    if (err < 0) {
	    goto finish;
    }

    /* Ensure DPLL is in WRITE_FREQUENCY (preserves SM_MODE via RMW) */
    err = bf_ptp_get_dpll_mode(dpll_index, &mode);
    if (err < 0) {
	    goto finish;
    }

    if (mode != PLL_MODE_WRITE_FREQUENCY) {
        err = bf_ptp_set_dpll_mode(dpll_index, PLL_MODE_WRITE_FREQUENCY);
        if (err < 0) {
            goto finish;
        }
    }

	/*
	 * Frequency Control Word unit is: 1.11 * 10^-10 ppm
	 *
	 * adjfreq:
	 *       ppb * 10^9
	 * FCW = ----------
	 *          111
	 *
	 * adjfine:
	 *       ppm_16 * 5^12
	 * FCW = -------------
	 *         111 * 2^4
	 */

	/* 2 ^ -53 = 1.1102230246251565404236316680908e-16 */

    /*
     * FCW conversion (matches upstream ptp_clockmatrix.c):
     *   FCW = scaled_ppm * 5^12 / (111 * 2^4)
     *       = scaled_ppm * 244140625 / 1776
     */
    int64_t fcw = (scaled_ppm * 244140625LL) / 1776;
    err = bf_ptp_set_dpll_freq(dpll_index, fcw);

finish:
    do_unlock();
    return err;
}

int bf_clock_reset(void)
{
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    int err;

	do_lock();

    err = bf_ptp_lookup_register_general("RESET_CTRL.SM_RESET",
            &regdesc);
    if (err) {
        goto finish;
    }

    err = bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), 0x5A);
    if (err < 0) {
        goto finish;
    }

    usleep(CLKMATRIX_DELAY_US);
finish:
	do_unlock();
    return err;
}

int bf_clock_settime(clockid_t clkid, struct timespec *tp)
{
    uint8_t tod_index = (uint8_t)clkid;
	int err;

    if (tod_index > 3 || !tp) {
        return -EINVAL;
    }

    /* No sec range check — 8A34004 ToD has 48-bit seconds
     * (max ~8.9 million years).  bf_ptp_set_tod_time takes uint32_t
     * sec (~136 years from epoch, sufficient until 2106). */
    if (tp->tv_sec < 0 || tp->tv_nsec < 0 ||
        (unsigned long)tp->tv_nsec >= NSEC_PER_SEC ||
        (uint64_t)tp->tv_sec >= (1ULL << 48)) {
        return -ERANGE;
    }

	do_lock();
	err = bf_ptp_set_tod_time(tod_index, tp->tv_sec, tp->tv_nsec);
	do_unlock();

    return err;
}

int bf_clock_gettime(clockid_t clkid, struct timespec *tp)
{
    uint8_t  tod_index = (uint8_t)clkid;
    uint64_t sec = 0, ns = 0;
	int rc;

    if (tod_index > 3 || !tp) {
        return -EINVAL;
    }

    tp->tv_sec  = 0;
    tp->tv_nsec = 0;

    rc = bf_ptp_get_tod_time(tod_index,
			&sec, &ns);
    if (rc < 0) {
        return rc;
    }

    tp->tv_sec  = (time_t)sec;
    tp->tv_nsec = (long)ns;
    return 0;
}

int bf_clock_getres(clockid_t clkid, struct timespec *res)
{
    uint8_t tod_index = (uint8_t)clkid;

    if (tod_index > 3 || !res) {
        return -EINVAL;
    }

    res->tv_sec  = 0;
    res->tv_nsec = 1;  /* 8A34004 ToD resolution: 1 ns */
    return 0;
}

/**
 * scaled_ppm_to_ppb() - convert scaled ppm to ppb
 *
 * @ppm:    Parts per million, but with a 16 bit binary fractional field
 */
static inline long scaled_ppm_to_ppb(long ppm)
{
	/*
	 * The 'freq' field in the 'struct timex' is in parts per
	 * million, but with a 16 bit binary fractional field.
	 *
	 * We want to calculate
	 *
	 *    ppb = scaled_ppm * 1000 / 2^16
	 *
	 * which simplifies to
	 *
	 *    ppb = scaled_ppm * 125 / 2^13
	 */
	int64_t ppb = 1 + ppm;

	ppb *= 125;
	ppb >>= 13;
	return (long)ppb;
}

int bf_clock_adjtime(clockid_t clkid, struct timex *tx)
{
    int err = -EOPNOTSUPP;
	struct clockmatrix_t *matrix = &clockmatrix;
    uint8_t tod_index = (uint8_t)clkid;

    if (tx->modes & ADJ_SETOFFSET) {
        int64_t delta;
		struct timespec64 ts;

        /* tx->time.{tv_sec, tv_usec} carries the offset (not absolute
         * time).  tv_usec -> nanoseconds unless ADJ_NANO is set. */
        ts.tv_sec  = tx->time.tv_sec;
        ts.tv_nsec = tx->time.tv_usec;

        if (!(tx->modes & ADJ_NANO)) {
            ts.tv_nsec *= NSEC_PER_USEC;
        }

		if ((unsigned long) ts.tv_nsec >= NSEC_PER_SEC)
			return -EINVAL;

        delta = (int64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
        err = bf_ptp_adjtime(tod_index, delta);

    } else if (tx->modes & ADJ_FREQUENCY) {
        long ppb = scaled_ppm_to_ppb(tx->freq);
        if (ppb > DPLL_PPB_THRESHOLD || ppb < -DPLL_PPB_THRESHOLD) {
            fprintf (stdout,
                    "Ppb out of range: %ld (|max| = %d ~ ±%d ppm)\n",
                    ppb, DPLL_PPB_THRESHOLD, DPLL_PPB_THRESHOLD);
            return -ERANGE;
        }

        err = bf_ptp_adjfine(tod_index, tx->freq);
		if (!err) {
			matrix->_tod[tod_index].dialed_frequency = tx->freq;
		}

	} else if (tx->modes & ADJ_OFFSET) {
        int32_t offset = (int32_t)tx->offset;

        /* tx->offset is in µs unless ADJ_NANO is set */
        if (!(tx->modes & ADJ_NANO)) {
            offset *= NSEC_PER_USEC;
        }

        if (offset > DPLL_PHASE_THRESHOLD || offset < -DPLL_PHASE_THRESHOLD) {
            fprintf (stdout,
                    "Offset out of range: %d (|max| = INT32_MAX/20 ~ 107.4 ms, HW phase limit)\n", offset);
            return -ERANGE;
        }

        err = bf_ptp_adjphase(tod_index, offset);

    } else if (tx->modes == 0) {
		tx->freq = matrix->_tod[tod_index].dialed_frequency;
        err = 0;
    }

    return err;
}

int bf_clockmatrix_init(void)
{
    if (! platform_type_equal (AFN_X732QT) ||
        ! platform_subtype_equal (V2P0)) {
        return 0;
    }

    fprintf (stdout,
             "\n\nInitializing clockmatrix   ...\n");

	pthread_mutex_init(&clockmatrix.lock, NULL);

	memset(&clockmatrix._tod, 0, sizeof(clockmatrix._tod));
	memset(&clockmatrix._pll, 0, sizeof(clockmatrix._pll));

	return 0;
}
