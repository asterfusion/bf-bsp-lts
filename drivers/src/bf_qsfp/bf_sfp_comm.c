/* sfp comm source file
 * author : luyi
 * date: 2020/07/03
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_qsfp/bf_sfp.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_sfp.h>

static bool sfp_debug_on = true;
static int bf_plt_max_sfp;

typedef struct bf_sfp_info_t {
    bool present;
    bool reset;
    bool soft_removed;
    bool flat_mem;
    bool passive_cu;
    bool fsm_detected;  /* by tsihang, 2023-06-26. */
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
    double module_temper_cache;

    //bf_qsfp_special_info_t special_case_port;
    bf_sys_mutex_t sfp_mtx;

    /* BF_TRANS_CTRLMASK_XXXX. It can only be set by bf_sfp_ctrlmask_set. */
    uint32_t ctrlmask;
} bf_sfp_info_t;

/*
 * Access is 1 based index, so zeroth index would be unused.
 * For any APIs mentioned in this file trying to index and
 * access bf_sfp_info_t, please FIRST get the param @port
 * through bf_sfp_get_port(conn, chnl, &port), then the
 * access of bf_sfp_info_arr[port] is allowed and right.
 *
 * by tsihang, 2021-07-18.
 */
static bf_sfp_info_t
bf_sfp_info_arr[BF_PLAT_MAX_SFP + 1];

/* Big Switch Network decoding engine. */
sff_eeprom_t bf_sfp_sff_eeprom[BF_PLAT_MAX_SFP +
                                               1];

/* converts qsfp temp sensor data to absolute temperature */
static double get_temp (const uint16_t temp)
{
    double data;
    data = temp / 256.0;
    if (data > 128) {
        data = data - 256;
    }
    return data;
}

/* converts qsfp vcc sensor data to absolute voltage */
static double get_vcc (const uint16_t temp)
{
    double data;
    data = temp / 10000.0;
    return data;
}

/* converts qsfp txbias sensor data to absolute value */
static double get_txbias (const uint16_t temp)
{
    double data;
    //data = temp * 2.0 / 1000;
    data = (131.0 * temp) / 65535;
    return data;
}

/* converts qsfp power sensor data to absolute power value */
static double get_pwr (const uint16_t temp)
{
    double data;
    data = temp * 0.1 / 1000;
    return data;
}

/* by tsihang, 2021-07-29. */
static bool bf_sfp_is_eth_ext_compliance_copper (
    uint8_t ext_comp)
{
    // fitler only copper cases; all other cases are optical
    switch (ext_comp) {
        case 0xB:
        case 0xC:
        case 0xD:
        case 0x16:
        case 0x1C:
        case 0x1D:
        case 0x1E:
        case 0x30:
        case 0x32:
        case 0x40:
            return true;
        default:
            break;
    }
    return false;
}

/** return qsfp ethernet extended compliance code information
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to Ethernet_extended_compliance
 *  @return
 *   0 on success and -1 on error
 */
int bf_sfp_get_eth_ext_compliance (int port,
                                   Ethernet_extended_compliance *code)
{
    uint8_t eth_ext_comp;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    eth_ext_comp = bf_sfp_info_arr[port].idprom[36];

    *code = eth_ext_comp;
    return 0;
}

bool bf_sfp_get_dom_support (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }

    return SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom);
}

/** return sfp presence information
*
*  @param port
*   port
*  @return
*   true if presence and false if absent
*/
bool bf_sfp_is_present (int port)
{
    if (port < 0 || port > bf_plt_max_sfp) {
        return false;
    }
    return bf_sfp_info_arr[port].present;
}

bool bf_sfp_is_detected (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }

    return bf_sfp_info_arr[port].fsm_detected;
}

void bf_sfp_set_detected (int port, bool detected)
{
    if (port > bf_plt_max_sfp) {
        return;
    }
    /* Set fsm_detected indicator to notify health monitor thread
     * now it's ready to read the ddm of this port.
     */
    bf_sfp_info_arr[port].fsm_detected = detected;
}

bool bf_sfp_is_sff8472 (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }

    if (bf_sfp_info_arr[port].memmap_format ==
        MMFORMAT_SFF8472) {
        return true;
    }
    return false;
}

static void
bf_sfp_sff_info_show (sff_info_t *info)
{
    LOG_DEBUG ("Vendor: %s Model: %s SN: %s Type: %s Module: %s Media: %s Length: %d",
               info->vendor, info->model, info->serial,
               info->sfp_type_name,
               info->module_type_name, info->media_type_name,
               info->length);
}

