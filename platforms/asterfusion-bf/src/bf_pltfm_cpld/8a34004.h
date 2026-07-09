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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define AFN_8A34004_MAX_DPLL    8
#define AFN_8A34004_MAX_TOD     4

#define MAX_DPLL_ENTRIES        (16 + 1)
#define MAX_TOD_ENTRIES         (16 + 1)

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

// System-wide 8A34004 Functional Register Matrix Array Definition - General Registers
static const ptp_reg_desc_t g_8a34004_register_map_general[] = {
    /* GLOBAL HARDWARE REVISION & ID ENTITIES */
    { "GENERAL_STATUS.PRODUCT_ID_L",    0xC0, 0x14, 0x1E,  1, REG_PERM_R  },
    { "GENERAL_STATUS.PRODUCT_ID_H",    0xC0, 0x14, 0x1F,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MAJOR",   0xC0, 0x14, 0x10,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MINOR",   0xC0, 0x14, 0x11,  1, REG_PERM_R  },
    { "HW_REVISION.REV_ID",             0x81, 0x80, 0x7A,  1, REG_PERM_R  }, // Product revision validation target

    /* ASTERFUSION CUSTOM HOLDOVER PROTECTION FLAGGING */
    { "SCRATCH.SCRATCH3",               0xCF, 0x50, 0x0E,  1, REG_PERM_RW },

    /* PHYSICAL LINE SYNCE INPUT CLOCK ARRAYS ((M/N) FRACTION FORM) */
    { "INPUT_0.IN_FREQ",                0xC1, 0xB0, 0x00,  8, REG_PERM_RW }, /* OBSCLK 0: 6B M + 2B N */
    { "INPUT_1.IN_FREQ",                0xC1, 0xC0, 0x00,  8, REG_PERM_RW }, /* OBSCLK 1: 6B M + 2B N */

    { "DPLL_PHASE.EXEC_TRIGGER",        0xC8, 0x00, 0x0F,  1, REG_PERM_W  }, /* Global Phase trigger gate (Forces Pull-in) */
    { "DPLL_FREQ.EXEC_TRIGGER",         0xC8, 0x00, 0x3F,  1, REG_PERM_W  }, /* Global Frequency trigger gate */
    __ptp_reg_desc_init_val__
};

