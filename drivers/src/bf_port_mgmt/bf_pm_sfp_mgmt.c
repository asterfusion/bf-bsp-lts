/*!
 * @file bf_pm_sfp_mgmt.c
 * @date 2020/05/27
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <ctype.h>
#include <unistd.h>

#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_port_mgmt/bf_port_mgmt_intf.h>
#include <bf_pltfm_ext_phy.h>
#include <bf_pltfm_types/bf_pltfm_types.h>

#include <bf_types/bf_types.h>
#include <tofino/bf_pal/bf_pal_pltfm_porting.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_porting.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#include "bf_pm_priv.h"

bool devport_state_tx_mode_chk (bf_dev_id_t dev_id,
                    bf_pal_front_port_handle_t *port_hdl);
bool devport_state_loopback_mode_chk (bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl);

void devport_state_enabled_set(bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl, bool set);
bool devport_state_enabled_chk (bf_dev_id_t dev_id,
                            bf_pal_front_port_handle_t *port_hdl);
int devport_speed_to_led_color (bf_port_speed_t speed,
    bf_led_condition_t *led_cond);


static uint32_t temper_monitor_interval_ms = 5000;

/* SFF-8472 delay and timing for soft control and status functions and requirements.
 * by tsihang, 2021-07-29. */
typedef enum {
    SFF8472_TMR_t_reset_init = 1,  // really only 2us
    SFF8472_TMR_t_reset = 2000,    // all times in ms

    SFF8472_TMR_ton_txdis = 100,
    SFF8472_TMR_toff_txdis = 400,

    SFF8472_TMR_ton_lpm = 100,
    SFF8472_TMR_toff_lpm = 300,

    SFF8472_TMR_ton_rxdis = 100,
    SFF8472_TMR_toff_rxdis = 100,

    SFF8472_TMR_ton_rxlos = 100,
    SFF8472_TMR_toff_rxlos = 100,
} sff8436_timer_val_t;

/* Module FSM states */
typedef enum {
    // SFP unknown or unused on board
    SFP_FSM_ST_IDLE = 0,
    // pseudo-states, assigned by qsfp scan timer to communicate
    // with qsfp fsm
    SFP_FSM_ST_REMOVED,
    SFP_FSM_ST_INSERTED,
    // states requiring fixed delays
    SFP_FSM_ST_WAIT_T_RESET,      // 2000ms
    SFP_FSM_ST_WAIT_TON_TXDIS,    //  100ms, SFF8472_TMR_ton_txdis
    SFP_FSM_ST_WAIT_TOFF_TXDIS,    // 100ms, SFF8472_TMR_toff_txdis
    SFP_FSM_ST_DETECTED,
    SFP_FSM_ST_UPDATE,
    SFP_FSM_ST_WAIT_UPDATE,  // update-specific delay
} sfp_fsm_state_t;

static char *sfp_fsm_st_to_str[] = {
    "SFP_FSM_ST_IDLE               ",
    "SFP_FSM_ST_REMOVED            ",
    "SFP_FSM_ST_INSERTED           ",
    "SFP_FSM_ST_WAIT_T_RESET       ",
    "SFP_FSM_ST_WAIT_TON_TXDIS     ",
    "SFP_FSM_ST_WAIT_TOFF_TXDIS    ",
    "SFP_FSM_ST_DETECTED           ",
    "SFP_FSM_ST_UPDATE             ",
    "SFP_FSM_ST_WAIT_UPDATE        ",
};

/* Module CH FSM states */
typedef enum {
    SFP_CH_FSM_ST_DISABLED,
    SFP_CH_FSM_ST_ENABLING,        // kick off enable sequence
    SFP_CH_FSM_ST_ENA_OPTICAL_TX,  // 100ms, SFF8472_TMR_toff_txdis
    SFP_CH_FSM_ST_NOTIFY_ENABLED,
    SFP_CH_FSM_ST_ENABLED,
    SFP_CH_FSM_ST_DISABLING,  // assert TX_DISABLE
} sfp_fsm_ch_en_state_t;

static char *sfp_ch_fsm_en_st_to_str[] = {
    "SFP_CH_FSM_ST_DISABLED        ",
    "SFP_CH_FSM_ST_ENABLING        ",
    "SFP_CH_FSM_ST_ENA_OPTICAL_TX  ",
    "SFP_CH_FSM_ST_NOTIFY_ENABLED  ",
    "SFP_CH_FSM_ST_ENABLED         ",
    "SFP_CH_FSM_ST_DISABLING       ",
};

typedef enum sfp_typ_t {
    SFP_TYP_UNKNOWN = 0,
    SFP_TYP_OPTICAL = 1,
    SFP_TYP_COPPER  = 2,
} sfp_typ_t;