MemMap_Format bf_sfp_get_memmap_format (
    int port)
{
    if (port > bf_plt_max_sfp) {
        return MMFORMAT_UNKNOWN;
    }

    return bf_sfp_info_arr[port].memmap_format;
}


/** return sfp memory map flat or paged
 *
 *  @param port
 *   port
 *  @return
 *   true if flat memory
 */
bool bf_sfp_is_flat_mem (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }
    return bf_sfp_info_arr[port].flat_mem;
}

/** return sfp passive copper
 *
 *  @param port
 *   port
 *  @return
 *   true if passive copper
 */
bool bf_sfp_is_passive_cu (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }
    return bf_sfp_info_arr[port].passive_cu;
}

/** return sfp is optical/AOC
 *
 *  @param port
 *   port
 *  @return
 *   true if Optical/AOC
 */
bool bf_sfp_is_optical (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }
    return !bf_sfp_info_arr[port].passive_cu;
}

/** clear the contents of sfp port idprom cached info
*
*  @param port
*   port
*/
static void bf_sfp_idprom_clr (int port)
{
    if (port > bf_plt_max_sfp) {
        return;
    }
    memset (& (bf_sfp_info_arr[port].idprom[0]), 0,
            MAX_SFP_PAGE_SIZE);
    memset (& (bf_sfp_info_arr[port].a2h[0]), 0,
            MAX_SFP_PAGE_SIZE);
    memset (& (bf_sfp_info_arr[port].page0[0]), 0,
            MAX_SFP_PAGE_SIZE);
    memset (& (bf_sfp_info_arr[port].page1[0]), 0,
            MAX_SFP_PAGE_SIZE);
    memset (& (bf_sfp_info_arr[port].page2[0]), 0,
            MAX_SFP_PAGE_SIZE);
    memset (& (bf_sfp_info_arr[port].page3[0]), 0,
            MAX_SFP_PAGE_SIZE);

    bf_sfp_info_arr[port].cache_dirty = true;
}

int bf_sfp_module_read (
    int port, int offset,
    int len, uint8_t *buf)
{
    int rc = 0;

    if (port > BF_PLAT_MAX_SFP) {
        return -1;
    }

    bf_sys_mutex_lock (
        &bf_sfp_info_arr[port].sfp_mtx);

    // For Tofino based system compatibility
    if (!bf_sfp_is_flat_mem (port) &&
        (offset >= MAX_SFP_PAGE_SIZE) &&
        (!bf_sfp_info_arr[port].cache_dirty)) {
        /* should we change page here ? */
    }

    rc |= (bf_pltfm_sfp_read_module (port, offset,
                                     len, buf));
    bf_sys_mutex_unlock (
        &bf_sfp_info_arr[port].sfp_mtx);
    return rc;
}

int bf_sfp_module_write (
    int port, int offset,
    int len, uint8_t *buf)
{
    int rc = 0;

    if (port > BF_PLAT_MAX_SFP) {
        return -1;
    }

    bf_sys_mutex_lock (
        &bf_sfp_info_arr[port].sfp_mtx);

    // For Tofino based system compatibility
    if (!bf_sfp_is_flat_mem (port) &&
        (offset >= MAX_SFP_PAGE_SIZE) &&
        (!bf_sfp_info_arr[port].cache_dirty)) {
        /* should we change page here ? */
    }

    rc |= (bf_pltfm_sfp_write_module (port, offset,
                                      len, buf));
    bf_sys_mutex_unlock (
        &bf_sfp_info_arr[port].sfp_mtx);
    return rc;
}

/* Quite important. by tsihang. */
static int bf_sfp_set_idprom (int port)
{
    sff_eeprom_t *se;
    uint8_t id;
    int rc = -1;

    if (port > bf_plt_max_sfp) {
        return -1;
    }
    if (!bf_sfp_info_arr[port].present) {
        LOG_ERROR (" SFP    %02d: IDProm set failed as SFP is not present",
                   port);
        return rc;
    }

    // fill idprom with SFP eeprom 0xA0h
    if (bf_pltfm_sfp_read_module (port, 0,
                                  MAX_SFP_PAGE_SIZE,
                                  bf_sfp_info_arr[port].idprom)) {
        return rc;
    }

    se = &bf_sfp_sff_eeprom[port];
    memset (se, 0, sizeof (sff_eeprom_t));
    rc = sff_eeprom_parse (se,
                           bf_sfp_info_arr[port].idprom);
    /* sff_eeprom_parse is quite an importand API for most sfp.
     * but it is still not ready for all kind of sfps.
     * so override the failure here and keep tracking.
     * by tsihang, 2022-06-17. */
#if 0
    if (!se->identified) {
        LOG_ERROR (" SFP    %02d: IDProm set failed as SFP is not decodable <rc=%d>",
                   port, rc);
        return rc;
    }
#endif
    id = bf_sfp_info_arr[port].idprom[0];
    if ((id == SFP) || (id == SFP_PLUS) ||
        (id == SFP_28)) {
        /* WANTED and just consider it following SFF-8472. */
        bf_sfp_info_arr[port].memmap_format =
            MMFORMAT_SFF8472;
    }

    // change dirty
    bf_sfp_info_arr[port].cache_dirty = false;

    return 0;
}

