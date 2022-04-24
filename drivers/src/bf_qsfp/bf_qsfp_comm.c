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
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>

#include <bf_types/bf_types.h>
#include <bfsys/bf_sal/bf_sys_sem.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_qsfp/bf_qsfp.h>
#include <bf_pltfm_qsfp.h>
#include <bf_pm/bf_pm_intf.h>

#include <bf_port_mgmt/bf_pm_priv.h>

#define BF_PLAT_MAX_QSFP 65

typedef double (*qsfp_conversion_fn) (
    uint16_t value);

typedef struct bf_qsfp_special_info_ {
    bf_pltfm_qsfp_type_t qsfp_type;
    bool is_set;
} bf_qsfp_special_info_t;

static bf_qsfp_special_info_t
qsfp_special_case_port[BF_PLAT_MAX_QSFP + 1];

/* cached QSFP values */
/* access is 1 based index, so zeroth index would be unused */
static uint8_t bf_qsfp_idprom[BF_PLAT_MAX_QSFP +
                              1][MAX_QSFP_PAGE_SIZE];
static uint8_t bf_qsfp_page0[BF_PLAT_MAX_QSFP +
                             1][MAX_QSFP_PAGE_SIZE];
static uint8_t bf_qsfp_page3[BF_PLAT_MAX_QSFP +
                             1][MAX_QSFP_PAGE_SIZE];

/* qsfp status */
static bool bf_qsfp_present[BF_PLAT_MAX_QSFP + 1];
static bool bf_qsfp_during_reset[BF_PLAT_MAX_QSFP + 1];
static bool bf_qsfp_cache_dirty[BF_PLAT_MAX_QSFP +
                                                 1];
static bool bf_qsfp_flat_mem[BF_PLAT_MAX_QSFP +
                                              1];

static int bf_plt_max_qsfp = 0;

/* SFF-8436, QSFP+ 10 Gbs 4X PLUGGABLE TRANSCEIVER spec */

static sff_field_map_t qsfp_field_map[] = {
    /* Base page values, including alarms and sensors */
    {IDENTIFIER, {QSFP_PAGE0_LOWER, 0, 1}},
    {
        STATUS,
        {QSFP_PAGE0_LOWER, 1, 2}
    },  // status[0] is version if using this field
    {TEMPERATURE_ALARMS, {QSFP_PAGE0_LOWER, 6, 1}},
    {VCC_ALARMS, {QSFP_PAGE0_LOWER, 7, 1}},
    {CHANNEL_RX_PWR_ALARMS, {QSFP_PAGE0_LOWER, 9, 2}},
    {CHANNEL_TX_BIAS_ALARMS, {QSFP_PAGE0_LOWER, 11, 2}},
    {TEMPERATURE, {QSFP_PAGE0_LOWER, 22, 2}},
    {VCC, {QSFP_PAGE0_LOWER, 26, 2}},
    {CHANNEL_RX_PWR, {QSFP_PAGE0_LOWER, 34, 8}},
    {CHANNEL_TX_BIAS, {QSFP_PAGE0_LOWER, 42, 8}},
    {CHANNEL_TX_DISABLE, {QSFP_PAGE0_LOWER, 50, 8}},
    {POWER_CONTROL, {QSFP_PAGE0_LOWER, 93, 1}},
    {PAGE_SELECT_BYTE, {QSFP_PAGE0_LOWER, 127, 1}},

    /* Page 0 values, including vendor info: */
    {EXTENDED_IDENTIFIER, {QSFP_PAGE0_UPPER, 129, 1}},
    {CONNECTOR_TYPE, {QSFP_PAGE0_UPPER, 130, 1}},
    {ETHERNET_COMPLIANCE, {QSFP_PAGE0_UPPER, 131, 1}},
    {LENGTH_SM_KM, {QSFP_PAGE0_UPPER, 142, 1}},
    {LENGTH_OM3, {QSFP_PAGE0_UPPER, 143, 1}},
    {LENGTH_OM2, {QSFP_PAGE0_UPPER, 144, 1}},
    {LENGTH_OM1, {QSFP_PAGE0_UPPER, 145, 1}},
    {LENGTH_COPPER, {QSFP_PAGE0_UPPER, 146, 1}},
    {VENDOR_NAME, {QSFP_PAGE0_UPPER, 148, 16}},
    {VENDOR_OUI, {QSFP_PAGE0_UPPER, 165, 3}},
    {PART_NUMBER, {QSFP_PAGE0_UPPER, 168, 16}},
    {REVISION_NUMBER, {QSFP_PAGE0_UPPER, 184, 2}},
    {ETHERNET_EXTENDED_COMPLIANCE, {QSFP_PAGE0_UPPER, 192, 1}},
    {VENDOR_SERIAL_NUMBER, {QSFP_PAGE0_UPPER, 196, 16}},
    {MFG_DATE, {QSFP_PAGE0_UPPER, 212, 8}},
    {DIAGNOSTIC_MONITORING_TYPE, {QSFP_PAGE0_UPPER, 220, 1}},

    /* Page 3 values, including alarm and warning threshold values: */
    {TEMPERATURE_THRESH, {QSFP_PAGE3, 128, 8}},
    {VCC_THRESH, {QSFP_PAGE3, 144, 8}},
    {RX_PWR_THRESH, {QSFP_PAGE3, 176, 8}},
    {TX_BIAS_THRESH, {QSFP_PAGE3, 184, 8}},

};

/* cable length multiplier values, from SFF specs */
static const sff_field_mult_t qsfp_multiplier[] =
{
    {LENGTH_SM_KM, 1000},
    {LENGTH_OM3, 2},
    {LENGTH_OM2, 1},
    {LENGTH_OM1, 1},
    {LENGTH_COPPER, 1},
};

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
    data = temp * 2.0 / 1000;
    return data;
}

/* converts qsfp power sensor data to absolute power value */
static double get_pwr (const uint16_t temp)
{
    double data;
    //data = temp * 0.1 / 1000;
    data = 10 * log10 (0.0001 * temp);
    return data;
}

/* return sff_field_info_t for a given field in qsfp_field_map[] */
static sff_field_info_t *get_sff_field_addr (
    const Sff_field field)
{
    int i, cnt = sizeof (qsfp_field_map) / sizeof (
                     sff_field_map_t);

    for (i = 0; i < cnt; i++) {
        if (qsfp_field_map[i].field == field) {
            return (& (qsfp_field_map[i].info));
        }
    }
    return NULL;
}