// A2h (byte 0-39 set the threshold and byte 110-113 tigger the alarm/warn)
typedef struct sfp_status_and_alarms_t {
#define SFF8472_SA_DATA_NOT_READY   (1 << 0)
#define SFF8472_SA_RX_LOS           (1 << 1)
#define SFF8472_SA_TX_FAULT         (1 << 2)
#define SFF8472_SA_TX_DIS           (1 << 7)
    union {
        /* Byte 110 */
        uint8_t data_not_ready      : 1,
                rx_los              : 1,
                tx_fault            : 1,
                soft_rs_select      : 1,
                rs0                 : 1,
                rs1                 : 1,
                soft_tx_dis_select  : 1,
                tx_dis_state        : 1;
        uint8_t value;
    } byte_110;

    /* Byte 111 */
    uint8_t byte_111;

    /* Combine high and low alarm. */
#define SFF8472_SA_TMP_ALARM        (3 << 6)
#define SFF8472_SA_VCC_ALARM        (3 << 4)
#define SFF8472_SA_TX_BIAS_ALARM    (3 << 2)
#define SFF8472_SA_TX_POWER_ALARM   (3 << 0)
    union {
        /* Byte 112 */
        uint8_t tx_power_low_alarm  : 1,
                tx_power_high_alarm : 1,
                tx_bias_low_alarm   : 1,
                tx_bias_high_alarm  : 1,
                vcc_low_alarm       : 1,
                vcc_high_alarm      : 1,
                tmp_low_alarm       : 1,
                tmp_high_alarm      : 1;
        uint8_t value;
    } byte_112;

#define SFF8472_SA_RX_POWER_ALARM   (3 << 6)
    union {
        /* Byte 113 */
        uint8_t rsv0     : 1,
                rsv1     : 1,
                tec_low  : 1,
                tec_high : 1,
                laser_tmp_low  : 1,
                laser_tmp_high : 1,
                rx_low_alarm   : 1,
                rx_high_alarm  : 1;
        uint8_t value;
    } byte_113;
} sfp_status_and_alarms_t;

#define sfp_status_and_alarm_init_val { {0},0,{0},{0} }

typedef struct sfp_state_t {
    sfp_typ_t sfp_type;
    sfp_fsm_state_t fsm_st;
    sfp_fsm_ch_en_state_t fsm_ch_st;
    struct timespec next_fsm_run_time;
    struct timespec next_fsm_ch_run_time;
    uint32_t flags;
    bool sfp_quick_removed;
    bool wr_inprog;
    bool los_detected;
    bool immediate_enable;  // used when retrying port init
    sfp_status_and_alarms_t status_and_alarms;
} sfp_state_t;

/* access is 1 based index, so zeroth index would be unused */
sfp_state_t sfp_state[BF_PLAT_MAX_SFP + 1] = {
    {
        SFP_TYP_UNKNOWN,
        SFP_FSM_ST_IDLE,
        SFP_CH_FSM_ST_DISABLED,
        {0, 0},
        {0, 0},
        0,
        false,
        false,
        true,
        false,
        sfp_status_and_alarm_init_val
    }
};
static sfp_status_and_alarms_t
sfp_alarms_initial_state = { {0}, 0, {0}, {0} };


/*****************************************************************
 * Return the current CH FSM state desc for a transceiver.
 *****************************************************************/
char *sfp_channel_fsm_st_get (int port)
{
    return sfp_ch_fsm_en_st_to_str[sfp_state[port].fsm_ch_st];
}

/*****************************************************************
 * Return the current FSM state desc for a transceiver.
 *****************************************************************/
char *sfp_module_fsm_st_get (int port)
{
    return sfp_fsm_st_to_str[sfp_state[port].fsm_st];
}

/*****************************************************************
 * Setting the time for which the next state would start.
 *****************************************************************/
void sfp_fsm_next_run_time_set (struct timespec
                                *next_run_time,
                                int delay_ms)
{
    struct timespec now;

    clock_gettime (CLOCK_MONOTONIC, &now);

    next_run_time->tv_sec = now.tv_sec +
                            (delay_ms / 1000);
    next_run_time->tv_nsec =
        now.tv_nsec + (((delay_ms % 1000) * 1000000) %
                       1000000000);
    if (next_run_time->tv_nsec >= 1000000000) {
        next_run_time->tv_sec++;
        next_run_time->tv_nsec -= 1000000000;
    }
}

#define fetch_by_module(module)     \
    uint32_t conn, chnl;            \
    if (bf_sfp_get_conn (           \
        (module),                   \
        &conn,                      \
        &chnl)) {                   \
        return;                     \
    }


/*****************************************************************
 * This routine is only used in Tofino 1 implementations
 *****************************************************************/
