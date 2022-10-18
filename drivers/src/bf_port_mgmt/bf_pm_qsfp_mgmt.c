/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

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
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>
#include "bf_pm_priv.h"

/*****************************************************************************
  QSFP Mgmt

  This code enforces timing constraints imposed by SFF-8436.


  It implements the following bring-up sequence for optical QSFPs. The
  goal is to place the QSFP into a known, stable state with their CDRs locked
  to a valid electrical signal from Tofino and the lasers OFF.

                        t_reset
               _________       _________________________________
  RESETL                \_____/  t_reset_init---|
                           _____________________________________
  LPMODE (hw)  ___________/
                                                   _____________ SW Override
LPMODE
  LPMODE (sw)  XXXXXXXX\__________________________/
                                               _________________
  TX_DISABLE   XXXXXXXX\______________________/

  Serdes TX   XXXXXXXXXXXXXX/ PRBS31 ...

  First, the module is reset using the external resetL pin.
  LPMODE is then asserted, and the Tofino serdes are configured to transmit a
  PRBS31 pattern with pre-determined (board-specific) Tx EQ settings.
  Next, resetL is released and the QSFP CDRs execute their internal tuning.
  After t_reset_init, the QSFP can be accessed by SW. The TX_DISABLEs
  are asserted and Power Control is set to override the LPMODE pin. Since
  TX_DISABLE is asserted no optical signal will be transmitted.


*****************************************************************************/

static void qsfp_module_fsm_complete_update (
    bf_dev_id_t dev_id, int conn_id);

/* SFF 8436 delay requirements */
typedef enum {
    SFF8436_TMR_t_reset_init = 1,  // really only 2us
    SFF8436_TMR_t_reset = 2000,    // all times in ms
    SFF8436_TMR_Toff_LPMode = 300,
    SFF8436_TMR_ton_txdis = 100,
    SFF8436_TMR_toff_txdis = 400,
} sff8436_timer_val_t;

/* Module FSM states */
typedef enum {
    // QSFP unknown or unused on board
    QSFP_FSM_ST_IDLE = 0,
    // pseudo-states, assigned by qsfp scan timer to communicate
    // with qsfp fsm
    QSFP_FSM_ST_REMOVED,
    QSFP_FSM_ST_INSERTED,
    // states requiring fixed delays
    QSFP_FSM_ST_WAIT_T_RESET,      // 2000ms
    QSFP_FSM_ST_WAIT_TON_TXDIS,    //  100ms
    QSFP_FSM_ST_WAIT_TOFF_LPMODE,  //  300ms
    QSFP_FSM_ST_DETECTED,
    QSFP_FSM_ST_UPDATE,
    QSFP_FSM_ST_WAIT_UPDATE,  // update-specific delay
} qsfp_fsm_state_t;

static char *qsfp_fsm_st_to_str[] = {
    "QSFP_FSM_ST_IDLE            ",
    "QSFP_FSM_ST_REMOVED         ",
    "QSFP_FSM_ST_INSERTED        ",
    "QSFP_FSM_ST_WAIT_T_RESET    ",
    "QSFP_FSM_ST_WAIT_TON_TXDIS  ",
    "QSFP_FSM_ST_WAIT_TOFF_LPMODE",
    "QSFP_FSM_ST_DETECTED        ",
    "QSFP_FSM_ST_UPDATE          ",
    "QSFP_FSM_ST_WAIT_UPDATE     ",
};

/* Channel FSM states */
typedef enum {
    QSFP_FSM_EN_ST_DISABLED = 0,
    QSFP_FSM_EN_ST_ENABLING,        // kick off enable sequence
    QSFP_FSM_EN_ST_ENA_CDR,         // 300ms (Luxtera PSM4)
    QSFP_FSM_EN_ST_ENA_OPTICAL_TX,  // 400ms, SFF8436_TMR_toff_txdis
    QSFP_FSM_EN_ST_NOTIFY_ENABLED,
    QSFP_FSM_EN_ST_ENABLED,
    QSFP_FSM_EN_ST_DISABLING,  // assert TX_DISABLE
} qsfp_fsm_ch_en_state_t;

static char *qsfp_fsm_ch_en_st_to_str[] = {
    "QSFP_FSM_EN_ST_DISABLED",
    "QSFP_FSM_EN_ST_ENABLING",
    "QSFP_FSM_EN_ST_ENA_CDR",
    "QSFP_FSM_EN_ST_ENA_OPTICAL_TX",
    "QSFP_FSM_EN_ST_NOTIFY_ENABLED",
    "QSFP_FSM_EN_ST_ENABLED",
    "QSFP_FSM_EN_ST_DISABLING",
};

typedef enum qsfp_typ_t {
    QSFP_TYP_UNKNOWN = 0,
    QSFP_TYP_OPTICAL = 1,
    QSFP_TYP_COPPER = 2,
} qsfp_typ_t;

#define BYTE2_FLAT_MEM (1 << 2)
#define BYTE2_INTL (1 << 1)
#define BYTE2_DATA_NOT_READY (1 << 0)

typedef struct qsfp_status_and_alarms_t {
    // Byte
    //  uint8_t identifier;     //  0. unused : identifier
    //  uint8_t status1;        //  1. unused : rev compliance
    uint8_t status2;         //  2. [2:0] = Flat_mem : IntL : Data_Not_Ready
    uint8_t los_ind;         //  3. [7:4] Tx LOS : [3:0] Rx LOS
    uint8_t eq_laser_fault;  //  4. [7:4] Tx Adapt Fault : [3:0] Tx laser fault
    uint8_t lol_ind;         //  5. [7:4] Tx LOL : [3:0] Rx LOL
    uint8_t temp_alarm;   //  6. [7:4] Temp alarm/warning : [0:0] init complete fl
    uint8_t vcc_alarm;    //  7. [7:4] Vcc alarm/warning
    uint8_t vendor_spec;  //  8. unused
    uint8_t rx_power_alarm12;  //  9. Rx power alarm/warning, Ch1 and Ch2
    uint8_t rx_power_alarm34;  // 10. Rx power alarm/warning, Ch3 and Ch4
    uint8_t tx_bias_alarm12;   // 11. Tx Bias  alarm/warning, Ch1 and Ch2
    uint8_t tx_bias_alarm34;   // 12. Tx Bias  alarm/warning, Ch3 and Ch4
    uint8_t tx_power_alarm12;  // 13. Rx power alarm/warning, Ch1 and Ch2
    uint8_t tx_power_alarm34;  // 14. Rx power alarm/warning, Ch3 and Ch4

} qsfp_status_and_alarms_t;

typedef struct qsfp_channel_fsm_t {
    qsfp_fsm_ch_en_state_t fsm_st;
    bool first_enable_after_reset;
    struct timespec next_fsm_run_time;
} qsfp_channel_fsm_t;

typedef struct qsfp_state_t {
    qsfp_typ_t qsfp_type;
    qsfp_fsm_state_t fsm_st;
    bool flat_mem;
    bool needs_hi_pwr_init;  // only Luxtera known to require this
    struct timespec next_fsm_run_time;

    qsfp_channel_fsm_t per_ch_fsm[4];

    /* structure used for coalescing writes to module
    * registers containing fields for multiple channels
    */
    struct {
        bool in_progress;
        int delay_ms;
        uint8_t pg;
        uint8_t ofs;
        uint8_t mask;
        uint8_t data;
    } wr_coalesce;
    qsfp_status_and_alarms_t status_and_alarms;
} qsfp_state_t;