/* return the contents of the  sff_field_info_t for a given field */
static int get_qsfp_field_addr (Sff_field field,
                                int *page,
                                int *offset,
                                int *length)
{
    if (field >= SFF_FIELD_MAX) {
        return -1;
    }
    sff_field_info_t *info = get_sff_field_addr (
                                 field);
    if (!info) {
        return -1;
    }
    *offset = info->offset;
    *page = info->page;
    *length = info->length;
    return 0;
}

/* return the qsfp multiplier for a given field in struct qsfp_multiplier[] */
static uint32_t get_qsfp_multiplier (
    Sff_field field)
{
    int i, cnt = sizeof (qsfp_multiplier) / sizeof (
                     sff_field_mult_t);

    for (i = 0; i < cnt; i++) {
        if (qsfp_multiplier[i].field == field) {
            return (qsfp_multiplier[i].value);
        }
    }
    return 0; /* invalid field value */
}

/*
 * Given a byte, extract bit fields for various alarm flags;
 * note the we might want to use the lower or the upper nibble,
 * so offset is the number of the bit to start at;  this is usually
 * 0 or 4.
 */
static int get_qsfp_flags (const uint8_t *data,
                           int offset,
                           qsfp_flags_level_t *flags)
{
    if (!flags || !data || (offset < 0) ||
        (offset > 4)) {
        return -1;
    }
    flags->warn.low = (*data & (1 << offset));
    flags->warn.high = (*data & (1 << ++offset));
    flags->alarm.low = (*data & (1 << ++offset));
    flags->alarm.high = (*data & (1 << ++offset));

    return 0;
}

/* Note that this needs to be called while holding the qsfp mutex */
static bool cache_is_valid (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_present[port] &&
           !bf_qsfp_cache_dirty[port];
}

/* return the cached data of qsfp memory */
static uint8_t *get_qsfp_value_ptr (int port,
                                    int data_addr,
                                    int offset,
                                    int len)
{
    if (port > bf_plt_max_qsfp) {
        return NULL;
    }
    /* if the cached values are not correct */
    if (!cache_is_valid (port)) {
        LOG_ERROR ("qsfp %d is not present or unread\n",
                   port);
        return NULL;
    }
    if (data_addr == QSFP_PAGE0_LOWER) {
        if ((offset < 0) ||
            (offset + len) > MAX_QSFP_PAGE_SIZE) {
            return NULL;
        } else {
            return (&bf_qsfp_idprom[port][0] + offset);
        }
        /* Copy data from the cache */
    } else {
        offset -= MAX_QSFP_PAGE_SIZE;
        if ((offset < 0) ||
            (offset + len) > MAX_QSFP_PAGE_SIZE) {
            return NULL;
        }
        if (data_addr == QSFP_PAGE0_UPPER) {
            return (&bf_qsfp_page0[port][0] + offset);
        } else if (data_addr == QSFP_PAGE3 &&
                   !bf_qsfp_flat_mem[port]) {
            return (&bf_qsfp_page3[port][0] + offset);
        } else {
            LOG_ERROR ("Invalid Page value %d\n", data_addr);
            return NULL;
        }
    }
}

/* return qsfp sensor flags */
static int get_qsfp_sensor_flags (int port,
                                  Sff_field fieldName,
                                  qsfp_flags_level_t *flags)
{
    int offset;
    int length;
    int data_addr = 0;
    uint8_t *data;

    /* Determine if QSFP is present */
    if (get_qsfp_field_addr (fieldName, &data_addr,
                             &offset, &length)) {
        return -1;
    }
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return -1;
    }
    return get_qsfp_flags (data, 4, flags);
}

static double get_qsfp_sensor (int port,
                               Sff_field field,
                               qsfp_conversion_fn con_fn)
{
    uint8_t *data;

    sff_field_info_t *info = get_sff_field_addr (
                                 field);
    if (!info) {
        return 0.0;
    }
    data = get_qsfp_value_ptr (port, info->page,
                               info->offset, info->length);
    if (!data) {
        return 0.0;
    }
    return con_fn (data[0] << 8 | data[1]);
}

/* return the string of the sff field */
/* TBD : The returned string might not be null terminated if the corresponding
 * string
 * in the qsfp module takes its full length.
 * FIX needed for that here.
 */
static void get_qsfp_str (int port,
                          Sff_field field,
                          char *buf,
                          size_t buf_size)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t *data = NULL;
    memset (buf, 0x20, buf_size); //fill with space

    get_qsfp_field_addr (field, &data_addr, &offset,
                         &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return;
    }

    while (length > 0 && data[length - 1] == ' ') {
        --length;
    }
    strncpy (buf, (char *)data, buf_size);
    if (length != 0) {
        if (buf_size > (uint32_t)length * sizeof (char)) {
            strncpy (buf, (char *)data,
                     length * sizeof (char));
        } else {
            strncpy (buf, (char *)data, buf_size);
        }
    }
}

/*
 * Cable length is report as a single byte;  each field has a
 * specific multiplier to use to get the true length.  For instance,
 * single mode fiber length is specified in km, so the multiplier
 * is 1000.  In addition, the raw value of 255 indicates that the
 * cable is longer than can be represented.  We use a negative
 * value of the appropriate magnitude to communicate that to thrift
 * clients.
 */
static int get_qsfp_cable_length (int port,
                                  Sff_field field)
{
    int length;
    uint8_t *data;
    uint32_t multiplier;

    sff_field_info_t *info = get_sff_field_addr (
                                 field);
    data = get_qsfp_value_ptr (port, info->page,
                               info->offset, info->length);
    if (!data) {
        return 0;
    }
    multiplier = get_qsfp_multiplier (field);
    length = *data * multiplier;
    if (*data == MAX_CABLE_LEN) {
        length = - (MAX_CABLE_LEN - 1) * multiplier;
    }
    return length;
}

/*
 * Threhold values are stored just once;  they aren't per-channel,
 * so in all cases we simple assemble two-byte values and convert
 * them based on the type of the field.
 */
static int get_threshold_values (int port,
                                 Sff_field field,
                                 qsfp_threshold_level_t *thresh,
                                 qsfp_conversion_fn con_fn)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t *data = NULL;

    if (bf_qsfp_flat_mem[port]) {
        return -1;
    }
    get_qsfp_field_addr (field, &data_addr, &offset,
                         &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return -1;
    }

    if (length < 8) {
        return -1;
    }
    thresh->alarm.high = con_fn (data[0] << 8 |
                                 data[1]);
    thresh->alarm.low = con_fn (data[2] << 8 |
                                data[3]);
    thresh->warn.high = con_fn (data[4] << 8 |
                                data[5]);
    thresh->warn.low = con_fn (data[6] << 8 |
                               data[7]);

    return 0;
}

