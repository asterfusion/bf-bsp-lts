/*!
 * @file 8a34004.h
 * @date 2026/07/08
 *
 * Hang Tsi (tsihang@asterfusion.com)
 *
 *
 */

#ifndef AFN_8A34004_REGS_H
#define AFN_8A34004_REGS_H

#include <stdint.h>

// Register Access Permissions
typedef enum {
    REG_PERM_R  = 0x01, // Read Only
    REG_PERM_W  = 0x02, // Write Only
    REG_PERM_RW = 0x03  // Read and Write
} reg_perm_t;

// Unified Register Descriptor Structure
typedef struct {
    const char *name;       // Human-readable string identifier
    uint8_t     page;       // Upper 8 bits (Page Selector, e.g., 0xC0)
    uint8_t     base;       // Base address (Base address, e.g., 0x14)
    uint16_t    offset;     // Lower 8 bits (Register Window Offset, e.g., 0x10)
    uint8_t     width_bytes;// Number of continuous bytes to burst read/write
    reg_perm_t  perm;       // Software barrier flags
} ptp_reg_desc_t;
#define __ptp_reg_desc_init_val__ { NULL, 0, 0, 0, 0, REG_PERM_R }

// System-wide 8A34004 Functional Register Matrix Array Definition
static const ptp_reg_desc_t g_8a34004_register_map[] = {    
    /* ========================================================================= */
    /* 1. GLOBAL HARDWARE REVISION & ID ENTITIES                                 */
    /* ========================================================================= */
    { "GENERAL_STATUS.PRODUCT_ID_L", 0xC0, 0x14, 0x1E, 1, REG_PERM_R  },
    { "GENERAL_STATUS.PRODUCT_ID_H", 0xC0, 0x14, 0x1F, 1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MAJOR",0xC0, 0x14, 0x10, 1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MINOR",0xC0, 0x14, 0x11, 1, REG_PERM_R  },
    { "HW_REVISION.REV_ID",          0x81, 0x80, 0x7A, 1, REG_PERM_R  }, // Product revision validation target

    /* ========================================================================= */
    /* 2. ASTERFUSION CUSTOM HOLDOVER PROTECTION FLAGGING                       */
    /* ========================================================================= */
    { "SCRATCH.SCRATCH3",            0xCF, 0x50, 0x0E, 1, REG_PERM_RW },

    /* ========================================================================= */
    /* 3. PHYSICAL LINE SYNCE INPUT CLOCK ARRAYS ((M/N) FRACTION FORM)          */
    /* ========================================================================= */
    { "INPUT_0.IN_FREQ",             0xC1, 0xB0, 0x00, 8, REG_PERM_RW }, /* OBSCLK 0: 6B M + 2B N */
    { "INPUT_1.IN_FREQ",             0xC1, 0xC0, 0x00, 8, REG_PERM_RW }, /* OBSCLK 1: 6B M + 2B N */

    /* ========================================================================= */
    /* 4. DPLL_5 TIMING METRICS & MODE CONTROL PIPELINE (SyncE / FREQUENCY)      */
    /* ========================================================================= */
    { "STATUS.DPLL5_REF_STAT",       0xC0, 0x3C, 0x27,  1, REG_PERM_R  },
    { "STATUS.DPLL5_STATUS",         0xC0, 0x3C, 0x1D,  1, REG_PERM_R  },
    { "STATUS.DPLL5_PHASE_STATUS",   0xC0, 0x3C, 0x104, 5, REG_PERM_R  }, /* 40-bit Signed sub-ns error */
    { "DPLL_5.DPLL_MODE",            0xC5, 0x00, 0x37,  1, REG_PERM_RW }, /* Verified Mode Switch register */
    { "DPLL_PHASE_5",                0xC8, 0x2C, 0x00,  4, REG_PERM_W  }, /* 32-bit Signed Phase command */

    /* ========================================================================= */
    /* 5. DPLL_6 TIMING METRICS & MODE CONTROL PIPELINE (PTP / PHASE / ToD)      */
    /* ========================================================================= */
    { "STATUS.DPLL6_REF_STAT",       0xC0, 0x3C, 0x28,  1, REG_PERM_R  },
    { "STATUS.DPLL6_STATUS",         0xC0, 0x3C, 0x1E,  1, REG_PERM_R  },
    { "STATUS.DPLL6_PHASE_STATUS",   0xC0, 0x3C, 0x10C, 5, REG_PERM_R  }, /* 40-bit Signed PTP phase error */
    { "DPLL_6.DPLL_MODE",            0xC5, 0x38, 0x37,  1, REG_PERM_RW }, /* Verified Mode Switch register */
    { "DPLL_PHASE_6",                0xC8, 0x30, 0x00,  4, REG_PERM_W  }, /* 32-bit Signed Phase command */

#if 0
    // =========================================================================
    // 2. SYSTEM TELEMETRY AND GLOBAL LOCK STATUS MODULE (Base: 0xC03C)
    // =========================================================================
    { "STATUS.IN0_MON_STATUS",      0xC0, 0x44, 1, REG_PERM_R  }, // General scratchpad register for API checks
    { "STATUS.IN5_MON_STATUS",      0xC0, 0x4D, 1, REG_PERM_R  }, // Loss of Signal (LOS) / Activity line trackers
    { "STATUS.DPLL5_STATUS",        0xC0, 0x59, 1, REG_PERM_R  }, // Frequency tracking state (SyncE Engine)
    { "STATUS.DPLL6_STATUS",        0xC0, 0x5A, 1, REG_PERM_R  }, // Phase tracking state (PTP 1PPS Engine)
    { "STATUS.DPLL5_PHASE_STATUS",  0xC1, 0x40, 5, REG_PERM_R  }, // 40-bit Signed sub-ns residual Phase Error [D0-D4]
    { "STATUS.DPLL6_PHASE_STATUS",  0xC1, 0x4C, 5, REG_PERM_R  }, // 40-bit Signed secondary tracking Phase Error
    
    // =========================================================================
    // 3. PHYSICAL LINE LAYER SYNCE INPUT CONFIGURATION MODULE (Base: 0xC1B0)
    // =========================================================================
    { "INPUT_0.IN_FREQ.M_VALUE",    0xC1, 0xB0, 6, REG_PERM_RW }, // 48-bit Fraction Frequency M-Numerator [D0-D5]
    { "INPUT_0.IN_FREQ.N_VALUE",    0xC1, 0xB6, 2, REG_PERM_RW }, // 16-bit Fraction Frequency N-Denominator [D6-D7]
    
    // =========================================================================
    // 4. DIGITAL PHASE-LOCKED LOOP OPERATIONAL MODE CONTROLLERS
    // =========================================================================
    { "DPLL_5.DPLL_MODE",           0xC2, 0x20, 1, REG_PERM_RW }, // Controls PLL_MODE[5:3] for DPLL5 steering
    { "DPLL_6.DPLL_MODE",           0xC2, 0x28, 1, REG_PERM_RW }, // Controls PLL_MODE[5:3] for DPLL6 steering
    { "DPLL_5.DPLL_REF_SEL",        0xC0, 0x84, 1, REG_PERM_RW }, // Target hardware input multiplexer selection
    
    // =========================================================================
    // 5. HARDWARE PHASE STEERING INJECTION MODULE (Base: 0xC818)
    // =========================================================================
    { "DPLL_PHASE_0.DPLL5_WRITE_PH", 0xC8, 0x2C, 4, REG_PERM_W  }, // 32-bit Signed Phase Increment [D0-D3] (DPLL5)
    { "DPLL_PHASE_0.DPLL6_WRITE_PH", 0xC8, 0x30, 4, REG_PERM_W  }, // 32-bit Signed Phase Increment [D0-D3] (DPLL6)
    { "DPLL_PHASE_0.PH_EXEC_TRIGGER",0xC8, 0x0F, 1, REG_PERM_W  }, // Operational Phase execution gate trigger
    
    // =========================================================================
    // 6. HARDWARE FREQUENCY MICRO-ADJUSTMENT MODULE (Base: 0xC838)
    // =========================================================================
    { "DPLL_FREQ_0.DPLL5_WR_FREQ",  0xC8, 0x60, 6, REG_PERM_W  }, // 42-bit Signed Frequency FFO Adj [D0-D5] (DPLL5)
    { "DPLL_FREQ_0.DPLL6_WR_FREQ",  0xC8, 0x68, 6, REG_PERM_W  }, // 42-bit Signed Frequency FFO Adj [D0-D5] (DPLL6)
    { "DPLL_FREQ_0.FREQ_EXEC_TRIGGER",0xC8, 0x3F, 1, REG_PERM_W  }, // Operational Frequency execution gate trigger
    
    // =========================================================================
    // 7. REAL-TIME ATOMIC TIME-OF-DAY (ToD) READ / WRITE MODULES
    // =========================================================================
    { "TOD_WRITE_0.WRITE_COUNTER",  0xCB, 0x0C, 1, REG_PERM_R  }, // Validates ToD write completion increments
    { "TOD_READ_0.SUB_NANOSECONDS", 0xCC, 0x00, 2, REG_PERM_R  }, // Real-time Sub-nanosecond capture [D0-D1]
    { "TOD_READ_0.NANOSECONDS",     0xCC, 0x02, 4, REG_PERM_R  }, // Real-time Nanosecond capture array [D2-D5]
    { "TOD_READ_0.SECONDS",         0xCC, 0x06, 4, REG_PERM_R  }, // Real-time Absolute Unix Epoch Seconds [D6-D9]
    { "TOD_READ_0.READ_TRIGGER",    0xCC, 0x0A, 1, REG_PERM_W  }, // Absolute hardware snapshot capture latch gate
#endif
};

