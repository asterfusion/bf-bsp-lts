/*!
 * @file bf_pltfm_clockmatrix.h
 * @date 2026/07/08
 *
 * Hang Tsi (tsihang@asterfusion.com)
 *
 *
 */

#ifndef BF_PLTFM_CLOCKMATRIX_H
#define BF_PLTFM_CLOCKMATRIX_H

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/timex.h>
#include <pthread.h>
#include "8a34004.h"

/* Mandatory internal Renesas firmware micro-sequencer latch settlement stall;
 * after SM_RESET, chip transitions through BOOT_STATUS 0xA0
 * over approximately 3 seconds (30 × 100ms polling windows). */
#define CLKMATRIX_DELAY_US    (200)

/* Phase pull-in threshold: deltas below this use the firmware
 * autonomous phase-slewing state machine instead of a direct ToD step.
 * Matches upstream PHASE_PULL_IN_THRESHOLD_NS (15000 ns). */
#define PHASE_PULL_IN_THRESHOLD_NS  (15000)


#define CLOCKMATRIX_ADDR        (0x58)

/* ── POSIX clock_adjtime mode bits (matching UAPI <linux/timex.h>) ───
 * Used by bf_clock_adjtime() to dispatch between adjtime/adjfine/adjphase/read. */
#ifndef ADJ_OFFSET
#define ADJ_OFFSET               0x0001  /* time offset (→ adjphase) */
#endif
#ifndef ADJ_FREQUENCY
#define ADJ_FREQUENCY            0x0002  /* frequency offset (→ adjfine) */
#endif
#ifndef ADJ_SETOFFSET
#define ADJ_SETOFFSET            0x0100  /* add 'time' to current time (→ adjtime) */
#endif
#ifndef ADJ_NANO
#define ADJ_NANO                 0x2000  /* nanosecond resolution */
#endif

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC             1000000000LL
#endif
#ifndef NSEC_PER_USEC
#define NSEC_PER_USEC            1000L
#endif

struct timespec64 {
	int64_t		tv_sec;		/* seconds */
	long		tv_nsec;	/* nanoseconds */
};

struct tod_priv_t {
	long 		dialed_frequency;
};
struct pll_priv_t {
	int  		resv;
};

struct clockmatrix_t {
	struct tod_priv_t _tod[MAX_TOD];
	struct pll_priv_t _pll[MAX_PLL];
	pthread_mutex_t lock;
};

extern struct clockmatrix_t clockmatrix;

int bf_clockmatrix_init(void);

int bf_pltfm_tx_rst_pulse();
int bf_pltfm_set_clk(bf_pltfm_clk_source clk_source);
int bf_pltfm_read_ptp_reg(uint8_t page, uint8_t reg, uint8_t* value);
int bf_pltfm_write_ptp_reg(uint8_t page, uint8_t reg, uint8_t value);
int bf_pltfm_write_ptp_reg_burst(uint8_t page, uint8_t reg, uint8_t *value, size_t l);
int bf_pltfm_read_ptp_reg_burst(uint8_t page, uint8_t reg, uint8_t *value, size_t l);

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
int bf_ptp_get_sys_apll_state(uint8_t sys_dpll_index, uint8_t *state);


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
int bf_ptp_get_sys_dpll_state(uint8_t sys_apll_index, enum dpll_state *state);


/**
 * @brief Clears the BOOT_STATUS register (4 bytes) to prepare for
 *        post-reset polling.
 *
 *        GENERAL_STATUS.BOOT_STATUS at page 0xC0, base 0x14, offset 0x00.
 *
 * @return int 0 on success; negative on error.
 */
int bf_ptp_clr_boot_status(void);

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
int bf_ptp_get_boot_status(uint32_t *status);