/** return qsfp vendor information
 *
 *  @param port
 *   port
 *  @param vendor
 *   qsfp vendor information struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_vendor_info (int port,
                              qsfp_vendor_info_t *vendor)
{
    get_qsfp_str (port, VENDOR_NAME, vendor->name,
                  sizeof (vendor->name));
    get_qsfp_str (port, VENDOR_OUI, vendor->oui,
                  sizeof (vendor->oui));
    get_qsfp_str (
        port, PART_NUMBER, vendor->part_number,
        sizeof (vendor->part_number));
    get_qsfp_str (port, REVISION_NUMBER, vendor->rev,
                  sizeof (vendor->rev));
    get_qsfp_str (port,
                  VENDOR_SERIAL_NUMBER,
                  vendor->serial_number,
                  sizeof (vendor->serial_number));
    get_qsfp_str (port, MFG_DATE, vendor->date_code,
                  sizeof (vendor->date_code));
    return true;
}

/** return qsfp alarm threshold information
 *
 *  @param port
 *   port
 *  @param thresh
 *   qsfp alarm threshold struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_threshold_info (int port,
                                 qsfp_alarm_threshold_t *thresh)
{
    int rc;

    if (bf_qsfp_flat_mem[port]) {
        return false;
    }
    rc = get_threshold_values (port,
                               TEMPERATURE_THRESH, &thresh->temp, get_temp);
    rc |= get_threshold_values (port, VCC_THRESH,
                                &thresh->vcc, get_vcc);
    rc |= get_threshold_values (port, RX_PWR_THRESH,
                                &thresh->rx_pwr, get_pwr);
    rc |=
        get_threshold_values (port, TX_BIAS_THRESH,
                              &thresh->tx_bias, get_txbias);
    return (rc ? false : true);
}

/** return qsfp sensor information
 *
 *  @param port
 *   port
 *  @param info
 *   qsfp global sensor struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_sensor_info (int port,
                              qsfp_global_sensor_t *info)
{
    info->temp.value = get_qsfp_sensor (port,
                                        TEMPERATURE, get_temp);
    info->temp._isset.value = true;
    get_qsfp_sensor_flags (port, TEMPERATURE_ALARMS,
                           & (info->temp.flags));
    info->temp._isset.flags = true;
    info->vcc.value = get_qsfp_sensor (port, VCC,
                                       get_vcc);
    info->vcc._isset.value = true;
    get_qsfp_sensor_flags (port, VCC_ALARMS,
                           & (info->vcc.flags));
    info->vcc._isset.flags = true;
    return true;
}

/** return qsfp cable information
 *
 *  @param port
 *   port
 *  @param cable
 *   qsfp cable struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_cable_info (int port,
                             qsfp_cable_t *cable)
{
    cable->single_mode = get_qsfp_cable_length (port,
                         LENGTH_SM_KM);
    cable->_isset.single_mode = (cable->single_mode !=
                                 0);
    cable->om3 = get_qsfp_cable_length (port,
                                        LENGTH_OM3);
    cable->_isset.om3 = (cable->om3 != 0);
    cable->om2 = get_qsfp_cable_length (port,
                                        LENGTH_OM2);
    cable->_isset.om2 = (cable->om2 != 0);
    cable->om1 = get_qsfp_cable_length (port,
                                        LENGTH_OM1);
    cable->_isset.om1 = (cable->om1 != 0);
    cable->copper = get_qsfp_cable_length (port,
                                           LENGTH_COPPER);
    cable->_isset.copper = (cable->copper != 0);
    return (cable->_isset.copper ||
            cable->_isset.om1 || cable->_isset.om2 ||
            cable->_isset.om3 || cable->_isset.single_mode);
}

/*
 * Iterate through the four channels collecting appropriate data;
 * always assumes CHANNEL_COUNT channels is four.
 */
/** return per channel sensor information
 *
 *  @param port
 *   port
 *  @param chn
 *   qsfp channel struct
 *  @return
 *   true on success and false on error
 */
bool bf_qsfp_get_sensors_per_chan_info (int port,
                                        qsfp_channel_t *chn)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    int channel = 0;
    uint8_t *data = NULL;
    uint16_t val = 0;

    /*
     * Interestingly enough, the QSFP stores the four alarm flags
     * (alarm high, alarm low, warning high, warning low) in two bytes by
     * channel in order 2, 1, 4, 3;  by using this set of offsets, we
     * should be able to read them in order, by reading the appriopriate
     * bit offsets combined with a byte offset into the data.
     *
     * That is, read bits 4 through 7 of the first byte, then 0 through 3,
     * then 4 through 7 of the second byte, and so on.  Ugh.
     */
    int bitoffset[] = {4, 0, 4, 0};
    int byteoffset[] = {0, 0, 1, 1};

    get_qsfp_field_addr (CHANNEL_RX_PWR_ALARMS,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);

    for (channel = 0; channel < CHANNEL_COUNT;
         channel++) {
        get_qsfp_flags (data + byteoffset[channel],
                        bitoffset[channel],
                        &chn[channel].sensors.rx_pwr.flags);
        chn[channel].sensors.rx_pwr._isset.flags = true;
    }

    get_qsfp_field_addr (CHANNEL_TX_BIAS_ALARMS,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);

    for (int channel = 0; channel < CHANNEL_COUNT;
         channel++) {
        get_qsfp_flags (data + byteoffset[channel],
                        bitoffset[channel],
                        &chn[channel].sensors.tx_bias.flags);
        chn[channel].sensors.tx_bias._isset.flags = true;
    }

    get_qsfp_field_addr (CHANNEL_RX_PWR, &data_addr,
                         &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);

    for (int channel = 0; channel < CHANNEL_COUNT;
         channel++) {
        val = data[0] << 8 | data[1];
        chn[channel].sensors.rx_pwr.value = get_pwr (val);
        chn[channel].sensors.rx_pwr._isset.value = true;
        data += 2;
        length--;
    }

    get_qsfp_field_addr (CHANNEL_TX_BIAS, &data_addr,
                         &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    for (int channel = 0; channel < CHANNEL_COUNT;
         channel++) {
        val = data[0] << 8 | data[1];
        chn[channel].sensors.tx_bias.value = get_txbias (
                val);
        chn[channel].sensors.tx_bias._isset.value = true;
        data += 2;
        length--;
    }
    /* QSFP doesn't report Tx power, so we can't try to report that. */
    get_qsfp_field_addr (CHANNEL_TX_DISABLE,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);

    for (int channel = 0; channel < CHANNEL_COUNT;
         channel++) {
        val = data[0] << 8 | data[1];
        chn[channel].sensors.tx_pwr.value = get_pwr (val);
        chn[channel].sensors.tx_pwr._isset.value = true;
        data += 2;
        length--;
    }

    return true;
}

