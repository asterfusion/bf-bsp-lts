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

#include <bf_types/bf_types.h>
#include <bfsys/bf_sal/bf_sys_sem.h>
#include <bfsys/bf_sal/bf_sys_timer.h>

#include <tofino/bf_pal/bf_pal_pltfm_porting.h>
#include <bf_pm/bf_pm_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_porting.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_pltfm_sfp.h>
#include "bf_pm_priv.h"

void sfp_fsm_notify_bf_pltfm (bf_dev_id_t dev_id,
                              int module);

/* SFF-8472 delay and timing for soft control and status functions and requirements.
 * by tsihang, 2021-07-29. */
typedef enum {
    SFF8472_TMR_t_reset_init = 1,  // really only 2us
    SFF8472_TMR_t_reset = 2000,    // all times in ms

    /* max delay in ms defined in sff-8472 table 8-7 -
     * I/O timeing for soft contol and status functions.
     * by tsihang, 2021-07-29.
     */
    SFF8472_TMR_ton_txdis = 100,
    SFF8472_TMR_toff_txdis = 100,

    SFF8472_TMR_ton_rxlos = 100,
    SFF8472_TMR_toff_rxlos = 100,
} sff8436_timer_val_t;

/* Module FSM states */
typedef enum {
    // QSFP unknown or unused on board
    SFP_FSM_ST_IDLE = 0,
    // pseudo-states, assigned by qsfp scan timer to communicate
    // with qsfp fsm
    SFP_FSM_ST_REMOVED,
    SFP_FSM_ST_INSERTED,
    // states requiring fixed delays
    SFP_FSM_ST_WAIT_T_RESET,      // 2000ms
    SFP_FSM_ST_WAIT_TON_TXDIS,    //  100ms, SFF8472_TMR_ton_txdis
    SFP_FSM_ST_DETECTED,
    SFP_FSM_ST_UPDATE,
    SFP_FSM_ST_WAIT_UPDATE,  // update-specific delay

    SFP_FSM_EN_ST_DISABLED,
    SFP_FSM_EN_ST_ENABLING,        // kick off enable sequence
    SFP_FSM_EN_ST_ENA_OPTICAL_TX,  // 100ms, SFF8472_TMR_toff_txdis
    SFP_FSM_EN_ST_NOTIFY_ENABLED,
    SFP_FSM_EN_ST_ENABLED,
    SFP_FSM_EN_ST_DISABLING,  // assert TX_DISABLE
} sfp_fsm_state_t;

static char *sfp_fsm_st_to_str[] = {
    "SFP_FSM_ST_IDLE               ",
    "SFP_FSM_ST_REMOVED            ",
    "SFP_FSM_ST_INSERTED           ",
    "SFP_FSM_ST_WAIT_T_RESET       ",
    "SFP_FSM_ST_WAIT_TON_TXDIS     ",
    "SFP_FSM_ST_DETECTED           ",
    "SFP_FSM_ST_UPDATE             ",
    "SFP_FSM_ST_WAIT_UPDATE        ",

    "SFP_FSM_EN_ST_DISABLED        ",
    "SFP_FSM_EN_ST_ENABLING        ",
    "SFP_FSM_EN_ST_ENA_OPTICAL_TX  ",
    "SFP_FSM_EN_ST_NOTIFY_ENABLED  ",
    "SFP_FSM_EN_ST_ENABLED         ",
    "SFP_FSM_EN_ST_DISABLING       ",
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
    struct timespec next_fsm_run_time;
    bool sfp_quick_removed;

    sfp_status_and_alarms_t status_and_alarms;

} sfp_state_t;

/* access is 1 based index, so zeroth index would be unused */
sfp_state_t sfp_state[BF_PLAT_MAX_SFP + 1] = {
    {
        SFP_TYP_UNKNOWN,
        SFP_FSM_ST_IDLE,
        {0, 0},
        false,

        sfp_status_and_alarm_init_val
    }
};

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

/****************************************************
*
****************************************************/
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