/**
 * @brief Retrieves the 16-bit Product ID from the GENERAL_STATUS registers.
 * 
 * @param product_id Output pointer populated with the 16-bit hardware Product ID.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_product_id(uint16_t *product_id);

/**
 * @brief Retrieves the firmware Release Number (Major, Minor, and Revision).
 * 
 * @param major Output pointer populated with the Major release version.
 * @param minor Output pointer populated with the Minor release version.
 * @param rev Output pointer populated with the Revision version.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_release_no(uint8_t *major, uint8_t *minor, uint8_t *rev);

/**
 * @brief Retrieves the input clock frequency for physical clock inputs (INPUT_0 or INPUT_1).
 * 
 * @param input_index Target input channel index (Valid range: 0 or 1).
 * @param freq Output pointer populated with the calculated input frequency in Hz.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_input_freq(int input_index, double *freq);

/**
 * @brief Sets the 32-bit signed phase increment for DPLL_n.
 *        Performs sequential per-byte writes over the 4-byte phase offset region
 *        with a 200 µs micro-sequencer latch settlement stall.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param phase Signed 32-bit phase target value.
 * @return int 0 on success; negative on hardware fault.
 */
int bf_ptp_set_dpll_phase(uint8_t dpll_index, int32_t phase);

/**
 * @brief Gets the 32-bit signed phase increment currently cached or running in DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_phase Output pointer populated with the running 32-bit signed phase.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_phase(uint8_t dpll_index, int32_t *out_phase);

/**
 * @brief Atomically extracts and decodes the 40-bit residual sub-ns phase offset error of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param phase_ns Output pointer populated with the decoded absolute phase error in nanoseconds.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_phase_status(uint8_t dpll_index, double *phase_ns);

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
int bf_ptp_set_dpll_freq(uint8_t dpll_index, int64_t freq_ffo_q53);

/**
 * @brief Gets the 42-bit signed frequency offset currently configured in DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_freq_ffo_q53 Output pointer populated with the signed 42-bit frequency offset in Q53 format.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_freq(uint8_t dpll_index, int64_t *out_freq_ffo_q53);

/**
 * @brief Retrieves the current operational configuration mode byte of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param op_mode Output pointer populated with the raw operational mode control byte.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_mode(uint8_t dpll_index, enum pll_mode *op_mode);

/**
 * @brief Configures the operational mode and state machine mode of DPLL_n.
 *        Enforces Read-Modify-Write to protect RESERVED[7:6] bits.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param op_mode Operational mode (0=PLL, 1=PHASE, 2=FREQ, 3=GPIO, 4=SYNTH, 5=PMEAS, 6=OFF).
 * @param sm_mode State machine mode (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_mode(uint8_t dpll_index, enum pll_mode op_mode);

/**
 * @brief Retrieves the state-machine mode from the DPLL_MODE register of DPLL_n.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param sm_mode Output pointer receiving the SM_MODE field [2:0]
 *                (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_sm(uint8_t dpll_index, enum pll_state *sm_mode);

/**
 * @brief Sets the state-machine mode of DPLL_n via RMW, preserving OP_MODE and reserved bits.
 *
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param sm_mode State machine mode (0=AUTO, 1=FORCE_LOCK, 2=FORCE_FRUN, 3=FORCE_HLDOVER).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_set_dpll_sm(uint8_t dpll_index, enum pll_state sm_mode);

/**
 * @brief Retrieves the live tracking reference status of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param ref_stat Output pointer populated with the upstream physical CLK channel index.
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_ref_stat(uint8_t dpll_index, uint8_t *ref_stat);

/**
 * @brief Retrieves the core state machine lock configuration of DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param state Output pointer receiving the extracted DPLL_STATE field [3:0] (enum dpll_state).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_state(uint8_t dpll_index, enum dpll_state *state);

/**
 * @brief Retrieves the TOD synchronization configuration for DPLL_n.
 * 
 * @param dpll_index Target DPLL block index (Valid range: 0 to 7).
 * @param out_enabled Output pointer populated with the ToD sync enable status.
 * @param out_tod_source Output pointer populated with the ToD source index (0 to 3).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_get_dpll_tod_sync_cfg(uint8_t dpll_index,
        bool *out_enabled, uint8_t *out_tod_source);

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
        bool enable_sync, uint8_t tod_source);

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
        bool *out_pri_en, bool *out_pri_filt, uint8_t *out_pri_src_id);

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
        bool pri_en, bool pri_filt, uint8_t pri_src_id);

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
        bool *out_sec_en, bool *out_sec_filt, uint8_t *out_sec_src_id);

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
        bool sec_en, bool sec_filt, uint8_t sec_src_id);

/**
 * @brief Executes the mandatory hardware commit trigger to enforce phase pull-in / frequency step alignment.
 *        Maps perfectly to DPLL_PHASE_PULL_IN_n and FREQ alignment triggers.
 * 
 * @param trigger_type 0 for Phase Pull-In (0x0F), 1 for Frequency Commit (0x3F).
 * @return int 0 on success; negative on error.
 */