static void sfp_log_alarms (bf_dev_id_t dev_id,
                            int module,
                            sfp_status_and_alarms_t *old,
                            sfp_status_and_alarms_t *new)
{
    if (memcmp ((char *)old, (char *)new,
                sizeof (*old)) != 0) {
        // log the raw fields to provide current state of all alarms
        LOG_DEBUG (
            " SFP    %2d : Alarms (A2h Bytes 110-113) : %02x %02x %02x %02x",
            module,
            new->byte_110.value,
            new->byte_111,
            new->byte_112.value,
            new->byte_113.value);


        if (old->byte_110.value !=
            new->byte_110.value) {

            if (old->byte_110.tx_dis_state !=
                new->byte_110.tx_dis_state) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Tx Dis:",
                           old->byte_110.tx_dis_state,
                           new->byte_110.tx_dis_state);
            }

            if (old->byte_110.data_not_ready !=
                new->byte_110.data_not_ready) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Data_Not_Ready:",
                           old->byte_110.data_not_ready,
                           new->byte_110.data_not_ready);
            }

            if (old->byte_110.rx_los !=
                new->byte_110.rx_los) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Rx Los:",
                           old->byte_110.rx_los, new->byte_110.rx_los);
            }
        }

        if (old->byte_112.value !=
            new->byte_112.value) {
            if ((old->byte_112.value & SFF8472_SA_TMP_ALARM)
                !=
                (new->byte_112.value & SFF8472_SA_TMP_ALARM)) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Tmp Alarm/Warn:",
                           (old->byte_112.value & SFF8472_SA_TMP_ALARM) >> 6,
                           (new->byte_112.value & SFF8472_SA_TMP_ALARM) >>
                           6);
            }
            if ((old->byte_112.value & SFF8472_SA_VCC_ALARM)
                !=
                (new->byte_112.value & SFF8472_SA_VCC_ALARM)) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Vcc Alarm/Warn:",
                           (old->byte_112.value & SFF8472_SA_VCC_ALARM) >> 4,
                           (new->byte_112.value & SFF8472_SA_VCC_ALARM) >>
                           4);
            }
            if ((old->byte_112.value &
                 SFF8472_SA_TX_BIAS_ALARM) !=
                (new->byte_112.value &
                 SFF8472_SA_TX_BIAS_ALARM)) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Tx Bias Alarm/Warn:",
                           (old->byte_112.value & SFF8472_SA_TX_BIAS_ALARM)
                           >> 2,
                           (new->byte_112.value & SFF8472_SA_TX_BIAS_ALARM)
                           >> 2);
            }
            if ((old->byte_112.value &
                 SFF8472_SA_TX_POWER_ALARM) !=
                (new->byte_112.value &
                 SFF8472_SA_TX_POWER_ALARM)) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Tx Power Alarm/Warn:",
                           (old->byte_112.value & SFF8472_SA_TX_POWER_ALARM)
                           >> 0,
                           (new->byte_112.value & SFF8472_SA_TX_POWER_ALARM)
                           >> 0);
            }
        }

        if (old->byte_113.value !=
            new->byte_113.value) {
            if ((old->byte_113.value &
                 SFF8472_SA_RX_POWER_ALARM) !=
                (new->byte_113.value &
                 SFF8472_SA_RX_POWER_ALARM)) {
                LOG_DEBUG (" SFP    %2d : %28s %02x -> %02x",
                           module,
                           "Rx Power Alarm/Warn:",
                           (old->byte_113.value & SFF8472_SA_RX_POWER_ALARM)
                           >> 6,
                           (new->byte_113.value & SFF8472_SA_RX_POWER_ALARM)
                           >> 6);
            }
        }
    }
}

/*****************************************************************
 * Read a SFP transceiver.
 *****************************************************************/
int bf_fsm_sfp_rd (
    unsigned int module, int offset,
    int len, uint8_t *buf)
{
    int rc;

    rc = bf_sfp_module_read (module, offset, len,
                             buf);

    // LOG_DEBUG(" SFP    %2d : Rd : offset=%2d : len=%3d :
    // data[0]=%02x",
    //          module, offset, len, buf[0]);

    return rc;
}

/*****************************************************************
 * Write a SFP transceiver.
 *****************************************************************/
int bf_fsm_sfp_wr (
    unsigned int module, int offset, int len,
    uint8_t *buf)
{
    int rc;

    rc = bf_sfp_module_write (module, offset, len,
                              buf);

    // LOG_DEBUG(" SFP    %2d : Wr : offset=%2d : len=%3d :
    // data[0]=%02x",
    //          module, offset, len, buf[0]);

    return rc;
}

/*****************************************************************
 * Polling the los signal for a transceiver. The LOS signal could
 * be provided by CPLD or just reading from transceiver itself.
 *
 * Try to notify the SDK driver if los detected.
 *
 * Also this routine will decide whether a quick removal action
 * detected or not, if so it will set sfp_quick_removed to true
 * by calling bf_pm_sfp_quick_removal_detected_set.
 *
 * Make sure that the poll_los API read data as less as possible
 * thru i2c bus.
 *****************************************************************/
static int sfp_fsm_poll_los (bf_dev_id_t dev_id,
                             int module)
{
    int rc;
    sfp_status_and_alarms_t status_and_alarms;

    // Donot move FSM until removal is hanlded
    if (sfp_state[module].sfp_quick_removed) {
        return -1;
    }

    // read status and alarms A2h (bytes 110-113) onto stack. This is in case
    // there is an error on the read we dont corrupt the sfp_state
    // structure. If no error, copy into struct
    if (bf_sfp_get_dom_support (module)) {
        rc = bf_fsm_sfp_rd (
                module, MAX_SFP_PAGE_SIZE + 110,
                sizeof (status_and_alarms),
                (uint8_t *)&status_and_alarms);
        if (rc) {
            // Detect and latch the removal
            // state, so that can sfp-scan handle the rest.
            if (!sfp_state[module].sfp_quick_removed) {
                bool is_present = 0;
                bf_sfp_detect_transceiver (module, &is_present);
                if (!is_present) {
                    sfp_state[module].sfp_quick_removed = true;
                    LOG_DEBUG (" SFP    %2d : Removal detected",
                            module);
                    sfp_state[module].sfp_quick_removed = true;
                    bf_pm_sfp_quick_removal_detected_set (module,
                                                        true);
                    return -1;
                }
            }
            LOG_ERROR (" SFP    %2d : Error <%d> reading status fields",
                    module, rc);
            return 0;  // can't trust the data, just leave
        }

        sfp_log_alarms (dev_id, module,
                        &sfp_state[module].status_and_alarms,
                        &status_and_alarms);
    }

    /* Read LOS signal via CPLD. Not very sure, but very helpful. */
    bool rx_los_now = false;   /* Init to false in case a misjudge to a GOOD link when i2c hang on. */
    bool rx_los_before = sfp_state[module].los_detected;
    rc = bf_pltfm_sfp_los_read (module, &rx_los_now);
    if (!rc) {
        bf_dev_port_t dev_port;
        bf_status_t bf_status;
        bf_pal_front_port_handle_t port_hdl;
        bool los = rx_los_now ? true : false;

        bf_sfp_get_conn (
            (module),
            &port_hdl.conn_id,
            &port_hdl.chnl_id);

        FP2DP (dev_id, &port_hdl, &dev_port);

        // hack, really need # lanes on port
        // TBD: Move optical-type to qsfp_detection_actions
        bf_port_is_optical_xcvr_set (dev_id, dev_port,
                                     true);
        bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                        true);
        // If there is NO RX, SDK probably will detect link-down faster.
        //
        // If there is a RX and link is already UP, SDK will NOT act on this
        // los-flag. Hence we donot have to tie to link-up here or to port-FSM
        // states.
        bf_status = bf_port_optical_los_set (dev_id, dev_port,
                                 los);
        if (bf_status == BF_SUCCESS) {
            sfp_state[module].los_detected = los;
            if (rx_los_now ^ rx_los_before) {
                LOG_DEBUG (" SFP    %2d : dev_port=%3d : LOS=%d",
                           module, dev_port, los);
            }
        }
    }

    // copy new alarm state into out cached struct
    sfp_state[module].status_and_alarms =
        status_and_alarms;

    return rc;
}

