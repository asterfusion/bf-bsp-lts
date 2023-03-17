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

/* As per SFF-8436, QSFP+ 10 Gbs 4X PLUGGABLE TRANSCEIVER spec */

enum {
    /* Size of page read from QSFP via I2C */
    MAX_QSFP_PAGE_SIZE = 128,
    /* Number of channels per module */
    CHANNEL_COUNT = 4,
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
    char name[32];
    char oui[12];
    char part_number[32];
    char rev[4];
    char serial_number[32];
    char date_code[16];
} qsfp_vendor_info_t;

typedef struct {
    int32_t single_mode_km;
    int32_t single_mode;
    int32_t om3;
    int32_t om2;
    int32_t om1;
    int32_t copper;
    struct {
        bool single_mode_km;
        bool single_mode;
        bool om3;
        bool om2;
        bool om1;
        bool copper;
    } _isset;
} qsfp_cable_t;

typedef enum {
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
    uint16_t module_capability;       //luyi:
    //      QSFP: (Page 0 Byte 131) << 8 | (Page 0 Byte 192)  i
    //      SFP: (0x00) | (0xA0 Byte 36)
    uint8_t module_type;
    uint8_t connector_type;
    int port;
    qsfp_global_sensor_t sensor;
    qsfp_alarm_threshold_t thresh;
    qsfp_vendor_info_t vendor;
    qsfp_cable_t cable;
    qsfp_channel_t chn[CHANNEL_COUNT];
    struct {
        bool type;
        bool sensor;
        bool vendor;
        bool cable;
        bool thresh;
        bool chn;
    } _isset;
} qsfp_transciever_info_t;

/* qsfp common APIs */
/* set custom power control */
void bf_qsfp_set_pwr_ctrl (int port);
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
/* get lpmode status mask of QSFPs */
int bf_qsfp_get_transceiver_lpmode (
    uint32_t *lower_ports,
    uint32_t *upper_ports,
    uint32_t *cpu_ports);
/* get presence status mask of QSFPs */
int bf_qsfp_set_transceiver_lpmode (int port,
                                    bool lpmode);
/* read the memory of qsfp transceiver */
int bf_qsfp_read_transceiver (unsigned int port,
                              int offset,
                              int len,
                              uint8_t *buf);
/* write the memory of qsfp transceiver */
int bf_qsfp_write_transceiver (unsigned int port,
                               int offset,
                               int len,
                               uint8_t *buf);
/* detect a transceiver */
int bf_qsfp_detect_transceiver (int port,
                                bool *is_present);

int bf_qsfp_get_transceiver_info (int port,
                                  qsfp_transciever_info_t *info);
/* force mark a transceiver present or not-present */
void bf_qsfp_set_present (int port, bool present);
/* return if a transceiver is present */
bool bf_qsfp_is_present (int port);
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
bool bf_qsfp_get_sensors_per_chan_info (int port,
                                        qsfp_channel_t *chn);
/* get cable information from QSFP memory */
bool bf_qsfp_get_cable_info (int port,
                             qsfp_cable_t *cable);
/* get sensor information from QSFP memory */
bool bf_qsfp_get_sensor_info (int port,
                              qsfp_global_sensor_t *info);
/* get various thresholds information from QSFP memory */
bool bf_qsfp_get_threshold_info (int port,
                                 qsfp_alarm_threshold_t *thresh);
/* get vendor information from QSFP memory */
bool bf_qsfp_get_vendor_info (int port,
                              qsfp_vendor_info_t *vendor);
/* get qsfp connector type */
int bf_qsfp_get_connector_type (int port,
                                uint8_t *value);
/* get qsfp module type */
int bf_qsfp_get_module_type (int port,
                             uint8_t *value);

/* read and cache QSFP memory */
int bf_qsfp_update_data (int port);
/* return QSFP cached info */
int bf_qsfp_get_cached_info (int port, int page,
                             uint8_t *buf);
/* enable or disable a QSFP module transmitter */
int bf_qsfp_tx_disable (int port, int channel,
                        bool disable);
/* is QSFP module transmitter disabled */
bool bf_qsfp_tx_is_disabled (int port,
                             int channel);
/* reset or unreset a QSFP module */
int bf_qsfp_reset (int port, bool reset);
/* Get reset state */
bool bf_qsfp_get_reset (int port);

/* get qsfp compliance code */
int bf_qsfp_get_eth_compliance (int port,
                                Ethernet_compliance *code);
/* get qsfp extended compliance code */
int bf_qsfp_get_eth_ext_compliance (int port,
                                    Ethernet_extended_compliance *code);
bool bf_qsfp_is_flat_mem(int port);
bool bf_qsfp_is_optic (bf_pltfm_qsfp_type_t
                       qsfp_type);
bool bf_qsfp_is_optical(int port);
uint8_t bf_qsfp_get_ch_cnt (int port);
uint8_t bf_qsfp_get_media_ch_cnt (int port);
void bf_qsfp_debug_clear_all_presence_bits (void);
bool bf_qsfp_is_luxtera_optic (int conn_id);
int bf_qsfp_special_case_set (int port,
                              bf_pltfm_qsfp_type_t qsfp_type,
                              bool is_set);
bool bf_qsfp_is_dd (int port);

bf_dev_port_t bf_get_dev_port_by_interface_name (
    char *name);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_qsfp_ucli_node_create (
    ucli_node_t *m);
#endif

int bf_qsfp_type_get (int port,
                      bf_pltfm_qsfp_type_t *qsfp_type);
#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_QSFP_H */