#define BF_8A34004_REG_MAP_SIZE (sizeof(g_8a34004_register_map) / sizeof(ptp_reg_desc_t))
#define REGADDR(regdesc) ((regdesc).base + (regdesc).offset)
#define REGPAGE(regdesc) ((regdesc).page)
#define REGWIDB(regdesc) ((regdesc).width_bytes)

static inline
const char* dpll_status_str(int dpllno, uint8_t regval) {
    (void)dpllno;
    uint8_t s = (regval  & 0x0F);
    switch (s) {
        case 0:     return "FreeRun";
        case 1:     return "LockACQ";
        case 2:     return "LockREC";
        case 3:     return "Locked";
        case 4:     return "Holdover";
        case 5:     return "OpenLoop";
        case 6:     return "Disabled";
        default:    return "Unknown";
    }
}
// Operation mode
static inline
const char* dpll_mode_op_str(uint8_t regval) {
    uint8_t pll_mode = (regval >> 3) & 0x07;

    switch (pll_mode) {
        case 0:
            return "PLL mode (Closed Loop)";
        case 1:
            return "Write Phase Mode (PTP Phase Steering)";
        case 2:
            return "Write Frequency Mode (SyncE FFO Steering)";
        case 3:
            return "GPIO inc/dec mode";
        case 4:
            return "Synthesizer Mode";
        case 5:
            return "Phase Measurement Mode";
        case 6:
            return "Disabled Mode (Reduced Phase Noise)";
        default:
            return "Reserved / Unknown Mode";
    }
}
// State Machine Mode
static inline
const char* dpll_mode_sm_str(uint8_t regval) {
    uint8_t sm_mode = (regval & 0x07);

    switch (sm_mode) {
        case 0:
            return "Automatic";
        case 1:
            return "Force Lock";
        case 2:
            return "Force freerun";
        case 3:
            return "Force holdover";
        default:
            return "Reserved / Unknown SM Mode";
    }
}
static inline
int32_t dpll_phase(uint8_t *reg_bytes) {
    uint32_t unsigned_32bit = 0;

    unsigned_32bit = ((uint32_t)reg_bytes[3] << 24) |
                     ((uint32_t)reg_bytes[2] << 16) |
                     ((uint32_t)reg_bytes[1] << 8)  |
                     ((uint32_t)reg_bytes[0]);

    int32_t signed_phase = (int32_t)unsigned_32bit;

    return signed_phase;
}