int bf_ptp_commit_dpll_trigger(uint8_t trigger_type);

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
int bf_ptp_set_phase_pull_in_offset(uint8_t dpll_index, int32_t offset_50ps);

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
int bf_ptp_set_phase_pull_in_slope_limit(uint8_t dpll_index, uint32_t max_ffo_ppb);

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
int bf_ptp_start_phase_pull_in(uint8_t dpll_index);

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
int bf_ptp_phase_pull_in(uint8_t dpll_index, int32_t delta_ns, uint32_t max_ffo_ppb);

/**
 * @brief Retrieves the live driving clock source (DPLL linkage index) of a specified TOD instance.
 *        Queries DPLL_n.DPLL_TOD_SYNC_CFG registers to locate which DPLL is actively synchronized
 *        with this ToD index.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_dpll_src Output pointer populated with the driving DPLL index (Range: 0 to 7).
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_clk_src(uint8_t tod_index, uint8_t *out_dpll_src);

/**
 * @brief Retrieves the historical write completion metrics from the specified TOD_WRITE_n module.
 *        Corresponds to StepC logic.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_write_counter Output pointer populated with the raw hardware write count byte.
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_write_counter(uint8_t tod_index, uint8_t *out_write_counter);

/**
 * @brief Retrieves the historical read snapshot completion metrics from the specified TOD_READ_PRIMARY_n module.
 * 
 * @param tod_index Target TOD instance index (Valid range: 0 to 3).
 * @param out_read_counter Output pointer populated with the raw hardware read count byte.
 * @return int 0 on success; -1 on lookup or parameter error; -2 on hardware I2C transaction failure.
 */
int bf_ptp_get_tod_read_counter(uint8_t tod_index, uint8_t *out_read_counter);

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
        bool *out_enabled, bool *out_even_pps, bool *out_sync_disabled);

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
        bool enable_tod, bool even_pps_mode, bool disable_out_sync);

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
int bf_ptp_get_tod_time(uint8_t tod_index, uint64_t *out_sec, uint64_t *out_ns);

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
int bf_ptp_set_tod_time(uint8_t tod_index, uint64_t sec, uint64_t ns);

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
                                   uint64_t *out_sec, uint64_t *out_ns);

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
int bf_ptp_arm_tod_read_trigger_refclk(uint8_t tod_index, uint8_t ref);

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
int bf_ptp_arm_tod_read_trigger_immediate(uint8_t tod_index);

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
int bf_clock_reset(void);

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
 * @param delta      Signed nanosecond offset (positive = advance).
 * @return int       0 on success; negative on error.
 */
int bf_ptp_adjtime(uint8_t tod_index, int64_t delta);

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
 * @param tod_index  Target TOD instance (0-3).
 * @param delta      Signed nanosecond phase offset (resolution: 50 ps).
 * @return int        0 on success; negative on error.
 */
int bf_ptp_adjphase(uint8_t tod_index, int32_t delta);

/**
 * @brief Adjusts DPLL frequency using a scaled PPM offset (persistent rate).
 *
 * Converts scaled_ppm (2^-16 ppm) to raw FCW via
 *   FCW = scaled_ppm * 5^8 / (111 * 2^8) = scaled_ppm * 390625 / 28416,
 * writes LE to DPLL_n.FREQ in Q5.3 format.  Auto-switches the DPLL to
 * WRITE_FREQUENCY op_mode.
 *
 * This is a persistent frequency offset — not a time-step or phase slew.
 * For time-step correction, see bf_ptp_adjtime().
 * For gradual phase slew, see bf_ptp_adjphase().
 *
 * @param tod_index  Target TOD instance (0-3).
 * @param scaled_ppm  Signed frequency offset in 2^-16 ppm units.
 * @return int        0 on success; negative on error.
 */