static bool bf_sfp_special_case_handled (
    int port,
    bf_pltfm_sfp_type_t *sfp_type,
    bool *type_copper)
{
    return 0;
}

int bf_sfp_type_get (int port,
                     bf_pltfm_sfp_type_t *sfp_type)
{
    /* idprom already been written to cache. */
    Ethernet_extended_compliance eth_ext_comp;
    bool type_copper = true;  // default to copper

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    /* In order to make it easy, I set the media type
     * as OPTICAL by default.
     * Please rich this API after all things done.
     * by tsihang, 2021-07-18. */
    *sfp_type = BF_PLTFM_QSFP_UNKNOWN;

    /* there are some odd module/cable types that have inconsistent information
     * in it. We need to categorise them seperately
     */
    if (bf_sfp_special_case_handled (port, sfp_type,
                                     &type_copper)) {
        return 0;
    }

    /* See SFF-8024 spec rev 4.9,
     * by tsihang, 2021-07-29.
     */
    if (bf_sfp_get_eth_ext_compliance (port,
                                       &eth_ext_comp) == 0) {
        type_copper =
            bf_sfp_is_eth_ext_compliance_copper (
                eth_ext_comp);
    } else {
        type_copper = true;
    }

    if (!type_copper) {  // we are done if found optical module
        *sfp_type = BF_PLTFM_QSFP_OPT;
        return 0;
    }

    /* Do not support copper at this moment.
     * by tsihang, 2021-07-29. */
#if 0
    uint8_t cable_len;
    if (bf_sfp_field_read_onebank (port,
                                   LENGTH_CBLASSY, 0, 0, 1, &cable_len) <
        0) {
        return -1;
    }
    switch (cable_len) {
        case 0:
            *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
            return 0;
        case 1:
            *qsfp_type = BF_PLTFM_QSFP_CU_1_M;
            return 0;
        case 2:
            *qsfp_type = BF_PLTFM_QSFP_CU_2_M;
            return 0;
        default:
            *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
            return 0;
    }
#endif
    return 0;
}

void bf_sfp_debug_clear_all_presence_bits (void)
{
    int port;

    for (port = 0; port < BF_PLAT_MAX_SFP + 1;
        port++) {
        if (bf_sfp_info_arr[port].present) {
            bf_sfp_set_present (port, false);
        }
    }
}


/** update sfp cached memory map data
 *
 *  @param port
 *   port
 */
