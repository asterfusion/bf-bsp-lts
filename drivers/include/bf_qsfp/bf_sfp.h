/* sfp cli header file
 * author : luyi
 * date: 2020/07/02
 */


#ifndef _BF_SFP_H
#define _BF_SFP_H

#include <bf_qsfp/sff.h>
#include <bf_qsfp/bf_qsfp.h>

#ifdef INC_PLTFM_UCLI
#include <bfutils/uCli/ucli.h>
#endif

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#define BF_PLAT_MAX_SFP        (BF_PLAT_MAX_QSFP * MAX_CHAN_PER_CONNECTOR)


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

typedef sff_flags_level_t sfp_flags_level_t;
typedef struct {
    sff_sensor_t rx_pwr;
    sff_sensor_t tx_bias;
    sff_sensor_t tx_pwr;
} sfp_channel_sensor_t;

typedef struct {
    uint32_t chn;
    sfp_channel_sensor_t sensors;
} sfp_channel_t;

typedef struct {
    sff_sensor_t temp;
    sff_sensor_t vcc;
} sfp_global_sensor_t;

typedef struct {
    double low;
    double high;
} sfp_threshold_t;

typedef struct {
    sfp_threshold_t warn;
    sfp_threshold_t alarm;
} sfp_threshold_level_t;

typedef struct {
    sfp_threshold_level_t temp;
    sfp_threshold_level_t vcc;
    sfp_threshold_level_t rx_pwr;
    sfp_threshold_level_t tx_bias;
    sfp_threshold_level_t tx_pwr;
} sfp_alarm_threshold_t;

typedef struct bf_sfp_info_t {
    bool present;
    bool reset;
    bool soft_removed;
    bool flat_mem;
    bool passive_cu;
    //uint8_t num_ch;
    MemMap_Format memmap_format;

    /* cached SFP values */
    bool cache_dirty;

    /* idprom for 0xA0h byte 0-127. */
    uint8_t idprom[MAX_SFP_PAGE_SIZE];
    uint8_t a2h[MAX_SFP_PAGE_SIZE];

    uint8_t page0[MAX_SFP_PAGE_SIZE];
    uint8_t page1[MAX_SFP_PAGE_SIZE];
    uint8_t page2[MAX_SFP_PAGE_SIZE];
    uint8_t page3[MAX_SFP_PAGE_SIZE];

    uint8_t media_type;

    sfp_alarm_threshold_t alarm_threshold;

    /* Big Switch Network decoding engine. */
    sff_eeprom_t se;

    //bf_qsfp_special_info_t special_case_port;
    bf_sys_mutex_t sfp_mtx;
} bf_sfp_info_t;

/* force mark a transceiver present or not-present */
void bf_sfp_set_present (int port,
                         bool present);
/* return if a transceiver is present */
bool bf_sfp_is_present (int port);

int bf_sfp_type_get (int port,
                     bf_pltfm_qsfp_type_t *qsfp_type);

bool bf_sfp_is_optical (int port);
bool bf_sfp_is_passive_cu (int port);

void bf_sfp_set_num (int num_ports);
int bf_sfp_get_max_sfp_ports (void);

int bf_sfp_get_transceiver_pres (uint32_t
                                 *lower_ports,
                                 uint32_t *upper_ports);
bool bf_sfp_soft_removal_get (int port);
void bf_sfp_soft_removal_set (int port,
                              bool removed);

int bf_sfp_set_transceiver_lpmode (int port,
                                   bool lpmode);

int bf_sfp_get_transceiver_info (int port,
                                 qsfp_transciever_info_t *info);

int bf_sfp_init();

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

int bf_sfp_get_conn (int port, uint32_t *conn,
                     uint32_t *chnl);
int bf_sfp_get_port (uint32_t conn, uint32_t chnl,
                     int *port);

// for sfp info
bf_sfp_info_t *bf_sfp_get_info (int port);

int check_sfp_module_vendor (uint32_t conn_id,
                             uint32_t chnl_id,
                             char *target_vendor);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_sfp_ucli_node_create (
    ucli_node_t *m);
#endif

#ifdef __cplusplus
}
#endif /* C++ */


#endif /* _BF_SFP_H */