/*****************************************************************
 * Handle special transceiver.
 *****************************************************************/
static int sfp_fsm_special_case_handled (
    bf_dev_id_t dev_id,
    int module)
{
    // define
    int rc = 0;
    uint8_t buf = 0xFF;
    uint32_t conn, chnl;
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    do {
        if (bf_sfp_get_conn (
                (module),
                &conn,
                &chnl)) {
            rc = -1;
            break;
        }

        port_hdl.conn_id = conn;
        port_hdl.chnl_id = chnl;
        FP2DP (dev_id, &port_hdl, &dev_port);

        if (check_sfp_module_vendor (conn, chnl,
                                     "TRIXON") == 0) {
            //if vendor is Trixon
            // get port speed
            bf_port_speed_t speed = BF_SPEED_NONE;
            bf_port_speed_get (dev_id, dev_port, &speed);
            // set module speed
            if (speed == BF_SPEED_10G) {
                buf = 0x10;
            } else if (speed == BF_SPEED_25G) {
                buf = 0x00;
            }
            if (buf != 0xFF) {
                if (bf_fsm_sfp_wr (module,
                                   MAX_SFP_PAGE_SIZE + 0x79, 1, &buf)) {
                    rc = -2;
                    break;
                }
            }
        } // some other vendors
    } while (0);

    return rc;
}

/*****************************************************************
 * Reset a transceiver
 *****************************************************************/
static void sfp_fsm_reset_assert (bf_dev_id_t
                                  dev_id,
                                  int module)
{
    int rc;
    fetch_by_module (module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) RESETL = true",
               module, conn, chnl);
    // assert resetL
    rc = bf_sfp_reset (module, true);
    if (rc != 0) {
        LOG_ERROR (" SFP    %2d : (%02d/%d) Error <%d> asserting resetL",
                   module, conn, chnl, rc);
    }
}

/*****************************************************************
 * Non-reset a transceiver
 *****************************************************************/
static void sfp_fsm_reset_de_assert (
    bf_dev_id_t dev_id,
    int module)
{
    int rc;
    fetch_by_module (module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) RESETL = false",
               module, conn, chnl);
    // de-assert resetL
    rc = bf_sfp_reset (module, false);
    if (rc != 0) {
        LOG_ERROR (" SFP    %2d : (%02d/%d) Error <%d> de-asserting resetL",
                   module, conn, chnl, rc);
    }
}

/*****************************************************************
 * Reset and non reset all transceivers
 *****************************************************************/
void sfp_deassert_all_reset_pins (void)
{
    int module;

    LOG_DEBUG (" SFP Assert RESETL on all SFPs");

    for (module = 1; module < bf_pm_num_sfp_get();
         module++) {
        sfp_fsm_reset_assert (0, module);
    }

    bf_sys_usleep (SFF8472_TMR_t_reset_init *
                   1000);  // takes micro-seconds

    LOG_DEBUG (" SFP Assert LPMODE and De-assert RESETL on all SFPs");

    for (module = 1; module < bf_pm_num_sfp_get();
         module++) {
        // assert LP_MODE
        //sfp_fsm_lpmode_assert (module);
        sfp_fsm_reset_de_assert (0, module);
    }
    LOG_DEBUG (" SFP Wait 2 sec (t_reset) for SFPs to initialize..");
    bf_sys_usleep (SFF8472_TMR_t_reset *
                   1000); // takes micro-seconds
}

/*****************************************************************
 * An insertion is detected. Ready to kick off transceiver FSM.
 *****************************************************************/
void sfp_fsm_inserted (int module)
{
    fetch_by_module (module);
    //sfp_fsm_reset_assert (module);

    sfp_fsm_state_t prev_st =
        sfp_state[module].fsm_st;

    sfp_state[module].fsm_st = SFP_FSM_ST_INSERTED;
    sfp_state[module].next_fsm_run_time.tv_sec = 0;
    sfp_state[module].next_fsm_run_time.tv_nsec = 0;

    LOG_DEBUG (" SFPMSM %2d : (%02d/%d) %s -->  SFP_FSM_ST_INSERTED",
               module, conn, chnl,
               sfp_fsm_st_to_str[prev_st]);
}

/*****************************************************************
 * A removal is detected. Triggered by sfp_scan_helper.
 * Tranceiver FSM will notify SDK driver that the port is not ready.
 * The SDK driver will try to stop internal FSM.
 *****************************************************************/