/**
 * @brief Decodes the raw 5-byte data stream of STATUS.DPLL5_PHASE_STATUS into nanoseconds.
 *
 * @param reg_040h_buf Pointer to a 5-byte buffer containing raw data read
 *                      sequentially from address 0xC140 (Offset 0x40 inside PAGE 0xC1).
 * @return double The decoded residual phase offset error value in nanoseconds (ns).
 */
static inline
double dpll_decode_phase_status(int dpllno, uint8_t *reg_040h_buf) {
// 1 LSB of the internal phase detector is exactly 1 UI * 2^-16.
// For the nominal internal clock operating around 3.84GHz, 1 LSB = 3.970588 picoseconds (or 0.003970588 ns).
#define DPLL_PHASE_RESOLUTION_NS 0.003970588

    int64_t raw_val_40bit = 0;
    (void)dpllno;

    // 1. Construct the 40-bit integer from the 5-byte stream using Little-Endian order
    // reg_040h_buf[0] is D0 (LSB), reg_040h_buf[4] is D4 (MSB)
    raw_val_40bit = ((int64_t)reg_040h_buf[4] << 32) |
                    ((int64_t)reg_040h_buf[3] << 24) |
                    ((int64_t)reg_040h_buf[2] << 16) |
                    ((int64_t)reg_040h_buf[1] << 8)  |
                    ((int64_t)reg_040h_buf[0]);

    // 2. Perform manual 40-bit signed Two's Complement Sign-Extension to 64-bit int64_t.
    // Check if Bit 39 (the 40th bit, mask 0x8000000000) is set to 1 (indicating a negative value).
    if (raw_val_40bit & 0x0000008000000000LL) {
        // Sign-extend high 24 bits to 1s to properly register a negative integer in C
        raw_val_40bit |= 0xFFFFFF0000000000LL;
    } else {
        // Enforce pure 40-bit boundary limits for clear positive integers
        raw_val_40bit &= 0x000000FFFFFFFFFFLL;
    }

    // 3. Convert the signed scalar count into physical wall-time nanoseconds
    double phase_ns = (double)raw_val_40bit * DPLL_PHASE_RESOLUTION_NS;

    return phase_ns;
}