/** clear the contents of qsfp port idprom cached info
 *
 *  @param port
 *   port
 */
static void bf_qsfp_idprom_clr (int port)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    memset (& (bf_qsfp_idprom[port][0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_page0[port][0]), 0,
            MAX_QSFP_PAGE_SIZE);
    memset (& (bf_qsfp_page3[port][0]), 0,
            MAX_QSFP_PAGE_SIZE);
}

/** set number of QSFP ports on the system
 *
 *  @param num_ports
 */
void bf_qsfp_set_num (int num_ports)
{
    bf_plt_max_qsfp = num_ports;
}

/** Get number of QSFP ports on the system
 *
 *  @param num_ports
 */
int bf_qsfp_get_max_qsfp_ports (void)
{
    return bf_plt_max_qsfp;
}

/** init qsfp data structure
 *
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_init (void)
{
    int port;

    for (port = 0; port <= BF_PLAT_MAX_QSFP; port++) {
        bf_qsfp_idprom_clr (port);
        bf_qsfp_present[port] = false;
        bf_qsfp_during_reset[port] = false;
        bf_qsfp_cache_dirty[port] = true;
        bf_qsfp_flat_mem[port] = false;
    }
    return 0;
}

/** deinit or init qsfp data structure per port
 *
 *  @param port
 *   port
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_port_deinit (int port)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    bf_qsfp_idprom_clr (port);
    bf_qsfp_present[port] = false;
    bf_qsfp_during_reset[port] = false;
    bf_qsfp_cache_dirty[port] = true;
    bf_qsfp_flat_mem[port] = false;
    return 0;
}

/** does QSFP module upport pages
 *
 *  @param port
 *   port
 *  @return
 *   true  if the module supports pages, false otherwise
 */
bool bf_qsfp_has_pages (int port)
{
    return !bf_qsfp_flat_mem[port];
}

static void get_qsfp_value (
    int port, int data_addr, int offset, int length,
    uint8_t *data)
{
    uint8_t *ptr = get_qsfp_value_ptr (port,
                                       data_addr, offset, length);

    if (!ptr) {
        return;
    }
    memcpy (data, ptr, length);
}

/** disable qsfp transmitter
 *
 *  @param port
 *   port
 *  @param channel
 *   channel within a port (0-3)
 *  @param disable
 *   disable true: to disable a channel on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_pltfm_qsfp_tx_disable_single_lane (
    int port, int channel, bool disable)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t val = 0, *data = NULL;
    bool chn_disable_st = false;

    if (port > bf_plt_max_qsfp || channel >= 4) {
        return -1;
    }
    LOG_ERROR ("Port %d/%d: TX_DISABLE = %d", port,
               channel, disable ? 1 : 0);

    /* read the cached value and change qsfp memory if needed
     * we are not doing a read/modify/write on qsfp memory
     */
    get_qsfp_field_addr (CHANNEL_TX_DISABLE,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return -1;
    }
    chn_disable_st = *data & (1 << channel);
    if (chn_disable_st == disable) {
        return 0; /* nothing to do */
    } else {
        /* write into qsfp memory */
        if (disable) {
            val = *data | (1 << channel);
        } else {
            val = *data & ~ (1 << channel);
        }
        if (bf_pltfm_qsfp_write_module (port, offset, 1,
                                        &val)) {
            LOG_ERROR (
                "Error writing tx_disable in qsfp port %d chn %d\n",
                port, channel);
            return -1;
        }
        /* change the cached value */
        *data = val;
        return 0;
    }
}

/** disable qsfp transmitter
 *
 *  @param port
 *   port
 *  @param bit-mask of channels (0x01 - 0x0F) within a port
 *  @param disable
 *   disable true: to disable a channel on qsfp, false: to enable
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_tx_disable (int port,
                        int channel_mask, bool disable)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t *data = NULL;
    uint8_t chn_disable_st = 0;

    if (port > bf_plt_max_qsfp || channel_mask > 15) {
        return -1;
    }

    fprintf (stdout,
             "Port %d: mask=0x%x : TX_DISABLE = %d",
             port, channel_mask, disable ? 1 : 0);

    /* read the cached value and change qsfp memory if needed
     * we are not doing a read/modify/write on qsfp memory
     */
    get_qsfp_field_addr (CHANNEL_TX_DISABLE,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return -1;
    }
    chn_disable_st = (*data & ~channel_mask) |
                     (disable ? channel_mask : 0);

    if (chn_disable_st !=
        *data) {  // changing the setting
        int rc;
        if ((rc = bf_pltfm_qsfp_write_module (port,
                                              offset, 1, &chn_disable_st))) {
            LOG_ERROR ("Error (%d) writing tx_disable in QSFP %2d chn %2d\n",
                       rc,
                       port,
                       channel_mask);
            return -1;
        }
        if (disable) {
            bf_sys_usleep (100000);
        } else {
            bf_sys_usleep (400000);
        }
    } else {
        LOG_ERROR (
            "Port %d: TX_DISABLE already matches: %0x\n",
            port, chn_disable_st);
    }
    /* change the cached value */
    *data = chn_disable_st;
    return 0;
}

/** reset qsfp module
 *
 *  @param port
 *   port
 *  @param reset
 *   reset 1: to reset the qsfp, 0: to unreset the qsfp
 *  @return
 *   0 on success and  -1 on error
 */
int bf_qsfp_reset (int port, bool reset)
{
    if (port <= bf_plt_max_qsfp) {
        bf_qsfp_during_reset[port] = reset;
        return (bf_pltfm_qsfp_module_reset (port, reset));
    } else {
        return -1; /* TBD: handle cpu port QSFP */
    }
}

bool bf_qsfp_get_reset (int port)
{
    if (port <= bf_plt_max_qsfp) {
        return bf_qsfp_during_reset[port];
    }
    return false;
}

static int set_qsfp_idprom (int port)
{
    uint8_t status[2];
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (!bf_qsfp_present[port]) {
        LOG_ERROR ("QSFP %d IDProm set failed as QSFP is not present\n",
                   port);
        return -1;
    }

    /* set flat_mem appropriately */
    get_qsfp_field_addr (STATUS, &data_addr, &offset,
                         &length);
    get_qsfp_value (port, data_addr, offset, length,
                    status);
    if (bf_qsfp_is_dd (port)) {
        bf_qsfp_flat_mem[port] = status[1] & (1 << 7);
    } else {
        bf_qsfp_flat_mem[port] = status[1] & (1 << 2);
    }
    bf_qsfp_cache_dirty[port] = false;
    return 0;
}