int bf_sfp_update_cache (int port)
{
    if (port > bf_plt_max_sfp) {
        return -1;
    }
    bf_sfp_idprom_clr (port);

    if (!bf_sfp_info_arr[port].present) {
        return 0;  // not present, exit now that the cache is cleared
    }

    // first, figure out memory map format.
    if (bf_sfp_set_idprom (port)) {
        //if (!bf_sfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
        LOG_ERROR ("Error setting idprom for sfp %d\n",
                   port);
        //}
        return -1;
    }

    // judge media type SFF-8472 Rev 12.3 Page 14
    // quite complex.
    if (( bf_sfp_info_arr[port].idprom[7] == 0x00 &&
          bf_sfp_info_arr[port].idprom[8] == 0x04 &&
          bf_sfp_info_arr[port].idprom[60] == 0x01 &&
          bf_sfp_info_arr[port].idprom[61] == 0x00) ||
        ( bf_sfp_info_arr[port].idprom[7] == 0x00 &&
          bf_sfp_info_arr[port].idprom[8] == 0x08 &&
          bf_sfp_info_arr[port].idprom[60] == 0x01 &&
          bf_sfp_info_arr[port].idprom[61] == 0x00) ||
        ( bf_sfp_info_arr[port].idprom[7] == 0x00 &&
          bf_sfp_info_arr[port].idprom[8] == 0x08 &&
          bf_sfp_info_arr[port].idprom[60] == 0x04 &&
          bf_sfp_info_arr[port].idprom[61] == 0x00) ||
        ( bf_sfp_info_arr[port].idprom[7] == 0x00 &&
          bf_sfp_info_arr[port].idprom[8] == 0x08 &&
          bf_sfp_info_arr[port].idprom[60] == 0x0C &&
          bf_sfp_info_arr[port].idprom[61] == 0x00)) {
        bf_sfp_info_arr[port].passive_cu = true;
    } else {
        bf_sfp_info_arr[port].passive_cu = false;
    }

    if (sfp_debug_on) {
        LOG_DEBUG (
            " SFP    %2d : Spec %s : %s : %s", port,
            bf_sfp_is_sff8472 (port)    ? "SFF-8472" : "Unknown",
            bf_sfp_is_passive_cu (port) ? "Passive copper" : "Active/Optical",
            bf_sfp_is_flat_mem (port)   ? "Flat" : "Paged");
    }

    sff_dom_info_t sdi;
    sff_eeprom_t *se;
    sff_info_t *sff;
    se = &bf_sfp_sff_eeprom[port];
    sff = &se->info;

    // now that we know the memory map format, we can selectively read the
    // locations that are static into our cache.
    /* How to check whether there's A2h or not ?
     * by tsihang, 2021-07-29. */
    if (SFF8472_DOM_SUPPORTED (
            bf_sfp_info_arr[port].idprom)) {
        if (bf_pltfm_sfp_read_module (port,
                                      MAX_SFP_PAGE_SIZE, MAX_SFP_PAGE_SIZE,
                                      bf_sfp_info_arr[port].a2h)) {
            LOG_ERROR ("Error reading dom for sfp %d\n",
                       port);
            return -1;
        }
    }

    sff_dom_info_get (&sdi, sff,
                      bf_sfp_info_arr[port].idprom,
                      bf_sfp_info_arr[port].a2h);

    if (sfp_debug_on) {
        bf_sfp_sff_info_show (sff);
        LOG_DEBUG (" SFP    %2d : Update cache complete",
                   port);
    }
    return 0;

}

/** get sfp info ptr
 * @param port
 *  front panel port
 * @return
 * bf_sfp_info_t ptr if sfp present and readable
 * NULL for other conditions
 */

static bf_sfp_info_t *bf_sfp_get_info (int port)
{
    if (port < 0 || port > bf_plt_max_sfp) {
        return NULL;
    }

    if (bf_sfp_info_arr[port].cache_dirty &&
        bf_sfp_info_arr[port].present) {
        // just update it
        bf_sfp_update_cache (port);
    }

    if (bf_sfp_info_arr[port].cache_dirty) {
        // if still dirty
        return NULL;
    }
    return &bf_sfp_info_arr[port];
}

int bf_sfp_get_cached_info (int port, int page,
                            uint8_t *buf)
{
    bf_sfp_info_t *sfp;
    uint8_t *cptr;

    sfp = bf_sfp_get_info (port);
    if (!sfp) {
        return -1;
    }

    /* param 'page' is unused at this moment. */
    if (page == 0) {
        cptr = &sfp->idprom[0];
    } else {
        cptr = &sfp->a2h[0];
    }
    memcpy (buf, cptr, MAX_SFP_PAGE_SIZE);

    return 0;
}

void bf_sfp_ddm_convert (const uint8_t *a2h,
             double *temp, double *volt, double *txBias, double *txPower, double *rxPower)
{
    double rx_p, tx_p;

    *temp = get_temp(((a2h[96] << 8) | a2h[97]));
    *volt = get_vcc(((a2h[98] << 8) | a2h[99]));

    /*
     * Measured Tx bias current is represented in mA as a 16-bit unsigned integer with the
     * current defined as the full 16-bit value (0 to 65535) with LSB equal to 2mA.
     * TX bias ranges from 0mA to 131mA.
     */
    *txBias = get_txbias(((a2h[100] << 8) | a2h[101]));
    /*
     * Measured Rx optical power and Tx optical power.
     * RX/Tx power ranges from 0mW to 6.5535mW
     */
#if 0
    *txPower = 10 * log10 (get_pwr (((a2h[102] << 8) | a2h[103])));
    *rxPower = 10 * log10 (get_pwr (((a2h[104] << 8) | a2h[105])));
#endif

    tx_p = get_pwr ((a2h[102] << 8) | a2h[103]);
    *txPower = (tx_p != 0) ? (10 * log10 (tx_p)) : -40;

    rx_p = get_pwr ((a2h[104] << 8) | a2h[105]);
    *rxPower = (rx_p != 0) ? (10 * log10 (rx_p)) : -40;
}

