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

#define MAX_TOD		(4)
#define MAX_PLL		(8)

// DPLL0 ~ DPLL7 plus DPLL8 for SYSDPLL
#define AFN_8A34004_MAX_DPLL    (MAX_PLL + 1)
#define AFN_8A34004_MAX_TOD     (MAX_TOD)

#define MAX_ENTRIES_PER_DPLL    (16 + 1)
#define MAX_ENTRIES_PER_TOD     (24 + 1)

/* ToD read trigger mode select */
#define TOD_READ_TRIG_SEL_DISABLE            0
#define TOD_READ_TRIG_SEL_IMMEDIATE          1
#define TOD_READ_TRIG_SEL_TODPPS             2
#define TOD_READ_TRIG_SEL_REFCLK             3
#define TOD_READ_TRIG_SEL_PWMPPS             4
#define TOD_READ_TRIG_SEL_RESERVED           5
#define TOD_READ_TRIG_SEL_WRITEFREQEVENT     6
#define TOD_READ_TRIG_SEL_GPIO               7

#define TOD_READ_TRIGGER_MASK                0x0F
#define TOD_REF_INDEX_MASK                   0x0F

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

/* Values of DPLL_CTRL_n.DPLL_MANU_REF_CFG.MANUAL_REFERENCE */
enum manual_reference {
	MANU_REF_MIN = 0,
	MANU_REF_CLK0 = MANU_REF_MIN,
	MANU_REF_CLK1,
	MANU_REF_CLK2,
	MANU_REF_CLK3,
	MANU_REF_CLK4,
	MANU_REF_CLK5,
	MANU_REF_CLK6,
	MANU_REF_CLK7,
	MANU_REF_CLK8,
	MANU_REF_CLK9,
	MANU_REF_CLK10,
	MANU_REF_CLK11,
	MANU_REF_CLK12,
	MANU_REF_CLK13,
	MANU_REF_CLK14,
	MANU_REF_CLK15,
	MANU_REF_WRITE_PHASE,
	MANU_REF_WRITE_FREQUENCY,
	MANU_REF_XO_DPLL,
	MANU_REF_MAX = MANU_REF_XO_DPLL,
};

/* Values of DPLL_N.DPLL_MODE.PLL_MODE */
enum pll_mode {
	PLL_MODE_MIN = 0,
	PLL_MODE_PLL = PLL_MODE_MIN,
	PLL_MODE_WRITE_PHASE = 1,
	PLL_MODE_WRITE_FREQUENCY = 2,
	PLL_MODE_GPIO_INC_DEC = 3,
	PLL_MODE_SYNTHESIS = 4,
	PLL_MODE_PHASE_MEASUREMENT = 5,
	PLL_MODE_DISABLED = 6,
	PLL_MODE_MAX = PLL_MODE_DISABLED,
};

/* Values STATUS.DPLL_SYS_STATUS.DPLL_SYS_STATE */
enum dpll_state {
	DPLL_STATE_MIN = 0,
	DPLL_STATE_FREERUN = DPLL_STATE_MIN,
	DPLL_STATE_LOCKACQ = 1,
	DPLL_STATE_LOCKREC = 2,
	DPLL_STATE_LOCKED = 3,
	DPLL_STATE_HOLDOVER = 4,
	DPLL_STATE_OPEN_LOOP = 5,
	DPLL_STATE_MAX = DPLL_STATE_OPEN_LOOP,
};

/* 4.8.7 only */
enum scsr_tod_write_trig_sel {
	SCSR_TOD_WR_TRIG_SEL_DISABLE = 0,
	SCSR_TOD_WR_TRIG_SEL_IMMEDIATE = 1,
	SCSR_TOD_WR_TRIG_SEL_REFCLK = 2,
	SCSR_TOD_WR_TRIG_SEL_PWMPPS = 3,
	SCSR_TOD_WR_TRIG_SEL_TODPPS = 4,
	SCSR_TOD_WR_TRIG_SEL_SYNCFOD = 5,
	SCSR_TOD_WR_TRIG_SEL_GPIO = 6,
	SCSR_TOD_WR_TRIG_SEL_MAX = SCSR_TOD_WR_TRIG_SEL_GPIO,
};

