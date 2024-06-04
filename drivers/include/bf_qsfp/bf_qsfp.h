/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

/*******************************************************************************
 *  the contents of this file is partially derived from
 *  https://github.com/facebook/fboss
 *
 *  Many changes have been applied all thru to derive the contents in this
 *  file.
 *  Please refer to the licensing information on the link proved above.
 *
 ******************************************************************************/
#ifndef _BF_QSFP_H
#define _BF_QSFP_H

#include <bf_types/bf_types.h>
#include <bf_qsfp/sff.h>

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* After an optical transceiver plugged-in, by default the laser is off
 * till user performs port-enb on it. While this macro allows to turn on
 * laser without performing port-enb. This feature allow faster link to
 * remote for some kind of cases. Also, this feature can be implemented
 * to a transceiver as well by clearing flags BF_TRANS_CTRLMASK_LASER_OFF
 * in /etc/transceiver-cases.conf if you know its Vendor and PN.
 * .
 * by tsihang, 2023/09/01. */
//#define DEFAULT_LASER_ON

/* Control word added by tsihang for /etc/transceiver-cases.conf, 2023/05/05. */
#define BF_TRANS_CTRLMASK_RX_CDR_OFF            (1 << 0)
#define BF_TRANS_CTRLMASK_TX_CDR_OFF            (1 << 1)
#define BF_TRANS_CTRLMASK_CDR_OFF               (BF_TRANS_CTRLMASK_RX_CDR_OFF|BF_TRANS_CTRLMASK_TX_CDR_OFF) /* PFXONLV1-1418 with QSFP P/N TSQ885S101E1 from Teraspek. */
#define BF_TRANS_CTRLMASK_LASER_OFF             (1 << 2) /* Keep laser off until user enable it. */
#define BF_TRANS_CTRLMASK_OVERWRITE_DEFAULT     (1 << 7)
#define BF_TRANS_CTRLMASK_IGNORE_RX_LOS         (1 << 16) // start here for tof2 QSFP-DD
#define BF_TRANS_CTRLMASK_IGNORE_RX_LOL         (1 << 17)
#define BF_TRANS_CTRLMASK_FSM_LOG_ENA           (1 << 18) // More detailed log for a given port, disabled by default and
// enabled via command "bf-sde> [qsfp|sfp] ctrlmask-set 0x4****". This feature helps to reduce log in bf_drivers.log.
// Any new logic implementation should consider of this.
#define BF_TRANS_CTRLMASK_FORCELY_APPLY_DPINIT  (1 << 24) // The bit starts from here should never been seen in /etc/transceiver-cases.conf.
#define BF_TRANS_CTRLMASK_IGNORE_DPAPPLY_STATE  (1 << 25)
#define BF_TRANS_CTRLMASK_REQUIRE_HIGH_PWR_INIT (1 << 26)

/* Cached state for [q]sfp_info[port].trans_state.
 * To be clarified, the belowing runtime state will not be updated
 * if forcely configured via writing through its register by i2c.
 * added by tsihang, 2023/08/31 */
#define BF_TRANS_STATE_RX_CDR_ON             (1 << 0)
#define BF_TRANS_STATE_TX_CDR_ON             (1 << 1)
#define BF_TRANS_STATE_LASER_ON              (1 << 2)
#define BF_TRANS_STATE_DETECTED              (1 << 3)
#define BF_TRANS_STATE_SOFT_REMOVED          (1 << 4)
#define BF_TRANS_STATE_RESET                 (1 << 5)

#define QSFP_RXLOS_DEBOUNCE_DFLT \
  0  // Can modify depending on if debouncing is required. This value can be set
// in corelation with FLAG_POLL_INTERVAL_MS. If FLAG_POLL_INTERVAL_MS is
// high enough, then debouncing can be avoided.

/* As per SFF-8436, QSFP+ 10 Gbs 4X PLUGGABLE TRANSCEIVER spec */