qsfp_state_t qsfp_state[BF_PLAT_MAX_QSFP + 1] = {
    {
        QSFP_TYP_UNKNOWN,
        QSFP_FSM_ST_IDLE,
        false,
        false,
        {0, 0},
        {   {0, true, {0, 0}},
            {0, true, {0, 0}},
            {0, true, {0, 0}},
            {0, true, {0, 0}}
        },
        {false, 0, 0, 0, 0, 0},
        {0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    }
};

static qsfp_channel_fsm_t
qsfp_channel_fsm_initial_state = {0, true, {0, 0}};
static qsfp_status_and_alarms_t
qsfp_alarms_initial_state = {
    0, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// fwd ref
static int qsfp_num_lanes_get (int conn_id,
                               int chnl_id);

/****************************************************
*
****************************************************/
static int bf_fsm_qsfp_rd (unsigned int module,
                           int offset,
                           int len,
                           uint8_t *buf)
{
    int rc;

    rc = bf_qsfp_read_transceiver (module, offset,
                                   len, buf);

    // LOG_DEBUG("QSFP    %2d : Rd : offset=%2d : len=%3d : data[0]=%02x",
    //          module, offset, len, buf[0]);

    return rc;
}

/****************************************************
*
****************************************************/
static int bf_fsm_qsfp_wr (unsigned int module,
                           int offset,
                           int len,
                           uint8_t *buf)
{
    int rc;

    // LOG_DEBUG("QSFP    %2d : Wr : offset=%2d : len=%3d : data[0]=%02x",
    //          module, offset, len, buf[0]);

    rc = bf_qsfp_write_transceiver (module, offset,
                                    len, buf);
    return rc;
}

/****************************************************
* called by qsfp_fsm to set next time to process QSFP
****************************************************/
static void qsfp_fsm_next_run_time_set (
    struct timespec *next_run_time,
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

/****************************************************
* called by qsfp_scan to identify new QSFP to process
****************************************************/
void qsfp_fsm_inserted (int conn_id)
{
    if (conn_id > BF_PLAT_MAX_QSFP) {
        LOG_ERROR (
            "QSFPMSM %s : %2d conn_id exceeed max supported\n",
            __func__, conn_id);
        return;
    }

    qsfp_fsm_state_t prev_st =
        qsfp_state[conn_id].fsm_st;

    qsfp_state[conn_id].fsm_st = QSFP_FSM_ST_INSERTED;
    qsfp_state[conn_id].next_fsm_run_time.tv_sec = 0;
    qsfp_state[conn_id].next_fsm_run_time.tv_nsec = 0;

    qsfp_state[conn_id].status_and_alarms.los_ind =
        0xFF;  // all LOS bits
    LOG_DEBUG ("QSFP    %2d : %s -> QSFP_FSM_ST_INSERTED\n",
               conn_id,
               qsfp_fsm_st_to_str[prev_st]);
}

/****************************************************
* called by qsfp_scan to indicate QSFP no longer
* requires processing
****************************************************/
void qsfp_fsm_removed (int conn_id)
{
    qsfp_fsm_state_t prev_st =
        qsfp_state[conn_id].fsm_st;

    qsfp_state[conn_id].next_fsm_run_time.tv_sec = 0;
    qsfp_state[conn_id].next_fsm_run_time.tv_nsec = 0;

    qsfp_state[conn_id].fsm_st = QSFP_FSM_ST_REMOVED;
    LOG_DEBUG ("QSFP    %2d : %s -> QSFP_FSM_ST_REMOVED",
               conn_id,
               qsfp_fsm_st_to_str[prev_st]);
}

/****************************************************
*
****************************************************/
/*static*/ void qsfp_needs_hi_pwr_init_set (
    int conn_id, bool needs)
{
    qsfp_state[conn_id].needs_hi_pwr_init = needs;
}

/****************************************************
*
****************************************************/
static bool qsfp_needs_hi_pwr_init (int conn_id)
{
    return (qsfp_state[conn_id].needs_hi_pwr_init);
}

/****************************************************
*
****************************************************/
void qsfp_set_pg (int conn_id, uint8_t pg)
{
    if (qsfp_state[conn_id].flat_mem == false) {
        bf_fsm_qsfp_wr (conn_id, 127, 1, &pg);
        bf_sys_usleep (50000);
    }
}

/****************************************************
* called by qsfp_fsm. If QSFP type has already been
* identified, returns known type. If not, attempts
* to determine type based on compliance and
* extended compliance fields.
* If any error determining type return -1
****************************************************/
static int qsfp_fsm_identify_type (int conn_id,
                                   bool *is_optical)
{
    int rc;
    uint8_t status, page = 0;
    bf_pltfm_qsfp_type_t qsfp_type;

    if (qsfp_state[conn_id].qsfp_type ==
        QSFP_TYP_OPTICAL) {
        *is_optical = true;
        return 0;
    }
    if (qsfp_state[conn_id].qsfp_type ==
        QSFP_TYP_COPPER) {
        *is_optical = false;
        return 0;
    }

    // see if flat_mem or paged
    rc = bf_fsm_qsfp_rd (conn_id, 2, 1, &status);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading status (byte 2)",
                   conn_id, rc);
        return -1;
    }
    qsfp_state[conn_id].flat_mem = ((status >> 2) & 1)
                                   ? true : false;
    // set pg 0 (if neccessary)
    qsfp_set_pg (conn_id, page);

    // else, need to identify it
    if (bf_qsfp_type_get (conn_id, &qsfp_type) != 0) {
        /*100G Terspeak read tranceiver data err on sometimes, so need retry to update data*/
        int err = bf_qsfp_update_data(conn_id);
        if (err) {
            LOG_ERROR ("QSFP    %2d : Error bf_qsfp_update_data failure\n", conn_id);
            return -1;
        }
        // set pg 0 (if neccessary)
        qsfp_set_pg (conn_id, page);
        if (bf_qsfp_type_get (conn_id, &qsfp_type) != 0) {
            LOG_ERROR ("QSFP    %2d : Error determining type\n", conn_id);
            return -1;
        }
    }
    if (qsfp_type == BF_PLTFM_QSFP_OPT) {
        qsfp_state[conn_id].qsfp_type = QSFP_TYP_OPTICAL;
        *is_optical = true;
    } else {
        qsfp_state[conn_id].qsfp_type = QSFP_TYP_COPPER;
        *is_optical = false;
    }
    return 0;
}

/****************************************************
* Determine if QSFP has any special requirements.
* Based on a priori knowledge of QSFP Vendor/PN
****************************************************/
static int qsfp_fsm_identify_model_requirements (
    int conn_id)
{
    int rc;
    uint8_t vendor_name[17] = {0};
    uint8_t vendor_pn[17] = {0};

    rc = bf_fsm_qsfp_rd (conn_id, 148, 16,
                         vendor_name);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading vendor name (bytes 148-163)",
                   conn_id,
                   rc);
        return -1;
    }

    rc = bf_fsm_qsfp_rd (conn_id, 168, 16, vendor_pn);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading vendor Part Nbr (bytes 168-183)",
                   conn_id,
                   rc);
        return -1;
    }
    // "normalize" the ascii array contents
    for (int i = 0; i < 16; i++) {
        if (!isprint ((char)vendor_name[i])) {
            vendor_name[i] = 0;
        }
        if (!isprint ((char)vendor_pn[i])) {
            vendor_pn[i] = 0;
        }
    }
    vendor_name[16] = 0;
    vendor_pn[16] = 0;

    /* edit by tsihang, 2019.8.1.
     * Enable Teraspek QSFP detection and handling */
    if (strncmp ((char *)vendor_name, "Teraspek",
                 8) == 0) {
        if (strncmp ((char *)vendor_pn, "TSQ885S101E1",
                     12) == 0) {
            /*
            qsfp_needs_hi_pwr_init_set(
                conn_id,
                true);
            */
        }
    }

    if (strncmp ((char *)vendor_name, "LUXTERA",
                 7) == 0) {
        qsfp_needs_hi_pwr_init_set (conn_id, true);
    } else if (strncmp ((char *)vendor_name,
                        "Alibaba", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "AB-QT0613P4n",
                     12) ==
            0) {  // JT 6/12/18 : from alibaba_co_v23.hex
            qsfp_needs_hi_pwr_init_set (
                conn_id,
                true);  // JT 6/12/18 : enable Alibaba-Luxtera detection and handling
        }
    } else if (strncmp ((char *)vendor_name,
                        "Alibaba", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "AB-QT0613P4i03",
                     12) ==
            0) {  // JT 6/13/18 : from email Craig@Luxtera
            qsfp_needs_hi_pwr_init_set (
                conn_id,
                true);  // JT 6/13/18 : enable Alibaba-Luxtera detection and handling
        }
    } else if (strncmp ((char *)vendor_name,
                        "JUNIPER-LUXTERA", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "LUX42604BO",
                     12) == 0) {  // JT 6/13/18
            qsfp_needs_hi_pwr_init_set (conn_id,
                                        true);              // JT 6/13/18
        }
    } else if (strncmp ((char *)vendor_name,
                        "Arista Networks", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "QSFP-100G-PSM4",
                     12) == 0) {  // JT 6/13/18
            qsfp_needs_hi_pwr_init_set (conn_id,
                                        true);                  // JT 6/13/18
        }
    } else if (strncmp ((char *)vendor_name,
                        "CISCO-LUXTERA", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "LUX42604BO",
                     12) == 0) {  // JT 6/13/18
            qsfp_needs_hi_pwr_init_set (conn_id,
                                        true);              // JT 6/13/18
        }
    } else if (strncmp ((char *)vendor_name,
                        "CISCO-LUXTERA", 7) == 0) {
        if (strncmp ((char *)vendor_pn, "LUX42604BOC",
                     12) == 0) {  // JT 6/13/18
            qsfp_needs_hi_pwr_init_set (conn_id,
                                        true);               // JT 6/13/18
        }
    }

    if (qsfp_needs_hi_pwr_init (conn_id)) {
        LOG_DEBUG ("QSFP    %2d : %s : %s : requires High-Power init",
                   conn_id,
                   vendor_name,
                   vendor_pn);
    }
    return 0;
}

/****************************************************
* called by qsfp_fsm. If QSFP type has already been
* identified, returns known type. If not, attempts
* to determine type based on compliance and
* extended compliance fields.
****************************************************/
static bool qsfp_fsm_is_optical (int conn_id)
{
    if (qsfp_state[conn_id].qsfp_type ==
        QSFP_TYP_OPTICAL) {
        return true;
    }
    return false;
}