/** return qsfp presence information
 *
 *  @param port
 *   port
 *  @return
 *   true if presence and false if absent
 */
bool bf_qsfp_is_present (int port)
{
    if (port > bf_plt_max_qsfp) {
        return false;
    }
    return bf_qsfp_present[port];
}

/** mark qsfp module presence
 *
 *  @param port
 *   port
 *  @param present
 *   true if present and false if removed
 */
void bf_qsfp_set_present (int port, bool present)
{
    if (port > bf_plt_max_qsfp) {
        return;
    }
    bf_qsfp_present[port] = present;
    /* Set the dirty bit as the QSFP was removed and
     * the cached data is no longer valid until next
     * set IDProm is called
     */
    if (bf_qsfp_present[port] == false) {
        bf_qsfp_port_deinit (port);
    }
}

bool bf_qsfp_get_module_capability (int port,
                                    uint16_t *type)
{

    Ethernet_compliance eth_comp;
    if (bf_qsfp_get_eth_compliance (port,
                                    &eth_comp) != 0) {
        *type = 0;
    }

    if (eth_comp & 0x80) {
        Ethernet_extended_compliance ext_comp;
        if (bf_qsfp_get_eth_ext_compliance (port,
                                            &ext_comp) == 0) {
            *type = (eth_comp << 8) | ext_comp;
        } else {
            *type = 0;
        }
    } else {
        *type = eth_comp << 8;
    }

    return true;
}

/** return qsfp transceiver information
 *
 *  @param port
 *   port
 *  @param info
 *   qsfp transceiver information struct
 */
int bf_qsfp_get_transceiver_info (int port,
                                  qsfp_transciever_info_t *info)
{
    memset (info, 0,
            sizeof (qsfp_transciever_info_t));
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    info->present = bf_qsfp_present[port];
    if (!info->present) {
        return 0;
    }
    info->port = port;
    bf_qsfp_get_connector_type (port,
                                &info->connector_type);
    bf_qsfp_get_module_type (port,
                             &info->module_type);
    if (!cache_is_valid (port)) {
        return -2;
    }


    if (bf_qsfp_get_module_capability (port,
                                       &info->module_capability)) {
        info->_isset.type = true;
    }
    if (bf_qsfp_get_sensor_info (port,
                                 &info->sensor)) {
        info->_isset.sensor = true;
    }
    if (bf_qsfp_get_vendor_info (port,
                                 &info->vendor)) {
        info->_isset.vendor = true;
    }
    if (bf_qsfp_get_cable_info (port, &info->cable)) {
        info->_isset.cable = true;
    }
    if (bf_qsfp_get_threshold_info (port,
                                    &info->thresh)) {
        info->_isset.thresh = true;
    }
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        info->chn[i].chn = i;
    }

    if (bf_qsfp_get_sensors_per_chan_info (port,
                                           info->chn)) {
        info->_isset.chn = true;
    } else {
        info->_isset.chn = false;
    }
    return 0;
}

/** detect qsfp module by accessing its memory and update cached information
 * accordingly
 *
 *  @param port
 *   port
 */
int bf_qsfp_detect_transceiver (int port,
                                bool *is_present)
{
    if (port < 0 || port > bf_plt_max_qsfp) {
        return -1;
    }
    int err;
    bool st = bf_pltfm_detect_qsfp (port);
    if (st != bf_qsfp_present[port] ||
        bf_qsfp_cache_dirty[port]) {
        bf_qsfp_set_present (port, st);
        *is_present = st;
        if (st) {
            LOG_DEBUG (
                "QSFP: %2d : %s\n",
                port, st ? "PRESENT" : "REMOVED");

            err = bf_qsfp_update_data (port);
            if (err) {
                /*
                 * NOTE, we do not clear IDPROM data here so that we can read
                 * whatever data it returned.
                 */
                LOG_ERROR ("Error<%d>: "
                           "Update data for QSFP %2d\n", err, port);
                return -1;
            }
            bf_qsfp_set_pwr_ctrl (port);
        }
    } else {
        LOG_DEBUG (
            "(%s:%d) "
            "QSFP: %2d : %s\n",
            __func__, __LINE__,
            port, st ? "PRESENT" : "REMOVED");
        *is_present = st;
    }
    return 0;
}

/** return qsfp presence information as per modules' presence pins
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0=present, 1=not present
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0=present, 1=not present
 */
int bf_qsfp_get_transceiver_pres (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports)
{
    return (bf_pltfm_qsfp_get_presence_mask (
                lower_ports, upper_ports, cpu_ports));
}

/** return qsfp interrupt information as per modules' interrupt pins
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0=int present, 1=no interrupt
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0=int present, 1=no interrupt
 */
void bf_qsfp_get_transceiver_int (uint32_t
                                  *lower_ports,
                                  uint32_t *upper_ports,
                                  uint32_t *cpu_ports)
{
    bf_pltfm_qsfp_get_int_mask (lower_ports,
                                upper_ports, cpu_ports);
}

/** return qsfp lpmode hardware pin information
 *
 *  @param lower_ports
 *   bitwise info for lower 32 ports 0= no lpmode, 1= lpmode
 *  @param upper_ports
 *   bitwise info for upper 32 ports 0= no lpmode, 1= lpmode
 */
int bf_qsfp_get_transceiver_lpmode (
    uint32_t *lower_ports,
    uint32_t *upper_ports,
    uint32_t *cpu_ports)
{
    return (bf_pltfm_qsfp_get_lpmode_mask (
                lower_ports, upper_ports, cpu_ports));
}

/** set qsfp lpmode by using hardware pin
 *
 *  @param port
 *    front panel port
 *  @param lpmode
 *    true : set lpmode, false: set no-lpmode
 *  @return
 *    0 success, -1 failure;
 */
int bf_qsfp_set_transceiver_lpmode (int port,
                                    bool lpmode)
{
#if 0
    // For Tofino based system compatibility
    if (!bf_qsfp_flat_mem[port]) &&
        (offset >= MAX_QSFP_PAGE_SIZE) &&
        (!bf_qsfp_cache_dirty[port])) {
        uint8_t pg = (uint8_t)page;
        rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                       &pg);
    }
#endif
    return (bf_pltfm_qsfp_set_lpmode (port, lpmode));
}