enum {
    /* Size of page read from QSFP via I2C */
    MAX_QSFP_PAGE_SIZE = 128,
    /* limit of memory address space in QSFP */
    MAX_QSFP_PAGE_SIZE_255 = 255,
    /* Maximum number of channels per module */
    CHANNEL_COUNT = 8,
    /* Maximum cable length reported */
    MAX_CABLE_LEN = 255,
};

typedef sff_flags_level_t qsfp_flags_level_t;
typedef struct {
    sff_sensor_t rx_pwr;
    sff_sensor_t tx_bias;
    sff_sensor_t tx_pwr;
} qsfp_channel_sensor_t;

typedef struct {
    uint32_t chn;
    qsfp_channel_sensor_t sensors;
} qsfp_channel_t;

typedef struct {
    sff_sensor_t temp;
    sff_sensor_t vcc;
} qsfp_global_sensor_t;

typedef struct {
    double low;
    double high;
} qsfp_threshold_t;

typedef struct {
    qsfp_threshold_t warn;
    qsfp_threshold_t alarm;
} qsfp_threshold_level_t;

typedef struct {
    qsfp_threshold_level_t temp;
    qsfp_threshold_level_t vcc;
    qsfp_threshold_level_t rx_pwr;
    qsfp_threshold_level_t tx_bias;
    qsfp_threshold_level_t tx_pwr;
} qsfp_alarm_threshold_t;

typedef struct {
    char name[17];
    char part_number[17];
    char rev[3];
    char serial_number[17];
    char date_code[9];
    uint8_t oui[3];
} qsfp_vendor_info_t;

typedef struct {
    bool is_cable_assy;
    int32_t xcvr_single_mode;
    int32_t xcvr_om5;
    int32_t xcvr_om4;
    int32_t xcvr_om3;
    int32_t xcvr_om2;
    int32_t xcvr_om1;
    float cable_assy;
    struct {
        bool xcvr_single_mode;
        bool xcvr_om5;
        bool xcvr_om4;
        bool xcvr_om3;
        bool xcvr_om2;
        bool xcvr_om1;
        bool cable_assy;
    } _isset;
} qsfp_cable_t;

typedef struct {
    uint8_t module_state;
    char module_state_str[15];
    bool lowPwr;
    bool forceLowPwr;
} qsfp_module_state_info_t;

typedef struct {
    uint8_t host_if_id;
    uint8_t media_if_id;
    char host_if_id_str[41];
    char media_if_id_str[41];
    uint8_t host_lane_cnt;
    uint8_t media_lane_cnt;
    uint8_t host_lane_assign_mask;
    uint8_t media_lane_assign_mask;
} qsfp_application_info_t;

typedef struct {
    uint8_t selected_ApSel_code;
    uint8_t data_path_id;
    bool explicit_control;
    bool adaptive_eq_en;
    //  uint8_t adaptive_input_eq_recall;  not currently needed in BF S/W
    uint8_t fixed_tx_input_eq;
    bool tx_cdr_en;
    bool rx_cdr_en;
    uint8_t rx_output_eq_pre;
    uint8_t rx_output_eq_post;
    uint8_t rx_output_ampl;
} qsfp_datapath_staged_set_info_t;  // per lane

typedef struct {
    DataPath_State datapath_state;
    char datapath_state_str[25];
    char datapath_state_str_short[5];
    bool data_path_deinit_bit;
} qsfp_datapath_state_info_t;  // per lane

typedef enum {
    qsfp,
    QSFP_100G_AOC,
    QSFP_100GBASE_SR4,
    QSFP_100GBASE_LR4,
    QSFP_100GBASE_ER4,
    QSFP_100GBASE_SR10,
    QSFP_100GBASE_CR4,
    QSFP_100GBASE_LRM,
    QSFP_100GBASE_LR,
    QSFP_100GBASE_SR,
    QSFP_40GBASE_CR4,
    QSFP_40GBASE_SR4,
    QSFP_40GBASE_LR4,
    QSFP_40GBASE_AC,
    QSFP_UNKNOWN
} transceiver_type;