bool bf_sfp_get_chan_temp (int port,
                            sfp_global_sensor_t *chn)
{
    uint8_t data[2];
    uint8_t offset = 96;
    int rc;

    chn->temp.value = (double)0;
    chn->temp._isset.value = false;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    /* A2h, what should we do if there's no A2h ? */
    if (!SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom)) {
        return -1;
    }

    rc = bf_pltfm_sfp_read_module (port,
                             MAX_SFP_PAGE_SIZE + offset, 2,
                             /* Update cache first. */
                             &bf_sfp_info_arr[port].a2h[offset]);

    if (rc) {
        return 0;
    }

    data[0] = bf_sfp_info_arr[port].a2h[offset];
    data[1] = bf_sfp_info_arr[port].a2h[offset + 1];

    uint16_t val = data[0] << 8 | data[1];
    chn->temp.value = get_temp (val);
    chn->temp._isset.value = true;

    return true;
}

bool bf_sfp_get_chan_volt (int port,
                            sfp_global_sensor_t *chn)
{
    uint8_t data[2];
    uint8_t offset = 98;
    int rc;

    chn->vcc.value = (double)0;
    chn->vcc._isset.value = false;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    /* A2h, what should we do if there's no A2h ? */
    if (!SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom)) {
        return -1;
    }

    rc = bf_pltfm_sfp_read_module (port,
                             MAX_SFP_PAGE_SIZE + offset, 2,
                             &bf_sfp_info_arr[port].a2h[offset]);
    if (rc) {
        return 0;
    }

    data[0] = bf_sfp_info_arr[port].a2h[offset];
    data[1] = bf_sfp_info_arr[port].a2h[offset + 1];

    uint16_t val = data[0] << 8 | data[1];
    chn->vcc.value = get_vcc (val);
    chn->vcc._isset.value = true;

    return true;
}

/** update sfp real time diagnostic tx_bias value to cache.
 *
 *  @param port
 *   port
 */
bool bf_sfp_get_chan_tx_bias (int port,
                               sfp_channel_t *chn)
{
    uint8_t data[2];
    uint8_t offset = 100;
    int rc;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    /* A2h, what should we do if there's no A2h ? */
    if (!SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom)) {
        return -1;
    }

    rc = bf_pltfm_sfp_read_module (port,
                                  MAX_SFP_PAGE_SIZE + offset, 2,
                                  &bf_sfp_info_arr[port].a2h[offset]);
    if (rc) {
        return 0;
    }

    data[0] = bf_sfp_info_arr[port].a2h[offset];
    data[1] = bf_sfp_info_arr[port].a2h[offset + 1];

    uint16_t val = data[0] << 8 | data[1];
    chn->sensors.tx_bias.value = get_txbias (val);
    chn->sensors.tx_bias._isset.value = true;

    return true;
}

bool bf_sfp_get_chan_tx_pwr (int port,
                              sfp_channel_t *chn)
{
   uint8_t data[2];
   uint8_t offset = 102;
   int rc;

   if (port > bf_plt_max_sfp) {
       return -1;
   }

   /* A2h, what should we do if there's no A2h ? */
   if (!SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom)) {
       return -1;
   }

   rc = bf_pltfm_sfp_read_module (port,
                                 MAX_SFP_PAGE_SIZE + offset, 2,
                                 &bf_sfp_info_arr[port].a2h[offset]);
   if (rc) {
       return 0;
   }

   data[0] = bf_sfp_info_arr[port].a2h[offset];
   data[1] = bf_sfp_info_arr[port].a2h[offset + 1];

   uint16_t val = data[0] << 8 | data[1];
   chn->sensors.tx_pwr.value = ((val == 0) ? (-40 * 1.0) : (10 * log10 (get_pwr (val))));
   chn->sensors.tx_pwr._isset.value = true;

   return true;
}

bool bf_sfp_get_chan_rx_pwr (int port,
                              sfp_channel_t *chn)
{
    uint8_t data[2];
    uint8_t offset = 104;
    int rc;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    /* A2h, what should we do if there's no A2h ? */
    if (!SFF8472_DOM_SUPPORTED (bf_sfp_info_arr[port].idprom)) {
        return -1;
    }

    rc = bf_pltfm_sfp_read_module (port,
                               MAX_SFP_PAGE_SIZE + offset, 2,
                               &bf_sfp_info_arr[port].a2h[offset]);
    if (rc) {
        return 0;
    }

    data[0] = bf_sfp_info_arr[port].a2h[offset];
    data[1] = bf_sfp_info_arr[port].a2h[offset + 1];

    uint16_t val = data[0] << 8 | data[1];
    chn->sensors.rx_pwr.value = ((val == 0) ? (-40 * 1.0) : (10 * log10 (get_pwr (val))));
    chn->sensors.rx_pwr._isset.value = true;

    return true;
}