/** read the memory of qsfp  transceiver
 *
 *  @param port
 *   port
 *  @param offset
 *    offset into QSFP memory to read from
 *  @param len
 *    number of bytes to read
 *  @param buf
 *    buffer to read into
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_read_transceiver (unsigned int port,
                              int offset,
                              int len,
                              uint8_t *buf)
{
#if 0
    // For Tofino based system compatibility
    if (!bf_qsfp_flat_mem[port]) &&
        (offset >= MAX_QSFP_PAGE_SIZE) &&
        (!bf_qsfp_cache_dirty[port])) {
        uint8_t pg = (uint8_t)page;
        rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                       &pg);
    }
#endif
    return (bf_pltfm_qsfp_read_module (port, offset,
                                       len, buf));
}

/** write into the memory of qsfp  transceiver
 *
 *  @param port
 *   port
 *  @param offset
 *    offset into QSFP memory to write into
 *  @param len
 *    number of bytes to write
 *  @param buf
 *    buffer to write
 *  @return
 *    0 on success, -1 on error
 */
int bf_qsfp_write_transceiver (unsigned int port,
                               int offset,
                               int len,
                               uint8_t *buf)
{
    return (bf_pltfm_qsfp_write_module (port, offset,
                                        len, buf));
}

/** update qsfp data by reading qsfp memory page0 and page3
 *
 *  @param port
 *   port
 */
int bf_qsfp_update_data (int port)
{
    int rc;
    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    bf_qsfp_idprom_clr (port);
    if (bf_qsfp_present[port]) {
        rc = bf_pltfm_qsfp_read_module (
                 port, 0, MAX_QSFP_PAGE_SIZE,
                 &bf_qsfp_idprom[port][0]);
        if (rc) {
            LOG_ERROR ("Error<%d>: "
                       "Reading QSFP %2d\n", rc, port);
            return -1;
        }
        bf_qsfp_cache_dirty[port] = false;
        rc = set_qsfp_idprom (port);
        if (rc) {
            LOG_ERROR ("Error<%d>: "
                       "Set IDPROM for QSFP %2d\n", rc, port);
            return -1;
        }
        /* should the other data be read ??? */
        /* If we have flat memory, we don't have to set the page */
        if (!bf_qsfp_flat_mem[port]) {
            uint8_t page = 0;
            uint8_t page_read;

            rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                             &page);
            if (rc) {
                LOG_ERROR ("Error<%d>: "
                           "Select page %2d for QSFP %2d\n", rc, page, port);
                return -1;
            }
            bf_sys_usleep (50000);
            rc = bf_pltfm_qsfp_read_module (port, 127, 1,
                                            &page_read);
            if ( page ^ page_read) {
                LOG_ERROR ("Error<%d>: "
                           "Select page %2d for QSFP %2d(read check)\n", rc,
                           page, port);
                return -1;
            }
        }
        rc = bf_pltfm_qsfp_read_module (
                 port, 128, MAX_QSFP_PAGE_SIZE,
                 &bf_qsfp_page0[port][0]);
        if (rc) {
            LOG_ERROR ("Error<%d>: "
                       "Reading page_0 from QSFP %2d\n", rc, port);
            return -1;
        }
        if (!bf_qsfp_flat_mem[port]) {
            uint8_t page = 3;
            uint8_t page_read;
            rc = bf_pltfm_qsfp_write_module (port, 127, 1,
                                             &page);
            if (rc) {
                LOG_ERROR ("Error<%d>: "
                           "Select page %2d for QSFP %2d\n", rc, page, port);
                return -1;
            }
            bf_sys_usleep (50000);
            rc = bf_pltfm_qsfp_read_module (
                     port, 128, MAX_QSFP_PAGE_SIZE,
                     &bf_qsfp_page3[port][0]);
            if (rc) {
                LOG_ERROR ("Error<%d>: "
                           "Reading page_3 from QSFP %2d\n", rc, port);
                return -1;
            }
            rc = bf_pltfm_qsfp_read_module (port, 127, 1,
                                            &page_read);
            if ( page ^ page_read) {
                LOG_ERROR ("Error<%d>: "
                           "Select page %2d for QSFP %2d(read check)\n", rc,
                           page, port);
                return -1;
            }
        }
    }
    return 0;
}

/** set qsfp customised power control
 *
 *  @param port
 *   port
 */
void bf_qsfp_set_pwr_ctrl (int port)
{
    return;  /* Mavericks/Montara do not force LP mode on pins and
             so, we just skip this function. Retain the function
             for future use. */
    /*
     * Determine whether we need to customize any of the QSFP registers.
     * Wedge forces Low Power mode via a pin;  we have to reset this
     * to force High Power mode on LR4s.
     *
     * Note that this function expects to be called with qsfpModuleMutex_
     * held.
     */
    if (port > bf_plt_max_qsfp) {
        return;
    }
    if (bf_qsfp_cache_dirty[port] == true) {
        return;
    }

    int offset = 0;
    int length = 0;
    int data_addr = 0;

    get_qsfp_field_addr (EXTENDED_IDENTIFIER,
                         &data_addr, &offset, &length);
    const uint8_t *extId = get_qsfp_value_ptr (port,
                           data_addr, offset, length);
    get_qsfp_field_addr (ETHERNET_COMPLIANCE,
                         &data_addr, &offset, &length);
    const uint8_t *eth_compliance =
        get_qsfp_value_ptr (port, data_addr, offset,
                            length);

    int pwr_ctrl_addr = 0;
    int pwr_ctrl_offset = 0;
    int pwr_ctrl_len = 0;
    get_qsfp_field_addr (
        POWER_CONTROL, &pwr_ctrl_addr, &pwr_ctrl_offset,
        &pwr_ctrl_len);
    const uint8_t *pwr_ctrl =
        get_qsfp_value_ptr (port, pwr_ctrl_addr,
                            pwr_ctrl_offset, pwr_ctrl_len);

    LOG_DEBUG ("port %d qsfp ExtId %d Ether Compliance %d Power Ctrl %d\n",
               port,
               *extId,
               *eth_compliance,
               *pwr_ctrl);

    int high_pwr_level = (*extId &
                          EXT_ID_HI_POWER_MASK);
    int power_level = (*extId & EXT_ID_MASK) >>
                      EXT_ID_SHIFT;

    if (high_pwr_level > 0 || power_level > 0) {
        uint8_t power = POWER_OVERRIDE;
        if (high_pwr_level > 0) {
            power |= HIGH_POWER_OVERRIDE;
        }

        /* Note that we don't have to set the page here, but there should
         * probably be a setQsfpValue() function to handle pages, etc.
         */

        if (pwr_ctrl_addr != QSFP_PAGE0_LOWER) {
            LOG_ERROR ("Error setting qsfp POWER_CTRL, incorrect page number\n");
            return;
        }
        if (pwr_ctrl_len != sizeof (power)) {
            LOG_ERROR ("Error setting qsfp POWER_CTRL, incorrect length\n");
            return;
        }

        if (bf_pltfm_qsfp_write_module (
                port, pwr_ctrl_offset, sizeof (power), &power)) {
            LOG_ERROR ("Error writing POWER_CTRL to qsfp %d\n",
                       port);
        }
    }
}