typedef struct {
    bool present;
    transceiver_type transceiver;
    int port;

    /* The following 3 entries are extended by tsihang. 2023-03-24. */
    uint8_t module_type;        /* See Module_identifier */
    uint8_t connector_type;     /* See Connector_type */
    uint16_t module_capability;

    qsfp_global_sensor_t sensor;
    qsfp_alarm_threshold_t thresh;
    qsfp_vendor_info_t vendor;
    qsfp_cable_t cable;
    qsfp_channel_t chn[CHANNEL_COUNT];
    struct {
        /* The following 3 entries are extended by tsihang. 2023-03-24. */
        bool mtype;
        bool ctype;
        bool mcapa;

        bool sensor;
        bool vendor;
        bool cable;
        bool thresh;
        bool chn;
    } _isset;
} qsfp_transciever_info_t;

// Read/write vectors to be registered by platforms
typedef struct _bf_qsfp_vec_ {
    int (*pltfm_write) (unsigned int port,
                        uint8_t bank,
                        uint8_t page,
                        int offset,
                        int len,
                        uint8_t *buf,
                        uint32_t debug_flags,
                        void *);  // void for future/platform specific

    int (*pltfm_read) (unsigned int port,
                       uint8_t bank,
                       uint8_t page,
                       int offset,
                       int len,
                       uint8_t *buf,
                       uint32_t debug_flags,
                       void *);  // void for future/platform specific

} bf_qsfp_vec_t;

/* qsfp common APIs */
/* set custom power control through software reg */
void bf_qsfp_set_pwr_ctrl (int port, bool lpmode);
/* get custom power control through software reg */
int bf_qsfp_get_pwr_ctrl(int port);
/* get interrupt status mask of QSFPs */
void bf_qsfp_get_transceiver_int (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports);
/* get presence status mask of QSFPs */
int bf_qsfp_get_transceiver_pres (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports);
int bf_qsfp_read_transceiver (unsigned int port,
                              int offset,
                              int len,
                              uint8_t *buf);
int bf_qsfp_write_transceiver (unsigned int port,
                               int offset,
                               int len,
                               uint8_t *buf);
int bf_qsfp_update_monitor_value (int port);

/* get lpmode status mask of QSFPs */
int bf_qsfp_get_transceiver_lpmode (
    uint32_t *lower_ports,
    uint32_t *upper_ports,
    uint32_t *cpu_ports);
/* get presence status mask of QSFPs */
int bf_qsfp_set_transceiver_lpmode (int port,
                                    bool lpmode);
/* Performs a read-modify-write to a single-byte bitfield in qsfp memory */
int bf_qsfp_bitfield_rmw (
    int port, Sff_field field, uint host_chmask,
    uint chmask, uint newdata);
/** read from a specific location in the memory map **/
int bf_qsfp_module_read (
    unsigned int port, int bank, int page, int offset,
    int len, uint8_t *buf);
/** write to a specific location in the memory map **/
int bf_qsfp_module_write (
    unsigned int port, int bank, int page, int offset,
    int len, uint8_t *buf);
/* detect a transceiver */
int bf_qsfp_detect_transceiver (int port,
                                bool *is_present);
/* get transceiver information */
void bf_qsfp_get_transceiver_info (int port,
                                   qsfp_transciever_info_t *info);
/* force mark a transceiver present or not-present */
void bf_qsfp_set_present (int port, bool present);
// returns the spec revision, CMIS modules only
int bf_cmis_spec_rev_get (int port,
                          uint8_t *major, uint8_t *minor);
/* return if a transceiver is present */
bool bf_qsfp_is_present (int port);
/* return if a transceiver checksum has passed */
bool bf_qsfp_is_checksum_passed (int port);
/* init qsfp module internal data structure */
int bf_qsfp_init();
/* deinit qsfp module internal data structure per port */
int bf_qsfp_deinit (int port);
/* set the number of qsfp ports, excluding CPU port */
void bf_qsfp_set_num (int num_ports);
/* get the number of qsfp ports, excluding CPU port */
int bf_qsfp_get_max_qsfp_ports (void);
/* if qsfp module supports pages */
bool bf_qsfp_has_pages (int port);
/* get per channel information of QSFP */
bool bf_qsfp_get_chan_sensor_data (int port,
                                   qsfp_channel_t *chn);