/** detect sfp module by accessing its memory and update cached information
 * accordingly
 *
 *  @param port
 *   port
 */
int bf_sfp_detect_transceiver (int port,
                               bool *is_present)
{
    if (port < 0 || port > bf_plt_max_sfp) {
        return -1;
    }
    int err;
    bool st = bf_pltfm_detect_sfp (port);
    // Just overwrite presence bit
    if (bf_sfp_soft_removal_get (port) && st) {
        st = 0;
    }

    if (st != bf_sfp_info_arr[port].present ||
        bf_sfp_info_arr[port].cache_dirty) {
        /* closed by tsihang. */
        //if (!bf_sfp_info_arr[port].suppress_repeated_rd_fail_msgs) {
        LOG_DEBUG (" SFP    %2d : %s",
                   port, st ? "PRESENT" : "REMOVED");
        //}
        bf_sfp_set_present (port, st);
        *is_present = st;
        if (st) {
            err = bf_sfp_update_cache (port);
            if (err) {
                /* NOTE, we do not clear IDPROM data here so that we can read
                 * whatever data it returned.
                 */
                //bf_sfp_info_arr[port].suppress_repeated_rd_fail_msgs
                //    = true;
                LOG_ERROR ("Error<%d>: "
                           "Update data for SFP %2d\n", err, port);
                return -1;
            }
            //bf_sfp_info_arr[port].suppress_repeated_rd_fail_msgs
            //    = false;
            // bf_qsfp_set_pwr_ctrl(port);  don't power up the module here, do it in
            // the module FSM
        }
    } else {
        LOG_DEBUG (
            "(%s:%d) "
            " SFP    %2d : %s\n",
            __func__, __LINE__,
            port, st ? "PRESENT" : "REMOVED");

        *is_present = st;
    }
    return 0;
}

void bf_sfp_set_num (int num_ports)
{
    int i;
    if (num_ports > BF_PLAT_MAX_SFP) {
        LOG_ERROR ("%s SFP init %d max port set failed \n",
                   __func__, num_ports);
        return;
    }
    for (i = 0; i <= BF_PLAT_MAX_QSFP; i++) {
        bf_sys_mutex_init (&bf_sfp_info_arr[i].sfp_mtx);
    }
    bf_plt_max_sfp = num_ports;
}

/** reset sfp module
 *
 *  @param port
 *   port
 *  @param reset
 *   reset 1: to reset the sfp, 0: to unreset the sfp
 *  @return
 *   0 on success and  -1 on error
 */
int bf_sfp_reset (int port, bool reset)
{
    if (port <= bf_plt_max_sfp) {
        bf_sfp_info_arr[port].reset = reset;
        return (bf_pltfm_sfp_module_reset (port, reset));
    } else {
        return -1; /* TBD: handle cpu port SFP */
    }
}

bool bf_sfp_get_reset (int port)
{
    if (port <= bf_plt_max_sfp) {
        return bf_sfp_info_arr[port].reset;
    }
    return false;
}

int bf_sfp_tx_disable (int port, bool tx_dis)
{
    if (port <= bf_plt_max_sfp) {
        return (bf_pltfm_sfp_module_tx_disable (port, tx_dis));
    } else {
        return -1; /* TBD: handle cpu port SFP */
    }
}

/** Get number of SFP ports on the system
 *
 *  @param num_ports
 */
int bf_sfp_get_max_sfp_ports (void)
{
    return bf_plt_max_sfp;
}

void bf_sfp_soft_removal_set (int port,
                              bool removed)
{
    if (port > bf_plt_max_sfp) {
        return;
    }
    bf_sfp_info_arr[port].soft_removed = removed;
}

bool bf_sfp_soft_removal_get (int port)
{
    if (port > bf_plt_max_sfp) {
        return false;
    }
    return bf_sfp_info_arr[port].soft_removed;
}

/** return sfp presence information as per modules' presence pins
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0=present, 1=not present
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0=present, 1=not present
 */