/*****************************************************************
*
*****************************************************************/
/*static*/ void qsfp_start_prbs31 (
    bf_dev_id_t dev_id, int conn_id)
{
    int ch;
    bf_pltfm_port_info_t port_info;
    bf_pltfm_ext_phy_cfg_t port_cfg;
    bool is_present;

    LOG_DEBUG ("QSFP    %2d : Start PRBS31", conn_id);
    bf_serdes_prbs_mode_set (dev_id,
                             BF_PORT_PRBS_MODE_31);

    port_info.conn_id = conn_id;
    port_info.chnl_id = 0;
    bf_bd_cfg_port_has_rtmr (&port_info, &is_present);

    for (ch = 0; ch < 4; ch++) {
        bf_dev_port_t dev_port;
        bf_pal_front_port_handle_t port_hdl;
        bf_status_t rc;
        bf_pltfm_mac_lane_info_t mac_lane_info;

        port_hdl.conn_id = conn_id;
        port_hdl.chnl_id = ch;
        FP2DP (dev_id, &port_hdl, &dev_port);

        if (!is_present) {
            bf_serdes_prbs_init (dev_id, dev_port, 0, 25);
        }

        // set Tx Eq settings
        port_info.conn_id = conn_id;
        port_info.chnl_id = ch;
        rc = bf_bd_cfg_port_mac_lane_info_get (
                 &port_info, BF_PLTFM_QSFP_OPT, &mac_lane_info);

        LOG_DEBUG (
            "QSFP    %2d : ch[%d] : rc=%d : PN=%d : Atn=%d : Post=%d : Pre=%d",
            conn_id,
            ch,
            rc,
            mac_lane_info.tx_phy_pn_swap,
            mac_lane_info.tx_attn,
            mac_lane_info.tx_post,
            mac_lane_info.tx_pre);
        if (rc != BF_SUCCESS) {
            LOG_DEBUG ("QSFP    %2d : ch[%d] : Error <%x> getting Tx EQ+P/N",
                       conn_id,
                       ch,
                       rc);
            continue;
        }
        rc = bf_serdes_tx_drv_inv_set_allow_unassigned (
                 dev_id, dev_port, 0,
                 mac_lane_info.tx_phy_pn_swap);
        if (rc != BF_SUCCESS) {
            LOG_DEBUG (
                "QSFP    %2d : ch[%d] : Error <%x> setting Tx P/N",
                conn_id, ch, rc);
        }
        rc = bf_serdes_tx_drv_attn_set_allow_unassigned (
                 dev_id,
                 dev_port,
                 0,
                 mac_lane_info.tx_attn,
                 mac_lane_info.tx_post,
                 mac_lane_info.tx_pre);
        if (rc != BF_SUCCESS) {
            LOG_DEBUG ("QSFP    %2d : ch[%d] : Error <%x> Tx EQ",
                       conn_id, ch, rc);
        }
        // Luxtera seems to "like" a certain setting during MZI init
        bf_serdes_tx_loop_bandwidth_set (
            dev_id, dev_port, 0,
            BF_SDS_TOF_TX_LOOP_BANDWIDTH_9MHZ);
    }
    // If this is a retimer port, set the retimer in 25g/optical mode
    port_info.conn_id = conn_id;
    port_info.chnl_id = 0;
    bf_bd_cfg_port_has_rtmr (&port_info, &is_present);
    if (is_present) {
        LOG_DEBUG ("QSFP    %2d : Set retimer mode 25g+optical",
                   conn_id);
        for (ch = 0; ch < 4; ch++) {
            port_info.conn_id = conn_id;
            port_info.chnl_id = ch;

            port_cfg.speed_cfg = BF_SPEED_25G;
            port_cfg.is_an_on = 0;
            port_cfg.is_optic = true;

            bf_pltfm_ext_phy_set_mode (&port_info, &port_cfg);
        }
    }
    if (is_present) {
        LOG_DEBUG ("QSFP    %2d : Start retimer PRBS gen",
                   conn_id);
        for (ch = 3; ch >= 0; ch--) {
            port_info.chnl_id = ch;
            bf_pltfm_platform_ext_phy_prbs_set (&port_info, 1,
                                                0 /* PRBS9 prbs_mode*/);
        }
    }
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_st_removed (bf_dev_id_t
                                 dev_id, int conn_id)
{
    int ch;

    // reset type
    qsfp_state[conn_id].qsfp_type = QSFP_TYP_UNKNOWN;
    // reset alarm states
    qsfp_state[conn_id].status_and_alarms =
        qsfp_alarms_initial_state;
    // reset wr_coalesce state
    qsfp_module_fsm_complete_update (dev_id, conn_id);

    // fixme
    // clear "is_optical" indications on any ports the driver may have
    for (ch = 0; ch < 4; ch++) {
        bf_dev_port_t dev_port;
        bf_pal_front_port_handle_t port_hdl;

        port_hdl.conn_id = conn_id;
        port_hdl.chnl_id = ch;
        FP2DP (dev_id, &port_hdl, &dev_port);
        bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                        false);
        // hack, really need # lanes on port
        // clear is_optical to driver
        bf_port_is_optical_xcvr_set (dev_id, dev_port,
                                     false);
        // assert LOS to driver
        // this wont matter if the next QSFP inserted is not optical
        bf_port_optical_los_set (dev_id, dev_port, true);
    }
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_reset_de_assert (
    int conn_id)
{
#if 1
    int rc;

    LOG_DEBUG ("QSFP    %2d : RESETL = false",
               conn_id);
    // de-assert resetL
    rc = bf_qsfp_reset (conn_id, false);
    if (rc != 0) {
        LOG_ERROR ("QSFP    %2d : Error <%d> de-asserting resetL",
                   conn_id, rc);
    }
#endif
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_reset_assert (int conn_id)
{
#if 1
    int rc;

    LOG_DEBUG ("QSFP    %2d : RESETL = true",
               conn_id);
    // assert resetL
    rc = bf_qsfp_reset (conn_id, true);
    if (rc != 0) {
        LOG_ERROR ("QSFP    %2d : Error <%d> asserting resetL",
                   conn_id, rc);
    }
#endif
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_lpmode_de_assert (
    int conn_id)
{
#if 0
    int rc;

    LOG_DEBUG ("QSFP    %2d : LPMODE = false",
               conn_id);
    // de-assert LPMODE
    rc = bf_qsfp_set_transceiver_lpmode (conn_id,
                                         false);
    if (rc != 0) {
        LOG_ERROR ("QSFP    %2d : Error <%d> de-asserting LPMODE",
                   conn_id, rc);
    }
#endif
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_lpmode_assert (int conn_id)
{
#if 0
    int rc;

    LOG_DEBUG ("QSFP    %2d : LPMODE = true",
               conn_id);
    // assert LPMODE
    rc = bf_qsfp_set_transceiver_lpmode (conn_id,
                                         true);
    if (rc != 0) {
        LOG_ERROR ("QSFP    %2d : Error <%d> asserting LPMODE",
                   conn_id, rc);
    }
#endif
}

/*****************************************************************
*
*****************************************************************/
void qsfp_deassert_all_reset_pins (void)
{
    int conn_id;

    LOG_DEBUG ("QSFP: Assert RESETL on all QSFPs");
    fprintf (stdout,
             "QSFP: Assert RESETL on all QSFPs\n");

    for (conn_id = 1; conn_id <= bf_pm_num_qsfp_get();
         conn_id++) {
        /* Skip chnl which is a panel sfp module.
         * by tsihang, 2021-07-16. */
        if (is_panel_sfp (conn_id, 0) ||
            bf_bd_is_this_port_internal (conn_id, 0)) {
            continue;
        }

        qsfp_fsm_reset_assert (conn_id);
    }
    bf_sys_usleep (SFF8436_TMR_t_reset_init *
                   1000); // takes micro-seconds

    LOG_DEBUG ("QSFP: Assert LPMODE and De-assert RESETL on all QSFPs");
    fprintf (stdout,
             "QSFP: Assert LPMODE and De-assert RESETL on %2d QSFPs\n",
			 bf_pm_num_qsfp_get());
    LOG_DEBUG ("QSFP: Assert LPMODE and De-assert RESETL on %2d QSFPs\n",
			 bf_pm_num_qsfp_get());

    for (conn_id = 1; conn_id <= bf_pm_num_qsfp_get();
         conn_id++) {
        /* Skip chnl which is a panel sfp module.
        * by tsihang, 2021-07-16. */
        if (is_panel_sfp (conn_id, 0) ||
            bf_bd_is_this_port_internal (conn_id, 0)) {
            continue;
        }

        // assert LP_MODE
        qsfp_fsm_lpmode_assert (conn_id);
        qsfp_fsm_reset_de_assert (conn_id);
    }
    /* SFF8436_TMR_t_reset */
    int sec = 5;
    fprintf (stdout,
             "QSFP: Wait %2d sec (t_reset) for %2d QSFP(s) to initialize.. \n",
             (sec * 1000) / 1000, bf_pm_num_qsfp_get());
    LOG_DEBUG ("QSFP: Wait %2d sec (t_reset) for %2d QSFP(s) to initialize..",
               (sec * 1000) / 1000, bf_pm_num_qsfp_get());
    bf_sys_usleep (sec * 1000); // takes micro-seconds
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_st_reset (bf_dev_id_t dev_id,
                               int conn_id)
{
    qsfp_fsm_reset_de_assert (conn_id);
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_st_inserted (bf_dev_id_t
                                  dev_id, int conn_id)
{
    // assert resetL
    qsfp_fsm_reset_assert (conn_id);

    // Luxtera modules require LPMODE de-asserted during init seq
    if (qsfp_needs_hi_pwr_init (conn_id)) {
        // de-assert LP_MODE
        qsfp_fsm_lpmode_de_assert (conn_id);
        // start PRBS31 on all 4 lanes
        qsfp_start_prbs31 (dev_id, conn_id);
    } else {
        // assert LP_MODE
        qsfp_fsm_lpmode_assert (conn_id);
    }
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_st_tx_disable (
    bf_dev_id_t dev_id, int conn_id)
{
    bf_status_t rc;
    uint8_t byte_86;
    // Luxtera modules require TX_DISABLE de-asserted during init seq
    if (qsfp_needs_hi_pwr_init (conn_id)) {
        byte_86 = 0x0;
    } else {
        byte_86 = 0xF;
    }
    byte_86 = 0x0;/* By default, de-assert TX_DISABLE during init seq. */
    LOG_DEBUG ("%s:%d  -> QSFP    %2d : TX_DISABLE = %2x\n",
               __func__, __LINE__, conn_id, byte_86);

    /* 0x56, Tx Disable Control */
    rc = bf_fsm_qsfp_wr (conn_id, 86, 1, &byte_86);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting TX_DISABLE=0x%x0",
                   conn_id,
                   rc,
                   byte_86);
    }
}

/*****************************************************************
*
*****************************************************************/
void qsfp_lpmode_sw_set (bf_dev_id_t dev_id,
                         int conn_id, uint8_t val)
{
    uint8_t byte_93;
    bf_status_t rc;

    rc = bf_fsm_qsfp_rd (conn_id, 93, 1, &byte_93);
    if (rc) {
        LOG_ERROR (
            "QSFP    %2d : Error <%d> reading Power ctrl (byte 93)",
            conn_id, rc);
    }
    byte_93 = (byte_93 & ~3) | (val & 3);
    rc = bf_fsm_qsfp_wr (conn_id, 93, 1, &byte_93);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> writing Power ctrl (byte 93) = %02x",
                   conn_id,
                   rc,
                   byte_93);
    } else {
        LOG_DEBUG ("QSFP    %2d : Power ctrl (byte 93) = %02x",
                   conn_id, byte_93);
    }
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_st_lp_mode_off (
    bf_dev_id_t dev_id, int conn_id)
{
    qsfp_lpmode_sw_set (dev_id, conn_id, 0);
}

/*****************************************************************
* This routine is only used in Tofino 1 implementations
*****************************************************************/
void qsfp_log_alarms (bf_dev_id_t dev_id,
                      int conn_id,
                      qsfp_status_and_alarms_t *old,
                      qsfp_status_and_alarms_t *new)
{
    if (memcmp ((char *)old, (char *)new,
                sizeof (*old)) != 0) {
        // log the raw fields to provide current state of all alarms
        LOG_DEBUG (
            "QSFP    %2d : Alarms (Bytes 2-14) : %02x %02x %02x %02x %02x"
            " %02x %02x %02x %02x %02x %02x %02x %02x",
            conn_id,
            new->status2,
            new->los_ind,
            new->eq_laser_fault,
            new->lol_ind,
            new->temp_alarm,
            new->vcc_alarm,
            new->vendor_spec,
            new->rx_power_alarm12,
            new->rx_power_alarm34,
            new->tx_bias_alarm12,
            new->tx_bias_alarm34,
            new->tx_power_alarm12,
            new->tx_power_alarm34);
        // now, log changes
        if (old->status2 != new->status2) {
            if ((old->status2 & BYTE2_INTL) !=
                (new->status2 & BYTE2_INTL)) {
                LOG_DEBUG ("QSFP    %2d : IntL : %d -> %d",
                           conn_id,
                           (old->status2 & BYTE2_INTL) >> 1,
                           (new->status2 & BYTE2_INTL) >> 1);
            }
            if ((old->status2 & BYTE2_DATA_NOT_READY) !=
                (new->status2 & BYTE2_DATA_NOT_READY)) {
                LOG_DEBUG ("QSFP    %2d : Data_Not_Ready: %d -> %d",
                           conn_id,
                           (old->status2 & BYTE2_DATA_NOT_READY),
                           (new->status2 & BYTE2_DATA_NOT_READY));
            }
        }
        if (old->los_ind != new->los_ind) {
            LOG_DEBUG (
                "QSFP    %2d : [7:4] Tx LOS: %1x -> %1x : [3:0] Rx LOS: %1x -> %1x",
                conn_id,
                old->los_ind >> 4,
                new->los_ind >> 4,
                old->los_ind & 0xF,
                new->los_ind & 0xF);
        }
        if (old->eq_laser_fault != new->eq_laser_fault) {
            LOG_DEBUG (
                "QSFP    %2d : [7:4] Tx Adapt EQ Fault: %1x -> %1x : [3:0] Tx Laser "
                "fault: %1x -> %1x",
                conn_id,
                old->eq_laser_fault >> 4,
                new->eq_laser_fault >> 4,
                old->eq_laser_fault & 0xF,
                new->eq_laser_fault & 0xF);
        }
        if (old->lol_ind != new->lol_ind) {
            LOG_DEBUG (
                "QSFP    %2d : [7:4] Tx CDR LOL: %1x -> %1x : [3:0] Rx CDR LOL: %1x "
                "-> "
                "%1x",
                conn_id,
                old->lol_ind >> 4,
                new->lol_ind >> 4,
                old->lol_ind & 0xF,
                new->lol_ind & 0xF);
        }
        if (old->temp_alarm != new->temp_alarm) {
            LOG_DEBUG ("QSFP    %2d : Temp Alarm/Warn: %02x -> %02x",
                       conn_id,
                       old->temp_alarm,
                       new->temp_alarm);
        }
        if (old->vcc_alarm != new->vcc_alarm) {
            LOG_DEBUG ("QSFP    %2d : Vcc Alarm/Warn: %02x -> %02x",
                       conn_id,
                       old->vcc_alarm,
                       new->vcc_alarm);
        }
        if (old->vendor_spec != new->vendor_spec) {
            LOG_DEBUG ("QSFP    %2d : Vendor Sepcific (Byte 8): %02x -> %02x",
                       conn_id,
                       old->vendor_spec,
                       new->vendor_spec);
        }
        if (old->rx_power_alarm12 !=
            new->rx_power_alarm12) {
            LOG_DEBUG ("QSFP    %2d : Rx Power Alarm/Warn CH1/2: %02x -> %02x",
                       conn_id,
                       old->rx_power_alarm12,
                       new->rx_power_alarm12);
        }
        if (old->rx_power_alarm34 !=
            new->rx_power_alarm34) {
            LOG_DEBUG ("QSFP    %2d : Rx Power Alarm/Warn CH3/4: %02x -> %02x",
                       conn_id,
                       old->rx_power_alarm34,
                       new->rx_power_alarm34);
        }
        if (old->tx_bias_alarm12 !=
            new->tx_bias_alarm12) {
            LOG_DEBUG ("QSFP    %2d : Tx Bias Alarm/Warn CH1/2: %02x -> %02x",
                       conn_id,
                       old->tx_bias_alarm12,
                       new->tx_bias_alarm12);
        }
        if (old->tx_bias_alarm34 !=
            new->tx_bias_alarm34) {
            LOG_DEBUG ("QSFP    %2d : Tx Bias Alarm/Warn CH3/4: %02x -> %02x",
                       conn_id,
                       old->tx_bias_alarm34,
                       new->tx_bias_alarm34);
        }
        if (old->tx_power_alarm12 !=
            new->tx_power_alarm12) {
            LOG_DEBUG ("QSFP    %2d : Tx Power Alarm/Warn CH1/2: %02x -> %02x",
                       conn_id,
                       old->tx_power_alarm12,
                       new->tx_power_alarm12);
        }
        if (old->tx_power_alarm34 !=
            new->tx_power_alarm34) {
            LOG_DEBUG ("QSFP    %2d : Tx Power Alarm/Warn CH3/4: %02x -> %02x",
                       conn_id,
                       old->tx_power_alarm34,
                       new->tx_power_alarm34);
        }
    }
}

/*****************************************************************
* This routine is only used in Tofino 1 implementations
*****************************************************************/
static void qsfp_fsm_check_alarms_after_unreset (
    bf_dev_id_t dev_id,
    int conn_id)
{
    qsfp_status_and_alarms_t status_and_alarms;
    int rc;

    LOG_DEBUG ("QSFP    %2d : Clear existing alarms..",
               conn_id);
    rc = bf_fsm_qsfp_rd (
             conn_id, 2, sizeof (status_and_alarms),
             (uint8_t *)&status_and_alarms);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading status fields",
                   conn_id, rc);
        return;  // can't trust the data, just leave
    }
    qsfp_log_alarms (dev_id,
                     conn_id,
                     &qsfp_state[conn_id].status_and_alarms,
                     &status_and_alarms);

    // copy new alarm state into out cached struct
    qsfp_state[conn_id].status_and_alarms =
        status_and_alarms;

    LOG_DEBUG ("QSFP    %2d : Now check for current alarms..",
               conn_id);
    rc = bf_fsm_qsfp_rd (
             conn_id, 2, sizeof (status_and_alarms),
             (uint8_t *)&status_and_alarms);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading status fields",
                   conn_id, rc);
        return;  // can't trust the data, just leave
    }
    qsfp_log_alarms (dev_id,
                     conn_id,
                     &qsfp_state[conn_id].status_and_alarms,
                     &status_and_alarms);

    // copy new alarm state into out cached struct
    qsfp_state[conn_id].status_and_alarms =
        status_and_alarms;
}