// System-wide 8A34004 Functional Register Matrix Array Definition - DPLL-Index-Based Registers
// End with NULL
static const ptp_reg_desc_t g_8a34004_register_map_dpll[AFN_8A34004_MAX_DPLL][MAX_DPLL_ENTRIES] = {
    /* DPLL 0 */
    {
        { "DPLL_FREQ_0",                    0xC8, 0x38, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_0",                   0xC8, 0x18, 0x00, 4, REG_PERM_W  },
        { "DPLL_0.DPLL_TOD_SYNC_CFG",       0xC3, 0xB0, 0x31, 1, REG_PERM_RW },
        { "DPLL_0.DPLL_COMBO_SLAVE_CFG_0",  0xC3, 0xB0, 0x32, 1, REG_PERM_RW },
        { "DPLL_0.DPLL_COMBO_SLAVE_CFG_1",  0xC3, 0xB0, 0x33, 1, REG_PERM_RW },
        { "DPLL_0.DPLL_MODE",               0xC3, 0xB0, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL0_STATUS",            0xC0, 0x3C, 0x18, 1, REG_PERM_R  },
        { "STATUS.DPLL0_REF_STAT",          0xC0, 0x3C, 0x22, 1, REG_PERM_R  },
        { "STATUS.DPLL0_PHASE_STATUS",      0xC0, 0x3C, 0xDC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 1 */
    {
        { "DPLL_FREQ_1",                    0xC8, 0x40, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_1",                   0xC8, 0x1C, 0x00, 4, REG_PERM_W  },
        { "DPLL_1.DPLL_TOD_SYNC_CFG",       0xC4, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL_1.DPLL_COMBO_SLAVE_CFG_0",  0xC4, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL_1.DPLL_COMBO_SLAVE_CFG_1",  0xC4, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL_1.DPLL_MODE",               0xC4, 0x00, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL1_STATUS",            0xC0, 0x3C, 0x19, 1, REG_PERM_R  },
        { "STATUS.DPLL1_REF_STAT",          0xC0, 0x3C, 0x23, 1, REG_PERM_R  },
        { "STATUS.DPLL1_PHASE_STATUS",      0xC0, 0x3C, 0xE4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 2 */
    {
        { "DPLL_FREQ_2",                    0xC8, 0x48, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_2",                   0xC8, 0x20, 0x00, 4, REG_PERM_W  },
        { "DPLL_2.DPLL_TOD_SYNC_CFG",       0xC4, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL_2.DPLL_COMBO_SLAVE_CFG_0",  0xC4, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL_2.DPLL_COMBO_SLAVE_CFG_1",  0xC4, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL_2.DPLL_MODE",               0xC4, 0x38, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL2_STATUS",            0xC0, 0x3C, 0x1A, 1, REG_PERM_R  },
        { "STATUS.DPLL2_REF_STAT",          0xC0, 0x3C, 0x24, 1, REG_PERM_R  },
        { "STATUS.DPLL2_PHASE_STATUS",      0xC0, 0x3C, 0xEC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 3 */
    {
        { "DPLL_FREQ_3",                    0xC8, 0x50, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_3",                   0xC8, 0x24, 0x00, 4, REG_PERM_W  },
        { "DPLL_3.DPLL_TOD_SYNC_CFG",       0xC4, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL_3.DPLL_COMBO_SLAVE_CFG_0",  0xC4, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL_3.DPLL_COMBO_SLAVE_CFG_1",  0xC4, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL_3.DPLL_MODE",               0xC4, 0x80, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL3_STATUS",            0xC0, 0x3C, 0x1B, 1, REG_PERM_R  },
        { "STATUS.DPLL3_REF_STAT",          0xC0, 0x3C, 0x25, 1, REG_PERM_R  },
        { "STATUS.DPLL3_PHASE_STATUS",      0xC0, 0x3C, 0xF4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 4 */
    {
        { "DPLL_FREQ_4",                    0xC8, 0x58, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_4",                   0xC8, 0x28, 0x00, 4, REG_PERM_W  },
        { "DPLL_4.DPLL_TOD_SYNC_CFG",       0xC4, 0xB8, 0x31, 1, REG_PERM_RW },
        { "DPLL_4.DPLL_COMBO_SLAVE_CFG_0",  0xC4, 0xB8, 0x32, 1, REG_PERM_RW },
        { "DPLL_4.DPLL_COMBO_SLAVE_CFG_1",  0xC4, 0xB8, 0x33, 1, REG_PERM_RW },
        { "DPLL_4.DPLL_MODE",               0xC4, 0xB8, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL4_STATUS",            0xC0, 0x3C, 0x1C, 1, REG_PERM_R  },
        { "STATUS.DPLL4_REF_STAT",          0xC0, 0x3C, 0x26, 1, REG_PERM_R  },
        { "STATUS.DPLL4_PHASE_STATUS",      0xC0, 0x3C, 0xFC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 5 */
    {
        { "DPLL_FREQ_5",                    0xC8, 0x60, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_5",                   0xC8, 0x2C, 0x00, 4, REG_PERM_W  },
        { "DPLL_5.DPLL_TOD_SYNC_CFG",       0xC5, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL_5.DPLL_COMBO_SLAVE_CFG_0",  0xC5, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL_5.DPLL_COMBO_SLAVE_CFG_1",  0xC5, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL_5.DPLL_MODE",               0xC5, 0x00, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL5_STATUS",            0xC0, 0x3C, 0x1D, 1, REG_PERM_R  },
        { "STATUS.DPLL5_REF_STAT",          0xC0, 0x3C, 0x27, 1, REG_PERM_R  },
        { "STATUS.DPLL5_PHASE_STATUS",      0xC0, 0x3C, 0x104, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 6 */
    {
        { "DPLL_FREQ_6",                    0xC8, 0x68, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_6",                   0xC8, 0x30, 0x00, 4, REG_PERM_W  },
        { "DPLL_6.DPLL_TOD_SYNC_CFG",       0xC5, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL_6.DPLL_COMBO_SLAVE_CFG_0",  0xC5, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL_6.DPLL_COMBO_SLAVE_CFG_1",  0xC5, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL_6.DPLL_MODE",               0xC5, 0x38, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL6_STATUS",            0xC0, 0x3C, 0x1E, 1, REG_PERM_R  },
        { "STATUS.DPLL6_REF_STAT",          0xC0, 0x3C, 0x28, 1, REG_PERM_R  },
        { "STATUS.DPLL6_PHASE_STATUS",      0xC0, 0x3C, 0x10C, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 7 */
    {
        { "DPLL_FREQ_7",                    0xC8, 0x70, 0x00, 6, REG_PERM_RW },
        { "DPLL_PHASE_7",                   0xC8, 0x34, 0x00, 4, REG_PERM_W  },
        { "DPLL_7.DPLL_TOD_SYNC_CFG",       0xC5, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL_7.DPLL_COMBO_SLAVE_CFG_0",  0xC5, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL_7.DPLL_COMBO_SLAVE_CFG_1",  0xC5, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL_7.DPLL_MODE",               0xC5, 0x80, 0x37, 1, REG_PERM_RW },
        { "STATUS.DPLL7_STATUS",            0xC0, 0x3C, 0x1F, 1, REG_PERM_R  },
        { "STATUS.DPLL7_REF_STAT",          0xC0, 0x3C, 0x29, 1, REG_PERM_R  },
        { "STATUS.DPLL7_PHASE_STATUS",      0xC0, 0x3C, 0x114, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    }
};

static const ptp_reg_desc_t g_8a34004_register_map_tod[AFN_8A34004_MAX_TOD][MAX_TOD_ENTRIES] = {
    /* TOD 0 */
    {
        { "TOD_READ_0.SUB_NANOSECONDS",     0xCC, 0x00, 0x00,  2, REG_PERM_R  },
        { "TOD_READ_0.NANOSECONDS",         0xCC, 0x00, 0x02,  4, REG_PERM_R  },
        { "TOD_READ_0.SECONDS",             0xCC, 0x00, 0x06,  4, REG_PERM_R  },
        { "TOD_READ_0.READ_TRIGGER",        0xCC, 0x00, 0x0A,  1, REG_PERM_W  },
        { "TOD_WRITE_0.SUB_NANOSECONDS",    0xCB, 0x00, 0x00,  2, REG_PERM_W  },
        { "TOD_WRITE_0.NANOSECONDS",        0xCB, 0x00, 0x02,  4, REG_PERM_W  },
        { "TOD_WRITE_0.SECONDS",            0xCB, 0x00, 0x06,  4, REG_PERM_W  },
        { "TOD_WRITE_0.WRITE_COUNTER",      0xCB, 0x00, 0x0C,  1, REG_PERM_R  },
        { "TOD_WRITE_0.WRITE_TRIGGER",      0xCB, 0x00, 0x0F,  1, REG_PERM_W  },
        { "TOD_0.TOD_CFG",                  0xCB, 0x00, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 1 */
    {
        { "TOD_READ_1.SUB_NANOSECONDS",     0xCC, 0x10, 0x00,  2, REG_PERM_R  },
        { "TOD_READ_1.NANOSECONDS",         0xCC, 0x10, 0x02,  4, REG_PERM_R  },
        { "TOD_READ_1.SECONDS",             0xCC, 0x10, 0x06,  4, REG_PERM_R  },
        { "TOD_READ_1.READ_TRIGGER",        0xCC, 0x10, 0x0A,  1, REG_PERM_W  },
        { "TOD_WRITE_1.SUB_NANOSECONDS",    0xCB, 0x10, 0x00,  2, REG_PERM_W  },
        { "TOD_WRITE_1.NANOSECONDS",        0xCB, 0x10, 0x02,  4, REG_PERM_W  },
        { "TOD_WRITE_1.SECONDS",            0xCB, 0x10, 0x06,  4, REG_PERM_W  },
        { "TOD_WRITE_1.WRITE_COUNTER",      0xCB, 0x10, 0x0C,  1, REG_PERM_R  },
        { "TOD_WRITE_1.WRITE_TRIGGER",      0xCB, 0x10, 0x0F,  1, REG_PERM_W  },
        { "TOD_1.TOD_CFG",                  0xCB, 0x10, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 2 */
    {
        { "TOD_READ_2.SUB_NANOSECONDS",     0xCC, 0x20, 0x00,  2, REG_PERM_R  },
        { "TOD_READ_2.NANOSECONDS",         0xCC, 0x20, 0x02,  4, REG_PERM_R  },
        { "TOD_READ_2.SECONDS",             0xCC, 0x20, 0x06,  4, REG_PERM_R  },
        { "TOD_READ_2.READ_TRIGGER",        0xCC, 0x20, 0x0A,  1, REG_PERM_W  },
        { "TOD_WRITE_2.SUB_NANOSECONDS",    0xCB, 0x20, 0x00,  2, REG_PERM_W  },
        { "TOD_WRITE_2.NANOSECONDS",        0xCB, 0x20, 0x02,  4, REG_PERM_W  },
        { "TOD_WRITE_2.SECONDS",            0xCB, 0x20, 0x06,  4, REG_PERM_W  },
        { "TOD_WRITE_2.WRITE_COUNTER",      0xCB, 0x20, 0x0C,  1, REG_PERM_R  },
        { "TOD_WRITE_2.WRITE_TRIGGER",      0xCB, 0x20, 0x0F,  1, REG_PERM_W  },
        { "TOD_2.TOD_CFG",                  0xCB, 0x20, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 3 */
    {
        { "TOD_READ_3.SUB_NANOSECONDS",     0xCC, 0x30, 0x00,  2, REG_PERM_R  },
        { "TOD_READ_3.NANOSECONDS",         0xCC, 0x30, 0x02,  4, REG_PERM_R  },
        { "TOD_READ_3.SECONDS",             0xCC, 0x30, 0x06,  4, REG_PERM_R  },
        { "TOD_READ_3.READ_TRIGGER",        0xCC, 0x30, 0x0A,  1, REG_PERM_W  },
        { "TOD_WRITE_3.SUB_NANOSECONDS",    0xCB, 0x30, 0x00,  2, REG_PERM_W  },
        { "TOD_WRITE_3.NANOSECONDS",        0xCB, 0x30, 0x02,  4, REG_PERM_W  },
        { "TOD_WRITE_3.SECONDS",            0xCB, 0x30, 0x06,  4, REG_PERM_W  },
        { "TOD_WRITE_3.WRITE_COUNTER",      0xCB, 0x30, 0x0C,  1, REG_PERM_R  },
        { "TOD_WRITE_3.WRITE_TRIGGER",      0xCB, 0x30, 0x0F,  1, REG_PERM_W  },
        { "TOD_3.TOD_CFG",                  0xCB, 0x30, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    }
};

#define BF_8A34004_REG_MAP_GENERAL_SIZE (sizeof(g_8a34004_register_map_general) / sizeof(ptp_reg_desc_t))
#define REGADDR(regdesc) ((regdesc).base + (regdesc).offset)
#define REGPAGE(regdesc) ((regdesc).page)
#define REGWIDB(regdesc) ((regdesc).width_bytes)
// 1 LSB of the internal phase detector is exactly 1 UI * 2^-16.
// For the nominal internal clock operating around 3.84GHz, 1 LSB = 3.970588 picoseconds (or 0.003970588 ns).
#define DPLL_PHASE_RESOLUTION_NS 0.003970588

static inline
const char* dpll_status_str(int dpll_index, uint8_t reg_byte) {
    (void)dpll_index;
    uint8_t s = (reg_byte  & 0x0F);
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

static inline
const char* dpll_refclk_str(uint8_t reg_byte)
{
    switch (reg_byte) {
        case 0x00: return "CLK0           ";
        case 0x01: return "CLK1           ";
        case 0x02: return "CLK2           ";
        case 0x03: return "CLK3           ";
        case 0x04: return "CLK4           ";
        case 0x05: return "CLK5           ";
        case 0x06: return "CLK6           ";
        case 0x07: return "CLK7           ";
        case 0x08: return "CLK8           ";
        case 0x09: return "CLK9           ";
        case 0x0A: return "CLK10          ";
        case 0x0B: return "CLK11          ";
        case 0x0C: return "CLK12          ";
        case 0x0D: return "CLK13          ";
        case 0x0E: return "CLK14          ";
        case 0x0F: return "CLK15          ";
        case 0x10: return "WRPhaseInput   ";
        case 0x11: return "WRFreqInput    ";
        case 0x12: return "XO_DPLL        ";
        case 0x13: return "CLK19(DPLL0)   ";
        case 0x14: return "CLK20(DPLL1)   ";
        case 0x15: return "CLK11(DPLL2)   ";
        case 0x16: return "CLK22(DPLL3)   ";
        case 0x17: return "CLK23(DPLL4)   ";
        case 0x18: return "CLK24(DPLL5)   ";
        case 0x19: return "CLK25(DPLL6)   ";
        case 0x1A: return "CLK26(DPLL7)   ";
        case 0x1B: return "CLK27(SYSDPLL) ";
        case 0x1C: return "CLK28          ";
        case 0x1F: return "NoReference    ";
        default:   return "Unknown        ";
    }
}

// Operation mode
static inline
const char* dpll_mode_op_str(uint8_t reg_byte) {
    uint8_t pll_mode = (reg_byte >> 3) & 0x07;

    switch (pll_mode) {
        case 0:
            return "PLL(Closed)";
        case 1:
            return "WritePhase";
        case 2:
            return "WriteFreq";
        case 3:
            return "GPIOSteer";
        case 4:
            return "Synthesizer";
        case 5:
            return "PhaseMeasure";
        case 6:
            return "Disabled";
        default:
            return "Reserved";
    }
}
// State Machine Mode
static inline
const char* dpll_mode_sm_str(uint8_t reg_byte) {
    uint8_t sm_mode = (reg_byte & 0x07);

    switch (sm_mode) {
        case 0:
            return "Automatic";
        case 1:
            return "ForceLock";
        case 2:
            return "ForceFreerun";
        case 3:
            return "ForceHoldover";
        default:
            return "Reserved";
    }
}
static inline
int32_t dpll_phase_le32(uint8_t *reg_bytes) {
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
double dpll_phase_status(int dpll_index, uint8_t *reg_bytes) {
    int64_t raw_val_40bit = 0;
    (void)dpll_index;

    // 1. Construct the 40-bit integer from the 5-byte stream using Little-Endian order
    // reg_040h_buf[0] is D0 (LSB), reg_040h_buf[4] is D4 (MSB)
    raw_val_40bit = ((int64_t)reg_bytes[4] << 32) |
                    ((int64_t)reg_bytes[3] << 24) |
                    ((int64_t)reg_bytes[2] << 16) |
                    ((int64_t)reg_bytes[1] << 8)  |
                    ((int64_t)reg_bytes[0]);

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
int dpll_write_phase(int dpll_index, int32_t phase_le32) {
    uint8_t burst_buf[4], page = 0xC8, reg, offset = 0x00;

    if(dpll_index == 5) {
        reg = 0x2C;
    } else if (dpll_index == 6) {
        reg = 0x30;
    } else {
        return -1; // Invalid DPLL channel
    }

    /* DPLL_n.DPLL_MODE.PLL_MODE = write phase mode */

    burst_buf[0] = (uint8_t)(phase_le32 & 0xFF);
    burst_buf[1] = (uint8_t)((phase_le32 >> 8) & 0xFF);
    burst_buf[2] = (uint8_t)((phase_le32 >> 16) & 0xFF);
    burst_buf[3] = (uint8_t)((phase_le32 >> 24) & 0xFF);

    for(int i = 0; i < 4; i ++) {
        if (bf_pltfm_write_ptp_reg(page, reg + offset + i,
                burst_buf[i]) < 0) {
            return -2;
        }
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
int dpll_write_freq(int dpll_index, int64_t freq) {
    uint8_t burst_buf[6], page = 0xC8, reg, offset = 0x00, original_d5;

    int64_t cleaned_freq = freq & 0x3FFFFFFFFFFLL;

    if(dpll_index == 5) {
        /* 0xC860 */
        reg = 0x60;
    } else if (dpll_index == 6) {
        /* 0xC868 */
        reg = 0x68;
    } else {
        return -1;
    }

    /* DPLL_n.DPLL_MODE.PLL_MODE = write frequency mode */

    if (bf_pltfm_read_ptp_reg(page, offset + 5,
            &original_d5) < 0) {
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
        if (bf_pltfm_write_ptp_reg(page, reg + offset + i,
                burst_buf[i]) < 0) {
            return -3;
        }
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
double decode_obsclk_freq(int input_index, uint8_t *reg_bytes) {
    uint64_t m_val = 0;
    uint16_t n_val = 0;
    (void)input_index;

    m_val = ((uint64_t)reg_bytes[5] << 40) |
            ((uint64_t)reg_bytes[4] << 32) |
            ((uint64_t)reg_bytes[3] << 24) |
            ((uint64_t)reg_bytes[2] << 16) |
            ((uint64_t)reg_bytes[1] << 8)  |
            ((uint64_t)reg_bytes[0]);

    n_val = ((uint16_t)reg_bytes[7] << 8) | 
            ((uint16_t)reg_bytes[6]);

    if (n_val == 0) {
        n_val = 1;
    }
    //fprintf(stdout, "m_val = %lu n_val=%u\n", m_val, n_val);
    double freq_hz = (double)m_val / (double)n_val;

    return freq_hz;
}

static inline
int bf_ptp_lookup_register_general(const char *name, ptp_reg_desc_t *out_desc) {
    if (!name || !out_desc) return -1;

    for (size_t i = 0; i < BF_8A34004_REG_MAP_GENERAL_SIZE; i++) {
        if (strcmp(g_8a34004_register_map_general[i].name, name) == 0) {
            *out_desc = g_8a34004_register_map_general[i];
            return 0; // Target found
        }
    }
    return -1;
}

static inline
int bf_ptp_lookup_register_dpll(int dpll_index, const char *name, ptp_reg_desc_t *out_desc) {
    if (dpll_index > 7 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_DPLL_ENTRIES; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_dpll[dpll_index][i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

static inline
int bf_ptp_lookup_register_tod(int tod_index, const char *name, ptp_reg_desc_t *out_desc) {
    if (tod_index > 3 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_TOD_ENTRIES; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_tod[tod_index][i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

static inline
int bf_ptp_lookup_register(const char *name, ptp_reg_desc_t *out_desc) {
    if (!name || !out_desc) return -1;

    if (bf_ptp_lookup_register_general(name, out_desc) == 0) return 0;

    for (int dpll_index = 0; dpll_index < AFN_8A34004_MAX_DPLL; dpll_index++) {
        if (bf_ptp_lookup_register_dpll(dpll_index, name, out_desc) == 0) return 0;
    }

    for (int tod_index = 0; tod_index < AFN_8A34004_MAX_TOD; tod_index++) {
        if (bf_ptp_lookup_register_tod(tod_index, name, out_desc) == 0) return 0;
    }

    return -1;
}

/**
 * @brief Retrieves the 16-bit Product ID from the GENERAL_STATUS registers.
 * 
 * @param product_id Output pointer populated with the 16-bit hardware Product ID.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_product_id(uint16_t *product_id) {
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
static inline
int bf_ptp_get_release_no(uint8_t *major, uint8_t *minor, uint8_t *rev) {
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
 * @brief Retrieves the input clock frequency for physical clock inputs (INPUT_0 or INPUT_1).
 * 
 * @param input_index Target input channel index (Valid range: 0 or 1).
 * @param freq Output pointer populated with the calculated input frequency in Hz.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_input_freq(int input_index, double *freq) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[BUFSIZ] = { 0 };
    char name_buf[32];

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
 * @brief Sets the 32-bit signed phase command increment for DPLL5 or DPLL6.
 * @param dpll_index Target dpll block index (Pass 5 or 6).
 * @param phase_cmd Signed 32-bit phase target value.
 * @return int 0 on success; negative on hardware fault.
 */
static inline
int bf_ptp_set_dpll_phase(uint8_t dpll_index, int32_t phase_cmd) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[4];
    char name_buf[32];

    if (dpll_index != 5 && dpll_index != 6) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL_PHASE_%d", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    burst_buf[0] = (uint8_t)(phase_cmd & 0xFF);
    burst_buf[1] = (uint8_t)((phase_cmd >> 8) & 0xFF);
    burst_buf[2] = (uint8_t)((phase_cmd >> 16) & 0xFF);
    burst_buf[3] = (uint8_t)((phase_cmd >> 24) & 0xFF);

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 4) < 0) {
        return -3;
    }

    /* Renesas firmware micro-sequencer latch settlement stall */
    usleep(200);

    return 0;
}

/**
 * @brief Gets the 32-bit signed phase command increment currently cached or running in DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param out_phase_cmd Output pointer populated with the running 32-bit signed phase command.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_phase(uint8_t dpll_index, int32_t *out_phase_cmd) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[4];
    char name_buf[32];

    if ((dpll_index != 5 && dpll_index != 6) || !out_phase_cmd) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL_PHASE_%d", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    // Use loop to fetch 4 bytes if direct read burst isn't implemented for reading
    for (int i = 0; i < 4; i++) {
        if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc) + i,
                &burst_buf[i]) < 0) {
            return -3;
        }
    }

    *out_phase_cmd = dpll_phase_le32(burst_buf);

    return 0;
}

/**
 * @brief Atomically extracts and decodes the 40-bit residual sub-ns phase offset error of DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param phase_ns Output pointer populated with the decoded absolute phase error in nanoseconds.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_phase_status(uint8_t dpll_index, double *phase_ns) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[BUFSIZ] = { 0 };
    char name_buf[32];

    if (dpll_index > 7 || !phase_ns) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "STATUS.DPLL%d_PHASE_STATUS", dpll_index);
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
 * @brief Sets the 42-bit signed frequency micro-adjustment (FFO) for DPLL5 or DPLL6.
 *        Strictly implements Read-Modify-Write to safeguard the D5 [7:2] Reserved bits.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param freq_ffo_q53 Signed 42-bit frequency offset in units of 2^-53 (Q53 format).
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_set_dpll_freq(uint8_t dpll_index, int64_t freq_ffo_q53) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[6];
    char name_buf[32];
    uint8_t original_d5 = 0;

    if (dpll_index != 5 && dpll_index != 6) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL_FREQ_%d", dpll_index);
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

    /* Renesas firmware micro-sequencer latch settlement stall */
    usleep(200);

    return 0;
}

/**
 * @brief Gets the 42-bit signed frequency offset currently configured in DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param out_freq_ffo_q53 Output pointer populated with the signed 42-bit frequency offset in Q53 format.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_freq(uint8_t dpll_index, int64_t *out_freq_ffo_q53) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[6];
    char name_buf[32];

    if ((dpll_index != 5 && dpll_index != 6) || !out_freq_ffo_q53) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL_FREQ_%d", dpll_index);
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
 * @brief Retrieves the current operational configuration mode byte of DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param mode_val Output pointer populated with the raw operational mode control byte.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_mode(uint8_t dpll_index, uint8_t *mode_val) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[32];

    if (dpll_index > 7 || !mode_val) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_MODE", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }
    *mode_val = reg_byte;
    return 0;
}

/**
 * @brief Retrieves the live tracking reference status of DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param ref_stat Output pointer populated with the upstream physical CLK channel index.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_ref_stat(uint8_t dpll_index, uint8_t *ref_stat) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[32];

    if (dpll_index > 7 || !ref_stat) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "STATUS.DPLL%d_REF_STAT", dpll_index);
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
 * @brief Retrieves the core state machine lock configuration of DPLL5 or DPLL6.
 * 
 * @param dpll_index Target DPLL block index (Pass 5 or 6).
 * @param status Output pointer populated with the raw state machine byte token.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_status(uint8_t dpll_index, uint8_t *status) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[32];

    if (dpll_index > 7 || !status) {
        return -1;
    }
    snprintf(name_buf, sizeof(name_buf), "STATUS.DPLL%d_STATUS", dpll_index);
    if (bf_ptp_lookup_register_dpll((int)dpll_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_read_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            &reg_byte) < 0) {
        return -3;
    }

    *status = reg_byte;
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
static inline
int bf_ptp_get_dpll_tod_sync_cfg(uint8_t dpll_index, bool *out_enabled, uint8_t *out_tod_source) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_enabled || !out_tod_source) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_TOD_SYNC_CFG", dpll_index);
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
static inline
int bf_ptp_set_dpll_tod_sync_cfg(uint8_t dpll_index, bool enable_sync, uint8_t tod_source) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || tod_source > 3) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_TOD_SYNC_CFG", dpll_index);
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

    usleep(200); /* Latch settle stall */

    return 0;
}

/**
 * @brief Retrieves the Combo Mode Slave Primary Source configuration for a given DPLL.
 * 
 * @param dpll_index DPLL index (0 to 7).
 * @param out_pri_en Output pointer populated with the primary source enablement state.
 * @param out_pri_filt Output pointer populated with the filtered DCO configuration state.
 * @param out_pri_src_id Output pointer populated with the primary combo source DPLL index.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_combo_slave_pri_cfg(uint8_t dpll_index, bool *out_pri_en, bool *out_pri_filt, uint8_t *out_pri_src_id) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_pri_en || !out_pri_filt || !out_pri_src_id) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_COMBO_SLAVE_CFG_0", dpll_index);
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
 * @param dpll_index Target DPLL block index (0 to 7).
 * @param pri_en Enable the primary combo source.
 * @param pri_filt Select filtered configuration for the primary combo source.
 * @param pri_src_id Primary combo source DPLL index (0 to 7).
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_set_dpll_combo_slave_pri_cfg(uint8_t dpll_index, bool pri_en, bool pri_filt, uint8_t pri_src_id) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || pri_src_id > 7) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_COMBO_SLAVE_CFG_0", dpll_index);
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

    usleep(200); /* settle stall */
    return 0;
}

/**
 * @brief Retrieves the Combo Mode Slave Secondary Source configuration for a given DPLL.
 * 
 * @param dpll_index DPLL index (0 to 7).
 * @param out_sec_en Output pointer populated with the secondary source enablement state.
 * @param out_sec_filt Output pointer populated with the filtered configuration state.
 * @param out_sec_src_id Output pointer populated with the secondary combo source DPLL index.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_dpll_combo_slave_sec_cfg(uint8_t dpll_index, bool *out_sec_en, bool *out_sec_filt, uint8_t *out_sec_src_id) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || !out_sec_en || !out_sec_filt || !out_sec_src_id) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_COMBO_SLAVE_CFG_1", dpll_index);
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
 * @param dpll_index Target DPLL block index (0 to 7).
 * @param sec_en Enable the secondary combo source.
 * @param sec_filt Select filtered configuration for the secondary combo source.
 * @param sec_src_id Secondary combo source DPLL index (0 to 7).
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_set_dpll_combo_slave_sec_cfg(uint8_t dpll_index, bool sec_en, bool sec_filt, uint8_t sec_src_id) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (dpll_index > 7 || sec_src_id > 7) {
        return -1;
    }

    snprintf(name_buf, sizeof(name_buf), "DPLL_%d.DPLL_COMBO_SLAVE_CFG_1", dpll_index);
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

    usleep(200); /* settle stall */
    return 0;
}

/**
 * @brief Extracts the active 32-bit signed phase command offset applied to DPLL6.
 * 
 * @param out_phase_cmd Output pointer populated with the integer command value in hardware steps.
 * @return int 0 on success; -1 if the registry mapping lookup fails.
 */
static inline
int bf_ptp_get_dpll6_phase(int32_t *out_phase_cmd) {
    return bf_ptp_get_dpll_phase(6, out_phase_cmd);
}

/**
 * @brief Executes the mandatory hardware commit trigger to enforce phase pull-in / frequency step alignment.
 *        Maps perfectly to DPLL_PHASE_PULL_IN_n and FREQ alignment triggers.
 * 
 * @param trigger_type 0 for Phase Pull-In (0x0F), 1 for Frequency Commit (0x3F).
 */
static inline
int bf_ptp_commit_dpll_trigger(uint8_t trigger_type) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    const char *name_buf = (trigger_type == 0) ? "DPLL_PHASE.EXEC_TRIGGER" : "DPLL_FREQ.EXEC_TRIGGER";

    if (bf_ptp_lookup_register_general(name_buf, &regdesc)) {
        return -1;
    }

    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -2;
    }

    usleep(200); // Critical microprocessor latch stall period
    return 0;
}

/**
 * @brief Retrieves the live driving clock source (DPLL linkage index) of a specified TOD instance.
 *        On this platform, the hardware clock linkage is static:
 *        - TOD_0 is clocked/driven by SyncE DPLL5.
 *        - TOD_1 is clocked/driven by PTP DPLL6.
 *        - TOD_2/3 are auxiliary/unused (default to DPLL0).
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_dpll_src Output pointer populated with the driving DPLL index (Range: 0 to 7).
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_tod_clk_src_default(uint8_t tod_index, uint8_t *out_dpll_src) {
    if (tod_index > 3 || !out_dpll_src) {
        return -1;
    }

    /* Fallback mapping when active synchronization isn't set:
     * TOD0 -> DPLL5 (SyncE / Frequency axis)
     * TOD1 -> DPLL6 (PTP / Phase / Time-of-Day axis)
     */
    if (tod_index == 0) {
        *out_dpll_src = 5;
    } else if (tod_index == 1) {
        *out_dpll_src = 6;
    } else {
        *out_dpll_src = 0;
    }

    return 0;
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
static inline
int bf_ptp_get_tod_clk_src(uint8_t tod_index, uint8_t *out_dpll_src) {
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

    //return bf_ptp_get_tod_clk_src_default(tod_index, out_dpll_src);
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
static inline
int bf_ptp_get_tod_write_counter(uint8_t tod_index, uint8_t *out_write_counter) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_write_counter) {
        return -1;
    }

    /* Dynamic string construction matching "TOD_WRITE_n.WRITE_COUNTER" within Page 0xCB */
    snprintf(name_buf, sizeof(name_buf), "TOD_WRITE_%d.WRITE_COUNTER", tod_index);
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
 * @brief Retrieves and decodes the current configuration bits of the specified TOD_n.TOD_CFG register.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_enabled Output pointer populated with the TOD_ENABLE[0] status.
 * @param out_even_pps Output pointer populated with the TOD_EVEN_PPS_MODE[2] status.
 * @param out_sync_disabled Output pointer populated with the TOD_OUT_SYNC_DISABLE[1] status.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_tod_cfg(uint8_t tod_index, bool *out_enabled, bool *out_even_pps, bool *out_sync_disabled) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t reg_byte = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_enabled || !out_even_pps || !out_sync_disabled) {
        return -1;
    }

    /* 1. Dynamic string construction matching "TOD_n.TOD_CFG" */
    snprintf(name_buf, sizeof(name_buf), "TOD_%d.TOD_CFG", tod_index);
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
static inline
int bf_ptp_set_tod_cfg(uint8_t tod_index, bool enable_tod, bool even_pps_mode, bool disable_out_sync) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t original_val = 0;
    uint8_t updated_val = 0;
    char name_buf[64] = { 0 };

    if (tod_index > 3) {
        return -1;
    }

    /* 1. Dynamic string construction matching "TOD_n.TOD_CFG" */
    snprintf(name_buf, sizeof(name_buf), "TOD_%d.TOD_CFG", tod_index);
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

    /* Mandatory internal Renesas firmware micro-sequencer latch settlement stall */
    usleep(200);

    return 0;
}

/**
 * @brief Freezes and extracts a coherent 64-bit absolute Time-of-Day (ToD) 
 *        from any specified hardware TOD instance (0 to 3) based on the dictionary.
 *        Enforces 10-byte continuous burst reading loops to eliminate timestamp tearing.
 * 
 * @param tod_index Target TOD instance to look up and read.
 * @param out_sec Output pointer populated with the decoded Unix Epoch seconds.
 * @param out_ns Output pointer populated with the residual sub-second nanoseconds.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_get_tod_time(uint8_t tod_index, uint32_t *out_sec, uint32_t *out_ns) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[10] = { 0 };
    char name_buf[64] = { 0 };

    if (tod_index > 3 || !out_sec || !out_ns) {
        return -1;
    }

    /* 1. HARDWARE SNAPSHOT LATCH: Instantly freeze the live running clock counters into shadow cache */
    snprintf(name_buf, sizeof(name_buf), "TOD_READ_%d.READ_TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }
    
    /* Write 0x01 to execute the hardware snapshot freeze */
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -2;
    }
    usleep(10); /* Brief physical bus flush stall */

    /* 2. Reset coordinates back to the start of the locked 10-byte time array */
    snprintf(name_buf, sizeof(name_buf), "TOD_READ_%d.SUB_NANOSECONDS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -3;
    }
    uint8_t page = REGPAGE(regdesc);
    uint8_t addr = REGADDR(regdesc);

    /* 3. CONTINUOUS STRIDE BURST READ: Extract all 10 bytes sequentially to prevent phase tearing */
    for (int offset = 0; offset < 10; offset++) {
        if (bf_pltfm_read_ptp_reg(page, addr + offset,
                &burst_buf[offset]) < 0) {
            return -4;
        }
    }

    /* 4. DECODE NANOSECONDS (Little-Endian assembly across offsets 002h to 005h) */
    *out_ns = ((uint32_t)burst_buf[5] << 24) |
              ((uint32_t)burst_buf[4] << 16) |
              ((uint32_t)burst_buf[3] << 8)  |
              ((uint32_t)burst_buf[2]);

    /* 5. DECODE SECONDS (Little-Endian assembly across offsets 006h to 009h) */
    *out_sec = ((uint32_t)burst_buf[9] << 24) |
               ((uint32_t)burst_buf[8] << 16) |
               ((uint32_t)burst_buf[7] << 8)  |
               ((uint32_t)burst_buf[6]);

    return 0; /* Coherent ToD fetched successfully */
}

/**
 * @brief Preloads and commits a 64-bit absolute Time-of-Day (ToD)
 *        to any specified hardware TOD instance (0 to 3).
 *        Writes sub-nanoseconds, nanoseconds, and seconds, then triggers the commit.
 * 
 * @param tod_index Target TOD instance to look up and write.
 * @param sec Unix Epoch absolute seconds to set.
 * @param ns Residual sub-second nanoseconds to set.
 * @return int 0 on success; negative on error.
 */
static inline
int bf_ptp_set_tod_time(uint8_t tod_index, uint32_t sec, uint32_t ns) {
    ptp_reg_desc_t regdesc = __ptp_reg_desc_init_val__;
    uint8_t burst_buf[10] = { 0 };
    char name_buf[64] = { 0 };

    if (tod_index > 3) {
        return -1;
    }

    /* 1. Preload sub-nanoseconds (set to 0) */
    burst_buf[0] = 0x00;
    burst_buf[1] = 0x00;

    /* 2. Preload nanoseconds (Little-Endian assembly) */
    burst_buf[2] = (uint8_t)(ns & 0xFF);
    burst_buf[3] = (uint8_t)((ns >> 8) & 0xFF);
    burst_buf[4] = (uint8_t)((ns >> 16) & 0xFF);
    burst_buf[5] = (uint8_t)((ns >> 24) & 0xFF);

    /* 3. Preload seconds (Little-Endian assembly) */
    burst_buf[6] = (uint8_t)(sec & 0xFF);
    burst_buf[7] = (uint8_t)((sec >> 8) & 0xFF);
    burst_buf[8] = (uint8_t)((sec >> 16) & 0xFF);
    burst_buf[9] = (uint8_t)((sec >> 24) & 0xFF);

    /* 4. Write all 10 bytes starting from SUB_NANOSECONDS using burst write */
    snprintf(name_buf, sizeof(name_buf), "TOD_WRITE_%d.SUB_NANOSECONDS", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -2;
    }

    if (bf_pltfm_write_ptp_reg_burst(REGPAGE(regdesc), REGADDR(regdesc),
            burst_buf, 10) < 0) {
        return -3;
    }

    /* 5. HARDWARE WRITE COMMIT TRIGGER: Commit the preloaded values into the active counters */
    snprintf(name_buf, sizeof(name_buf), "TOD_WRITE_%d.WRITE_TRIGGER", tod_index);
    if (bf_ptp_lookup_register_tod((int)tod_index, name_buf, &regdesc)) {
        return -4;
    }

    /* Write 0x01 (Absolute TOD, Immediate Trigger) */
    if (bf_pltfm_write_ptp_reg(REGPAGE(regdesc), REGADDR(regdesc),
            0x01) < 0) {
        return -5;
    }
    
    usleep(200); /* Latch settle stall */

    return 0;
}
#endif // AFN_8A34004_REGS_H