int bf_ptp_adjfine(uint8_t tod_index, int64_t scaled_ppm);

/**
 * @brief Unified PTP clock adjustment — POSIX clock_adjtime equivalent.
 *
 *        Dispatches based on tx->modes:
 *
 *        - ADJ_SETOFFSET → bf_ptp_adjtime  (ToD step / phase pull-in)
 *          Bounds: |delta| < 2^48 ns (~78.1 h); epoch >= 0 enforced.
 *          Returns: 0 on success; -1 on range/param error; -3 on epoch
 *          underflow (step would go below Unix epoch).
 *
 *        - ADJ_FREQUENCY → bf_ptp_adjfine  (DPLL FCW, persistent rate)
 *          Bounds: |scaled_ppm| ≤ 65536000 (±1000 ppm, DPLL DCO limit).
 *          Returns: 0 on success; -1 on error.
 *
 *        - ADJ_OFFSET    → bf_ptp_adjphase (DPLL phase slew, gradual)
 *          Bounds: |offset_ns| ≤ 107374182 ns (~107.4 ms, phase reg limit).
 *          Returns: 0 on success; -1 on error.
 *
 *        - modes == 0    → read-back (tx->freq returned unchanged).
 *          Returns: 0.
 *
 *        For ADJ_FREQUENCY and ADJ_OFFSET, the driving DPLL is resolved
 *        from the given ToD index via bf_ptp_get_tod_clk_src().
 *
 *        For ADJ_SETOFFSET, the current ToD is read to validate that the
 *        resulting time stays within the valid Unix epoch range.
 *
 * @param clkid      Clock ID (mapped to tod_index 0-3 via (uint8_t) cast).
 * @param tx         POSIX timex struct with mode bits and adjustment params.
 * @return int       0 on success; -1 on error; -3 on epoch underflow.
 */
int bf_clock_adjtime(clockid_t clkid, struct timex *tx);

/**
 * @brief POSIX clock_settime — sets the ToD counter to an absolute time.
 *
 *        Validates: tv_sec in [0, 2^48-1] and tv_nsec in [0, 999999999],
 *        then writes to the hardware PRIMARY WRITE path via
 *        bf_ptp_set_tod_time().
 *
 *        The 8A34004 ToD has 48-bit seconds + 32-bit nanoseconds registers
 *        (max ~8.9 million years).  The validation caps at 2^48 seconds as
 *        a practical guard, not a hardware limitation.
 *
 * @param clkid  Clock ID (mapped to tod_index 0-3 via (uint8_t) cast).
 * @param tp     Absolute time to set (seconds + nanoseconds).
 * @return int   0 on success; -1 on invalid parameter or out of range.
 */
int bf_clock_settime(clockid_t clkid, struct timespec *tp);

/**
 * @brief POSIX clock_gettime — reads the current ToD counter value.
 *
 *        Matches upstream ptp_clock_gettime(): fires an IMMEDIATE trigger
 *        on the PRIMARY snapshot buffer, polls for latch settle (up to 10
 *        iterations), then burst-reads and decodes the 11-byte time array.
 *
 * @param clkid  Clock ID (mapped to tod_index 0-3 via (uint8_t) cast).
 * @param tp     Output pointer populated with seconds + nanoseconds.
 * @return int   0 on success; negative on error.
 */
int bf_clock_gettime(clockid_t clkid, struct timespec *tp);

/**
 * @brief POSIX clock_getres — returns the ToD counter resolution.
 *
 *        The 8A34004 ToD counter increments in 1-nanosecond steps,
 *        so the resolution reported is 1 ns, matching upstream
 *        ptp_clock_getres().
 *
 * @param clkid  Clock ID (mapped to tod_index 0-3 via (uint8_t) cast).
 * @param res    Output pointer: tv_sec=0, tv_nsec=1 (1 ns resolution).
 * @return int   0 on success; negative on error.
 */
int bf_clock_getres(clockid_t clkid, struct timespec *res);

int bf_clockmatrix_init(void);

#endif // BF_PLTFM_CLOCKMATRIX_H