/*****************************************************************
* This routine is only used in Tofino 1 implementations
*****************************************************************/
static void qsfp_fsm_poll_los (bf_dev_id_t dev_id,
                               int conn_id)
{
    qsfp_status_and_alarms_t status_and_alarms;
    int rc, ch;

    // read status and alarms (bytes2-14) onto stack. This is in case
    // there is an error on the read we dont corrupt the qsfp_state
    // structure. If no error, copy into struct
    rc = bf_fsm_qsfp_rd (
             conn_id, 2, sizeof (status_and_alarms),
             (uint8_t *)&status_and_alarms);
    if (rc) {
        LOG_DEBUG ("QSFP    %2d : Error <%d> reading status fields",
                   conn_id, rc);
        return;  // can't trust the data, just leave
    }
    qsfp_log_alarms (dev_id,
                     conn_id,
                     &qsfp_state[conn_id].status_and_alarms,
                     &status_and_alarms);

    // During HA, we really donot know when the config replay ends.
    // So simply push the current RX-los to the SDK in order for port-FSM
    // to continue after warm-init ends.
    //
    // If there is NO RX, SDK probably will detect link-down faster.
    //
    // If there is a RX and link is already UP, SDK will NOT act on this
    // los-flag. Hence we donot have to tie to link-up here or to port-FSM
    // states.

    // only want Rx LOS bits
    uint8_t rx_los_now, rx_los_before;
    rx_los_now = status_and_alarms.los_ind & 0xF;
    rx_los_before =
        qsfp_state[conn_id].status_and_alarms.los_ind &
        0xF;
    for (ch = 0; ch < 4; ch++) {
        bf_dev_port_t dev_port;
        bf_status_t bf_status;
        bf_pal_front_port_handle_t port_hdl;
        bool los = ((rx_los_now >> ch) & 1) ? true :
                   false;

        port_hdl.conn_id = conn_id;
        port_hdl.chnl_id = ch;
        FP2DP (dev_id, &port_hdl, &dev_port);
        // hack, really need # lanes on port
        // TBD: Move optical-type to qsfp_detection_actions
        bf_port_is_optical_xcvr_set (dev_id, dev_port,
                                     true);
        bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                        true);
        bf_status = bf_port_optical_los_set (dev_id,
                                             dev_port, los);
        if (bf_status == BF_SUCCESS) {
            if ((rx_los_now & (1 << ch)) ^ (rx_los_before &
                                            (1 << ch))) {
                LOG_DEBUG ("QSFP    %2d : dev_port=%3d : LOS=%d",
                           conn_id, dev_port, los);
            }
        }
    }
    // copy new alarm state into out cached struct
    qsfp_state[conn_id].status_and_alarms =
        status_and_alarms;
}