void sfp_fsm_removed (int module)
{
    fetch_by_module (module);

    sfp_fsm_state_t prev_st =
        sfp_state[module].fsm_st;

    sfp_state[module].next_fsm_run_time.tv_sec = 0;
    sfp_state[module].next_fsm_run_time.tv_nsec = 0;

    sfp_state[module].fsm_st = SFP_FSM_ST_REMOVED;
    LOG_DEBUG (" SFPMSM %2d : (%02d/%d) %s -->  SFP_FSM_ST_REMOVED",
               module, conn, chnl,
               sfp_fsm_st_to_str[prev_st]);
}

/*****************************************************************
 * Notify the SDK driver that the transceiver is ready.
 * The Ready means transceiver inserted, Tx turned on and no los
 * detected from peer.
 *****************************************************************/
void sfp_fsm_notify_ready (bf_dev_id_t dev_id,
                           int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);

    bf_port_optical_los_set (dev_id, dev_port, false);
    bf_port_optical_xcvr_ready_set (dev_id, dev_port, true);
    LOG_DEBUG (
        " SFP    %2d : (%02d/%d) Ready",
        module, conn, chnl);
}

/*****************************************************************
 * Notify the SDK driver that the transceiver is not ready.
 *****************************************************************/
void sfp_fsm_notify_not_ready (bf_dev_id_t dev_id,
                               int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);

    bf_port_optical_xcvr_ready_set (dev_id, dev_port, false);
    bf_port_optical_los_set (dev_id, dev_port, true);

    LOG_DEBUG (
        " SFP    %2d : (%02d/%d) Not Ready",
        module, conn, chnl);

}

/*****************************************************************
 * Called by sfp_fsm. If SFP type has already been identified,
 * returns known type. If not, attempts to determine type based on
 * compliance and extended compliance fields. If any error
 * determining type return -1
 *****************************************************************/
static int sfp_fsm_identify_type (int module,
                                  bool *is_optical)
{
    /* TBD. */
    sfp_typ_t sfp_type = SFP_TYP_UNKNOWN;

    /* Force to OPTICAL and return. Should identify from real. */
    sfp_state[module].sfp_type = SFP_TYP_OPTICAL;
    *is_optical = true;
    return 0;

    if (sfp_state[module].sfp_type ==
        SFP_TYP_OPTICAL) {
        *is_optical = true;
        return 0;
    }
    if (sfp_state[module].sfp_type ==
        SFP_TYP_COPPER) {
        *is_optical = false;
        return 0;
    }

    if (sfp_type == SFP_TYP_OPTICAL) {
        sfp_state[module].sfp_type = SFP_TYP_OPTICAL;
        *is_optical = true;
    } else {
        sfp_state[module].sfp_type = SFP_TYP_COPPER;
        *is_optical = false;
    }
    return 0;
}

/*****************************************************************
 * Called by sfp_fsm. If SFP type has already been identified,
 * returns known type. If not, attempts to determine type based on
 * compliance and extended compliance fields.
 *****************************************************************/
bool sfp_fsm_is_optical (int module)
{
    if (sfp_state[module].sfp_type ==
        SFP_TYP_OPTICAL) {
        return true;
    }
    return false;
}
/*****************************************************************
 * Turn off optical Tx.
 *****************************************************************/
static void sfp_fsm_st_tx_off (
    bf_dev_id_t dev_id,
    uint32_t module)
{
    int err;
    fetch_by_module (module);

    err = bf_sfp_tx_disable (module, true);
    if (!err) {
        sfp_state[module].flags &= ~BF_TRANS_STATE_LASER_ON;
    }

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}

/*****************************************************************
 * Turn on optical Tx.
 *****************************************************************/
static void sfp_fsm_st_tx_on (
    bf_dev_id_t dev_id,
    uint32_t module)
{
    int err;
    fetch_by_module (module);

    err = bf_sfp_tx_disable (module, false);
    if (!err) {
        sfp_state[module].flags |= BF_TRANS_STATE_LASER_ON;
    }
    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}

/*****************************************************************
 * A removal detected and confirmed, then notify the SDK driver
 * that the transceiver and then disable optical Tx.
 *****************************************************************/
static void sfp_fsm_st_removed (bf_dev_id_t
                                dev_id, int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    // reset type
    sfp_state[module].sfp_type = SFP_TYP_UNKNOWN;
    // reset alarm states
    sfp_state[module].status_and_alarms =
        sfp_alarms_initial_state;

    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);

    // deassert reset, in case module was removed while reset was asserted
    sfp_fsm_reset_de_assert (dev_id, module);

    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    false);
    bf_port_is_optical_xcvr_set (dev_id, dev_port,
                                 false);
    bf_port_optical_los_set (dev_id, dev_port, true);

    sfp_fsm_st_tx_off (dev_id, module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);

}

/*****************************************************************
 * A insertion detected and confirmed, reset it.
 *****************************************************************/