/**
 * @brief Programs the 32-bit signed phase increment to DPLL5 or DPLL6.
 *        Enforces atomic multi-byte burst writes and handles the mandatory latch triggers.
 *
 * @DPLL_FREQ_n.DPLL_WR_PH
 */
static inline
int dpll_write_phase(int dpllno, int32_t phase) {
    uint8_t burst_buf[4], page = 0xC8, reg, offset = 0x00;

    if(dpllno == 5) {
        reg = 0x2C;
    } else if (dpllno == 6) {
        reg = 0x30;
    } else {
        return -1; // Invalid DPLL channel
    }

    /* DPLL_n.DPLL_MODE.PLL_MODE = write phase mode */

    burst_buf[0] = (uint8_t)(phase & 0xFF);
    burst_buf[1] = (uint8_t)((phase >> 8) & 0xFF);
    burst_buf[2] = (uint8_t)((phase >> 16) & 0xFF);
    burst_buf[3] = (uint8_t)((phase >> 24) & 0xFF);

    for(int i = 0; i < 4; i ++) {
        bf_pltfm_write_ptp_reg(page, reg + offset + i, burst_buf[i]);
    }

    // Hard sleep required by Renesas firmware to let the micro-sequencer latch the phase shift safely
    usleep(200);

    // Phase Pull-in ?
    return 0;
}

/**
 * @brief Programs the 42-bit signed frequency micro-adjustment to DPLL5 or DPLL6.
 *        Accepts a 64-bit parameter to protect high-bit data integrity boundaries.
 *
 * @DPLL_FREQ_n.DPLL_WR_FREQ
 */ 
static inline
int write_dpll_freq(int dpllno, int64_t freq) {
    uint8_t burst_buf[6], page = 0xC8, reg, offset = 0x00, original_d5;

    int64_t cleaned_freq = freq & 0x3FFFFFFFFFFLL;

    if(dpllno == 5) {
        /* 0xC860 */
        reg = 0x60;
    } else if (dpllno == 6) {
        /* 0xC868 */
        reg = 0x68;
    } else {
        return -1;
    }

    /* DPLL_n.DPLL_MODE.PLL_MODE = write frequency mode */

    if (bf_pltfm_read_ptp_reg(page, offset + 5, &original_d5) < 0) {
        return -2;
    }

    // Map the entire 42-bit word across 6 continuous Little-Endian bytes (D0 to D5)
    burst_buf[0] = (uint8_t)(cleaned_freq & 0xFF);         // D0
    burst_buf[1] = (uint8_t)((cleaned_freq >> 8) & 0xFF);  // D1
    burst_buf[2] = (uint8_t)((cleaned_freq >> 16) & 0xFF); // D2
    burst_buf[3] = (uint8_t)((cleaned_freq >> 24) & 0xFF); // D3
    burst_buf[4] = (uint8_t)((cleaned_freq >> 32) & 0xFF); // D4
    burst_buf[5] = (original_d5 & 0xFC) | (uint8_t)((cleaned_freq >> 40) & 0x03);

    for(int i = 0; i < 6; i ++) {
        bf_pltfm_write_ptp_reg(page, reg + offset + i, burst_buf[i]);
    }

    // Hard sleep required by Renesas firmware to let the micro-sequencer latch the phase shift safely
    usleep(200);

    return 0;
}