/*****************************************************************
*
*****************************************************************/
static void qsfp_module_fsm_update (
    bf_dev_id_t dev_id, int conn_id)
{
    uint8_t pg = qsfp_state[conn_id].wr_coalesce.pg;
    uint8_t ofs = qsfp_state[conn_id].wr_coalesce.ofs;
    uint8_t mask =
        qsfp_state[conn_id].wr_coalesce.mask;
    uint8_t data =
        qsfp_state[conn_id].wr_coalesce.data;
    uint8_t cur_val, new_val;
    bf_status_t rc;

    if (ofs > 128) {  // if not lower page 0, set page
        qsfp_set_pg (conn_id, pg);
    }

    if (mask !=
        0xFF) {  // only need to read if not changing all bits
        // read current value at "ofs"
        rc = bf_fsm_qsfp_rd (conn_id, ofs, 1, &cur_val);
        if (rc) {
            LOG_ERROR ("QSFP    %2d : Error <%d> reading pg=%d : ofs=%d to coalesce",
                       conn_id,
                       rc,
                       pg,
                       ofs);
            return;
        }
    }

    if (mask == 0xFF) {
        LOG_DEBUG (
            "QSFP    %2d : Coalesced Wr: page %d : offset %d : mask %02xh : data "
            "%02xh : cur=XX",
            conn_id,
            pg,
            ofs,
            mask,
            data);
    } else {
        LOG_DEBUG (
            "QSFP    %2d : Coalesced Wr: page %d : offset %d : mask %02xh : data "
            "%02xh : cur=%02x",
            conn_id,
            pg,
            ofs,
            mask,
            data,
            cur_val);
    }

    new_val = (cur_val & ~mask) | data;
    if (new_val != cur_val) {
        rc = bf_fsm_qsfp_wr (conn_id, ofs, 1, &new_val);
        if (rc) {
            LOG_ERROR ("QSFP    %2d : Error <%d> Setting pg=%d : ofs=%d : new=%02x",
                       conn_id,
                       rc,
                       pg,
                       ofs,
                       new_val);
        }
    }
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_module_fsm_complete_update (
    bf_dev_id_t dev_id, int conn_id)
{
    qsfp_state[conn_id].wr_coalesce.in_progress = 0;
    qsfp_state[conn_id].wr_coalesce.delay_ms = 0;
    qsfp_state[conn_id].wr_coalesce.pg = 0;
    qsfp_state[conn_id].wr_coalesce.ofs = 0;
    qsfp_state[conn_id].wr_coalesce.mask = 0;
    qsfp_state[conn_id].wr_coalesce.data = 0;
}

/*****************************************************************
* Module FSM
*****************************************************************/
static void qsfp_module_fsm_run (bf_dev_id_t
                                 dev_id, int conn_id)
{
    qsfp_fsm_state_t st = qsfp_state[conn_id].fsm_st;
    qsfp_fsm_state_t next_st = st;
    int rc, delay_ms = 0;
    bool is_optical = false;

    switch (st) {
        case QSFP_FSM_ST_IDLE:
            break;

        case QSFP_FSM_ST_REMOVED:
            // reset to initial conditions
            // resetL          <-- 1 # not reset
            // lp_mode         <-- 1 # low power mode
            // tx_disable[3:0] <-- 1 # laser OFF
            qsfp_fsm_st_removed (dev_id, conn_id);

            next_st = QSFP_FSM_ST_IDLE;
            delay_ms = 0;
            break;

        case QSFP_FSM_ST_INSERTED:
            // identify type.
            // If not optial got directly to DETECTED
            // else ensure initial conditions
            //   resetL          <-- 0 # assert reset
            //   lp_mode         <-- 1 low power mode
            //   tx_disable[3:0] <-- 1 # laser OFF
            rc = qsfp_fsm_identify_type (conn_id,
                                         &is_optical);
            if ((rc != 0) ||
                (qsfp_state[conn_id].qsfp_type ==
                 QSFP_TYP_UNKNOWN)) {
                // error identifying type. Stay in this state
                next_st = QSFP_FSM_ST_INSERTED;
                delay_ms = 1000;  // try again in 1sec
                break;
            }
            if (is_optical) {
                int ch;

                // checks for QSFPs with "special" requirements
                qsfp_fsm_identify_model_requirements (conn_id);

                // set initial state for each of the 4 channel FSMs
                for (ch = 0; ch < 4; ch++) {
                    qsfp_state[conn_id].per_ch_fsm[ch] =
                        qsfp_channel_fsm_initial_state;
                }
                qsfp_fsm_st_inserted (dev_id, conn_id);

                next_st = QSFP_FSM_ST_WAIT_T_RESET;
                delay_ms = SFF8436_TMR_t_reset_init;
            } else {
                fprintf (stdout,
                         "TSIHANG : QSFP    %2d : NOT OPTICAL ...\n",
                         conn_id);
                LOG_WARNING ("TSIHANG : QSFP    %2d : NOT OPTICAL ...",
                             conn_id);
                qsfp_fsm_notify_bf_pltfm (dev_id, conn_id);

                next_st = QSFP_FSM_ST_DETECTED;
                delay_ms = 0;
            }
            break;

        case QSFP_FSM_ST_WAIT_T_RESET:
            qsfp_fsm_st_reset (dev_id, conn_id);

            next_st = QSFP_FSM_ST_WAIT_TON_TXDIS;
            delay_ms = SFF8436_TMR_t_reset;
            break;

        case QSFP_FSM_ST_WAIT_TON_TXDIS:
            // check for alarms during intialization sequence
            qsfp_fsm_check_alarms_after_unreset (dev_id,
                                                 conn_id);

            qsfp_fsm_st_tx_disable (dev_id, conn_id);

            next_st = QSFP_FSM_ST_WAIT_TOFF_LPMODE;
            /* if the module needs hi-pwr init then we actually de-asserted
            * TX_DISABLE above and need to wait for SFF8436_TMR_toff_txdis
            * PLUS the Luxtera MZI init time of 3.1 seconds for a total of 3.5
            * seconds*/
            if (qsfp_needs_hi_pwr_init (conn_id)) {
                delay_ms =
                    SFF8436_TMR_toff_txdis +
                    3100;  // 3.5 sec for Luxtera MZI init;
            } else {
                delay_ms = SFF8436_TMR_ton_txdis;
            }
            break;

        case QSFP_FSM_ST_WAIT_TOFF_LPMODE:
            qsfp_fsm_st_lp_mode_off (dev_id, conn_id);

            next_st = QSFP_FSM_ST_DETECTED;
            delay_ms = SFF8436_TMR_Toff_LPMode;

            qsfp_fsm_notify_bf_pltfm (dev_id, conn_id);
            break;

        case QSFP_FSM_ST_DETECTED:
            if (qsfp_fsm_is_optical (conn_id)) {
                // check LOS
                qsfp_fsm_poll_los (dev_id, conn_id);
            }

            next_st = QSFP_FSM_ST_DETECTED;
            delay_ms = 200;  // 200ms poll time
            break;

        case QSFP_FSM_ST_UPDATE:
            qsfp_module_fsm_update (dev_id, conn_id);
            next_st = QSFP_FSM_ST_WAIT_UPDATE;
            delay_ms =
                qsfp_state[conn_id].wr_coalesce.delay_ms;
            break;

        case QSFP_FSM_ST_WAIT_UPDATE:
            qsfp_module_fsm_complete_update (dev_id, conn_id);
            next_st = QSFP_FSM_ST_DETECTED;
            delay_ms = 0;
            break;

        default:
            assert (0);
            break;
    }
    qsfp_state[conn_id].fsm_st = next_st;
    qsfp_fsm_next_run_time_set (
        &qsfp_state[conn_id].next_fsm_run_time, delay_ms);

    if (st != next_st) {
        LOG_DEBUG ("QSFP    %2d : %s --> %s (%dms)\n",
                   conn_id,
                   qsfp_fsm_st_to_str[st],
                   qsfp_fsm_st_to_str[next_st],
                   delay_ms);
    }
}

/*****************************************************************
*
 Returns 0 = write merged
        !0 = different write in-progress, try later
*****************************************************************/
static int qsfp_fsm_coalesce_wr (bf_dev_id_t
                                 dev_id,
                                 int conn_id,
                                 int delay_ms,
                                 uint8_t pg,
                                 uint8_t ofs,
                                 uint8_t mask,
                                 uint8_t data)
{
    if (!qsfp_state[conn_id].wr_coalesce.in_progress) {
        qsfp_state[conn_id].wr_coalesce.in_progress =
            true;
        qsfp_state[conn_id].wr_coalesce.delay_ms =
            delay_ms;
        qsfp_state[conn_id].wr_coalesce.pg = pg;
        qsfp_state[conn_id].wr_coalesce.ofs = ofs;
        qsfp_state[conn_id].wr_coalesce.mask = mask;
        qsfp_state[conn_id].wr_coalesce.data = data;

        LOG_DEBUG ("QSFP    %2d : %s --> %s (%dms)",
                   conn_id,
                   qsfp_fsm_st_to_str[qsfp_state[conn_id].fsm_st],
                   qsfp_fsm_st_to_str[QSFP_FSM_ST_UPDATE],
                   0);
        qsfp_state[conn_id].fsm_st = QSFP_FSM_ST_UPDATE;
        qsfp_fsm_next_run_time_set (
            &qsfp_state[conn_id].next_fsm_run_time, 0);
    } else {
        // check if this write can be coalesced with the current one
        if ((qsfp_state[conn_id].wr_coalesce.pg == pg) &&
            (qsfp_state[conn_id].wr_coalesce.ofs == ofs)) {
            // merge this write with in-progress one
            // LOG_ERROR("QSFP    %2d : Coalesce pg=%d : ofs=%d : mask=%02x : data=%02x
            // : with : mask=%02x : data=%02x",
            //          conn_id, pg, ofs, mask, data,
            //          qsfp_state[conn_id].wr_coalesce.mask,
            //          qsfp_state[conn_id].wr_coalesce.data);
            qsfp_state[conn_id].wr_coalesce.mask |= mask;
            qsfp_state[conn_id].wr_coalesce.data |= data;
        } else {
            return -1;  // try again later
        }
    }
    return 0;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_ch_enable (bf_dev_id_t dev_id,
                         int conn_id, int ch)
{
    if (qsfp_state[conn_id].per_ch_fsm[ch].fsm_st !=
        QSFP_FSM_EN_ST_ENABLING) {
        LOG_DEBUG (
            "QSFP    %2d : ch[%d] : %s --> %s (%dms)",
            conn_id,
            ch,
            qsfp_fsm_ch_en_st_to_str[qsfp_state[conn_id].per_ch_fsm[ch].fsm_st],
            qsfp_fsm_ch_en_st_to_str[QSFP_FSM_EN_ST_ENABLING],
            0);
        qsfp_state[conn_id].per_ch_fsm[ch].fsm_st =
            QSFP_FSM_EN_ST_ENABLING;
        qsfp_fsm_next_run_time_set (
            &qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time,
            0);
    }
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_ch_disable (bf_dev_id_t dev_id,
                          int conn_id, int ch)
{
    if (qsfp_state[conn_id].per_ch_fsm[ch].fsm_st !=
        QSFP_FSM_EN_ST_DISABLING) {
        LOG_DEBUG (
            "QSFP    %2d : ch[%d] : %s --> %s (%dms)",
            conn_id,
            ch,
            qsfp_fsm_ch_en_st_to_str[qsfp_state[conn_id].per_ch_fsm[ch].fsm_st],
            qsfp_fsm_ch_en_st_to_str[QSFP_FSM_EN_ST_DISABLING],
            0);
        qsfp_state[conn_id].per_ch_fsm[ch].fsm_st =
            QSFP_FSM_EN_ST_DISABLING;
        qsfp_fsm_next_run_time_set (
            &qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time,
            0);
    }
}

/*****************************************************************
*
*****************************************************************/
static int qsfp_fsm_ch_enable_cdr (
    bf_dev_id_t dev_id, int conn_id, int ch)
{
    int rc;
    rc = qsfp_fsm_coalesce_wr (dev_id,
                               conn_id,
                               300 /*300ms*/,
                               0 /*pg=0*/,
                               98 /*98 (0x62)*/,
                               (0x11 << ch),
                               (0x11 << ch));
    return rc;  // non-0 means "try later"
}

#if 0
/*****************************************************************
*
*****************************************************************/
static int qsfp_fsm_ch_disable_cdr (
    bf_dev_id_t dev_id, int conn_id, int ch)
{
    int rc;
    rc = qsfp_fsm_coalesce_wr (dev_id,
                               conn_id,
                               300 /*300ms*/,
                               0 /*pg=0*/,
                               98 /*98 (0x62)*/,
                               (0x11 << ch),
                               (0x00 << ch));
    return rc;  // non-0 means "try later"
}
#endif

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_ch_check_tx_cdr_lol (
    bf_dev_id_t dev_id,
    int conn_id,
    int ch,
    bool *ok,
    uint8_t *los_val,
    uint8_t *lol_val)
{
    uint8_t byte_3, byte_5;
    bf_status_t rc;
    uint32_t lane_mask[] = {0, 1, 3, 0xF};
    int num_lanes = qsfp_num_lanes_get (conn_id, ch);
    uint32_t msk = lane_mask[num_lanes] << (ch + 4);

    rc = bf_fsm_qsfp_rd (conn_id, 3, 1, &byte_3);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading TX LOS (byte 3)",
                   conn_id, rc);
    }
    if (byte_3 & 0xF0 &
        msk) {  // only check L-TX1-4 LOS
        *ok = false;
    } else {
        *ok = true;
    }

    rc = bf_fsm_qsfp_rd (conn_id, 5, 1, &byte_5);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading CDR LOL (byte 5)",
                   conn_id, rc);
    }
    if (byte_5 & 0xF0 &
        msk) {  // only check L-TX1-4 LOL
        *ok = false;
    }
    *los_val = byte_3;
    *lol_val = byte_5;

    LOG_DEBUG (
        "QSFP    %2d : ch[%d] : TX LOS (byte 3) = %02x : TX CDR LOL (byte 5) = "
        "%02x : %s <num_lanes=%d : msk=%x>",
        conn_id,
        ch,
        byte_3,
        byte_5,
        *ok ? "[ok]" : "[fault present]",
        num_lanes,
        msk);
}