/* get cable information from QSFP memory */
bool bf_qsfp_get_cable_info (int port,
                             qsfp_cable_t *cable);
/* return cmis module state information */
bool bf_qsfp_get_module_state_info (int port,
                                    qsfp_module_state_info_t *module_state_info);
/* return cmis datapath state information */
bool bf_qsfp_get_dp_state_info (int port,
                                int num_ch,
                                qsfp_datapath_state_info_t *dp_state_info);
/* return cmis datapath configuration in the specified control set */
bool bf_qsfp_get_dp_state_config (int port,
                                  int num_ch,
                                  qsfp_datapath_staged_set_info_t *set_cfg,
                                  Control_Sets cur_set);
/* returns TX bias current for each channel */
bool bf_qsfp_get_chan_tx_bias (int port,
                               qsfp_channel_t *chn);
/* returns TX power for each channel */
bool bf_qsfp_get_chan_tx_pwr (int port,
                              qsfp_channel_t *chn);
/* returns RX power for each channel */
bool bf_qsfp_get_chan_rx_pwr (int port,
                              qsfp_channel_t *chn);
/* returns TX bias alarm for each channel */
bool bf_qsfp_get_chan_tx_bias_alarm (int port,
                                     qsfp_channel_t *chn);
/* returns TX power alarm for each channel */
bool bf_qsfp_get_chan_tx_pwr_alarm (int port,
                                    qsfp_channel_t *chn);
/* returns RX power alarm for each channel */
bool bf_qsfp_get_chan_rx_pwr_alarm (int port,
                                    qsfp_channel_t *chn);
/* returns voltage */
double bf_qsfp_get_voltage (int port);
/* returns array of flag values for each channel */
int bf_qsfp_get_flag_for_all_chans (int port,
                                    Sff_flag field, int *flag_val);
int bf_qsfp_get_threshold_temp (int port,
                                qsfp_threshold_level_t *thresh);
int bf_qsfp_get_threshold_vcc (int port,
                               qsfp_threshold_level_t *thresh);
int bf_qsfp_get_threshold_rx_pwr (int port,
                                  qsfp_threshold_level_t *thresh);
int bf_qsfp_get_threshold_tx_bias (int port,
                                   qsfp_threshold_level_t *thresh);
/* returns the active and inactive firmware versions */
bool bf_qsfp_get_firmware_ver (int port,
                               uint8_t *active_ver_major,
                               uint8_t *active_ver_minor,
                               uint8_t *inactive_ver_major,
                               uint8_t *inactive_ver_minor);
/* returns the hardware version */
bool bf_qsfp_get_hardware_ver (int port,
                               uint8_t *hw_ver_major,
                               uint8_t *hw_ver_minor);
/* get module-level sensor information from QSFP memory */
bool bf_qsfp_get_module_sensor_info (int port,
                                     qsfp_global_sensor_t *info);
/* get various thresholds information from QSFP memory */
bool bf_qsfp_get_threshold_info (int port,
                                 qsfp_alarm_threshold_t *thresh);
/* return qsfp temp sensor information */
double bf_qsfp_get_temp_sensor (int port);
/* return module type identifier number */
int bf_qsfp_get_module_type (int port,
                             uint8_t *module_type);
/* return module type identifier string */
bool bf_qsfp_get_module_type_str (int port,
                                  char *module_type_str,
                                  bool incl_hex);
/* get vendor information from QSFP memory */
bool bf_qsfp_get_vendor_info (int port,
                              qsfp_vendor_info_t *vendor);
/* return QSFP cached info */
int bf_qsfp_get_cached_info (int port, int page,
                             uint8_t *buf);
/* enable or disable a QSFP module transmitter */
int bf_qsfp_tx_disable (int port,
                        int channel_mask, bool disable);
/* enable or disable a QSFP channel transmitter */
int bf_qsfp_tx_disable_single_lane (int port,
                                    int channel, bool disable);