/**
 * @brief Parse the raw 4-byte data returned from page C1 of the 8A34004 chip,
 *        and calculate the actual physical layer observation clock frequency.
 * @param le_bytes Pointer to a 4-byte little-endian array returned by the chip
 *                 (e.g., 40 2b 7f 5e).
 * @return double The computed real frequency value in MHz.
 */
static inline
double bf_ptp_decode_obsclk_freq(int inputno, uint8_t *reg_000h_buf) {
    uint64_t m_val = 0;
    uint16_t n_val = 0;
    (void)inputno;
    m_val = ((uint64_t)reg_000h_buf[5] << 40) |
            ((uint64_t)reg_000h_buf[4] << 32) |
            ((uint64_t)reg_000h_buf[3] << 24) |
            ((uint64_t)reg_000h_buf[2] << 16) |
            ((uint64_t)reg_000h_buf[1] << 8)  |
            ((uint64_t)reg_000h_buf[0]);

    n_val = ((uint16_t)reg_000h_buf[7] << 8) | 
            ((uint16_t)reg_000h_buf[6]);

    if (n_val == 0) {
        n_val = 1;
    }
    //fprintf(stdout, "m_val = %lu n_val=%u\n", m_val, n_val);
    double freq_hz = (double)m_val / (double)n_val;

    return freq_hz;
}

/**
 * @brief Resolves paged timing register specifications from a descriptor string name.
 * 
 * @param name The target structural register path string (e.g., "STATUS.DPLL5_PHASE_STATUS")
 * @param out_desc Output pointer populated with the verified hardware properties.
 * @return int 0 if matched; -1 if the requested string is unknown.
 */
static inline
int bf_ptp_lookup_register(const char *name, ptp_reg_desc_t *out_desc) {
    if (!name || !out_desc) return -1;

    for (size_t i = 0; i < BF_8A34004_REG_MAP_SIZE; i++) {
        if (strcmp(g_8a34004_register_map[i].name, name) == 0) {
            *out_desc = g_8a34004_register_map[i];
            return 0; // Target found
        }
    }

    return -1; // Unregistered address identifier
}

static inline
int bf_ptp_get_product_id(uint16_t *product_id) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if(bf_ptp_lookup_register("GENERAL_STATUS.PRODUCT_ID_L", &regdesc)) {
        return -1;
    }

    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *product_id = regval;

    if(bf_ptp_lookup_register("GENERAL_STATUS.PRODUCT_ID_H", &regdesc)) {
        return -1;
    }

    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *product_id |= regval << 8;


    return 0;
}