/*****************************************************************
*
*****************************************************************/
static int qsfp_fsm_ch_enable_optical_tx (
    bf_dev_id_t dev_id,
    int conn_id,
    int ch,
    uint32_t delay_ms)
{
    int rc;
    rc = qsfp_fsm_coalesce_wr (dev_id,
                               conn_id,
                               delay_ms,
                               0 /*pg=0*/,
                               86 /*offset*/,
                               (0x1 << ch),
                               (0x0 << ch));
    return rc;  // non-0 means "try later"
}

/*****************************************************************
*
*****************************************************************/
static int qsfp_fsm_ch_disable_optical_tx (
    bf_dev_id_t dev_id,
    int conn_id,
    int ch)
{
    int rc;
    rc = qsfp_fsm_coalesce_wr (dev_id,
                               conn_id,
                               100 /*100ms*/,
                               0 /*pg=0*/,
                               86 /*offset*/,
                               (0x1 << ch),
                               (0x1 << ch));
    return rc;  // non-0 means "try later"
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_fsm_ch_check_tx_optical_fault (
    bf_dev_id_t dev_id,
    int conn_id,
    int ch,
    bool clear_only,
    bool *ok,
    uint8_t *val)
{
    uint8_t byte_4, byte, faults_and_alarms[16];
    bf_status_t rc;
    bool fault_or_alarm_set = false;
    uint32_t lane_mask[] = {0, 1, 3, 0xF};
    int num_lanes = qsfp_num_lanes_get (conn_id, ch);
    uint32_t msk = lane_mask[num_lanes] << ch;

    // read "Interrupt Flags" bytes, 3-15, most are faults and alarms
    rc = bf_fsm_qsfp_rd (conn_id, 0, 15,
                         faults_and_alarms);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading faults and alarms",
                   conn_id, rc);
    }
    if (clear_only) {
        return;
    }

    byte_4 = faults_and_alarms[4];

    // ignore some Rx faults and alarms
    faults_and_alarms[3] &= 0xF0;
    faults_and_alarms[5] &= 0xF0;
    faults_and_alarms[8] = 0;
    faults_and_alarms[9] = 0;
    faults_and_alarms[10] = 0;

    for (byte = 3; byte < 16; byte++) {
        if (faults_and_alarms[byte] != 0) {
            if (byte == 4) {
                fault_or_alarm_set = true;
            }
            LOG_DEBUG ("QSFP    %2d : Page 0: Faults and alarms: Byte %2d: %02xh",
                       conn_id,
                       byte,
                       faults_and_alarms[byte]);
        }
    }
    if (fault_or_alarm_set) {
        *ok = false;
        *val = byte_4;
        //    return;
    }

    if (byte_4 & 0x0F &
        msk) {  // only check L-TX1-4 Fault
        *ok = false;
    } else {
        *ok = true;
    }
    *val = byte_4;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_ch_notify_ready (bf_dev_id_t dev_id,
                               int conn_id, int ch)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;
    bool los;

    port_hdl.conn_id = conn_id;
    port_hdl.chnl_id = ch;
    FP2DP (dev_id, &port_hdl, &dev_port);
    // notify of LOS state first
    los = (qsfp_state[conn_id].status_and_alarms.los_ind
           & (1 << ch)) ? true
          : false;
    bf_port_optical_los_set (dev_id, dev_port, los);
    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    true);

    LOG_DEBUG ("QSFP    %2d : ch[%d] : Ready %s",
               conn_id, ch, los ? "<LOS>" : "");
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_ch_notify_not_ready (
    bf_dev_id_t dev_id, int conn_id, int ch)
{
    bf_dev_port_t dev_port;
    bf_pal_front_port_handle_t port_hdl;
    bool los;

    port_hdl.conn_id = conn_id;
    port_hdl.chnl_id = ch;
    FP2DP (dev_id, &port_hdl, &dev_port);
    bf_port_optical_xcvr_ready_set (dev_id, dev_port,
                                    false);
    bf_port_optical_los_set (dev_id, dev_port, true);
    los = (qsfp_state[conn_id].status_and_alarms.los_ind
           & (1 << ch)) ? true
          : false;
    LOG_DEBUG (
        "QSFP    %2d : ch[%d] : NOT Ready %s", conn_id, ch,
        los ? "<LOS>" : "");
}