/****************************************************
*
****************************************************/
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
* Make sure that the poll_los API read data as less as possible
* thru i2c bus.
*****************************************************************/
static int sfp_fsm_poll_los (bf_dev_id_t dev_id,
                             int module, bool *los)
{
    int rc;
    bool has_a2h = true;    /* Please fixed it. */
    sfp_status_and_alarms_t status_and_alarms;

    // Donot move FSM until removal is hanlded
    if (sfp_state[module].sfp_quick_removed) {
        return -1;
    }

    if (!has_a2h) {
        *los = false;
        return 0;
    }

    // read status and alarms A2h (bytes 110-113) onto stack. This is in case
    // there is an error on the read we dont corrupt the sfp_state
    // structure. If no error, copy into struct
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

    // copy new alarm state into out cached struct
    sfp_state[module].status_and_alarms =
        status_and_alarms;

    /* Not very sure, but very helpful. */
    return bf_pltfm_sfp_los_read (module, los);
}

static void sfp_fsm_get_speed (bf_dev_id_t dev_id,
                               int module, bf_port_speed_t *speed)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;
    *speed = BF_SPEED_NONE;
    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);
    bf_port_speed_get (dev_id, dev_port, speed);

    return;
}

static int sfp_fsm_special_case_handled (
    bf_dev_id_t dev_id,
    int module)
{
    // define
    int rc = 0;
    bf_port_speed_t speed;
    uint8_t buf;
    uint32_t conn, chnl;

    do {
        if (bf_sfp_get_conn (
                (module),
                &conn,
                &chnl)) {
            rc = -1;
            break;
        }

        if (check_sfp_module_vendor (conn, chnl,
                                     "TRIXON") == 0) {
            //if vendor is Trixon
            // get port speed
            sfp_fsm_get_speed (dev_id, module, &speed);
            // set module speed
            if (speed == BF_SPEED_10G) {
                buf = 0x10;
                if (bf_pltfm_sfp_write_module (module,
                                               MAX_SFP_PAGE_SIZE + 0x79, 1, &buf)) {
                    rc = -2;
                    break;
                }
            } else if (speed == BF_SPEED_25G) {
                buf = 0x00;
                if (bf_pltfm_sfp_write_module (module,
                                               MAX_SFP_PAGE_SIZE + 0x79, 1, &buf)) {
                    rc = -2;
                    break;
                }
            } else {
                rc = -3;
                break;
            }
        } // some other vendors
    } while (0);

    return rc;
}

static void sfp_fsm_reset_assert (bf_dev_id_t
                                  dev_id,
                                  int module)
{
    int rc;
    fetch_by_module (module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) RESETL = true",
               module);
    // assert resetL
    rc = bf_sfp_reset (module, true);
    if (rc != 0) {
        LOG_ERROR (" SFP    %2d : (%02d/%d) Error <%d> asserting resetL",
                   module, conn, chnl, rc);
    }
}

/*****************************************************************
*
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
*
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
*
*****************************************************************/
void sfp_fsm_enable (bf_dev_id_t dev_id,
                     int module)
{
    fetch_by_module (module);
    dev_id = dev_id;

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module,
               conn, chnl, __func__);
}


/*****************************************************************
*
*****************************************************************/
void sfp_fsm_disable (bf_dev_id_t dev_id,
                      int module)
{
    fetch_by_module (module);
    dev_id = dev_id;

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module,
               conn, chnl, __func__);
}

void sfp_fsm_inserted (int module)
{
    fetch_by_module (module);
    //sfp_fsm_reset_assert (module);
    bf_sfp_set_present (module, true);

    sfp_fsm_state_t prev_st =
        sfp_state[module].fsm_st;

    sfp_state[module].fsm_st = SFP_FSM_ST_INSERTED;
    sfp_state[module].next_fsm_run_time.tv_sec = 0;
    sfp_state[module].next_fsm_run_time.tv_nsec = 0;

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ->  SFP_FSM_ST_INSERTED",
               module, conn, chnl,
               sfp_fsm_st_to_str[prev_st]);
}

void sfp_fsm_removed (int module)
{
    fetch_by_module (module);

    bf_sfp_set_present (module, false);

    sfp_fsm_state_t prev_st =
        sfp_state[module].fsm_st;

    sfp_state[module].next_fsm_run_time.tv_sec = 0;
    sfp_state[module].next_fsm_run_time.tv_nsec = 0;

    sfp_state[module].fsm_st = SFP_FSM_ST_REMOVED;
    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ->  SFP_FSM_ST_REMOVED",
               module, conn, chnl,
               sfp_fsm_st_to_str[prev_st]);
}

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
    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    true);
    /* I have no idea where the QSFP FSM call bringup API.
     * I have to call it here to bring the port up.
     * by tishang, 2021-07-30. */
    bf_pal_pm_front_port_ready_for_bringup (
        dev_id, &port_hdl, true);

    LOG_DEBUG (
        " SFP    %2d : (%02d/%d) Ready",
        module, conn, chnl);
}