static inline
int bf_ptp_get_release_no(uint8_t *major, uint8_t *minor, uint8_t *rev) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if(bf_ptp_lookup_register("GENERAL_STATUS.RELEASE_MAJOR", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *major = (regval >> 1);
    *rev   = (regval & 0x01);

    if(bf_ptp_lookup_register("GENERAL_STATUS.RELEASE_MINOR", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *minor |= regval;

    return 0;
}

static inline
int bf_ptp_get_input_freq(double *input_0, double *input_1) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[BUFSIZ] = { 0 };

    if(bf_ptp_lookup_register("INPUT_0.IN_FREQ", &regdesc)) {
        return -1;
    }

    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset ++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *input_0 = bf_ptp_decode_obsclk_freq(0, buf);

    if(bf_ptp_lookup_register("INPUT_1.IN_FREQ", &regdesc)) {
        return -1;
    }

    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset ++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *input_1 = bf_ptp_decode_obsclk_freq(1, buf);

    return 0;
}

/**
 * @brief Retrieves the live tracking reference status of DPLL5.
 * 
 * @param ref_stat Output pointer populated with the upstream physical CLK channel index.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll5_ref_stat(uint8_t *ref_stat) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("STATUS.DPLL5_REF_STAT", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *ref_stat = regval;

    return 0;
}

/**
 * @brief Retrieves the core state machine lock configuration of DPLL5.
 * 
 * @param status Output pointer populated with the raw state machine byte token.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll5_status(uint8_t *status) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("STATUS.DPLL5_STATUS", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *status = regval;

    return 0;
}

/**
 * @brief Atomically extracts and decodes the 40-bit residual sub-ns phase offset error of DPLL5.
 * 
 * @param phase_ns Output pointer populated with the decoded absolute phase error in nanoseconds.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll5_phase_status(double *phase_ns) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[BUFSIZ] = { 0 };

    if (bf_ptp_lookup_register("STATUS.DPLL5_PHASE_STATUS", &regdesc)) {
        return -1;
    }

    /* Enforce continuous stride alignment via the registered burst width (5 Bytes) */
    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *phase_ns = dpll_decode_phase_status(5, buf);

    return 0;
}

/**
 * @brief Retrieves the current operational operational configuration mode byte of DPLL5.
 * 
 * @param mode_val Output pointer populated with the raw operational mode control byte.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll5_mode(uint8_t *mode_val) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("DPLL_5.DPLL_MODE", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *mode_val = regval;

    return 0;
}

/**
 * @brief Extracts the active 32-bit signed phase command offset applied to DPLL5.
 * 
 * @param phase_cmd Output pointer populated with the integer command value in hardware steps.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll5_phase(int32_t *phase_cmd) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[BUFSIZ] = { 0 };

    if (bf_ptp_lookup_register("DPLL_PHASE_5", &regdesc)) {
        return -1;
    }

    /* Enforce continuous stride alignment via the registered burst width (4 Bytes) */
    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *phase_cmd = dpll_phase(buf);

    return 0;
}

/**
 * @brief Retrieves the live tracking reference status of DPLL6 (PTP Engine).
 * 
 * @param ref_stat Output pointer populated with the upstream physical CLK channel index.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_ref_stat(uint8_t *ref_stat) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("STATUS.DPLL6_REF_STAT", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *ref_stat = regval;

    return 0;
}

/**
 * @brief Retrieves the core state machine lock configuration of DPLL6.
 * 
 * @param status Output pointer populated with the raw state machine byte token.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_status(uint8_t *status) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("STATUS.DPLL6_STATUS", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *status = regval;

    return 0;
}

/**
 * @brief Atomically extracts and decodes the 40-bit residual sub-ns phase offset error of DPLL6.
 * 
 * @param phase_ns Output pointer populated with the decoded absolute phase error in nanoseconds.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_phase_status(double *phase_ns) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[BUFSIZ] = { 0 };

    if (bf_ptp_lookup_register("STATUS.DPLL6_PHASE_STATUS", &regdesc)) {
        return -1;
    }

    /* Enforce continuous stride alignment via the registered burst width (5 Bytes) */
    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *phase_ns = dpll_decode_phase_status(6, buf);

    return 0;
}

/**
 * @brief Retrieves the current operational configuration mode byte of DPLL6.
 * 
 * @param mode_val Output pointer populated with the raw operational mode control byte.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_mode(uint8_t *mode_val) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t regval = 0;

    if (bf_ptp_lookup_register("DPLL_6.DPLL_MODE", &regdesc)) {
        return -1;
    }
    bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc), &regval);
    *mode_val = regval;

    return 0;
}

/**
 * @brief Extracts the active 32-bit signed phase command offset applied to DPLL6.
 * 
 * @param phase_cmd Output pointer populated with the integer command value in hardware steps.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_phase(int32_t *phase_cmd) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t buf[BUFSIZ] = { 0 };

    if (bf_ptp_lookup_register("DPLL_PHASE_6", &regdesc)) {
        return -1;
    }

    /* Enforce continuous stride alignment via the registered burst width (4 Bytes) */
    for (int8_t offset = 0; offset < (int8_t)REGWIDB(regdesc); offset++) {
        bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + offset, &buf[offset]);
    }
    *phase_cmd = dpll_phase(buf);

    return 0;
}

#endif // AFN_8A34004_REGS_H