/*****************************************************************
*
*****************************************************************/
bool qsfp_fsm_ch_first_enable (bf_dev_id_t dev_id,
                               int conn_id, int ch)
{
    return qsfp_state[conn_id].per_ch_fsm[ch].first_enable_after_reset;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_fsm_ch_first_enable_clear (
    bf_dev_id_t dev_id, int conn_id, int ch)
{
    qsfp_state[conn_id].per_ch_fsm[ch].first_enable_after_reset
        = false;
}

/*****************************************************************
*
*****************************************************************/
static void qsfp_channel_fsm_run (bf_dev_id_t
                                  dev_id, int conn_id, int ch)
{
    qsfp_fsm_ch_en_state_t st =
        qsfp_state[conn_id].per_ch_fsm[ch].fsm_st;
    qsfp_fsm_ch_en_state_t next_st =
        st;  // default to no transition
    int wr_coalesce_err, delay_ms = 0;
    bool ok, discard_latched;
    uint8_t los_fault_val, lol_fault_val, fault_val;

    switch (st) {
        case QSFP_FSM_EN_ST_DISABLED:
            // waiting to start enabling
            break;
        case QSFP_FSM_EN_ST_ENABLING:
            // notify driver
            qsfp_fsm_ch_notify_not_ready (dev_id, conn_id,
                                          ch);
            delay_ms = 300;  // ms

            next_st = QSFP_FSM_EN_ST_ENA_CDR;
            break;
        case QSFP_FSM_EN_ST_ENA_CDR:
            wr_coalesce_err = qsfp_fsm_ch_enable_cdr (dev_id,
                              conn_id, ch);
            if (wr_coalesce_err == 0) {
                next_st = QSFP_FSM_EN_ST_ENA_OPTICAL_TX;
                delay_ms = 300;  // ms
            }
            break;
        case QSFP_FSM_EN_ST_ENA_OPTICAL_TX:
            qsfp_fsm_ch_check_tx_cdr_lol (dev_id,
                                          conn_id,
                                          ch,
                                          &discard_latched,
                                          &los_fault_val,
                                          &lol_fault_val);
            // for some reason, need to read twice to clear latched faults
            qsfp_fsm_ch_check_tx_cdr_lol (dev_id,
                                          conn_id,
                                          ch,
                                          &discard_latched,
                                          &los_fault_val,
                                          &lol_fault_val);
            qsfp_fsm_ch_check_tx_cdr_lol (
                dev_id, conn_id, ch, &ok, &los_fault_val,
                &lol_fault_val);
            if (!ok) {
                LOG_DEBUG (
                    "QSFP    %2d : ch[%d] : TX LOS <%02x>, TX CDR LOL <%02x>. retry..",
                    conn_id,
                    ch,
                    los_fault_val,
                    lol_fault_val);
                // next_st = QSFP_FSM_EN_ST_ENABLING;
                // delay_ms = 0;  // ms
            }
            if (qsfp_fsm_ch_first_enable (dev_id, conn_id,
                                          ch)) {
                delay_ms = 400 +
                           3100;  // 3.5 sec for Luxtera MZI init
                LOG_DEBUG (
                    "QSFP    %2d : ch[%d] : First enable after reset. Extra 3.1 sec "
                    "wait",
                    conn_id,
                    ch);
            } else {
                delay_ms = 400;  // ms
            }
            wr_coalesce_err =
                qsfp_fsm_ch_enable_optical_tx (dev_id, conn_id,
                                               ch, delay_ms);
            if (wr_coalesce_err == 0) {
                next_st = QSFP_FSM_EN_ST_NOTIFY_ENABLED;
            }
            break;

        case QSFP_FSM_EN_ST_NOTIFY_ENABLED:
            qsfp_fsm_ch_first_enable_clear (dev_id, conn_id,
                                            ch);
            qsfp_fsm_ch_check_tx_optical_fault (dev_id,
                                                conn_id,
                                                ch,
                                                true /*clear only*/,
                                                &discard_latched,
                                                &fault_val);
            // for some reason, need to read twice to clear latched faults
            qsfp_fsm_ch_check_tx_optical_fault (dev_id,
                                                conn_id,
                                                ch,
                                                true /*clear only*/,
                                                &discard_latched,
                                                &fault_val);
            qsfp_fsm_ch_check_tx_optical_fault (
                dev_id, conn_id, ch, false /* log faults*/, &ok,
                &fault_val);
            if (!ok) {
                LOG_DEBUG ("QSFP    %2d : ch[%d] : Optical TX FAULT <%02x>. retry..",
                           conn_id,
                           ch,
                           fault_val);
                wr_coalesce_err = qsfp_fsm_ch_disable_optical_tx (
                                      dev_id, conn_id, ch);
                if (wr_coalesce_err == 0) {
                    next_st = QSFP_FSM_EN_ST_ENABLING;
                }
                // if coalesce failed, retry this state after 100ms
                //    (we want TX_DISABLE asserted before moving on)
                // if coalesce succeeded, move to ENABLING after 100ms
                //    and retry the whole sequence
                delay_ms = 100;  // ms
            } else {
                // notify driver
                qsfp_fsm_ch_notify_ready (dev_id, conn_id, ch);
                next_st = QSFP_FSM_EN_ST_ENABLED;
                delay_ms = 0;  // ms
            }
            break;
        case QSFP_FSM_EN_ST_ENABLED:
            // waiting to start disabling
            delay_ms = 100;  // ms
            break;
        case QSFP_FSM_EN_ST_DISABLING:
            // notify driver
            qsfp_fsm_ch_notify_not_ready (dev_id, conn_id,
                                          ch);

            qsfp_fsm_ch_disable_optical_tx (dev_id, conn_id,
                                            ch);
            next_st = QSFP_FSM_EN_ST_DISABLED;
            delay_ms = 100;  // ms
            break;
        default:
            assert (0);
            break;
    }
    qsfp_state[conn_id].per_ch_fsm[ch].fsm_st =
        next_st;
    qsfp_fsm_next_run_time_set (
        &qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time,
        delay_ms);
    if (st != next_st) {
        LOG_DEBUG ("QSFP    %2d : ch[%d] : %s --> %s (%dms)",
                   conn_id,
                   ch,
                   qsfp_fsm_ch_en_st_to_str[st],
                   qsfp_fsm_ch_en_st_to_str[next_st],
                   delay_ms);
    }
}

// called by qsfp fsm timer to process QSFPs for a given device
bf_pltfm_status_t qsfp_fsm (bf_dev_id_t dev_id)
{
    int conn_id;
    struct timespec now;

    clock_gettime (CLOCK_MONOTONIC, &now);

    extern int in_rtmr_init;
    if (in_rtmr_init) {
        LOG_ERROR ("ERROR: Both retimer init and qsfp fsm running");
        return BF_PLTFM_SUCCESS;
    }

    /*
     * Note: the channel FSM is run before the module FSM so any coalesced
     * writes are performed immediately. Otherwise they would have to wait
     * one extra poll period.
     */
    for (conn_id = 1; conn_id <= bf_pm_num_qsfp_get();
         conn_id++) {
        if (qsfp_state[conn_id].fsm_st ==
            QSFP_FSM_ST_DETECTED) {
            int ch;

            if (!qsfp_fsm_is_optical (conn_id)) {
                //fprintf (stdout, "QSFP %2d  is not optical\n",
                //         conn_id);
                continue;
            }

            for (ch = 0; ch < 4; ch++) {
                // skip terminal states
                if (qsfp_state[conn_id].per_ch_fsm[ch].fsm_st ==
                    QSFP_FSM_EN_ST_DISABLED) {
                    continue;
                }
                //if (qsfp_state[conn_id].per_ch_fsm[ch].fsm_st == QSFP_FSM_EN_ST_ENABLED)
                //  continue;

                if (qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time.tv_sec
                    >
                    now.tv_sec) {
                    continue;
                }
                if ((qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time.tv_sec
                     ==
                     now.tv_sec) &&
                    (qsfp_state[conn_id].per_ch_fsm[ch].next_fsm_run_time.tv_nsec
                     >
                     now.tv_nsec)) {
                    continue;
                }
                // time to run this QSFP (channel) state
                qsfp_channel_fsm_run (dev_id, conn_id, ch);
            }
        }
    }

    for (conn_id = 1; conn_id <= bf_pm_num_qsfp_get();
         conn_id++) {
        if (qsfp_state[conn_id].fsm_st !=
            QSFP_FSM_ST_IDLE) {
            if (qsfp_state[conn_id].next_fsm_run_time.tv_sec >
                now.tv_sec) {
                continue;
            }
            if ((qsfp_state[conn_id].next_fsm_run_time.tv_sec
                 == now.tv_sec) &&
                (qsfp_state[conn_id].next_fsm_run_time.tv_nsec >
                 now.tv_nsec)) {
                continue;
            }
            // time to run this QSFP (module) state
            qsfp_module_fsm_run (dev_id, conn_id);
        }
    }
    return BF_PLTFM_SUCCESS;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_oper_info_get (int conn_id,
                         bool *present,
                         uint8_t *pg0_lower,
                         uint8_t *pg0_upper)
{
    uint8_t pg = 0;
    bf_status_t rc;

    if (!present || !pg0_lower || !pg0_upper) {
        return;
    }
    if ((qsfp_state[conn_id].qsfp_type !=
         QSFP_TYP_OPTICAL) &&
        (qsfp_state[conn_id].qsfp_type !=
         QSFP_TYP_COPPER)) {
        *present = false;
        return;
    }
    *present = true;

    rc = bf_fsm_qsfp_rd (conn_id, 0, MAX_QSFP_PAGE_SIZE, pg0_lower);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading page 0 (lower)",
                   conn_id, rc);
        *present = false;
        return;
    }

    // first, set to pg 0 (just in case)
    qsfp_set_pg (conn_id, pg);

    rc = bf_fsm_qsfp_rd (conn_id, 0 + MAX_QSFP_PAGE_SIZE, MAX_QSFP_PAGE_SIZE,
                         pg0_upper);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading page 0 (upper)",
                   conn_id, rc);
        *present = false;
        return;
    }
}

