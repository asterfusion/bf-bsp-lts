/* sfp cli header file
 * author : luyi
 * date: 2020/07/02
 */


#ifndef _BF_SFP_H
#define _BF_SFP_H

#include <bf_types/bf_types.h>
#include <bf_qsfp/sff.h>
#include <bf_qsfp/bf_qsfp.h>

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SFF_PAGE_SIZE       128

#define MAX_NAME_LEN                80
// Connector Type  from SFF-8024 */
#define MAX_CONNECTOR_TYPE_NAME_LEN         MAX_NAME_LEN


/* As per SFF-8436, QSFP+ 10 Gbs 4X PLUGGABLE TRANSCEIVER spec */

enum {
    /* Size of page read from SFP via I2C */
    MAX_SFP_PAGE_SIZE = MAX_SFF_PAGE_SIZE,
    /* limit of memory address space in SFP */
    MAX_SFP_PAGE_SIZE_255 = 255,
};

typedef enum {
    SFP_LR,
    SFP_SR,
    SFP_UNKNOWN
} sfp_transceiver_type;

typedef qsfp_channel_sensor_t sfp_channel_sensor_t;
typedef qsfp_channel_t sfp_channel_t;
typedef qsfp_global_sensor_t sfp_global_sensor_t;
typedef qsfp_alarm_threshold_t sfp_alarm_threshold_t;
typedef qsfp_cable_t sfp_cable_t;
typedef qsfp_vendor_info_t sfp_vendor_info_t;
typedef qsfp_transciever_info_t sfp_transciever_info_t;
typedef bf_pltfm_qsfp_type_t bf_pltfm_sfp_type_t;

/* force mark a transceiver present or not-present */
void bf_sfp_set_present (int port,
                         bool present);
/* return if a transceiver is present */
bool bf_sfp_is_present (int port);
bool bf_sfp_is_sff8472 (int port);
void bf_sfp_debug_clear_all_presence_bits (void);

int bf_sfp_type_get (int port,
                     bf_pltfm_sfp_type_t *sfp_type);

MemMap_Format bf_sfp_get_memmap_format (int port);

bool bf_sfp_is_optical (int port);
bool bf_sfp_is_passive_cu (int port);

void bf_sfp_set_num (int num_ports);
int bf_sfp_get_max_sfp_ports (void);

int bf_sfp_get_transceiver_pres (uint32_t
                                 *lower_ports,
                                 uint32_t *upper_ports);
void bf_sfp_set_pwr_ctrl (int port, bool lpmode);

bool bf_sfp_soft_removal_get (int port);
void bf_sfp_soft_removal_set (int port,
                              bool removed);

int bf_sfp_set_transceiver_lpmode (int port,
                                   bool lpmode);

int bf_sfp_get_transceiver_info (int port,
                                 sfp_transciever_info_t *info);

int bf_sfp_init();
bool bf_sfp_get_dom_support (int port);
bool bf_sfp_get_chan_tx_bias (int port,
                               sfp_channel_t *chn);
bool bf_sfp_get_chan_tx_pwr (int port,
                              sfp_channel_t *chn);
bool bf_sfp_get_chan_rx_pwr (int port,
                              sfp_channel_t *chn);
bool bf_sfp_get_chan_volt (int port,
                            sfp_global_sensor_t *chn);
bool bf_sfp_get_chan_temp (int port,
                            sfp_global_sensor_t *chn);

int bf_sfp_update_cache (int port);
int bf_sfp_get_cached_info (int port, int page,
                            uint8_t *buf);

int bf_sfp_detect_transceiver (int port,
                               bool *present);
int bf_sfp_module_read (int port, int offset,
                        int len, uint8_t *buf);
int bf_sfp_module_write (int port, int offset,
                         int len, uint8_t *buf);

/* Get reset state */
bool bf_sfp_get_reset (int port);
int bf_sfp_reset (int port, bool reset);

int bf_sfp_tx_disable (int port, bool tx_dis);

int bf_sfp_get_conn (int port, uint32_t *conn,
                     uint32_t *chnl);
int bf_sfp_get_port (uint32_t conn, uint32_t chnl,
                     int *port);

int check_sfp_module_vendor (uint32_t conn_id,
                             uint32_t chnl_id,
                             char *target_vendor);
void bf_sfp_print_ddm (int port, sfp_global_sensor_t *trans, sfp_channel_t *chnl);

bool bf_sfp_is_detected (int port);
void bf_sfp_set_detected (int port, bool detected);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_sfp_ucli_node_create (
    ucli_node_t *m);
#endif

#ifdef __cplusplus
}
#endif /* C++ */


#endif /* _BF_SFP_H */