/* copies cached x-ver data to user suppled buffer
 * buffer must take MAX_QSFP_PAGE_SIZE  bytes in it
 */
/** return qsfp cached information
 *
 *  @param port
 *   port
 *  @param page
 *   lower page, upper page0 or page3
 *  @param buf
 *   read the data into
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_cached_info (int port, int page,
                             uint8_t *buf)
{
    uint8_t *cptr;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    if (bf_qsfp_cache_dirty[port] == true) {
        return -1;
    }
    if (page == QSFP_PAGE0_LOWER) {
        cptr = &bf_qsfp_idprom[port][0];
    } else if (page == QSFP_PAGE0_UPPER) {
        cptr = &bf_qsfp_page0[port][0];
    } else if (page == QSFP_PAGE3) {
        cptr = &bf_qsfp_page3[port][0];
    } else {
        return -1;
    }
    memcpy (buf, cptr, MAX_QSFP_PAGE_SIZE);
    return 0;
}

/** return qsfp "disabled" state cached information
 *
 *  @param port
 *   port
 *  @param channel
 *   channel id
 *  @return
 *   true if channel tx is disabled (or bad param) and false if enabled
 */
bool bf_qsfp_tx_is_disabled (int port,
                             int channel)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t *data = NULL;

    if (port > bf_plt_max_qsfp || channel >= 4) {
        return true;
    }
    /* read the cached value */
    get_qsfp_field_addr (CHANNEL_TX_DISABLE,
                         &data_addr, &offset, &length);
    data = get_qsfp_value_ptr (port, data_addr,
                               offset, length);
    if (!data) {
        return true;
    }
    return (*data & (1 << channel));
}

/** return qsfp ethernet compliance code information
 *
 *  @param port
 *   port
 *  @param code
 *   pointer to Ethernet_compliance
 *  @return
 *   0 on success and -1 on error
 */
int bf_qsfp_get_eth_compliance (int port,
                                Ethernet_compliance *code)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (ETHERNET_COMPLIANCE,
                         &data_addr, &offset, &length);
    uint8_t *eth_compliance = get_qsfp_value_ptr (
                                  port, data_addr, offset, length);
    if (eth_compliance == NULL) {
        *code = COMPLIANCE_NONE;
    } else {
        *code = *eth_compliance; /* bit 7 is reserved */
    }
    return 0;
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
int bf_qsfp_get_eth_ext_compliance (int port,
                                    Ethernet_extended_compliance *code)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (
        ETHERNET_EXTENDED_COMPLIANCE, &data_addr, &offset,
        &length);
    uint8_t *eth_ext_comp = get_qsfp_value_ptr (port,
                            data_addr, offset, length);
    if (eth_ext_comp == NULL) {
        *code = EXT_COMPLIANCE_NONE;
    } else {
        *code = *eth_ext_comp;
    }
    return 0;
}

int bf_qsfp_get_connector_type (int port,
                                uint8_t *value)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (CONNECTOR_TYPE, &data_addr,
                         &offset, &length);
    uint8_t *tmp = get_qsfp_value_ptr (port,
                                       data_addr, offset, length);
    if (tmp == NULL) {
        *value = 0;
    } else {
        *value = *tmp;
    }

    return 0;
}

int bf_qsfp_get_module_type (int port,
                             uint8_t *value)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (IDENTIFIER, &data_addr,
                         &offset, &length);
    uint8_t *tmp = get_qsfp_value_ptr (port,
                                       data_addr, offset, length);
    if (tmp == NULL) {
        *value = 0;
    } else {
        *value = *tmp;
    }
    return 0;
}

bool bf_qsfp_is_optic (bf_pltfm_qsfp_type_t
                       qsfp_type)
{
    if (qsfp_type == BF_PLTFM_QSFP_OPT) {
        return true;
    } else {
        return false;
    }
}
#if 0
/** return qsfp is optical/AOC
*
*  @param port
*   port
*  @return
*   true if Optical/AOC
*/
bool bf_qsfp_is_optical (int port)
{
    if (port > bf_plt_max_qsfp) {
       return false;
    }
    return !bf_qsfp_info_arr[port].passive_cu;
}
#endif
/** return the number of actual channels in the module
*
*  @param port
*   port
*  @return
*   number of channels, 0 on error
*/
uint8_t bf_qsfp_get_ch_cnt (int port)
{
   if (port > bf_plt_max_qsfp) {
       return 0;
   }
   return 4;
}

/** return the number of media-side channels in the module
 *
 *  @param port
 *   port
 *  @return
 *   number of channels, 0 on error
 */
uint8_t bf_qsfp_get_media_ch_cnt (int port)
{
    return bf_qsfp_get_ch_cnt(port);
}

/*
 * Force sets qsft_type and type_copper
 */
int bf_qsfp_special_case_set (int port,
                              bf_pltfm_qsfp_type_t qsfp_type,
                              bool is_set)
{
    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    memset (&qsfp_special_case_port[port], 0,
            sizeof (bf_qsfp_special_info_t));
    if (is_set) {
        qsfp_special_case_port[port].qsfp_type =
            qsfp_type;
        qsfp_special_case_port[port].is_set = true;
    }

    return 0;
}

// returns true if copper type
static bool bf_qsfp_get_is_type_copper (
    bf_pltfm_qsfp_type_t qsfp_type)
{
    if (qsfp_type != BF_PLTFM_QSFP_OPT) {
        return true;
    }
    return false;
}

/* checks and vendor id and vendor P/N against a set of known special
 * cases and force sets qsft_type and type_copper
 * return true: if handled as special case and false, otherwise.
 */