void sfp_fsm_notify_not_ready (bf_dev_id_t dev_id,
                               int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);


    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    false);
    bf_port_optical_los_set (dev_id, dev_port, true);

    LOG_DEBUG (
        " SFP    %2d : (%02d/%d) Not Ready",
        module, conn, chnl);

}

/****************************************************
* called by qsfp_fsm. If QSFP type has already been
* identified, returns known type. If not, attempts
* to determine type based on compliance and
* extended compliance fields.
* If any error determining type return -1
****************************************************/
static int sfp_fsm_identify_type (int module,
                                  bool *is_optical)
{
    bf_pltfm_qsfp_type_t sfp_type = 0;

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

    if (sfp_type == BF_PLTFM_QSFP_OPT) {
        sfp_state[module].sfp_type = SFP_TYP_OPTICAL;
        *is_optical = true;
    } else {
        sfp_state[module].sfp_type = SFP_TYP_COPPER;
        *is_optical = false;
    }
    return 0;
}

/****************************************************
* called by qsfp_fsm. If QSFP type has already been
* identified, returns known type. If not, attempts
* to determine type based on compliance and
* extended compliance fields.
****************************************************/
bool sfp_fsm_is_optical (int module)
{
    if (sfp_state[module].sfp_type ==
        SFP_TYP_OPTICAL) {
        return true;
    }
    return false;
}

static void sfp_fsm_st_removed (bf_dev_id_t
                                dev_id, int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    // reset type
    sfp_state[module].sfp_type = SFP_TYP_UNKNOWN;

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

    bf_pltfm_sfp_module_tx_disable (module, true);

    bf_pal_pm_front_port_ready_for_bringup (dev_id,
                                            &port_hdl, false);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);

}

static void sfp_fsm_st_inserted (bf_dev_id_t
                                 dev_id, int module)
{
    fetch_by_module (module);

    sfp_fsm_reset_assert (dev_id, module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}

static void sfp_fsm_st_tx_off (
    bf_dev_id_t dev_id,
    uint32_t module)
{
    fetch_by_module (module);

    bf_pltfm_sfp_module_tx_disable (module, true);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}

static void sfp_fsm_st_tx_on (
    bf_dev_id_t dev_id,
    uint32_t module)
{
    fetch_by_module (module);

    bf_pltfm_sfp_module_tx_disable (module, false);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}


void sfp_fsm_st_disable (bf_dev_id_t dev_id,
                         int module)
{
    fetch_by_module (module);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);
}