int bf_sfp_get_transceiver_pres (uint32_t
                                 *lower_ports,
                                 uint32_t *upper_ports)
{
    uint32_t pos, port;
    int rc = 0;

    rc = bf_pltfm_sfp_get_presence_mask (lower_ports,
                                         upper_ports);
#if 0
    fprintf (stdout, "%2d SFPs : \n" \
             "lower_ports : %08x : upper_ports : %08x\n",
             bf_sfp_get_max_sfp_ports(), *lower_ports,
             *upper_ports);
#endif
    // If the qsfp is plugged-in and soft-removal is set, or force present is
    // set, override presence
    for (port = 0; port <= (uint32_t)bf_plt_max_sfp;
         port++) {
        pos = port % 32;
        if (port < 32) {
            if ((! (*lower_ports & (1 << pos))) &&
                (bf_sfp_soft_removal_get (port + 1))) {
                *lower_ports |= (1 << pos);
            }
        } else if (port < 64) {
            if ((! (*upper_ports & (1 << pos))) &&
                (bf_sfp_soft_removal_get (port + 1))) {
                *upper_ports |= (1 << pos);
            }
        }
        // add separate command for cpu-port soft-removal/insertion
    }
#if 0
    fprintf (stdout, "%2d SFPs : \n" \
             "lower_ports : %08x : upper_ports : %08x\n",
             bf_sfp_get_max_sfp_ports(), *lower_ports,
             *upper_ports);
#endif

    // return actual HW error
    return rc;
}

int bf_sfp_init()
{
    int i;

    for (i = 0; i < BF_PLAT_MAX_SFP + 1; i++) {
        bf_sfp_idprom_clr (i);
        bf_sfp_info_arr[i].present = false;
        bf_sfp_info_arr[i].reset = false;
        bf_sfp_info_arr[i].flat_mem = false;
        bf_sfp_info_arr[i].passive_cu = true;
        bf_sfp_info_arr[i].fsm_detected = false;
        //bf_sfp_info_arr[i].num_ch = 0;
        bf_sfp_info_arr[i].memmap_format =
            MMFORMAT_UNKNOWN;
        //bf_sfp_info_arr[i].suppress_repeated_rd_fail_msgs = false;
    }

    return 0;
}

int bf_sfp_port_deinit (int port)
{
    if (port > bf_plt_max_sfp) {
        return -1;
    }
    bf_sfp_idprom_clr (port);
    bf_sfp_info_arr[port].present = false;
    bf_sfp_info_arr[port].reset = false;
    bf_sfp_info_arr[port].flat_mem = false;
    bf_sfp_info_arr[port].passive_cu = true;
    //bf_sfp_info_arr[port].num_ch = 0;
    bf_sfp_info_arr[port].memmap_format =
        MMFORMAT_UNKNOWN;
    //bf_sfp_info_arr[port].suppress_repeated_rd_fail_msgs = false;

    return 0;
}


/** set sfp power control through software register
 *
 *  @param port
 *   port
 */
void bf_sfp_set_pwr_ctrl (int port, bool lpmode)
{
    if (port > bf_plt_max_sfp) {
        return;
    }

    LOG_DEBUG ("SFP    %2d : value Written to new Power Control byte : 0x%x",
               port,
               0);
}

/** mark sfp module presence
 *
 *  @param port
 *   port
 *  @param present
 *   true if present and false if removed
 */
void bf_sfp_set_present (int port, bool present)
{
    if (port > bf_plt_max_sfp) {
        return;
    }
    bf_sfp_info_arr[port].present = present;
    /* Set the dirty bit as the QSFP was removed and
     * the cached data is no longer valid until next
     * set IDProm is called
     */
    if (bf_sfp_info_arr[port].present == false) {
        bf_sfp_port_deinit (port);
    }
}

/* get port by a given conn and chnl. */
int bf_sfp_get_port (uint32_t conn, uint32_t chnl,
                     int *port)
{
    return bf_pltfm_sfp_lookup_by_conn (conn, chnl,
                                        (uint32_t *)port);
}

/* get conn and chnl by a given port. */
int bf_sfp_get_conn (int port, uint32_t *conn,
                     uint32_t *chnl)
{
    if (port > bf_plt_max_sfp) {
        return -1;
    }
    return bf_pltfm_sfp_lookup_by_module (port, conn,
                                          chnl);
}

/** set sfp lpmode by using hardware pin
 *
 *  @param port
 *    front panel port
 *  @param lpmode
 *    true : set lpmode, false: set no-lpmode
 *  @return
 *    0 success, -1 failure;
 */
int bf_sfp_set_transceiver_lpmode (int port,
                                   bool lpmode)
{
    return (bf_pltfm_sfp_set_lpmode (port, lpmode));
}