/*****************************************************************
*
*****************************************************************/
void qsfp_oper_info_get_pg3 (int conn_id,
                             bool *present, uint8_t *pg3)
{
    uint8_t pg = 3;
    bf_status_t rc;

    if (!present || !pg3) {
        return;
    }
    if (qsfp_state[conn_id].qsfp_type !=
        QSFP_TYP_OPTICAL) {
        *present = false;
        return;
    }
    if (qsfp_state[conn_id].flat_mem) {
        *present = false;
        return;
    }

    *present = true;

    // first, set to pg 0 (just in case)
    pg = 3;
    qsfp_set_pg (conn_id, pg);

    rc = bf_fsm_qsfp_rd (conn_id, 0 + MAX_QSFP_PAGE_SIZE, MAX_QSFP_PAGE_SIZE, pg3);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> reading page 3",
                   conn_id, rc);
        *present = false;
        return;
    }

    // set back to pg 0 (just in case)
    pg = 0;
    qsfp_set_pg (conn_id, pg);
}

/*****************************************************************
*
*****************************************************************/
void qsfp_luxtera_lpbk (int conn_id,
                        bool near_lpbk)
{
    uint8_t pwd[] = {0x4d, 0xe6, 0x40, 0xbb};
    uint8_t pg90 = 0x90;
    uint8_t lpbk_mode_elec_near[] = {0xf2, 0x00, 0x02, 0x55, 0x55};
    uint8_t lpbk_mode_opt_far[] = {0xf2, 0x00, 0x02, 0xaa, 0xaa};
    uint8_t byte80, byte81;
    int rc;

    rc = bf_fsm_qsfp_wr (conn_id, 0x7b, 4, pwd);
    if (rc) {
        LOG_ERROR (
            "QSFP    %2d : Error <%d> Setting pwd for loopback mode",
            conn_id, rc);
        return;
    }

    // set page to 0x90
    rc = bf_fsm_qsfp_wr (conn_id, 0x7f, 1, &pg90);
    ;
    bf_sys_usleep (50000);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting page 0x90 for loopback mode",
                   conn_id,
                   rc);
        return;
    }

    if (near_lpbk) {
        rc = bf_fsm_qsfp_wr (conn_id, 0x84, 5,
                             lpbk_mode_elec_near);
    } else {
        rc = bf_fsm_qsfp_wr (conn_id, 0x84, 5,
                             lpbk_mode_opt_far);
    }
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting loopback mode <%s>",
                   conn_id,
                   rc,
                   near_lpbk ? "NEAR/ELEC." : "FAR/OPTICAL");
        return;
    }

    rc = bf_fsm_qsfp_rd (conn_id, 0x80, 1, &byte80);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Reading byte 0x80 for loopback mode",
                   conn_id,
                   rc);
        return;
    }
    byte80 += 1;
    rc = bf_fsm_qsfp_wr (conn_id, 0x80, 1, &byte80);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting byte 0x80 for loopback mode",
                   conn_id,
                   rc);
        return;
    }

    byte81 = 0x85;
    rc = bf_fsm_qsfp_wr (conn_id, 0x81, 1, &byte81);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting byte 0x81 for loopback mode",
                   conn_id,
                   rc);
        return;
    }
    LOG_ERROR ("QSFP    %2d : loopback mode <%s> set.",
               conn_id,
               near_lpbk ? "NEAR/ELEC." : "FAR/OPTICAL");
}

bool bf_pm_qsfp_is_luxtera (int conn_id)
{
    if (qsfp_needs_hi_pwr_init (conn_id)) {
        return true;
    } else {
        return false;
    }
}

void bf_pm_qsfp_luxtera_state_capture (
    int conn_id, uint8_t arr_0x3k[0x3000])
{
    int rc, pg = 0;
    uint8_t dump_tag = 0xdd;
    int chunk = 0;
    uint8_t luxtera_pwd[] = {0x64, 0xb0, 0x05, 0xa2};

    // set page 0
    pg = 0;
    qsfp_set_pg (conn_id, pg);

    // set dump tag
    rc = bf_fsm_qsfp_wr (conn_id, 0x7A, 1, &dump_tag);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting byte 0x81 for loopback mode",
                   conn_id,
                   rc);
    }

    // set luxtera password
    rc = bf_fsm_qsfp_wr (conn_id, 0x7B, 4,
                         luxtera_pwd);
    if (rc) {
        LOG_ERROR ("QSFP    %2d : Error <%d> Setting byte 0x81 for loopback mode",
                   conn_id,
                   rc);
    }

    // wait 10 seconds for dump to complete
    sleep (10);

    for (chunk = 0; chunk < 0x3000 / 128; chunk++) {
        rc = bf_fsm_qsfp_rd (
                 conn_id, 0x0801d000 + (chunk * 128), 128,
                 &arr_0x3k[ (chunk * 128)]);
        if (rc) {
            LOG_ERROR ("QSFP    %2d : Error <%d> Reading 128 bytes from %08x.",
                       conn_id,
                       rc,
                       0x0801d000 + (chunk * 128));
        }
    }
}

/*****************************************************************
*
*****************************************************************/
void qsfp_state_ha_config_set (bf_dev_id_t dev_id,
                               int conn_id)
{
    bool is_optical = false;
    int chnl_id;

    qsfp_fsm_notify_bf_pltfm (dev_id, conn_id);

    qsfp_fsm_identify_type (conn_id, &is_optical);
    qsfp_state[conn_id].fsm_st = QSFP_FSM_ST_DETECTED;

    /* Make sure all channel state is disabled */
    for (chnl_id = 0; chnl_id < QSFP_NUM_CHN;
         chnl_id++) {
        qsfp_state[conn_id].per_ch_fsm[chnl_id].fsm_st =
            QSFP_FSM_EN_ST_ENABLED;
    }
    qsfp_fsm_next_run_time_set (
        &qsfp_state[conn_id].next_fsm_run_time, 0);
    qsfp_state[conn_id].status_and_alarms.status2 = 0;
}

void qsfp_state_ha_config_delete (int conn_id)
{
    qsfp_state[conn_id].fsm_st = QSFP_FSM_ST_IDLE;
}

/*****************************************************************
*
*****************************************************************/
void qsfp_state_ha_enable_config_set (int conn_id,
                                      int chnl_id)
{
    bool is_optical = qsfp_fsm_is_optical (conn_id);
    if (is_optical) {
        qsfp_state[conn_id].per_ch_fsm[chnl_id].fsm_st =
            QSFP_FSM_EN_ST_ENABLED;
        qsfp_fsm_next_run_time_set (
            &qsfp_state[conn_id].per_ch_fsm[chnl_id].next_fsm_run_time,
            0);
        qsfp_state[conn_id].status_and_alarms.los_ind &=
            ~ (1 << chnl_id);
    }
}

/*****************************************************************
*
*****************************************************************/
void qsfp_state_ha_enable_config_delete (
    int conn_id, int chnl_id)
{
    qsfp_state[conn_id].per_ch_fsm[chnl_id].fsm_st =
        QSFP_FSM_EN_ST_DISABLED;
}

/*****************************************************************
* qsfp_num_lanes_get
*
* Utility fn to return number of lanes associate with a port
*****************************************************************/
static int qsfp_num_lanes_get (int conn_id,
                               int chnl_id)
{
    bf_pal_front_port_handle_t port_hdl;
    bf_dev_port_t dev_port;
    bf_dev_id_t dev_id = 0;
    bf_status_t bf_status;
    int num_lanes;

    port_hdl.conn_id = conn_id;
    port_hdl.chnl_id = chnl_id;
    FP2DP (dev_id, &port_hdl, &dev_port);
    bf_status = bf_port_num_lanes_get (0, dev_port,
                                       &num_lanes);
    if (bf_status == BF_SUCCESS) {
        return num_lanes;
    }
    LOG_DEBUG ("QSFP    %2d : ch[%d] : Error: %d : attempting to get num_lanes",
               conn_id,
               chnl_id,
               bf_status);
    return 0;
}