static void sfp_fsm_st_inserted (bf_dev_id_t
                                 dev_id, int module)
{
    fetch_by_module (module);

    sfp_fsm_reset_assert (dev_id, module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}

/*****************************************************************
 * Stopping SFP fsm when enable a port via port-dis.
 *****************************************************************/
void sfp_fsm_st_disable (bf_dev_id_t dev_id,
                         int module)
{
    sfp_fsm_ch_en_state_t prev_st =
        sfp_state[module].fsm_ch_st;
    fetch_by_module (module);
    if (sfp_state[module].fsm_ch_st !=
        SFP_CH_FSM_ST_DISABLING) {
        LOG_DEBUG (" SFPCSM %2d : (%02d/%d) %s -->  %s (%dms)",
                   module, conn, chnl,
                   sfp_ch_fsm_en_st_to_str[prev_st],
                   sfp_ch_fsm_en_st_to_str[SFP_CH_FSM_ST_DISABLING],
                   0);
        sfp_state[module].fsm_ch_st =
            SFP_CH_FSM_ST_DISABLING;
        sfp_fsm_next_run_time_set (
            &sfp_state[module].next_fsm_run_time,
            0);
    }
}

/*****************************************************************
 * Starting SFP fsm when enable a port via port-enb.
 * This routine is a part of bf_pm_qsfp_mgmt_cb called by SDK.
 *****************************************************************/
void sfp_fsm_st_enable (bf_dev_id_t dev_id,
                        int module)
{
    sfp_fsm_ch_en_state_t prev_st =
        sfp_state[module].fsm_ch_st;
    fetch_by_module (module);
    if (sfp_state[module].fsm_ch_st !=
        SFP_CH_FSM_ST_ENABLING) {
        LOG_DEBUG (" SFPCSM %2d : (%02d/%d) %s -->  %s (%dms)",
                   module, conn, chnl,
                   sfp_ch_fsm_en_st_to_str[prev_st],
                   sfp_ch_fsm_en_st_to_str[SFP_CH_FSM_ST_ENABLING],
                   0);
        sfp_state[module].fsm_ch_st =
            SFP_CH_FSM_ST_ENABLING;
        sfp_fsm_next_run_time_set (
            &sfp_state[module].next_fsm_run_time,
            0);
    }
}

/*****************************************************************
 * Disables the port, and marks it to immediately enable once
 * it have completed DISABLING
 *****************************************************************/
void sfp_log_port_retry (bf_dev_id_t dev_id,
                          int module)
{
    sfp_state[module].immediate_enable
        = true;
    sfp_fsm_st_disable (dev_id, module);
}

/*****************************************************************
 * Module FSM
 *****************************************************************/
static void sfp_module_fsm_run (bf_dev_id_t
                                dev_id, int module)
{
    sfp_fsm_state_t st = sfp_state[module].fsm_st;
    sfp_fsm_state_t next_st = st;
    int rc, delay_ms = 0;
    bool is_optical = false;
    fetch_by_module (module);

    uint32_t ctrlmask = 0;

    bf_sfp_ctrlmask_get(module, &ctrlmask);

    switch (st) {
        case SFP_FSM_ST_IDLE:
            break;

        case SFP_FSM_ST_REMOVED:
            sfp_fsm_st_removed (dev_id, module);
            next_st = SFP_FSM_ST_IDLE;
            delay_ms = 0;

            sfp_state[module].sfp_quick_removed = false;
            sfp_state[module].flags = 0;

            /* If soft removal detected, try to turn off laser. */
            if (bf_sfp_soft_removal_get(module)) {
                sfp_fsm_st_tx_off (dev_id, module);
            }
            break;

        case SFP_FSM_ST_INSERTED:
            rc = sfp_fsm_identify_type (module, &is_optical);
            if ((rc != 0) ||
                (sfp_state[module].sfp_type == SFP_TYP_UNKNOWN)) {
                // error identifying type. Stay in this state
                next_st = SFP_FSM_ST_INSERTED;
                fprintf (stdout,
                         " SFP    %2d : (%02d/%d) Unknown SFP type\n",
                         module, conn, chnl);
                delay_ms = 1000;  // try again in 1sec
                break;
            }
            sfp_state[module].sfp_quick_removed = false;
            if (is_optical) {
                /* Insert detected, make sure the fsm_ch_st is DISABLED. */
                sfp_state[module].fsm_ch_st = SFP_CH_FSM_ST_DISABLED;
                sfp_state[module].los_detected = true;
                sfp_fsm_st_inserted (dev_id, module);

                next_st = SFP_FSM_ST_WAIT_T_RESET;
                delay_ms = 2;
            } else {
                fprintf (stdout,
                         " SFP    %2d : (%02d/%d) Notify platform ...\n",
                         module, conn, chnl);
                /* Every thing ready, notify barefoot core to start PM FSM. */
                sfp_fsm_notify_bf_pltfm (dev_id, module);

                next_st = SFP_FSM_ST_DETECTED;
                delay_ms = 0;
            }
            /* */
            break;

        case SFP_FSM_ST_WAIT_T_RESET:
            sfp_fsm_reset_de_assert (dev_id, module);
            next_st = SFP_FSM_ST_WAIT_TON_TXDIS;
            delay_ms = SFF8472_TMR_t_reset;
            break;

        case SFP_FSM_ST_WAIT_TON_TXDIS:
            /* Disable Tx */
            sfp_fsm_st_tx_off (dev_id, module);

            /* Every thing ready, notify barefoot core to start PM FSM. */
            sfp_fsm_notify_bf_pltfm (dev_id, module);

#if defined (DEFAULT_LASER_ON)
            next_st = SFP_FSM_ST_WAIT_TOFF_TXDIS;
#else
            next_st = SFP_FSM_ST_DETECTED;
#endif
            delay_ms = SFF8472_TMR_ton_txdis;
            break;

        case SFP_FSM_ST_WAIT_TOFF_TXDIS:
            if (ctrlmask & BF_TRANS_CTRLMASK_LASER_OFF) {
                /* Keep current state till the user clears the BF_TRANS_CTRLMASK_LASER_OFF bits,
                 * then FSM will issue SFP_FSM_ST_DETECTED stage. */
                next_st = SFP_FSM_ST_WAIT_TOFF_TXDIS;
                break;
            }

            /* Enable Tx */
            sfp_fsm_st_tx_on (dev_id, module);

            next_st = SFP_FSM_ST_DETECTED;
            delay_ms = SFF8472_TMR_toff_txdis;
            break;

        case SFP_FSM_ST_DETECTED:
            bf_sfp_set_detected (module, true);
            // check LOS
            if (bf_pm_intf_is_device_family_tofino (dev_id)) {
                if (sfp_fsm_is_optical (module) &&
                    (bf_sfp_get_memmap_format (module) ==
                     MMFORMAT_SFF8472)) {
                    if (sfp_fsm_poll_los (dev_id, module) != 0) {
                        if (sfp_state[module].sfp_quick_removed) {
                            // keep breaking
                            break;
                        }
                    }
                }
            }

            next_st = SFP_FSM_ST_DETECTED;
            delay_ms = 200; // 200ms poll time

#if 0
            if (ctrlmask & BF_TRANS_CTRLMASK_LASER_OFF) {
                if ((sfp_state[module].flags & BF_TRANS_STATE_LASER_ON)) {
                    /* User turns off Tx. */
                    sfp_fsm_st_tx_off (dev_id, module);
                }
            } else {
                if (!(sfp_state[module].flags & BF_TRANS_STATE_LASER_ON)) {
                    /* User turns on Tx. */
                    sfp_fsm_st_tx_on (dev_id, module);
                }
            }
#endif
            break;

        case SFP_FSM_ST_UPDATE:
            next_st = SFP_FSM_ST_WAIT_UPDATE;
            delay_ms = 0;
            break;

        case SFP_FSM_ST_WAIT_UPDATE:
            next_st = SFP_FSM_ST_DETECTED;
            delay_ms = 0;
            break;

        default:
            assert (0);
            break;
    }

    sfp_fsm_next_run_time_set (
        &sfp_state[module].next_fsm_run_time, delay_ms);

    if (st != next_st) {
        sfp_state[module].fsm_st = next_st;
        LOG_DEBUG (" SFPMSM %2d : (%02d/%d) %s --> %s (%dms)",
                   module, conn, chnl,
                   sfp_fsm_st_to_str[st],
                   sfp_fsm_st_to_str[next_st],
                   delay_ms);
    }
}

/*****************************************************************
 * Module CH FSM
 *****************************************************************/
static void sfp_channel_fsm_run (bf_dev_id_t
                                 dev_id, int module)
{
    sfp_fsm_ch_en_state_t st =
        sfp_state[module].fsm_ch_st;
    sfp_fsm_ch_en_state_t next_st = st;
    int delay_ms = 0;
    bool ok = true;
    fetch_by_module (module);

    switch (st) {
        case SFP_CH_FSM_ST_DISABLED:
            // waiting to start enabling
            break;

        case SFP_CH_FSM_ST_ENABLING:
            // initalize/re-initialize variables used in ch_fsm
            sfp_state[module].immediate_enable = false;

            // notify driver
            sfp_fsm_notify_not_ready (dev_id, module);
#if defined (DEFAULT_LASER_ON)
            next_st = SFP_CH_FSM_ST_NOTIFY_ENABLED;
#else
            next_st = SFP_CH_FSM_ST_ENA_OPTICAL_TX;
#endif
            delay_ms = 200;
            break;

        case SFP_CH_FSM_ST_ENA_OPTICAL_TX:
            /* Enable Tx */
            sfp_fsm_st_tx_on (dev_id, module);

            next_st = SFP_CH_FSM_ST_NOTIFY_ENABLED;
            delay_ms = SFF8472_TMR_toff_txdis;
            break;

        case SFP_CH_FSM_ST_NOTIFY_ENABLED:
            // Vendor specific case handle.
            sfp_fsm_special_case_handled (dev_id, module);
            /* Try to do more check on transceiver. */
            if (!ok) {
                // we had a fault in the tx, take all lanes in the logical port back
                // through the ch_fsm, starting in ENABLING
                sfp_log_port_retry (dev_id, module);
                next_st = SFP_CH_FSM_ST_DISABLING;
            } else {
                /* Notify BF that the SFP is ready for bringup.
                 * Try to detect the los. */
                sfp_fsm_notify_ready (dev_id, module);
                next_st = SFP_CH_FSM_ST_ENABLED;
                delay_ms = 200;
            }
            break;

        case SFP_CH_FSM_ST_ENABLED:
            delay_ms = 200;
            break;

        case SFP_CH_FSM_ST_DISABLING:
            sfp_fsm_notify_not_ready (dev_id, module);

#if defined (DEFAULT_LASER_ON)
            ; /* Do nothing. */
#else
            /* Disable Tx */
            sfp_fsm_st_tx_off (dev_id, module);
#endif
            if (sfp_state[module].immediate_enable) {
                next_st = SFP_CH_FSM_ST_ENABLING;
            } else {
                next_st = SFP_CH_FSM_ST_DISABLED;
            }
            delay_ms = SFF8472_TMR_ton_txdis;
            break;

        default:
            assert (0);
            break;
    }

    sfp_fsm_next_run_time_set (
        &sfp_state[module].next_fsm_ch_run_time, delay_ms);

    if (st != next_st) {
        sfp_state[module].fsm_ch_st = next_st;
        LOG_DEBUG (" SFPCSM %2d : (%02d/%d) %s --> %s (%dms)",
                   module, conn, chnl,
                   sfp_ch_fsm_en_st_to_str[st],
                   sfp_ch_fsm_en_st_to_str[next_st],
                   delay_ms);
    }
}

/*****************************************************************
 *
 *****************************************************************/
bf_pltfm_status_t sfp_fsm (bf_dev_id_t dev_id)
{
    int module;
    struct timespec now;
    static struct timespec next_temper_monitor_time = {0, 0};

    clock_gettime (CLOCK_MONOTONIC, &now);

    if (bf_pltfm_mgr_ctx()->flags && AF_PLAT_MNTR_SFP_REALTIME_DDM) {
        if (next_temper_monitor_time.tv_sec == 0) {
            sfp_fsm_next_run_time_set (
                &next_temper_monitor_time, 0);
        }
    }

    /*
     * Note: the channel FSM is run before the module FSM so any coalesced
     * writes are performed immediately. Otherwise they would have to wait
     * one extra poll period.
     */
    for (module = 1;
         module <= bf_sfp_get_max_sfp_ports(); module++) {
        if ((sfp_state[module].fsm_st ==
            SFP_FSM_ST_DETECTED) ||
            (sfp_state[module].fsm_st ==
              SFP_FSM_ST_UPDATE)) {

            if (!sfp_fsm_is_optical (module)) {
                //LOG_DEBUG (" SFP    %2d : is not optical\n",
                //         module);
                continue;
            }

            // skip terminal states
            if (sfp_state[module].fsm_ch_st ==
                SFP_CH_FSM_ST_DISABLED) {
                continue;
            }
            if (sfp_state[module].wr_inprog) {
                continue;
            }

            //if (sfp_state[module].fsm_st == SFP_CH_FSM_ST_ENABLED)
            //  continue;

            if (sfp_state[module].next_fsm_ch_run_time.tv_sec >
                now.tv_sec) {
                continue;
            }
            if ((sfp_state[module].next_fsm_ch_run_time.tv_sec ==
                 now.tv_sec) &&
                (sfp_state[module].next_fsm_ch_run_time.tv_nsec >
                 now.tv_nsec)) {
                continue;
            }
            // time to run this SFP (channel) state
            sfp_channel_fsm_run (dev_id, module);
        }
    }

    for (module = 1;
         module <= bf_sfp_get_max_sfp_ports(); module++) {
        if (sfp_state[module].fsm_st != SFP_FSM_ST_IDLE) {
            if (sfp_state[module].next_fsm_run_time.tv_sec >
                now.tv_sec) {
                continue;
            }
            if ((sfp_state[module].next_fsm_run_time.tv_sec ==
                 now.tv_sec) &&
                (sfp_state[module].next_fsm_run_time.tv_nsec >
                 now.tv_nsec)) {
                continue;
            }
            // time to run this SFP (module) state
            sfp_module_fsm_run (dev_id, module);
        }
    }

/* A thread called health monitor is charging for this. by tsihang, 2023-05-18. */
#if 0
    if ((next_temper_monitor_time.tv_sec < now.tv_sec) ||
      ((next_temper_monitor_time.tv_sec == now.tv_sec) &&
       (next_temper_monitor_time.tv_nsec < now.tv_nsec))) {
         sfp_fsm_next_run_time_set (
             &next_temper_monitor_time,
             temper_monitor_interval_ms);
         for (module = 1; module <= bf_pm_num_sfp_get();
              module++) {
             /* We hope this will help a lot to the boot for a new module.
              * Skip monitor during QSFP/CH FSM running in significant STATE.
              * by tsihang, 2023-05-18. */
             if (!temper_monitor_enable) break;
             // type checking also ensures module out of reset
             // But fsm may reset the module
             if ((!bf_bd_is_this_port_internal (module, 0)) &&
                 (!bf_sfp_get_reset (module)) &&
                 (sfp_fsm_is_optical(module)) &&
                 ((sfp_state[module].fsm_st == SFP_FSM_ST_DETECTED))) {
                  if ((sfp_state[module].fsm_ch_st !=
                        SFP_CH_FSM_ST_ENABLED) &&
                    (sfp_state[module].fsm_ch_st !=
                        SFP_CH_FSM_ST_DISABLED)) {
                        //LOG_DEBUG (" SFP    %2d : Skip temper monitor as CH in %s ",
                        //            module, sfp_channel_fsm_st_get(module));
                        continue;
                  }

                  sfp_global_sensor_t glob_sensor;
                  sfp_channel_t chnl_sensor;

                  bf_sfp_get_chan_tx_bias(module, &chnl_sensor);
                  bf_sfp_get_chan_tx_pwr(module, &chnl_sensor);
                  bf_sfp_get_chan_rx_pwr(module, &chnl_sensor);
                  bf_sfp_get_chan_temp(module, &glob_sensor);
                  bf_sfp_get_chan_volt(module, &glob_sensor);

                 if (temper_monitor_log_enable) {
                     LOG_DEBUG (
                                "      %d %10.1f %15.2f %10.2f %16.2f %16.2f",
                                module,
                                glob_sensor.temp.value,
                                glob_sensor.vcc.value,
                                chnl_sensor.sensors.tx_bias.value,
                                chnl_sensor.sensors.tx_pwr.value,
                                chnl_sensor.sensors.rx_pwr.value);
                }
            }
        }
    }
#endif
    return BF_PLTFM_SUCCESS;
}

void bf_port_sfp_mgmnt_temper_monitor_period_set (
    uint32_t poll_intv_sec)
{
    temper_monitor_interval_ms = poll_intv_sec * 1000;
}

uint32_t bf_port_sfp_mgmnt_temper_monitor_period_get (
    void)
{
    return temper_monitor_interval_ms / 1000;
}

bool bf_port_sfp_mgmnt_temper_high_alarm_flag_get (
    int port)
{
    //return qsfp_state[port].temper_high_alarm_flagged;
    return 0;
}

// return recorded temperature during module high-alarm
double bf_port_sfp_mgmnt_temper_high_record_get (
    int port)
{
    //return qsfp_state[port].temper_high_record;
    return 0;
}