/* 4.8.7 only */
enum scsr_tod_write_type_sel {
	SCSR_TOD_WR_TYPE_SEL_ABSOLUTE = 0,
	SCSR_TOD_WR_TYPE_SEL_DELTA_PLUS = 1,
	SCSR_TOD_WR_TYPE_SEL_DELTA_MINUS = 2,
	SCSR_TOD_WR_TYPE_SEL_MAX = SCSR_TOD_WR_TYPE_SEL_DELTA_MINUS,
};

// System-wide 8A34004 Functional Register Matrix Array Definition - General Registers
static const ptp_reg_desc_t g_8a34004_register_map_general[] = {
    { "RESET_CTRL.SM_RESET",            0xC0, 0x00, 0x12,  1, REG_PERM_RW },
    { "GENERAL_STATUS.BOOT_STATUS",     0xC0, 0x14, 0x00,  1, REG_PERM_RW },
    /* GLOBAL HARDWARE REVISION & ID ENTITIES */
    { "GENERAL_STATUS.PRODUCT_ID_L",    0xC0, 0x14, 0x1E,  1, REG_PERM_R  },
    { "GENERAL_STATUS.PRODUCT_ID_H",    0xC0, 0x14, 0x1F,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MAJOR",   0xC0, 0x14, 0x10,  1, REG_PERM_R  },
    { "GENERAL_STATUS.RELEASE_MINOR",   0xC0, 0x14, 0x11,  1, REG_PERM_R  },
    { "HW_REVISION.REV_ID",             0x81, 0x80, 0x7A,  1, REG_PERM_R  }, // Product revision validation target
    { "STATUS.SYS_DPLL_STATUS",         0xC0, 0x3C, 0x20,  1, REG_PERM_RW },
    { "STATUS.SYS_APLL_STATUS",         0xC0, 0x3C, 0x21,  1, REG_PERM_RW },
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
static const ptp_reg_desc_t g_8a34004_register_map_dpll[AFN_8A34004_MAX_DPLL][MAX_ENTRIES_PER_DPLL] = {
    /* DPLL 0 */
    {
        { "DPLL0.FREQ",                     0xC8, 0x38, 0x00, 6, REG_PERM_RW },
        { "DPLL0.PHASE",                    0xC8, 0x18, 0x00, 4, REG_PERM_W  },
        { "DPLL0.TOD_SYNC_CFG",             0xC3, 0xB0, 0x31, 1, REG_PERM_RW },
        { "DPLL0.COMBO_SLAVE_CFG_0",        0xC3, 0xB0, 0x32, 1, REG_PERM_RW },
        { "DPLL0.COMBO_SLAVE_CFG_1",        0xC3, 0xB0, 0x33, 1, REG_PERM_RW },
        { "DPLL0.MODE",                     0xC3, 0xB0, 0x37, 1, REG_PERM_RW },
        { "DPLL0.STATUS",                   0xC0, 0x3C, 0x18, 1, REG_PERM_R  },
        { "DPLL0.REF_STAT",                 0xC0, 0x3C, 0x22, 1, REG_PERM_R  },
        { "DPLL0.PHASE_STATUS",             0xC0, 0x3C, 0xDC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 1 */
    {
        { "DPLL1.FREQ",                     0xC8, 0x40, 0x00, 6, REG_PERM_RW },
        { "DPLL1.PHASE",                    0xC8, 0x1C, 0x00, 4, REG_PERM_W  },
        { "DPLL1.TOD_SYNC_CFG",             0xC4, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL1.COMBO_SLAVE_CFG_0",        0xC4, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL1.COMBO_SLAVE_CFG_1",        0xC4, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL1.MODE",                     0xC4, 0x00, 0x37, 1, REG_PERM_RW },
        { "DPLL1.STATUS",                   0xC0, 0x3C, 0x19, 1, REG_PERM_R  },
        { "DPLL1.REF_STAT",                 0xC0, 0x3C, 0x23, 1, REG_PERM_R  },
        { "DPLL1.PHASE_STATUS",             0xC0, 0x3C, 0xE4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 2 */
    {
        { "DPLL2.FREQ",                     0xC8, 0x48, 0x00, 6, REG_PERM_RW },
        { "DPLL2.PHASE",                    0xC8, 0x20, 0x00, 4, REG_PERM_W  },
        { "DPLL2.TOD_SYNC_CFG",             0xC4, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL2.COMBO_SLAVE_CFG_0",        0xC4, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL2.COMBO_SLAVE_CFG_1",        0xC4, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL2.MODE",                     0xC4, 0x38, 0x37, 1, REG_PERM_RW },
        { "DPLL2.STATUS",                   0xC0, 0x3C, 0x1A, 1, REG_PERM_R  },
        { "DPLL2.REF_STAT",                 0xC0, 0x3C, 0x24, 1, REG_PERM_R  },
        { "DPLL2.PHASE_STATUS",             0xC0, 0x3C, 0xEC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 3 */
    {
        { "DPLL3.FREQ",                     0xC8, 0x50, 0x00, 6, REG_PERM_RW },
        { "DPLL3.PHASE",                    0xC8, 0x24, 0x00, 4, REG_PERM_W  },
        { "DPLL3.TOD_SYNC_CFG",             0xC4, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL3.COMBO_SLAVE_CFG_0",        0xC4, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL3.COMBO_SLAVE_CFG_1",        0xC4, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL3.MODE",                     0xC4, 0x80, 0x37, 1, REG_PERM_RW },
        { "DPLL3.STATUS",                   0xC0, 0x3C, 0x1B, 1, REG_PERM_R  },
        { "DPLL3.REF_STAT",                 0xC0, 0x3C, 0x25, 1, REG_PERM_R  },
        { "DPLL3.PHASE_STATUS",             0xC0, 0x3C, 0xF4, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 4 */
    {
        { "DPLL4.FREQ",                     0xC8, 0x58, 0x00, 6, REG_PERM_RW },
        { "DPLL4.PHASE",                    0xC8, 0x28, 0x00, 4, REG_PERM_W  },
        { "DPLL4.TOD_SYNC_CFG",             0xC4, 0xB8, 0x31, 1, REG_PERM_RW },
        { "DPLL4.COMBO_SLAVE_CFG_0",        0xC4, 0xB8, 0x32, 1, REG_PERM_RW },
        { "DPLL4.COMBO_SLAVE_CFG_1",        0xC4, 0xB8, 0x33, 1, REG_PERM_RW },
        { "DPLL4.MODE",                     0xC4, 0xB8, 0x37, 1, REG_PERM_RW },
        { "DPLL4.STATUS",                   0xC0, 0x3C, 0x1C, 1, REG_PERM_R  },
        { "DPLL4.REF_STAT",                 0xC0, 0x3C, 0x26, 1, REG_PERM_R  },
        { "DPLL4.PHASE_STATUS",             0xC0, 0x3C, 0xFC, 5, REG_PERM_R  },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 5 */
    {
        { "DPLL5.FREQ",                     0xC8, 0x60, 0x00, 6, REG_PERM_RW },
        { "DPLL5.PHASE",                    0xC8, 0x2C, 0x00, 4, REG_PERM_W  },
        { "DPLL5.TOD_SYNC_CFG",             0xC5, 0x00, 0x31, 1, REG_PERM_RW },
        { "DPLL5.COMBO_SLAVE_CFG_0",        0xC5, 0x00, 0x32, 1, REG_PERM_RW },
        { "DPLL5.COMBO_SLAVE_CFG_1",        0xC5, 0x00, 0x33, 1, REG_PERM_RW },
        { "DPLL5.MODE",                     0xC5, 0x00, 0x37, 1, REG_PERM_RW },
        { "DPLL5.STATUS",                   0xC0, 0x3C, 0x1D, 1, REG_PERM_R  },
        { "DPLL5.REF_STAT",                 0xC0, 0x3C, 0x27, 1, REG_PERM_R  },
        { "DPLL5.PHASE_STATUS",             0xC0, 0x3C, 0x104, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 6 */
    {
        { "DPLL6.FREQ",                     0xC8, 0x68, 0x00, 6, REG_PERM_RW },
        { "DPLL6.PHASE",                    0xC8, 0x30, 0x00, 4, REG_PERM_W  },
        { "DPLL6.TOD_SYNC_CFG",             0xC5, 0x38, 0x31, 1, REG_PERM_RW },
        { "DPLL6.COMBO_SLAVE_CFG_0",        0xC5, 0x38, 0x32, 1, REG_PERM_RW },
        { "DPLL6.COMBO_SLAVE_CFG_1",        0xC5, 0x38, 0x33, 1, REG_PERM_RW },
        { "DPLL6.MODE",                     0xC5, 0x38, 0x37, 1, REG_PERM_RW },
        { "DPLL6.STATUS",                   0xC0, 0x3C, 0x1E, 1, REG_PERM_R  },
        { "DPLL6.REF_STAT",                 0xC0, 0x3C, 0x28, 1, REG_PERM_R  },
        { "DPLL6.PHASE_STATUS",             0xC0, 0x3C, 0x10C, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    },
    /* DPLL 7 */
    {
        { "DPLL7.FREQ",                     0xC8, 0x70, 0x00, 6, REG_PERM_RW },
        { "DPLL7.PHASE",                    0xC8, 0x34, 0x00, 4, REG_PERM_W  },
        { "DPLL7.TOD_SYNC_CFG",             0xC5, 0x80, 0x31, 1, REG_PERM_RW },
        { "DPLL7.COMBO_SLAVE_CFG_0",        0xC5, 0x80, 0x32, 1, REG_PERM_RW },
        { "DPLL7.COMBO_SLAVE_CFG_1",        0xC5, 0x80, 0x33, 1, REG_PERM_RW },
        { "DPLL7.MODE",                     0xC5, 0x80, 0x37, 1, REG_PERM_RW },
        { "DPLL7.STATUS",                   0xC0, 0x3C, 0x1F, 1, REG_PERM_R  },
        { "DPLL7.REF_STAT",                 0xC0, 0x3C, 0x29, 1, REG_PERM_R  },
        { "DPLL7.PHASE_STATUS",             0xC0, 0x3C, 0x114, 5, REG_PERM_R },
        __ptp_reg_desc_init_val__
    }
};

static const ptp_reg_desc_t g_8a34004_register_map_tod[AFN_8A34004_MAX_TOD][MAX_ENTRIES_PER_TOD] = {
    /* TOD 0 */
    {
        { "TOD0.READ_PRIMARY.SUB_NS",      0xCC, 0x40, 0x00,  1, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.NS",          0xCC, 0x40, 0x01,  4, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.SECONDS",     0xCC, 0x40, 0x05,  6, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.COUNTER",     0xCC, 0x40, 0x0B,  1, REG_PERM_R  },
        { "TOD0.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x40, 0x0C,  1, REG_PERM_RW },
        { "TOD0.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x40, 0x0D,  1, REG_PERM_RW },
        { "TOD0.READ_PRIMARY.TRIGGER",     0xCC, 0x40, 0x0E,  1, REG_PERM_RW },
        { "TOD0.WRITE.SUB_NS",             0xCC, 0x00, 0x00,  1, REG_PERM_RW },
        { "TOD0.WRITE.NS",                 0xCC, 0x00, 0x01,  4, REG_PERM_RW },
        { "TOD0.WRITE.SECONDS",            0xCC, 0x00, 0x05,  6, REG_PERM_RW },
        { "TOD0.WRITE.COUNTER",            0xCC, 0x00, 0x0C,  1, REG_PERM_R  },
        { "TOD0.WRITE.SEL_CFG_0",          0xCC, 0x00, 0x0D,  2, REG_PERM_RW },
        { "TOD0.WRITE.TRIGGER",            0xCC, 0x00, 0x0F,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.SUB_NS",    0xCC, 0x90, 0x00,  1, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.NS",        0xCC, 0x90, 0x01,  4, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.SECONDS",   0xCC, 0x90, 0x05,  6, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.COUNTER",   0xCC, 0x90, 0x0B,  1, REG_PERM_R  },
        { "TOD0.READ_SECONDARY.SEL_CFG_0", 0xCC, 0x90, 0x0C,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.SEL_CFG_1", 0xCC, 0x90, 0x0D,  1, REG_PERM_RW },
        { "TOD0.READ_SECONDARY.TRIGGER",   0xCC, 0x90, 0x0E,  1, REG_PERM_RW },
        { "TOD0.TOD_CFG",                  0xCB, 0xCC, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 1 */
    {
        { "TOD1.READ_PRIMARY.SUB_NS",      0xCC, 0x50, 0x00,  1, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.NS",          0xCC, 0x50, 0x01,  4, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.SECONDS",     0xCC, 0x50, 0x05,  6, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.COUNTER",     0xCC, 0x50, 0x0B,  1, REG_PERM_R  },
        { "TOD1.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x50, 0x0C,  1, REG_PERM_RW },
        { "TOD1.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x50, 0x0D,  1, REG_PERM_RW },
        { "TOD1.READ_PRIMARY.TRIGGER",     0xCC, 0x50, 0x0E,  1, REG_PERM_RW },
        { "TOD1.WRITE.SUB_NS",             0xCC, 0x10, 0x00,  1, REG_PERM_RW },
        { "TOD1.WRITE.NS",                 0xCC, 0x10, 0x01,  4, REG_PERM_RW },
        { "TOD1.WRITE.SECONDS",            0xCC, 0x10, 0x05,  6, REG_PERM_RW },
        { "TOD1.WRITE.COUNTER",            0xCC, 0x10, 0x0C,  1, REG_PERM_R  },
        { "TOD1.WRITE.SEL_CFG_0",          0xCC, 0x10, 0x0D,  2, REG_PERM_RW },
        { "TOD1.WRITE.TRIGGER",            0xCC, 0x10, 0x0F,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.SUB_NS",    0xCC, 0xA0, 0x00,  1, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.NS",        0xCC, 0xA0, 0x01,  4, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.SECONDS",   0xCC, 0xA0, 0x05,  6, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.COUNTER",   0xCC, 0xA0, 0x0B,  1, REG_PERM_R  },
        { "TOD1.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xA0, 0x0C,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xA0, 0x0D,  1, REG_PERM_RW },
        { "TOD1.READ_SECONDARY.TRIGGER",   0xCC, 0xA0, 0x0E,  1, REG_PERM_RW },
        { "TOD1.TOD_CFG",                  0xCB, 0xCE, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 2 */
    {
        { "TOD2.READ_PRIMARY.SUB_NS",      0xCC, 0x60, 0x00,  1, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.NS",          0xCC, 0x60, 0x01,  4, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.SECONDS",     0xCC, 0x60, 0x05,  6, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.COUNTER",     0xCC, 0x60, 0x0B,  1, REG_PERM_R  },
        { "TOD2.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x60, 0x0C,  1, REG_PERM_RW },
        { "TOD2.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x60, 0x0D,  1, REG_PERM_RW },
        { "TOD2.READ_PRIMARY.TRIGGER",     0xCC, 0x60, 0x0E,  1, REG_PERM_RW },
        { "TOD2.WRITE.SUB_NS",             0xCC, 0x20, 0x00,  1, REG_PERM_W  },
        { "TOD2.WRITE.NS",                 0xCC, 0x20, 0x01,  4, REG_PERM_W  },
        { "TOD2.WRITE.SECONDS",            0xCC, 0x20, 0x05,  6, REG_PERM_W  },
        { "TOD2.WRITE.COUNTER",            0xCC, 0x20, 0x0C,  1, REG_PERM_R  },
        { "TOD2.WRITE.SEL_CFG_0",          0xCC, 0x20, 0x0D,  2, REG_PERM_RW },
        { "TOD2.WRITE.TRIGGER",            0xCC, 0x20, 0x0F,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.SUB_NS",    0xCC, 0xB0, 0x00,  1, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.NS",        0xCC, 0xB0, 0x01,  4, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.SECONDS",   0xCC, 0xB0, 0x05,  6, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.COUNTER",   0xCC, 0xB0, 0x0B,  1, REG_PERM_R  },
        { "TOD2.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xB0, 0x0C,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xB0, 0x0D,  1, REG_PERM_RW },
        { "TOD2.READ_SECONDARY.TRIGGER",   0xCC, 0xB0, 0x0E,  1, REG_PERM_RW },
        { "TOD2.TOD_CFG",                  0xCB, 0xD0, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    },
    /* TOD 3 */
    {
        { "TOD3.READ_PRIMARY.SUB_NS",      0xCC, 0x80, 0x00,  1, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.NS",          0xCC, 0x80, 0x01,  4, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.SECONDS",     0xCC, 0x80, 0x05,  6, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.COUNTER",     0xCC, 0x80, 0x0B,  1, REG_PERM_R  },
        { "TOD3.READ_PRIMARY.SEL_CFG_0",   0xCC, 0x80, 0x0C,  1, REG_PERM_RW },
        { "TOD3.READ_PRIMARY.SEL_CFG_1",   0xCC, 0x80, 0x0D,  1, REG_PERM_RW },
        { "TOD3.READ_PRIMARY.TRIGGER",     0xCC, 0x80, 0x0E,  1, REG_PERM_RW },
        { "TOD3.WRITE.SUB_NS",             0xCC, 0x30, 0x00,  1, REG_PERM_W  },
        { "TOD3.WRITE.NS",                 0xCC, 0x30, 0x01,  4, REG_PERM_W  },
        { "TOD3.WRITE.SECONDS",            0xCC, 0x30, 0x05,  6, REG_PERM_W  },
        { "TOD3.WRITE.COUNTER",            0xCC, 0x30, 0x0C,  1, REG_PERM_R  },
        { "TOD3.WRITE.SEL_CFG_0",          0xCC, 0x30, 0x0D,  2, REG_PERM_RW },
        { "TOD3.WRITE.TRIGGER",            0xCC, 0x30, 0x0F,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.SUB_NS",    0xCC, 0xC0, 0x00,  1, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.NS",        0xCC, 0xC0, 0x01,  4, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.SECONDS",   0xCC, 0xC0, 0x05,  6, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.COUNTER",   0xCC, 0xC0, 0x0B,  1, REG_PERM_R  },
        { "TOD3.READ_SECONDARY.SEL_CFG_0", 0xCC, 0xC0, 0x0C,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.SEL_CFG_1", 0xCC, 0xC0, 0x0D,  1, REG_PERM_RW },
        { "TOD3.READ_SECONDARY.TRIGGER",   0xCC, 0xC0, 0x0E,  1, REG_PERM_RW },
        { "TOD3.TOD_CFG",                  0xCB, 0xD2, 0x00,  1, REG_PERM_RW },
        __ptp_reg_desc_init_val__
    }
};

#define BF_8A34004_REG_MAP_GENERAL_SIZE (sizeof(g_8a34004_register_map_general) / sizeof(ptp_reg_desc_t))
/* When base + offset exceeds 0xFF, the carry propagates into the page selector.
 * REGPAGE adds the overflow carry so that registers with large module offsets
 * (e.g. STATUS.DPLL0_PHASE_STATUS: base=0x3C + offset=0xDC = 0x118 -> page+1, addr=0x18)
 * resolve to the correct hardware page and address. */
#define REGADDR(regdesc) ((uint8_t)(((uint16_t)(regdesc).base + (regdesc).offset) & 0xFF))
#define REGPAGE(regdesc) ((uint8_t)((regdesc).page + (((uint16_t)(regdesc).base + (regdesc).offset) >> 8)))
#define REGWIDB(regdesc) ((regdesc).width_bytes)
// 1 LSB of the internal phase detector is exactly 1 UI * 2^-16.
// For the nominal internal clock operating around 3.84GHz, 1 LSB = 3.970588 picoseconds (or 0.003970588 ns).
#define DPLL_PHASE_RESOLUTION_NS 0.003970588

/**
 * @brief Decodes the DPLL status register byte into a human-readable string.
 *
 * @param dpll_index DPLL index (unused, but kept for signature consistency).
 * @param reg_byte Raw byte from STATUS.DPLLn_STATUS register.
 * @return const char* String representation of the DPLL state.
 */
static inline
const char* dpll_state_str(int dpll_index, uint8_t reg_byte)
{
    (void)dpll_index;
    uint8_t s = (reg_byte  & 0x0F);
    switch (s) {
        case 0:     return "FREERUN ";
        case 1:     return "LOCKACQ ";
        case 2:     return "LOCKREC ";
        case 3:     return "LOCKED  ";
        case 4:     return "HOLDOVER";
        case 5:     return "OPENLOOP";
        case 6:     return "DISABLED";
        default:    return "N/A     ";
    }
}

/**
 * @brief Decodes the DPLL reference clock status byte into a human-readable string.
 *
 * @param reg_byte Raw byte from STATUS.DPLLn_REF_STAT register.
 * @return const char* String representation of the active reference clock.
 */
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
        case 0x10: return "WRPHASE_INPUT  ";
        case 0x11: return "WRFREQ_INPUT   ";
        case 0x12: return "XO_DPLL        ";
        case 0x13: return "CLK19(DPLL0)   ";
        case 0x14: return "CLK20(DPLL1)   ";
        case 0x15: return "CLK21(DPLL2)   ";
        case 0x16: return "CLK22(DPLL3)   ";
        case 0x17: return "CLK23(DPLL4)   ";
        case 0x18: return "CLK24(DPLL5)   ";
        case 0x19: return "CLK25(DPLL6)   ";
        case 0x1A: return "CLK26(DPLL7)   ";
        case 0x1B: return "CLK27(SYSDPLL) ";
        case 0x1C: return "CLK28          ";
        case 0x1F: return "NOREF          ";
        default:   return "N/A            ";
    }
}

/**
 * @brief Decodes the operation mode from the DPLL mode register.
 *
 * @param reg_byte Raw byte from DPLL_n.DPLL_MODE register.
 * @return const char* String representation of the operation mode.
 */
static inline
const char* dpll_mode_op_str(uint8_t reg_byte)
{
    uint8_t pll_mode = (reg_byte >> 3) & 0x07;

    switch (pll_mode) {
        case 0:
            return "PLL";
        case 1:
            return "PHASE";
        case 2:
            return "FREQ";
        case 3:
            return "GPIO";
        case 4:
            return "SYNTH";
        case 5:
            return "PMEAS";
        case 6:
            return "OFF";
        default:
            return "N/A";
    }
}

/**
 * @brief Decodes the state machine mode from the DPLL mode register.
 *
 * @param reg_byte Raw byte from DPLL_n.DPLL_MODE register.
 * @return const char* String representation of the state machine mode.
 */
static inline
const char* dpll_mode_sm_str(uint8_t reg_byte)
{
    uint8_t sm_mode = (reg_byte & 0x07);

    switch (sm_mode) {
        case 0:
            return "AUTO         ";
        case 1:
            return "FORCE_LOCK   ";
        case 2:
            return "FORCE_FRUN   ";
        case 3:
            return "FORCE_HLDOVER";
        default:
            return "N/A          ";
    }
}

/* ─── 32-bit signed phase serialization helpers ──────────────────────────
 * The Renesas 8A34004 DPLL PHASE register is 4 bytes wide.
 * The BSP write in little-endian byte order (LSB at offset 0).
 *
 *   dpll_phase_le32_pack   / dpll_phase_le32_unpack   ── pack/unpack LE
 *   dpll_phase_be32_pack   / dpll_phase_be32_unpack   ── pack/unpack BE
 */

/** @brief Pack int32_t → 4-byte buffer, little-endian (LSB first). */
static inline
void dpll_phase_le32_pack(int32_t phase, uint8_t buf[4])
{
    buf[0] = (uint8_t)( phase        & 0xFF);
    buf[1] = (uint8_t)((phase >>  8) & 0xFF);
    buf[2] = (uint8_t)((phase >> 16) & 0xFF);
    buf[3] = (uint8_t)((phase >> 24) & 0xFF);
}

/**
 * @brief Unpacks a 4-byte little-endian buffer into a signed 32-bit integer.
 *
 * @param reg_bytes Pointer to a 4-byte little-endian array.
 * @return int32_t The decoded 32-bit signed integer.
 */
static inline
int32_t dpll_phase_le32_unpack(const uint8_t reg_bytes[4])
{
    uint32_t unsigned_32bit = 0;

    unsigned_32bit = ((uint32_t)reg_bytes[3] << 24) |
                     ((uint32_t)reg_bytes[2] << 16) |
                     ((uint32_t)reg_bytes[1] <<  8) |
                     ((uint32_t)reg_bytes[0]);

    return (int32_t)unsigned_32bit;
}

/** @brief Pack int32_t → 4-byte buffer, big-endian (MSB first). */
static inline
void dpll_phase_be32_pack(int32_t phase, uint8_t buf[4])
{
    buf[0] = (uint8_t)((phase >> 24) & 0xFF);
    buf[1] = (uint8_t)((phase >> 16) & 0xFF);
    buf[2] = (uint8_t)((phase >>  8) & 0xFF);
    buf[3] = (uint8_t)( phase        & 0xFF);
}

/** @brief Unpack 4-byte big-endian buffer → int32_t. */
static inline
int32_t dpll_phase_be32_unpack(const uint8_t reg_bytes[4])
{
    uint32_t unsigned_32bit = 0;

    unsigned_32bit = ((uint32_t)reg_bytes[0] << 24) |
                     ((uint32_t)reg_bytes[1] << 16) |
                     ((uint32_t)reg_bytes[2] <<  8) |
                     ((uint32_t)reg_bytes[3]);

    return (int32_t)unsigned_32bit;
}

/**
 * @brief Decodes the raw 5-byte data stream of STATUS.DPLLn_PHASE_STATUS into nanoseconds.
 *
 * @param dpll_index Target DPLL index.
 * @param reg_bytes Pointer to a 5-byte buffer containing raw data read
 *                  sequentially from the corresponding phase status registers.
 * @return double The decoded residual phase offset error value in nanoseconds (ns).
 */
static inline
double dpll_phase_status(int dpll_index, uint8_t *reg_bytes)
{
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
 * @brief Parse the raw 8-byte data returned from page C1 of the 8A34004 chip,
 *        and calculate the actual physical layer observation clock frequency.
 *
 * @param input_index Target input channel index.
 * @param reg_bytes Pointer to an 8-byte little-endian array returned by the chip.
 * @return double The computed real frequency value in Hz.
 */
static inline
double decode_obsclk_freq(int input_index, uint8_t *reg_bytes)
{
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

/**
 * @brief Looks up a register in the general register map.
 *
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_general(const char *name, ptp_reg_desc_t *out_desc)
{
    if (!name || !out_desc) return -1;

    for (size_t i = 0; i < BF_8A34004_REG_MAP_GENERAL_SIZE; i++) {
        const ptp_reg_desc_t *reg = &g_8a34004_register_map_general[i];
        if (reg->name == NULL)
            break;
        if (strcmp(reg->name, name) == 0) {
            *out_desc = *reg;
            return 0; // Target found
        }
    }
    return -1;
}

/**
 * @brief Looks up a register in a specific DPLL's register map.
 *
 * @param dpll_index DPLL index (0 to 7).
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_dpll(int dpll_index, const char *name, ptp_reg_desc_t *out_desc)
{
    if (dpll_index > 7 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_ENTRIES_PER_DPLL; i++) {
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

/**
 * @brief Looks up a register in a specific TOD's register map.
 *
 * @param tod_index TOD index (0 to 3).
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register_tod(int tod_index, const char *name, ptp_reg_desc_t *out_desc)
{
    if (tod_index > 3 || !name || !out_desc) return -1;

    for (size_t i = 0; i < MAX_ENTRIES_PER_TOD; i++) {
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

/**
 * @brief Global lookup for a register across all maps (General, DPLL, TOD).
 *
 * @param name Register name string.
 * @param out_desc Pointer to the output register descriptor.
 * @return int 0 if found; negative if not found or on error.
 */
static inline
int bf_ptp_lookup_register(const char *name, ptp_reg_desc_t *out_desc)
{
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

#endif // AFN_8A34004_REGS_H