/* is QSFP module transmitter disabled */
bool bf_qsfp_tx_is_disabled (int port,
                             int channel);
int bf_qsfp_cdr_disable_single_lane (int port,
                                    int channel, bool disable);
int bf_qsfp_cdr_disable (int port,
                        int channel_mask, bool disable);
/* deactivate all data paths */
int bf_qsfp_dp_deactivate_all (int port);
int bf_qsfp_dp_activate_all (int port);
/* reset or unreset a QSFP module */
int bf_qsfp_reset (int port, bool reset);
/* return the requested media type string */
int bf_qsfp_get_media_type_str (int port,
                                char *media_type_str);
/* get the number of advertsed Applications */
int bf_qsfp_get_application_count (int port);
/* get qsfp compliance code */
int bf_qsfp_get_eth_compliance (int port,
                                Ethernet_compliance *code);
/* get qsfp extended compliance code */
int bf_qsfp_get_eth_ext_compliance (int port,
                                    Ethernet_extended_compliance *code);
/* get qsfp secondary extended compliance code */
int bf_qsfp_get_eth_secondary_compliance (
    int port,
    Ethernet_extended_compliance *code);
bool bf_qsfp_is_flat_mem (int port);
bool bf_qsfp_is_passive_cu (int port);
bool bf_qsfp_is_optical (int port);
uint8_t bf_qsfp_get_ch_cnt (int port);
uint8_t bf_qsfp_get_media_ch_cnt (int port);
bool bf_qsfp_get_spec_rev_str (int port,
                               char *rev_str);
void bf_qsfp_debug_clear_all_presence_bits (void);
bool bf_qsfp_is_luxtera_optic (int conn_id);
int bf_qsfp_special_case_set (int port,
                              bf_pltfm_qsfp_type_t qsfp_type,
                              bool is_set);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_qsfp_ucli_node_create (
    ucli_node_t *m);
#endif

int bf_qsfp_type_get (int port,
                      bf_pltfm_qsfp_type_t *qsfp_type);
bool bf_qsfp_is_cable_assy (int port);
int bf_qsfp_is_cdr_required (int port, bool *rx_cdr_required, bool *tx_cdr_required);

bool bf_qsfp_is_lol_required (int port);

const char *bf_qsfp_get_conn_type_string (
    int port);
bool qsfp_needs_hi_pwr_init (int conn_id);
int bf_qsfp_is_fsm_logging (int port);
bool bf_qsfp_is_cmis (int port);
bool bf_qsfp_is_sff8636 (int port);
MemMap_Format bf_qsfp_get_memmap_format (
    int port);
int bf_cmis_type_get (int port,
                      bf_pltfm_qsfpdd_type_t *qsfp_type);
const char *bf_cmis_get_media_type_string (
    int port);
int bf_cmis_find_matching_Application (int port,
                                       bf_port_speed_t speed,
                                       int nlanes,
                                       int firstLane);
int bf_cmis_select_Application (int port,
                                int chnum,
                                uint8_t ApSel,
                                uint8_t datapath_ID,
                                bool explicit_control);
/* return the requested Application info */
int bf_qsfp_get_application (int port,
                             int ApSel,
                             qsfp_application_info_t *app_info);
int bf_cmis_get_datapath_state (int port,
                                int ch,
                                DataPath_State *datapath_state);
const char* bf_cmis_get_datapath_state_str(int port,
        int ch,
        DataPath_State state);
int bf_cmis_get_datapath_config_status (
    int port, int ch,
    DataPath_Config_Status *datapath_config_status);
const char* bf_cmis_get_datapath_config_state_str(int port,
        int ch,
        DataPath_Config_Status state);
// Generic applies to all platforms
void bf_qsfp_soft_removal_set (int port,
                               bool removed);
bool bf_qsfp_soft_removal_get (int port);

int bf_qsfp_field_read_onebank (int port,
                                Sff_field field,
                                int chmask,
                                int addl_offset,
                                int max_length,
                                uint8_t *data);