int check_sfp_module_vendor (uint32_t conn_id,
                             uint32_t chnl_id,
                             char *target_vendor)
{
    uint8_t *real_vendor;
    uint32_t module;
    bf_sfp_info_t *sfp_info_ptr;

    module = bf_pltfm_sfp_lookup_by_conn (conn_id,
                                          chnl_id, &module);
    sfp_info_ptr = bf_sfp_get_info (module);
    if (!sfp_info_ptr) {
        return -1;
    }
    real_vendor = &sfp_info_ptr->idprom[20];
    return memcmp (target_vendor, real_vendor,
                   strlen (target_vendor));
}

static void sfp_copy_string (uint8_t *dst,
                             uint8_t *src,
                             size_t offset,
                             size_t len)
{
    memcpy (dst, src + offset, len);
    dst[len] = '\0';
}

/** return sfp transceiver information
 *
 *  @param port
 *   port
 *  @param info
 *   sfp transceiver information struct
 */
int bf_sfp_get_transceiver_info (int port,
                                 sfp_transciever_info_t *info)
{
    uint8_t *p_a0h;
    uint8_t *p_a2h;

    if (port > bf_plt_max_sfp) {
        return -1;
    }

    if (!bf_sfp_info_arr[port].present) {
        return -1;
    }

    info->port = port;
    // if (!cache_is_valid(port)) {
    //  return;
    //}

    p_a0h = bf_sfp_info_arr[port].idprom;
    p_a2h = bf_sfp_info_arr[port].a2h;

    /* present */
    info->present = bf_sfp_info_arr[port].present;
    /* module capability */
    info->module_capability = (0x80 << 8) | p_a0h[36];
    info->_isset.mcapa = true;
    /* module type */
    info->module_type = p_a0h[0];
    info->_isset.mtype = true;
    /* connector type */
    info->connector_type = p_a0h[2];
    info->_isset.ctype = true;
    /* vendor info */
    sfp_copy_string ((uint8_t *)info->vendor.name,
                     p_a0h, 20, 16);
    sfp_copy_string ((uint8_t *)info->vendor.rev,
                     p_a0h, 56, 4);
    sfp_copy_string ((uint8_t *)
                     info->vendor.part_number, p_a0h, 40, 16);
    sfp_copy_string ((uint8_t *)
                     info->vendor.serial_number, p_a0h, 68, 16);
    sfp_copy_string ((uint8_t *)info->vendor.date_code
                     , p_a0h, 84, 8);
    memcpy (info->vendor.oui, &p_a0h[37], 3);
    info->_isset.vendor = true;

    /* The value of sensors in p_a2h is updated to cache by bf_pltfm_onlp_mntr_transceiver thread.
     * Here we just read out from cache.
     * While at this moment bf_qsfp_get_transceiver_info gets transceiver info from a real transceiver module.
     * by tsihang, 2023-06-25. */
    bf_sfp_ddm_convert (p_a2h,
        &info->sensor.temp.value,
        &info->sensor.vcc.value,
        &info->chn[0].sensors.tx_bias.value,
        &info->chn[0].sensors.tx_pwr.value,
        &info->chn[0].sensors.rx_pwr.value);
    info->sensor.temp._isset.value = true;
    info->sensor.vcc._isset.value = true;
    info->_isset.sensor = true;
    info->_isset.chn = true;

    return 0;
}

void bf_sfp_print_ddm (int port, sfp_global_sensor_t *trans, sfp_channel_t *chnl)
{
      LOG_DEBUG (" SFP DDM Info for port %d",
          port);
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "Port   ",
                  "Temp (C)",
                  "Voltage (V)",
                  "Bias(mA)",
                  "Tx Power (dBm)",
                  "Rx Power (dBm)");
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
       LOG_DEBUG ("      %d   %10.1f %15.2f %10.2f %16.2f %16.2f",
                  port,
                  trans->temp.value,
                  trans->vcc.value,
                  chnl->sensors.tx_bias.value,
                  chnl->sensors.tx_pwr.value,
                  chnl->sensors.rx_pwr.value);
      LOG_DEBUG (
                  "%10s %10s %15s %10s %16s %16s",
                  "-------",
                  "--------",
                  "-----------",
                  "--------",
                  "--------------",
                  "--------------");
}

int bf_sfp_ctrlmask_set (int port,
                  uint32_t ctrlmask) {
    if (port > bf_plt_max_sfp) {
        return -1;
    }

    bf_sfp_info_arr[port].ctrlmask = ctrlmask;

    return 0;
}

int bf_sfp_ctrlmask_get (int port,
                uint32_t *ctrlmask) {
    if (port > bf_plt_max_sfp) {
        return -1;
    }

    *ctrlmask = bf_sfp_info_arr[port].ctrlmask;

    return 0;
}