void sfp_fsm_st_enable (bf_dev_id_t dev_id,
                        int module)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;

    fetch_by_module (module);

    port_hdl.conn_id = conn;
    port_hdl.chnl_id = chnl;
    FP2DP (dev_id, &port_hdl, &dev_port);

    bf_port_is_optical_xcvr_set (dev_id, dev_port,
                                 true);
    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    true);
    bf_port_optical_los_set (dev_id, dev_port, false);
    bf_pltfm_sfp_module_tx_disable (module, false);

    LOG_DEBUG (" SFP    %2d : (%02d/%d) %s ...",
               module, conn, chnl, __func__);

    return;

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

    switch (st) {
        case SFP_FSM_ST_IDLE:
            break;

        case SFP_FSM_ST_REMOVED:
            sfp_fsm_st_removed (dev_id, module);
            next_st = SFP_FSM_ST_IDLE;
            delay_ms = 0;

            sfp_state[module].sfp_quick_removed = false;
            break;

        case SFP_FSM_ST_INSERTED:
            rc = sfp_fsm_identify_type (module, &is_optical);
            if ((rc != 0) ||
                (sfp_state[module].sfp_type == SFP_TYP_UNKNOWN)) {
                // error identifying type. Stay in this state
                next_st = SFP_FSM_ST_INSERTED;
                fprintf (stdout,
                         " SFP    %2d : (%02d/%d) Unknown sfp type\n",
                         module, conn, chnl);
                delay_ms = 1000;  // try again in 1sec
                break;
            }
            sfp_state[module].sfp_quick_removed = false;
            if (is_optical) {
                sfp_fsm_st_inserted (dev_id, module);

                next_st = SFP_FSM_ST_WAIT_T_RESET;
                delay_ms = 2;
            } else {
                fprintf (stdout,
                         " SFP    %2d : (%02d/%d) Notify platform ...\n",
                         module, conn, chnl);
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
            next_st = SFP_FSM_ST_DETECTED;
            delay_ms = SFF8472_TMR_ton_txdis;
            break;

        case SFP_FSM_ST_DETECTED:
            /* So far, just step over this state. */
            // Vendor specific case handle.
            sfp_fsm_special_case_handled (dev_id, module);
            next_st = SFP_FSM_EN_ST_ENA_OPTICAL_TX;
            delay_ms = 200;
            break;

        case SFP_FSM_EN_ST_ENA_OPTICAL_TX:
            /* Enable Tx */
            sfp_fsm_st_tx_on (dev_id, module);
            next_st = SFP_FSM_EN_ST_ENABLING;
            delay_ms = SFF8472_TMR_toff_txdis;
            break;

        case SFP_FSM_EN_ST_ENABLING:
            next_st = SFP_FSM_EN_ST_NOTIFY_ENABLED;
            delay_ms = 200;
            if (bf_pm_intf_is_device_family_tofino (dev_id)) {
                if (sfp_fsm_is_optical (module)) {
                    bool los = false;
                    if (sfp_fsm_poll_los (dev_id, module,
                                          &los) != 0) {
                        if (sfp_state[module].sfp_quick_removed) {
                            // keep breaking
                            LOG_WARNING (" SFP    %2d : (%02d/%d) %s ... Kick off",
                                         module, conn, chnl, __func__);
                            /* Maybe should to SFP_FSM_ST_REMOVED */
                            next_st = SFP_FSM_EN_ST_DISABLING;
                            break;
                        }
                    } else {
                        if (los) {
                            /* los detect, so we should notify SDE that the connection is not ready
                             * due to the reason from peer. Then, SDE could step into sigok state in PM FSM. */
                            /* TODO */
                            next_st = SFP_FSM_EN_ST_ENABLING;
                        }
                    }
                }
            }
            break;

        case SFP_FSM_EN_ST_NOTIFY_ENABLED:
            /* Notify BF that the SFP is ready for bringup.
             * Try to detect the los. */
            sfp_fsm_notify_ready (dev_id, module);
            next_st = SFP_FSM_EN_ST_ENABLED;
            delay_ms = 200;
            break;

        case SFP_FSM_EN_ST_ENABLED:
            next_st = SFP_FSM_EN_ST_ENABLED;
            delay_ms = SFF8472_TMR_ton_rxlos;
            if (bf_pm_intf_is_device_family_tofino (dev_id)) {
                if (sfp_fsm_is_optical (module)) {
                    bool los = false;
                    if (sfp_fsm_poll_los (dev_id, module,
                                          &los) != 0) {
                        if (sfp_state[module].sfp_quick_removed) {
                            // keep breaking
                            LOG_WARNING (" SFP    %2d : (%02d/%d) %s ... Kick off",
                                         module, conn, chnl, __func__);
                            /* Maybe should to SFP_FSM_ST_REMOVED */
                            next_st = SFP_FSM_EN_ST_DISABLING;
                            break;
                        }
                    } else {
                        if (los) {
                            /* los detect, so we should notify SDE that the connection is not ready
                             * due to the reason from peer. Then, SDE could step into sigok state in PM FSM. */
                            /* TODO */
                            next_st = SFP_FSM_EN_ST_DISABLING;
                        }
                    }
                }
            }
            break;

        case SFP_FSM_EN_ST_DISABLING:
            sfp_fsm_notify_not_ready (dev_id, module);
            next_st = SFP_FSM_EN_ST_DISABLED;
            delay_ms = 200;
            break;

        case SFP_FSM_EN_ST_DISABLED:
            /* Waiting for enabling, or return back to enabling to detect los.*/
            next_st = SFP_FSM_EN_ST_ENABLING;
            delay_ms = 200;
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

    sfp_state[module].fsm_st = next_st;
    sfp_fsm_next_run_time_set (
        &sfp_state[module].next_fsm_run_time, delay_ms);

    if (st != next_st) {
        LOG_DEBUG (" SFP    %2d : (%02d/%d) %s --> %s (%dms)",
                   module, conn, chnl,
                   sfp_fsm_st_to_str[st],
                   sfp_fsm_st_to_str[next_st],
                   delay_ms);
    }
}

bf_pltfm_status_t sfp_fsm (bf_dev_id_t dev_id)
{
    int module;
    struct timespec now;

    clock_gettime (CLOCK_MONOTONIC, &now);

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
    return BF_PLTFM_SUCCESS;
}