static bool bf_qsfp_special_case_handled (
    int port,
    bf_pltfm_qsfp_type_t *qsfp_type,
    bool *type_copper)
{
    qsfp_vendor_info_t vendor;

    bf_qsfp_get_vendor_info (port, &vendor);
    if (!memcmp (vendor.name, "Ampheno",
                 strlen ("Ampheno")) &&
        !memcmp (vendor.part_number, "62162000",
                 strlen ("62162000"))) {
        *qsfp_type = BF_PLTFM_QSFP_CU_3_M;
        *type_copper = true;
        /* vendor strings are not NULL terminated. So,doit here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to 3m-copper for %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (vendor.name, "ELPEU",
                        strlen ("ELPEU")) &&
               !memcmp (
                   vendor.part_number, "QSFP28-LB-6D",
                   strlen ("QSFP28-LB-6D"))) {
        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
        *type_copper = true;
        /* vendor strings are not NULL terminated. So,doit here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to copper-loopback for %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (vendor.name, "CISCO-FINISAR",
                        strlen ("CISCO-FINISAR")) &&
               !memcmp (vendor.part_number,
                        "FCBN410QE2C01-C2",
                        strlen ("FCBN410QE2C01-C2"))) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        *type_copper = false;
        /* vendor strings are not NULL terminated. So,do it here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to optical %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (!memcmp (
                   vendor.name, "Arista Networks",
                   strlen ("Arista Networks")) &&
               !memcmp (vendor.part_number,
                        "QSFP28-100G-AOC",
                        strlen ("QSFP28-100G-AOC"))) {
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        *type_copper = false;
        /* vendor strings are not NULL terminated. So,do it here. */
        vendor.name[15] = vendor.part_number[15] = 0;
        LOG_DEBUG ("forcing port %d qsfp type to optical %s p/n %s\n",
                   port,
                   vendor.name,
                   vendor.part_number);
        return true;
    } else if (qsfp_special_case_port[port].is_set) {
        // Set by cli
        *qsfp_type =
            qsfp_special_case_port[port].qsfp_type;
        *type_copper = bf_qsfp_get_is_type_copper (
                           *qsfp_type);
        vendor.name[15] = vendor.part_number[15] = 0;
        return true;
    }
    /* add more such problematic cases here */

    /* its not a special case */
    return false;
}

/** use qsfp vendor to judge the qsfp is reset over or not
*
*  @param port
*   port
*  @param code
*   pointer to Ethernet_compliance
*  @return
*   0 on success and -1 on error
*/
int bf_qsfp_init_over (int port)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (VENDOR_NAME, &data_addr,
                         &offset, &length);
    uint8_t *vendor_name = get_qsfp_value_ptr (port,
                           data_addr, offset, length);
    /* Empty vendor name (ID) or an invalid ASCII(0). */
    if (vendor_name == NULL || *vendor_name == 0) {
        return 0; //not ready
    } else {
        //        LOG_DEBUG("QSFP : %2d Vendor Name : %s", port, *vendor_name);
        return 1; //is OK
    }
    return 0;
}

int bf_qsfp_type_get (int port,
                      bf_pltfm_qsfp_type_t *qsfp_type)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    bool type_copper = true;  // default to copper

    if (port > bf_plt_max_qsfp) {
        return -1;
    }

    int retry_times = 0;
    const int max_retry_times = 11;
retry_begin :
    if (! (bf_qsfp_init_over (port))) {
        //wait some time and retry
        sleep (1);
        retry_times ++;
        LOG_DEBUG ("QSFP : %2d get_type retry %2d/%2d times",
                   port, retry_times, max_retry_times);
        //it is bad to retry
        if (retry_times == max_retry_times) {
            return -1;
        }
        goto retry_begin;
    }

    Ethernet_compliance eth_comp;
    if (bf_qsfp_get_eth_compliance (port,
                                    &eth_comp) != 0) {
        // Default to Copper Loop back in case of error

        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
        return -1;
    }

    /* there are some odd module/cable types that have inconsistent information
     * in it. We need to categorise them seperately
     */
    if (bf_qsfp_special_case_handled (port, qsfp_type,
                                      &type_copper)) {
        return 0;
    }

    /* SFF standard is not clear about necessity to look at both compliance
     * code and the extended compliance code. From the cable types known so far,
     * if bit-7 of compliance code is set, then, we look at extended compliance
     * code as well. Otherwise, we characterize the QSFP by regular compliance
     * code only.
     */
    LOG_DEBUG ("QSFP : %2d : eth_comp : %2d\n", port,
               eth_comp);
    if (eth_comp &
        0x77) {  // all except 40GBASE-CR4 and ext compliance bits
        type_copper = false;
    } else if (eth_comp & 0x80) {
        Ethernet_extended_compliance ext_comp;

        if (bf_qsfp_get_eth_ext_compliance (port,
                                            &ext_comp) == 0) {
            if ((ext_comp > 0) && (ext_comp < 9)) {
                type_copper = false;
            } else if ((ext_comp >= 16) &&
                       (ext_comp != 0x16)) {
                type_copper = false;
            } else {
                type_copper = true;
            }
        } else {
            type_copper = true;
        }
    }
    if (!type_copper) {  // we are done if found optical module
        *qsfp_type = BF_PLTFM_QSFP_OPT;
        return 0;
    }

    get_qsfp_field_addr (LENGTH_COPPER, &data_addr,
                         &offset, &length);
    uint8_t *qsfp_length = get_qsfp_value_ptr (port,
                           data_addr, offset, length);
    if (qsfp_length == NULL) {
        // Indicates that the QSFP length is not specified. Default to Cu Loopback
        *qsfp_type = BF_PLTFM_QSFP_CU_LOOP;
        return 0;
    }
    LOG_DEBUG ("QSFP : %2d : len_copp : %2d\n", port,
               *qsfp_length);
    switch (*qsfp_length) {
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
    return 0;
}

void bf_qsfp_debug_clear_all_presence_bits (void)
{
    int port;

    for (port = 0; port < BF_PLAT_MAX_QSFP + 1;
         port++) {
        if (bf_qsfp_present[port]) {
            bf_qsfp_set_present (port, false);
        }
    }
}

extern bool bf_pm_qsfp_is_luxtera (int conn_id);

bool bf_qsfp_is_luxtera_optic (int conn_id)
{
    return bf_pm_qsfp_is_luxtera (conn_id);
}

bool bf_qsfp_is_dd (int port)
{
    int offset = 0;
    int length = 0;
    int data_addr = 0;
    uint8_t *id = NULL;

    if (port > bf_plt_max_qsfp) {
        return -1;
    }
    get_qsfp_field_addr (IDENTIFIER, &data_addr,
                         &offset, &length);
    id = get_qsfp_value_ptr (port, data_addr, offset,
                             length);
    if (*id == QSFP_DD) {
        return true;
    }

    return 0;
}