/* refresh the cache for all of the per-module and per-lane flags */
int bf_qsfp_refresh_flags (int port);
/* clears the local cached copy of the specified flag */
int bf_qsfp_clear_flag (int port, Sff_flag flag,
                        int ln, int bit, bool clrcnt);
/* returns the last read value of the specified flag */
int bf_qsfp_get_flag (int port, Sff_flag flag,
                      int ln, int bit, uint32_t *cnt);
int bf_cmis_get_Application_media_info (int port,
                                        int app,
                                        int host_lane,
                                        int *media_lane,
                                        int *media_firstLane,
                                        int *media_nLanes);
int bf_cmis_get_module_state (int port,
                              Module_State *module_state,
                              bool *intl_deasserted);
const char* bf_cmis_get_module_state_str(int port,
        Module_State state);
int bf_cmis_get_module_fault_cause (int port,
                              Module_Fault_Cause *module_fault);
const char *bf_cmis_get_module_fault_cause_str (int port,
        Module_Fault_Cause state);

void qsfp_fsm_force_all_lpmode();
bool qsfp_fsm_query_all_lpmode();
int bf_qsfp_vec_init (bf_qsfp_vec_t *vec);
void bf_qsfp_get_sff_eth_extended_code_description (
    int port,
    char *ext_code_desc);

/* read from cache if possible, if not - directly */
bf_pltfm_status_t bf_qsfp_module_cached_read (
    unsigned int port,
    int bank,
    int page,
    int offset,
    int length,
    uint8_t *buf);
/* Get reset state */
bool bf_qsfp_get_reset (int port);
void bf_qsfp_internal_port_set (int port,
                                bool set);
bool bf_qsfp_internal_port_get (int port);
int bf_qsfp_rxlos_debounce_get (int port);
int bf_qsfp_rxlos_debounce_set (int port,
                                int count);
int bf_cmis_get_stagedSet (int port,
                           int chnum,
                           uint8_t *ApSel,
                           uint8_t *datapath_ID,
                           bool *explicit_control);
int bf_cmis_get_activeSet (int port,
                           int chnum,
                           uint8_t *ApSel,
                           uint8_t *datapath_ID,
                           bool *explicit_control);
double bf_qsfp_get_cached_module_temper (
    int port);

int bf_qsfp_get_cdr_support (int port,
                         bool *rx_cdr_support, bool *tx_cdr_support);
int bf_qsfp_get_rx_cdr_ctrl_support (int port,
                       bool *rx_cdr_ctrl_support);
int bf_qsfp_get_rxtx_cdr_ctrl_state (int port,
                         uint8_t *cdr_ctrl_state);
int bf_qsfp_get_tx_cdr_ctrl_support (int port,
                       bool *tx_cdr_ctrl_support);
int bf_qsfp_get_rx_cdr_lol_support (int port,
                      bool *rx_cdr_lol_support);
int bf_qsfp_get_tx_cdr_lol_support (int port,
                      bool *tx_cdr_lol_support);
void bf_qsfp_print_ddm (int conn_id, qsfp_global_sensor_t *trans, qsfp_channel_t *chnl);

int bf_qsfp_ctrlmask_set (int port,
                uint32_t ctrlmask);
int bf_qsfp_ctrlmask_get (int port,
                uint32_t *ctrlmask);
bool bf_qsfp_is_force_overwrite (int port);
int bf_qsfp_trans_state_set (int port,
                  uint32_t transate);
int bf_qsfp_trans_state_get (int port,
                  uint32_t *transate);
void bf_pltfm_qsfp_load_conf ();
int bf_qsfp_tc_entry_add (char *vendor, char *pn, char *option,
    uint32_t ctrlmask);
int bf_qsfp_tc_entry_dump (char *vendor, char *pn, char *option);
int bf_qsfp_tc_entry_find (char *vendor, char *pn, char *option,
    uint32_t *ctrlmask, bool rmv);
bool bf_qsfp_is_detected (int port);
void bf_qsfp_set_detected (int port, bool detected);

bf_dev_port_t bf_get_dev_port_by_interface_name (
    char *name);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_QSFP_H */
