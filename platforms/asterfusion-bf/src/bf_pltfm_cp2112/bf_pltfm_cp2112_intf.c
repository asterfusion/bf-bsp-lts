/*******************************************************************************
 * Barefoot Networks, Inc.
 * modified from original C++ version
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
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_bd_cfg/bf_bd_cfg_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_bmc_tty.h>
#include <bf_pltfm.h>
#include "bf_pltfm_uart.h"

#define DEFAULT_TIMEOUT_MS 500

/* one cp2112 by default, by tsihang, 2021-07-14. */
int g_max_cp2112_num = 1;
/* Make sure that the cp2112 hndl in CP2112_ID_1
 * for those platforms which has two CP2112 but only one on duty.
 * So here we only return CP2112_ID_1 for module IO.
 * See differentiate_cp2112_devices for reference.
 * by tsihang, 2021-07-14. */
#define HAVE_ONE_CP2112_ON_DUTY

static volatile int bf_pltfm_cp2112_initialized =
    0;

static bf_pltfm_board_id_t bd_id;

static bf_sys_mutex_t
cp2112_mutex[CP2112_ID_MAX];  // 0->Lower, 1->Upper

static bf_pltfm_cp2112_device_ctx_t
cp2112_dev_arr[CP2112_ID_MAX];

uint8_t cp2112_id_map[CP2112_ID_MAX] = {0, 1};

typedef enum bf_pltfm_cp2112_report_id_e {
    // Feature reports
    RESET_DEVICE = 0x01,
    GPIO_CONFIG = 0x02,
    GET_GPIO = 0x03,
    SET_GPIO = 0x04,
    GET_VERSION = 0x05,
    SMBUS_CONFIG = 0x06,
    // Interrupt reports
    READ_REQUEST = 0x10,
    WRITE_READ_REQUEST = 0x11,
    READ_FORCE_SEND = 0x12,
    READ_RESPONSE = 0x13,
    WRITE = 0x14,
    XFER_STATUS_REQUEST = 0x15,
    XFER_STATUS_RESPONSE = 0x16,
    CANCEL_XFER = 0x17,
    // Feature reports for one-time USB PROM customization
    LOCK_BYTE = 0x20,
    USB_CONFIG = 0x21,
    MFGR_STRING = 0x22,
    PRODUCT_STRING = 0x23,
    SERIAL_STRING = 0x24,
} bf_pltfm_cp2112_report_id_t;

typedef enum bf_pltfm_cp2112_report_type_e {
    INPUT = 0x0100,
    OUTPUT = 0x0200,
    FEATURE = 0x0300,
} bf_pltfm_cp2112_report_type_t;

typedef enum bf_pltfm_cp2112_hid_e {
    GET_REPORT = 1,
    SET_REPORT = 9,
} bf_pltfm_cp2112_hid_t;

bf_pltfm_status_t bf_pltfm_bmc_cp2112_reset (
    bool primary);

/*
 * SMBus configuration parameters.
 */
typedef struct bf_pltfm_cp2112_sm_bus_cfg_t {
    /*
     * Speed to drive the SMBus, in hertz.
     * The hardware defaults to 100,000 (100KHz)
     */
    uint32_t speed;

    /*
     * Slave address to respond on.  The CP2112 will ACK this address,
     * but won't respond to anything else.
     * The hardware default for this value is 2.
     */
    uint8_t address;

    /*
     * Auto send read.
     * 0: Only send a read response after a READ_FORCE_SEND request
     * 1: Send read response data (including partial data) whenever it is
     *    available when an interrupt poll is received.
     * Defaults to 0.
     *
     * Note: The auto-send read behavior appears to be slightly broken based on
     * my testing.  See the comments in initSettings().
     */
    uint8_t auto_send_read;

    /*
     * Write timeout, in milliseconds.
     * Defaults to 0 (no timeout).
     * Maximum allowed is 1000ms.
     */
    uint16_t write_timeout_ms;

    /*
     * Read timeout, in milliseconds.
     * Defaults to 0 (no timeout).
     * Maximum allowed is 1000ms.
     */
    uint16_t read_timeout_ms;

    /*
     * If sclLowTimeout is 1, the CP2112 will reset the bus if a slave
     * device holds the SCL line low for more than 25ms.
     *
     * Defaults to 0 (disabled).
     */
    uint8_t scl_low_timeout;

    /*
     * The number of times to retry a transfer before giving up.
     * Defaults to 0.  Values over 1000 are ignored.
     */
    uint16_t retry_limit;
} bf_pltfm_cp2112_sm_bus_cfg_t;

static bool is_big_endian_flag;

// Static function definitions
static void swap_endianness_32b (uint32_t *num)
{
    uint32_t temp = *num;
    uint32_t b0, b1, b2, b3;

    b0 = (temp & 0x000000ff) << 24;
    b1 = (temp & 0x0000ff00) << 8;
    b2 = (temp & 0x00ff0000) >> 8;
    b3 = (temp & 0xff000000) >> 24;

    *num = (b0 | b1 | b2 | b3);
}

static void swap_endianness_16b (uint16_t *num)
{
    uint16_t temp = *num;
    uint16_t b0, b1;

    b0 = (temp & 0x00ff) << 8;
    b1 = (temp & 0xff00) >> 8;

    *num = (b0 | b1);
}

static bool is_big_endian()
{
    uint32_t val = 0x12345678;
    uint8_t *ptr = (uint8_t *)&val;
    if (ptr[0] == 0x78) {
        return false;
    } else {
        return true;
    }
}

static int usb_transfer_control_out (
    libusb_device_handle *hndl,
    bf_pltfm_cp2112_report_id_t report,
    uint8_t *buf,
    uint16_t len)
{
    uint8_t b_request_type = (LIBUSB_ENDPOINT_OUT |
                              LIBUSB_REQUEST_TYPE_CLASS |
                              LIBUSB_RECIPIENT_INTERFACE);
    uint8_t b_request = SET_REPORT;
    uint16_t w_value = FEATURE | report;
    uint16_t w_index =
        0;  // the index of the interface
    unsigned int timeout_ms = 1000;
    int rc;

    rc = libusb_control_transfer (
             hndl, b_request_type, b_request, w_value, w_index,
             buf, len, timeout_ms);
    if (rc < 0) {
        LOG_DEBUG (
            "Error in performing a USB control transfer with error code %s at "
            "%s:%d",
            libusb_strerror (rc),
            __func__,
            __LINE__);
        return rc;
    }

    if (rc != len) {
        LOG_DEBUG ("Unexpected number of bytes actually written at %s:%d",
                   __func__,
                   __LINE__);
        return -1;
    }
    return 0;
}

static int sm_bus_config_set (libusb_device_handle
                              *hndl,
                              bf_pltfm_cp2112_sm_bus_cfg_t *config)
{
    // Caller need to ensure that the read and write timeouts are not
    // greater than 1000 ms as the chip will ignore it
    if (config->write_timeout_ms > 1000) {
        LOG_DEBUG ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return -1;
    }
    if (config->read_timeout_ms > 1000) {
        LOG_DEBUG ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return -1;
    }

    // Caller has to ensure that the retry limit is not greater than 1000
    if (config->retry_limit > 1000) {
        LOG_DEBUG ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return -1;
    }

    if (is_big_endian_flag == false) {
        swap_endianness_32b (&config->speed);
        swap_endianness_16b (&config->write_timeout_ms);
        swap_endianness_16b (&config->read_timeout_ms);
        swap_endianness_16b (&config->retry_limit);
    }

    uint8_t buf[14];
    int i = 0;
    for (i = 0; i < 14; i++) {
        buf[i] = 0;
    }
    buf[0] = SMBUS_CONFIG;
    memcpy (buf + 1, &config->speed, 4);
    buf[5] = config->address;
    buf[6] = config->auto_send_read;
    memcpy (buf + 7, &config->write_timeout_ms, 2);
    memcpy (buf + 9, &config->read_timeout_ms, 2);
    buf[11] = config->scl_low_timeout;
    memcpy (buf + 12, &config->retry_limit, 2);

    int rc = usb_transfer_control_out (hndl,
                                       SMBUS_CONFIG, buf, sizeof (buf));
    if (rc != 0) {
        LOG_DEBUG ("Error in setting the desired sm bus config at %s:%d",
                   __func__,
                   __LINE__);
        return -1;
    }
    return 0;
}

static int sm_bus_config_init (
    libusb_device_handle *hndl)
{
    bf_pltfm_cp2112_sm_bus_cfg_t cfg;
    int rc;

    cfg.address = 0x02;
    cfg.auto_send_read = 0;
    cfg.speed = 100000;
    cfg.write_timeout_ms = 25;
    cfg.read_timeout_ms = 25;
    cfg.retry_limit = 1;
    cfg.scl_low_timeout = 1;
    rc = sm_bus_config_set (hndl, &cfg);
    if (rc != 0) {
        LOG_DEBUG (
            "Error in setting the sm bus config at %s:%d",
            __func__, __LINE__);
        return -1;
    }

    return 0;
}

static enum libusb_error usb_intr_transfer_in (
    libusb_device_handle *hndl,
    bool *bus_good,
    uint8_t *buf,
    uint16_t len,
    int timeout_ms)
{
    // Use 64 byte interrupt transfers. Caller need to ensure this
    if (len != 64) {
        LOG_DEBUG ("Invalid argument %d instead of 64 at %s:%d",
                   len,
                   __func__,
                   __LINE__);
        return LIBUSB_ERROR_OTHER;
    }

    // Use endpoint 1 for interrupt transfers
    uint8_t end_point = LIBUSB_ENDPOINT_IN | 1;
    int transferred;

    // Set the timeout to atleast 1 ms
    unsigned int usb_timeout_ms = (timeout_ms > 50 ?
                                   timeout_ms : 50);
    int rc = libusb_interrupt_transfer (
                 hndl, end_point, buf, len, &transferred,
                 usb_timeout_ms);
    if (rc != 0) {
        *bus_good = false;
        LOG_DEBUG (
            "Error waiting for interrupt response with error code %s at %s;%d",
            libusb_strerror (rc),
            __func__,
            __LINE__);
        LOG_DEBUG ("Number of bytes transferred are %d",
                   transferred);
        return rc;
    }
    if (transferred != 64) {
        *bus_good = false;
        LOG_DEBUG (
            "Unexpected interrupt response length received from CP2112 at %s:%d",
            __func__,
            __LINE__);
        return LIBUSB_ERROR_OTHER;
    }
    return LIBUSB_SUCCESS;
}

static enum libusb_error usb_intr_transfer_out (
    libusb_device_handle *hndl,
    bool *bus_good,
    uint8_t *buf,
    uint16_t len,
    int timeout_ms,
    char *str)
{
    // Use 64 byte interrupt transfers. Caller need to ensure this
    if (len != 64) {
        LOG_DEBUG ("Error: Invalid arguent at %s:%d",
                   __func__, __LINE__);
        return LIBUSB_ERROR_OTHER;
    }

    // Use endpoint 1 for interrupt transfers
    uint8_t end_point = LIBUSB_ENDPOINT_OUT | 1;
    int transferred;

    /* Set the timeout to atleast 5 ms as we don't want to timeout inside
       libusb calls. If a timeout occurs then we are not sure if the transfer
       was sent to the device. To avoid this, use a minimum timeout of 5
       seconds */
    int usb_timeout_ms = (timeout_ms > 50 ?
                          timeout_ms : 50);
    int rc = libusb_interrupt_transfer (
                 hndl, end_point, buf, len, &transferred,
                 usb_timeout_ms);
    if (rc != 0) {
        *bus_good = false;
        LOG_DEBUG ("Failed to send %s request with error code %s at %s:%d",
                   str,
                   libusb_strerror (rc),
                   __func__,
                   __LINE__);
        return rc;
    }
    // LOG_DEBUG(
    //    "Libusb interrupt transfer out succeeded. Total bytes transferred %d\n",
    //    transferred);
    (void)timeout_ms;
    return LIBUSB_SUCCESS;
}

void cp2112_flush_transfers (
    bf_pltfm_cp2112_device_ctx_t *dev)
{
    /* Pull out and discard any interrupts in packets waiting for receipt.
       This ensures that all the future interrupts in packets we receive are
       in response for the requests we sent.*/
    uint8_t buf[64];
    enum libusb_error rc;
    int timeout_ms = 3;

    while (true) {
        rc = usb_intr_transfer_in (
                 dev->usb_device_handle, &dev->bus_good, buf,
                 sizeof (buf), timeout_ms);
        if (rc != LIBUSB_SUCCESS) {
            if (rc != LIBUSB_ERROR_TIMEOUT) {
                LOG_DEBUG ("Error in flushing transfers at %s:%d",
                           __func__, __LINE__);
                return;
            }
            dev->bus_good = true;
            break;
        }
    }
}

static void transfer_status_read (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t *buf,
    int timeout_ms,
    uint32_t iter,
    char *str)
{
    uint16_t b_size = 64;

    buf[0] = XFER_STATUS_REQUEST;
    buf[1] = 1;
    int rc;

    rc = usb_intr_transfer_out (
             dev->usb_device_handle,
             &dev->bus_good,
             buf,
             b_size,
             timeout_ms,
             "get xfer status");
    if (rc != LIBUSB_SUCCESS) {
        LOG_DEBUG (
            "Eror code : %s at %s:%d", libusb_strerror (rc),
            __func__, __LINE__);
        return;
    }

    rc = usb_intr_transfer_in (
             dev->usb_device_handle, &dev->bus_good, buf,
             b_size, 20);
    if (rc != LIBUSB_SUCCESS) {
        LOG_DEBUG (
            "Eror code : %s at %s:%d\n", libusb_strerror (rc),
            __func__, __LINE__);
        return;
    }

    if (buf[0] == READ_RESPONSE) {
        if (buf[1] != 0) {
            LOG_DEBUG ("Device is not idle at %s:%d, code is %d",
                       __func__,
                       __LINE__,
                       buf[1]);
            return;
        }  // Status should be idle
        if (iter != 1) {
            LOG_DEBUG ("Loop iteration is %d instead of 1 at %s:%d",
                       iter,
                       __func__,
                       __LINE__);
            return;
        }  // Loop iteration has to be 1 else it is an error
        if (buf[2] != 0) {
            LOG_DEBUG (
                "Response length %d is not 0 at %s:%d", buf[2],
                __func__, __LINE__);
            return;
        }

        // Read one more time to receive the XFER_STATUS_RESPONSE
        rc = usb_intr_transfer_in (
                 dev->usb_device_handle, &dev->bus_good, buf,
                 b_size, 20);
        if (rc != LIBUSB_SUCCESS) {
            LOG_DEBUG (
                "Eror code : %s at %s:%d\n", libusb_strerror (rc),
                __func__, __LINE__);
            return;
        }
    }

    if (buf[0] != XFER_STATUS_RESPONSE) {
        /* Even after reading the status twice, we don't get an expected response
           it indicates that we are out of sync with the device. Hence asserr */
        LOG_ERROR ("Received response %d while waiting on operation %s at %s:%d",
                   buf[0],
                   str,
                   __func__,
                   __LINE__);
        dev->bus_good = false;
        LOG_ERROR (
            "Error: Out of sync with the device at %s:%d",
            __func__, __LINE__);
    }
}

void cp2112_cancel_transfers (
    bf_pltfm_cp2112_device_ctx_t *dev)
{
    // Cancel any outstanding transfers on the bus
    uint8_t buf[64];
    int timeout_ms = 5;
    enum libusb_error rc;

    memset (buf, 0, sizeof (buf));
    buf[0] = CANCEL_XFER;
    buf[1] = 1;

    rc = usb_intr_transfer_out (
             dev->usb_device_handle,
             &dev->bus_good,
             buf,
             sizeof (buf),
             timeout_ms,
             "cancel transfer");
    if (rc != LIBUSB_SUCCESS) {
        LOG_DEBUG (
            "Error while performing a USB interrupt transfer with error code %s at "
            "%s:%d",
            libusb_strerror (rc),
            __func__,
            __LINE__);
    }

    uint8_t new_buf[64];

    // Read the status to read and clear out any failure state
    transfer_status_read (
        dev, new_buf, DEFAULT_TIMEOUT_MS, 1,
        "read and clear any failure state");
}

static void ensure_good_state (
    bf_pltfm_cp2112_device_ctx_t *dev)
{
    if (dev->bus_good) {
        return;
    }

    cp2112_flush_transfers (dev);
    dev->bus_good = true;
}

static void subtract_time_spec (struct timespec
                                *time1,
                                struct timespec *time2,
                                struct timespec *time3)
{
    if (time2->tv_nsec <= time1->tv_nsec) {
        time3->tv_sec = time1->tv_sec - time2->tv_sec;
        time3->tv_nsec = time1->tv_nsec - time2->tv_nsec;
    } else {
        time3->tv_sec = time1->tv_sec - time2->tv_sec - 1;
        time3->tv_nsec = time1->tv_nsec + (1000000000 -
                                           time2->tv_nsec);
    }
}

static bool is_timespec_neg (struct timespec *t)
{
    if (t->tv_sec < 0) {
        return true;
    } else if (t->tv_nsec < 0) {
        return true;
    }

    return false;
}

static void update_time_left (struct timespec
                              *time_end,
                              struct timespec *time_left,
                              bool is_sleep)
{
    struct timespec time_now;
    clock_gettime (CLOCK_MONOTONIC, &time_now);
    subtract_time_spec (time_end, &time_now,
                        time_left);
    if ((is_timespec_neg (time_left) == false) &&
        is_sleep) {
        int time_left_ms =
            ((time_left->tv_sec * 1000) +
             (time_left->tv_nsec) / 1000000);
        int sleep_dur_ms = time_left_ms < 1 ?
                           time_left_ms : 1;
        usleep (sleep_dur_ms * 1000);
        struct timespec temp;
        temp.tv_sec = sleep_dur_ms / 1000;
        temp.tv_nsec = (sleep_dur_ms % 1000) * 1000000;
        subtract_time_spec (time_left, &temp, time_left);
        (void)time_left_ms;  // Keep the compiler happy
    }
}

static char *get_busy_status_msg (uint8_t sts)
{
    switch (sts) {
        case 0:
            return "address acknowledged";
        case 1:
            return "address not acknowledged";
        case 2:
            return "read incomplete";
        case 3:
            return "write incomplete";
    }
    LOG_DEBUG ("Error: Unexpected timeout status at %s:%d",
               __func__, __LINE__);
    return NULL;
}

static int wait_for_transfer (
    bf_pltfm_cp2112_device_ctx_t *dev,
    struct timespec *time_end,
    struct timespec *time_left,
    char *str)
{
    struct timespec time_now;
    clock_gettime (CLOCK_MONOTONIC, &time_now);
    subtract_time_spec (time_end, &time_now,
                        time_left);

    int time_left_ms = time_left->tv_sec * 1000 +
                       time_left->tv_nsec / 1000000;

    uint8_t buf[64], sts_0, sts_1;
    uint32_t iter = 0;

    while (true) {
        ++iter;
        transfer_status_read (dev, buf, time_left_ms,
                              iter, str);

        sts_0 = buf[1];
        sts_1 = buf[2];

        if (sts_0 == 2) {
            // successfully completed
            return 0;
        } else if (sts_0 == 3) {
            // failed
            LOG_DEBUG ("Operation %s failed with status %d at %s:%d",
                       str,
                       sts_1,
                       __func__,
                       __LINE__);
            return -1;
        } else if (sts_0 != 1) {
            dev->bus_good = false;
            LOG_DEBUG ("Unexpected status while waiting on operation %s at %s:%d",
                       str,
                       __func__,
                       __LINE__);
            return -1;
        }
        update_time_left (time_end, time_left, true);
        if (is_timespec_neg (time_left)) {
            cp2112_cancel_transfers (dev);
            LOG_DEBUG ("Timed out waiting on operation %s with status '%s' at %s:%d",
                       str,
                       get_busy_status_msg (sts_1),
                       __func__,
                       __LINE__);
            return (0 - sts_1);
        }
    }
}

static int process_read_response (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t *buf,
    uint32_t buf_size,
    int timeout_ms)
{
    struct timespec time_left, time_now, time_end;
    clock_gettime (CLOCK_MONOTONIC, &time_now);
    time_end.tv_sec = time_now.tv_sec + timeout_ms /
                      1000;
    time_end.tv_nsec = time_now.tv_nsec + ((
            timeout_ms % 1000) * 1000000);
    if (time_end.tv_nsec >= 1000000000) {
        time_end.tv_nsec = time_end.tv_nsec - 1000000000;
        time_end.tv_sec++;
    }
    int rc = wait_for_transfer (dev, &time_end,
                                &time_left, "read");
    if (rc < 0) {
        LOG_DEBUG ("Error: at %s:%d", __func__,
                   __LINE__);
        return rc;
    }

    uint8_t usb_buf[64];
    uint16_t bytes_read = 0;
    bool send_read = true;
    enum libusb_error err;

    while (true) {
        if (send_read) {
            usb_buf[0] = READ_FORCE_SEND;
            usb_buf[1] = 1;
            err = usb_intr_transfer_out (
                      dev->usb_device_handle,
                      &dev->bus_good,
                      usb_buf,
                      sizeof (usb_buf),
                      5,
                      "read force send");
            if (err != LIBUSB_SUCCESS) {
                LOG_DEBUG ("Eror code : %s at %s:%d",
                           libusb_strerror (err),
                           __func__,
                           __LINE__);
                return -1;
            }
            send_read = false;
        }

        err = usb_intr_transfer_in (
                  dev->usb_device_handle, &dev->bus_good, usb_buf,
                  sizeof (usb_buf), 10);
        if (err != LIBUSB_SUCCESS) {
            if (err != LIBUSB_ERROR_TIMEOUT) {
                LOG_DEBUG (
                    "Unexpected error occurred in process read response at %s:%d",
                    __func__,
                    __LINE__);
                return -1;
            }

            update_time_left (&time_end, &time_left, false);
            if (is_timespec_neg (&time_left)) {
                LOG_DEBUG (
                    "Error: timedout while waiting for read response data at %s:%d",
                    __func__,
                    __LINE__);
                return -1;
            }

            send_read = true;
            dev->bus_good = true;
            continue;
        }

        if (usb_buf[0] != READ_RESPONSE) {
            LOG_DEBUG (
                "Error: Received unexpected interrupt response %d while waiting on "
                "read response. Possibly out of sync with the device at %s:%d",
                usb_buf[0],
                __func__,
                __LINE__);
            dev->bus_good = false;
            return -1;
        }

        uint8_t status = usb_buf[1];
        uint8_t length = usb_buf[2];

        if ((bytes_read + length) > buf_size) {
            LOG_ERROR (
                "Error: cp2112 read buffer overrun bytes_read %u len_read %u "
                "available buf_size %u",
                bytes_read,
                length,
                buf_size);
            return -1;
        }

        memcpy (buf + bytes_read, usb_buf + 3, length);
        bytes_read += length;

        if (status == 0 || status == 2) {
            if (bytes_read == buf_size && length == 0) {
                break;
            }
        } else if (status != 1) {
            LOG_DEBUG (
                "Error: Unepected read failure after successful XFER_STATUS_RESPONSE "
                "at %s:%d",
                __func__,
                __LINE__);
            dev->bus_good = false;
            return -1;
        }

        send_read = (bytes_read < buf_size) &&
                    (length < 61);

        bool sleep = (length == 0);
        update_time_left (&time_end, &time_left, sleep);
        if (is_timespec_neg (&time_left)) {
            LOG_DEBUG ("Error: timed out waiting on read response data at %s:%d",
                       __func__,
                       __LINE__);
            return -1;
        }

    }  // while

    return 0;
}

static int usb_device_find (int max_dev_to_find,
                            bf_pltfm_cp2112_device_ctx_t *cp2112_dev_arr,
                            uint16_t vendor,
                            uint16_t product)
{
    libusb_device **dev_list;
    libusb_device *dev;
    struct libusb_device_descriptor desc;

    int num_found = 0;
    int num_dev =
        libusb_get_device_list (
            cp2112_dev_arr[0].usb_context, &dev_list);
    LOG_DEBUG ("No. of USB devices found are : %d",
               num_dev);
    if (num_dev < 0) {
        LOG_ERROR ("Error: Failed to list USB devices with error code %s at %s:%d",
                   libusb_strerror (num_dev),
                   __func__,
                   __LINE__);
        return -1;
    }

    int i, rc;
    for (i = 0; i < num_dev; i++) {
        dev = dev_list[i];
        rc = libusb_get_device_descriptor (dev, &desc);
        if (rc != 0) {
            LOG_DEBUG (
                "Error: Failed to query device descriptor for device %d with error "
                "code %s at %s:%d",
                i,
                libusb_strerror (rc),
                __func__,
                __LINE__);
            continue;
        }

        if (desc.idVendor == vendor &&
            desc.idProduct == product) {
            cp2112_dev_arr[num_found].usb_device = dev;
            num_found++;
            if (num_found == max_dev_to_find) {
                return 0;
            }
        }
    }
    LOG_DEBUG ("Error: Device not found at %s:%d",
               __func__, __LINE__);
    return -1;
}

static int differentiate_cp2112_devices (
    bf_pltfm_board_id_t bd_id,
    bf_pltfm_cp2112_device_ctx_t *dev_arr)
{
    if (g_max_cp2112_num == 1) {
        /* For those platforms which has only one cp2112,
         * of casue only one cp2112 on duty, so here let them
         * point to the same cp2112 space.
         * by tsihang, 2021-07-14. */
        cp2112_id_map[CP2112_ID_1] = 0;
        cp2112_id_map[CP2112_ID_2] = 0;
        return 0;
    }

    for (int i = 0; i < g_max_cp2112_num; i++) {
        // Send the read request
        uint8_t byte_buf[1];
        uint8_t usb_buf[64];
        usb_buf[0] = READ_REQUEST;
        usb_buf[1] = 0xe0;  // Reapeater PCA9548 address
        uint16_t temp = 1;
        if (is_big_endian_flag == false) {
            swap_endianness_16b (&temp);
        }
        memcpy (usb_buf + 2, &temp, sizeof (temp));
        enum libusb_error rc = usb_intr_transfer_out (
                                   dev_arr[i].usb_device_handle,
                                   &dev_arr[i].bus_good,
                                   usb_buf,
                                   sizeof (usb_buf),
                                   DEFAULT_TIMEOUT_MS,
                                   "test read");
        if (rc != LIBUSB_SUCCESS) {
            LOG_DEBUG ("Error: Test Read failed at %s:%d",
                       __func__, __LINE__);
            return -1;
        }

        // Wait for the response data
        int err = process_read_response (
                      &dev_arr[i], byte_buf, sizeof (byte_buf),
                      DEFAULT_TIMEOUT_MS);
        if (err != 0) {
            if (err != -1) {
                LOG_ERROR (
                    "Error: error while differentiating CP2112 devices %d at %s:%d",
                    i, __func__,
                    __LINE__);
                return -1;
            }
            // Indicates that this cp2112 is on the upper board
            cp2112_id_map[CP2112_ID_2] = i;
            fprintf(stdout, "Not Detected ... %d\n", cp2112_id_map[CP2112_ID_2]);
        } else {
            // Success indicates that this cp2112 is on the lower board
            cp2112_id_map[CP2112_ID_1] = i;
            fprintf(stdout, "Detected ... %d\n", cp2112_id_map[CP2112_ID_1]);
        }
    }

    /* We can select correct channel by a detecting method for CGT CM,
     * while we can not do that when ADV15xx installed.
     * So here we have to point it case by case. */
    if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_CPLD_CP2112) {
        cp2112_id_map[CP2112_ID_1] = 1;
        cp2112_id_map[CP2112_ID_2] = 0;
    }
    fprintf(stdout, "cp2112_id_map[CP2112_ID_1] ... %d\n", cp2112_id_map[CP2112_ID_1]);
    fprintf(stdout, "cp2112_id_map[CP2112_ID_2] ... %d\n", cp2112_id_map[CP2112_ID_2]);

    return 0;
}

bf_pltfm_cp2112_id_t cp2112_dev_id_get (
    bf_pltfm_cp2112_device_ctx_t *dev)
{
    if ((bf_pltfm_cp2112_device_ctx_t *)
        &cp2112_dev_arr
        [cp2112_id_map[CP2112_ID_1]] == dev) {
        return CP2112_ID_1;
    } else if ((bf_pltfm_cp2112_device_ctx_t *)
               &cp2112_dev_arr
               [cp2112_id_map[CP2112_ID_2]] == dev) {
        return CP2112_ID_2;
    } else {
        return CP2112_ID_INVALID;
    }
}

static bf_pltfm_status_t bf_pltfm_cp2112_open (
    bf_pltfm_board_id_t bd_id,
    bf_pltfm_cp2112_device_ctx_t *dev_arr,
    bool auto_detach,
    bool set_sm_bus_cfg)
{
    int rc, i;

    if (is_big_endian()) {
        is_big_endian_flag = true;
    } else {
        is_big_endian_flag = false;
    }

    if (platform_type_equal (X532P) ||
        (platform_type_equal (X564P) && (platform_subtype_equal (v1dot2) || platform_subtype_equal (v2dot0))) ||
        platform_type_equal (X308P) ||
        platform_type_equal (HC)) {
        g_max_cp2112_num = 2;
    } else if (platform_type_equal (X312P)) {
        g_max_cp2112_num = 1;
    }

    int max_dev_to_find = g_max_cp2112_num;

    for (i = 0; i < CP2112_ID_MAX; i++) {
        // Initialize the USB library
        rc = libusb_init (&dev_arr[i].usb_context);
        if (rc != 0) {
            LOG_ERROR (
                "Error in initializing the USB library with error code %s at %s:%d",
                libusb_strerror (rc),
                __func__,
                __LINE__);
            return -1;
        }
    }

    LOG_DEBUG ("USB Initialized at %s:%d", __func__,
               __LINE__);

    // Get the USB devices
    rc = usb_device_find (
             max_dev_to_find, dev_arr, CP2112_VENDOR_ID,
             CP2112_PRODUCT_ID);
    if (rc != 0) {
        LOG_ERROR ("Error: Device not found with error code %s at %s:%d",
                   libusb_strerror (rc),
                   __func__,
                   __LINE__);
        return -2;
    }

    LOG_DEBUG ("Total %d/%d CP2112 devices are found.\n\n",
               max_dev_to_find, g_max_cp2112_num);

    for (i = 0; i < g_max_cp2112_num; i++) {
        // Get the USB device handle
        rc = libusb_open (dev_arr[i].usb_device,
                          &dev_arr[i].usb_device_handle);
        if (rc != 0) {
            LOG_ERROR (
                "Error in obtaining a USB device handle with error code %s at "
                "%s:%d",
                libusb_strerror (rc),
                __func__,
                __LINE__);
            return -3;
        }
        LOG_DEBUG ("\n\n");
        LOG_DEBUG ("The USB dev[%d] is %p.", i,
                   (void *)dev_arr[i].usb_device);
        LOG_DEBUG ("The USB dev[%d] contex is %p.", i,
                   (void *)dev_arr[i].usb_context);
        LOG_DEBUG ("The USB dev[%d] handle is %p.", i,
                   (void *)dev_arr[i].usb_device_handle);

        int val = auto_detach;
        if (auto_detach) {
            rc = libusb_set_auto_detach_kernel_driver (
                     dev_arr[i].usb_device_handle,
                     val);
            if (rc != 0) {
                LOG_DEBUG (
                    "Error in setting the kernel driver detach option with error code "
                    "%s "
                    "at %s:%d",
                    libusb_strerror (rc),
                    __func__,
                    __LINE__);
                return -4;
            }
        }

        LOG_DEBUG ("Kernel driver auto detach set.");

        // Claim the interface
        rc = libusb_claim_interface (
                 dev_arr[i].usb_device_handle, 0);
        if (rc != 0) {
            LOG_ERROR ("Error in claiming the interface with error code %s",
                       libusb_strerror (rc));
            //return -5;
        }

        LOG_DEBUG ("Interface Claimed.");

        // Initialize the sm bus config
        if (set_sm_bus_cfg) {
            rc = sm_bus_config_init (
                     dev_arr[i].usb_device_handle);
            if (rc != 0) {
                LOG_ERROR ("Error in initializing the sm bus config at %s:%d",
                           __func__,
                           __LINE__);
                //return -6;
            }
        }
        LOG_DEBUG ("SMBus initialized.");

        // Flush any ongoing transfers and ignore any pending interrupts in packets
        cp2112_flush_transfers (&dev_arr[i]);

        LOG_DEBUG ("Ongoing transfers flushed.");

        // Cancel any outstanding transfers
        cp2112_cancel_transfers (&dev_arr[i]);

        LOG_DEBUG ("Outstanding transfers cancelled.");
    }

    if (max_dev_to_find == g_max_cp2112_num) {
        if (differentiate_cp2112_devices (bd_id,
                                          dev_arr) != 0) {
            LOG_ERROR ("Error: Failed to differentiate CP2112 devices at %s:%d",
                       __func__,
                       __LINE__);
            return -7;
        }
    }
    LOG_DEBUG (
        "Upper and Lower Devices Differentiated.");

    return BF_PLTFM_SUCCESS;
}

static bf_pltfm_status_t bf_pltfm_cp2112_close (
    bf_pltfm_cp2112_device_ctx_t *dev)
{
    LOG_DEBUG (
        "%s[%d], "
        "cp2112.close(dev %p : context %p : handle %p)"
        "\n",
        __FILE__, __LINE__,
        (void *)dev->usb_device, (void *)dev->usb_context,
        (void *)dev->usb_device_handle);

    fprintf (stdout,
             "%s[%d], "
             "cp2112.close(dev %p : context %p : handle %p)"
             "\n",
             __FILE__, __LINE__,
             (void *)dev->usb_device, (void *)dev->usb_context,
             (void *)dev->usb_device_handle);

    if (dev == NULL) {
        LOG_ERROR ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_INVALID_ARG;
    }

    if (dev->usb_device_handle == NULL) {
        LOG_ERROR ("USB device handle is NULL at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }
#if 0
    /* do not call libusb_release_interface(). It does not seem necessary despite
     *  what documentation says. It seems to send an invalid Reoprt to the device
     *  making the device unresponsive for significant time or undergo some
     *  kind of reset. The next set_configuration operation fails sometimes.
     */
    /* TODO: We might have to call libusb_release_interface for all the interfaces
       we have claimed so far */
    int rc = libusb_release_interface (
                 dev->usb_device_handle, 0);
    if (rc != 0) {
        LOG_ERROR ("Error %s: in releasing the usb interface at %s:%d",
                   __func__,
                   __LINE__);
        /* do not return a FAILURE; continue with the rest of the
         * cleanup nevertheless
         */
    }
    sleep (2);
#endif
    // close the device handle
    libusb_close (dev->usb_device_handle);
    dev->usb_device_handle = NULL;

    // Decrease the reference count of the device
    libusb_unref_device (dev->usb_device);
    dev->usb_device = NULL;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_cp2112_write (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *byte_buf,
    uint32_t byte_buf_size,
    int timeout_ms)
{
    if (dev == NULL) {
        LOG_ERROR ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_INVALID_ARG;
    }

    // Length of the buffer should not be greater than 61. Caller to ensure
    if (byte_buf_size > 61) {
        LOG_DEBUG (
            "Error in trying to write more than 61 bytes at a time at %s:%d",
            __func__,
            __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }

    if (byte_buf_size < 1) {
        LOG_DEBUG ("0 length writes not supported at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }

    // Take the mutex
    bf_pltfm_cp2112_id_t cp2112_dev_id;
    cp2112_dev_id = cp2112_dev_id_get (dev);
    bf_sys_mutex_lock (&cp2112_mutex[cp2112_dev_id]);

    // Make sure that the bus is in a goos state
    ensure_good_state (dev);

    // Send the write request
    uint8_t buf[64];
    buf[0] = WRITE;
    buf[1] = addr;
    buf[2] = byte_buf_size;
    memcpy (buf + 3, byte_buf, byte_buf_size);

    // Wait for the write to complete
    struct timespec time_now;
    clock_gettime (CLOCK_MONOTONIC, &time_now);
    struct timespec time_end;
    time_end.tv_sec = time_now.tv_sec +
                      (timeout_ms / 1000);
    time_end.tv_nsec = time_now.tv_nsec + ((
            timeout_ms % 1000) * 1000000);
    if (time_end.tv_nsec >= 1000000000) {
        time_end.tv_nsec = time_end.tv_nsec - 1000000000;
        time_end.tv_sec++;
    }
    int rc;
    rc = usb_intr_transfer_out (
             dev->usb_device_handle,
             &dev->bus_good,
             buf,
             sizeof (buf),
             timeout_ms,
             "write request");
    if (rc != LIBUSB_SUCCESS) {
        LOG_DEBUG (
            "Eror code : %s at %s:%d\n", libusb_strerror (rc),
            __func__, __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }
    struct timespec time_left;
    rc = wait_for_transfer (dev, &time_end,
                            &time_left, "write");
    if (rc < 0) {
        LOG_DEBUG ("Error at %s:%d\n", __func__,
                   __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }
    bf_sys_mutex_unlock (
        &cp2112_mutex[cp2112_dev_id]);
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_cp2112_write_byte (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t val,
    int timeout_ms)
{
    if (dev == NULL) {
        LOG_ERROR ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_INVALID_ARG;
    }
    return bf_pltfm_cp2112_write (dev, addr, &val,
                                  sizeof (val), timeout_ms);
}

bf_pltfm_status_t
bf_pltfm_cp2112_write_read_unsafe (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *write_buf,
    uint8_t *read_buf,
    uint32_t write_buf_size,
    uint32_t read_buf_size,
    int timeout_ms)
{
    if (dev == NULL) {
        LOG_ERROR ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_INVALID_ARG;
    }

    if (write_buf_size > 16) {
        LOG_DEBUG (
            "Error: Trying to write more than 16 bytes at one for read after write "
            "at %s:%d",
            __func__,
            __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }
    if (write_buf_size < 1) {
        LOG_DEBUG ("Error: Trying to write 0 bytes for read after write at %s:%d",
                   __func__,
                   __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }
    if (read_buf_size > 512) {
        LOG_DEBUG ("Error: Trying to read more than 512 bytes at once at %s:%d",
                   __func__,
                   __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }
    if (read_buf_size < 1) {
        LOG_DEBUG ("Error: Trying to read 0 bytes at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }

    // Take the mutex
    bf_pltfm_cp2112_id_t cp2112_dev_id;
    cp2112_dev_id = cp2112_dev_id_get (dev);
    bf_sys_mutex_lock (&cp2112_mutex[cp2112_dev_id]);

    ensure_good_state (dev);

    // Send the write-read request
    uint8_t buf[64];
    buf[0] = WRITE_READ_REQUEST;
    buf[1] = addr;
    uint16_t temp = read_buf_size;
    if (is_big_endian_flag == false) {
        swap_endianness_16b (&temp);
    }
    memcpy (buf + 2, &temp, sizeof (temp));
    buf[4] = write_buf_size;
    memcpy (buf + 5, write_buf, write_buf_size);
    enum libusb_error err = usb_intr_transfer_out (
                                dev->usb_device_handle,
                                &dev->bus_good,
                                buf,
                                sizeof (buf),
                                timeout_ms,
                                "addressed read");
    if (err != LIBUSB_SUCCESS) {
        LOG_DEBUG ("Error in write read unsafe at %s:%d",
                   __func__, __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }

    // Wait for the response data
    int rc = process_read_response (dev, read_buf,
                                    read_buf_size, timeout_ms);
    if (rc != 0) {
        LOG_DEBUG ("Error: error while waiting for read response at %s:%d",
                   __func__,
                   __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }
    bf_sys_mutex_unlock (
        &cp2112_mutex[cp2112_dev_id]);
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_cp2112_read (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *byte_buf,
    uint32_t byte_buf_size,
    int timeout_ms)
{
    if (dev == NULL) {
        LOG_ERROR ("Error: Invalid argument at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_INVALID_ARG;
    }

    if (byte_buf_size > 512) {
        LOG_DEBUG ("Error: trying to read more than 512 bytes at once at %s:%d",
                   __func__,
                   __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }
    if (byte_buf_size < 1) {
        LOG_DEBUG ("Error: Trying to read 0 bytes at %s:%d",
                   __func__, __LINE__);
        return BF_PLTFM_COMM_FAILED;
    }

    // Take the mutex
    bf_pltfm_cp2112_id_t cp2112_dev_id;
    cp2112_dev_id = cp2112_dev_id_get (dev);
    bf_sys_mutex_lock (&cp2112_mutex[cp2112_dev_id]);

    ensure_good_state (dev);

    // Send the read request
    uint8_t usb_buf[64];
    usb_buf[0] = READ_REQUEST;
    usb_buf[1] = addr;
    uint16_t temp;
    memcpy (&temp, &byte_buf_size, sizeof (temp));
    if (is_big_endian_flag == false) {
        swap_endianness_16b (&temp);
    }
    memcpy (usb_buf + 2, &temp, sizeof (temp));
    enum libusb_error rc = usb_intr_transfer_out (
                               dev->usb_device_handle,
                               &dev->bus_good,
                               usb_buf,
                               sizeof (usb_buf),
                               timeout_ms,
                               "read");
    if (rc != LIBUSB_SUCCESS) {
        LOG_DEBUG ("Error: Read failed at %s:%d",
                   __func__, __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }

    // Wait for the response data
    int err = process_read_response (dev, byte_buf,
                                     byte_buf_size, timeout_ms);
    if (err != 0) {
        LOG_DEBUG ("Error: error while waiting read response at %s:%d",
                   __func__,
                   __LINE__);
        bf_sys_mutex_unlock (
            &cp2112_mutex[cp2112_dev_id]);
        return BF_PLTFM_COMM_FAILED;
    }
    bf_sys_mutex_unlock (
        &cp2112_mutex[cp2112_dev_id]);
    return BF_PLTFM_SUCCESS;
}

bf_pltfm_cp2112_device_ctx_t
*bf_pltfm_cp2112_get_handle (
    bf_pltfm_cp2112_id_t cp2112_id)
{
#if !defined(HAVE_ONE_CP2112_ON_DUTY)
    if ((bd_id == BF_PLTFM_BD_ID_X312PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V3DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V4DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X312PT_V5DOT0)) {
        if (cp2112_id != CP2112_ID_1) {
            return NULL;
        }
        return &cp2112_dev_arr[cp2112_id_map[cp2112_id]];
    } else if ((board_id == BF_PLTFM_BD_ID_X532PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X532PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X532PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V1DOT2) ||
        (bd_id == BF_PLTFM_BD_ID_X564PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V1DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V1DOT1) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V2DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_X308PT_V3DOT0) ||
        (bd_id == BF_PLTFM_BD_ID_HC36Y24C_V1DOT0)) {
        if ((cp2112_id != CP2112_ID_2) &&
            (cp2112_id != CP2112_ID_1)) {
            return NULL;
        }
        return &cp2112_dev_arr[cp2112_id_map[cp2112_id]];
    } else {
        return NULL;
    }
#else
    /* Make sure that the cp2112 hndl in CP2112_ID_1
     * for those platforms which has two CP2112 but only one on duty.
     * So here we only return CP2112_ID_1 for module IO.
     * See differentiate_cp2112_devices for reference.
     * by tsihang, 2021-07-14. */
    cp2112_id = cp2112_id;
    return &cp2112_dev_arr[cp2112_id_map[cp2112_id]];
#endif
}

/* Toggle the reset pin of CP2112 thru the COMe->BMC->CPLD path.
 * by tsihang, 2021-07-21. */
bf_pltfm_status_t bf_pltfm_bmc_cp2112_reset (
    bool primary)
{
    /* Make it very clear for those platforms which could offer cp2112 reset oper by doing below operations.
     * Because we cann't and shouldn't assume platforms which are NOT X312P-T could reset cp2112 like this.
     * by tsihang, 2022-04-27. */
    if (platform_type_equal (X564P) ||
        platform_type_equal (X532P) ||
        platform_type_equal (X308P)) {
        if (bf_pltfm_mgr_ctx()->flags & AF_PLAT_CTRL_BMC_UART) {
            uint8_t rd_buf[128];
            uint8_t cmd = 0x10;
            uint8_t wr_buf[2] = {0x01, 0xAA};
            bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                                          2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US);
            /* Giving a little time to wait the reset routine is completely done. */
            sleep (5);
            wr_buf[0] = 0x02;
            bf_pltfm_bmc_uart_write_read (cmd, wr_buf,
                                          2, rd_buf, 128 - 1, BMC_COMM_INTERVAL_US);
        } else {
            /* Early Hardware with I2C interface. */
        }
    } else if (platform_type_equal(X312P)) {
        /* TBD */
    } else if (platform_type_equal(HC)) {
        /* TBD */
    }

    (void)primary;
    return BF_PLTFM_SUCCESS;
}


/* This API must be called with care. Ensure that other subsystem
 *  is not currently using cp2112
 */
bf_pltfm_status_t bf_pltfm_cp2112_soft_reset()
{
    bf_pltfm_status_t sts;

    sts = bf_pltfm_cp2112_de_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: Unable to de init CP2112 devices at %s:%d",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
        return sts;
    }

    usleep (500000);

    sts = bf_pltfm_cp2112_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: Unable to init CP2112 devices at %s:%d",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
        return sts;
    }

    return BF_PLTFM_SUCCESS;
}

/* This API must be called with care. Ensure that other subsystem
 *  is not currently using cp2112
 */
bf_pltfm_status_t bf_pltfm_cp2112_hard_reset()
{
    bf_pltfm_status_t sts;

    sts = bf_pltfm_cp2112_de_init();
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Error %s: Unable to de init CP2112 devices at %s:%d",
                   bf_pltfm_err_str (sts),
                   __func__,
                   __LINE__);
        return sts;
    }

    usleep (5000);

    /* Use BMC interface to reset cp2112. */
    sts = bf_pltfm_bmc_cp2112_reset (true);
    if (sts) {
        LOG_ERROR ("Error resetting primary cp2112\n");
        return BF_PLTFM_COMM_FAILED;
    }

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_cp2112_init()
{
    int i;
    int tried_reset_on_error = 0;
    bf_pltfm_status_t sts;

    fprintf (stdout,
             "\n\n================== CP2112s INIT ==================\n");
    bf_pltfm_cp2112_initialized = 0;
    bf_pltfm_chss_mgmt_bd_type_get (&bd_id);

#if 0
    /* At the beginning, Use BMC interface to reset cp2112 first.
     * There could be a chance that the cp2112 device could be lost
     * after reseting in container environment.
     * So please reset cp2112 beyond syncd reloading and use a script instead.
     * by tsihang, 21 July, 2023. */
    sts = bf_pltfm_bmc_cp2112_reset (true);
    if (sts) {
        LOG_ERROR ("Error resetting primary cp2112\n");
        return BF_PLTFM_COMM_FAILED;
    }
    bf_sys_sleep (5);
#endif
    while (1) {
        // Initialize the cp2112 mutex
        for (i = 0; i < CP2112_ID_MAX; i++) {
            bf_sys_mutex_init (&cp2112_mutex[i]);
        }

        sts = bf_pltfm_cp2112_open (bd_id, cp2112_dev_arr,
                                    true, true);
        if (sts != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error: Unable to open CP2112 devices iteration %d <rc=%d>",
                       tried_reset_on_error, sts);
            if (tried_reset_on_error == 3) {
                return sts;
            }
            bf_pltfm_cp2112_hard_reset ();
            tried_reset_on_error ++;
            bf_sys_sleep (2);
        } else {
            LOG_DEBUG ("CP2112 summary: ");
            fprintf (stdout, "CP2112 summary: \n");
            for (i = 0; i < g_max_cp2112_num; i++) {
                LOG_DEBUG ("The USB dev[%d] dev %p.", i,
                           (void *)cp2112_dev_arr[i].usb_device);
                fprintf (stdout, "The USB dev[%d] dev %p.\n", i,
                         (void *)cp2112_dev_arr[i].usb_device);
                LOG_DEBUG ("The USB dev[%d] contex %p.", i,
                           (void *)cp2112_dev_arr[i].usb_context);
                fprintf (stdout, "The USB dev[%d] contex %p.\n",
                         i,
                         (void *)cp2112_dev_arr[i].usb_context);
                LOG_DEBUG ("The USB dev[%d] handle %p.", i,
                           (void *)cp2112_dev_arr[i].usb_device_handle);
                fprintf (stdout, "The USB dev[%d] handle %p.\n",
                         i,
                         (void *)cp2112_dev_arr[i].usb_device_handle);
            }
            bf_pltfm_cp2112_device_ctx_t *dev =
                &cp2112_dev_arr[cp2112_id_map[CP2112_ID_1]];
            LOG_DEBUG ("The USB dev[%d] dev %p : context %p : handle %p on duty.",
                       cp2112_id_map[CP2112_ID_1],
                       (void *)dev->usb_device,
                       (void *)dev->usb_context,
                       (void *)dev->usb_device_handle);
            fprintf (stdout,
                     "The USB dev[%d] dev %p : context %p : handle %p on duty.\n",
                     cp2112_id_map[CP2112_ID_1],
                     (void *)dev->usb_device,
                     (void *)dev->usb_context,
                     (void *)dev->usb_device_handle);
            bf_pltfm_cp2112_initialized = 1;
            return BF_PLTFM_SUCCESS;
        }
    }
}

extern void bf_pltfm_load_conf ();
bf_pltfm_status_t bf_pltfm_cp2112_util_init (
    bf_pltfm_board_id_t board_id)
{
    bd_id = board_id;

    //bf_pltfm_chss_mgmt_init();
    bf_pltfm_load_conf ();
    // Initialize the cp2112 mutex
    int i;
    for (i = 0; i < CP2112_ID_MAX; i++) {
        bf_sys_mutex_init (&cp2112_mutex[i]);
    }

    bf_pltfm_status_t sts;
    sts = bf_pltfm_cp2112_open (board_id,
                                cp2112_dev_arr,
                                true, true);
    if (sts != BF_PLTFM_SUCCESS) {
        LOG_ERROR (
            "Error: Unable to open CP2112 devices at %s:%d",
            __func__, __LINE__);
        return sts;
    } else {
        LOG_DEBUG ("CP2112 summary: ");
        for (i = 0; i < g_max_cp2112_num; i++) {
            LOG_DEBUG ("The USB dev[%d] dev %p.", i,
                       (void *)cp2112_dev_arr[i].usb_device);
            LOG_DEBUG ("The USB dev[%d] contex %p.", i,
                       (void *)cp2112_dev_arr[i].usb_context);
            LOG_DEBUG ("The USB dev[%d] handle %p.", i,
                       (void *)cp2112_dev_arr[i].usb_device_handle);
        }
        bf_pltfm_cp2112_device_ctx_t *dev =
            &cp2112_dev_arr[cp2112_id_map[CP2112_ID_1]];
        LOG_DEBUG ("The USB dev[%d] dev %p : context %p : handle %p on duty.",
                   cp2112_id_map[CP2112_ID_1],
                   (void *)dev->usb_device,
                   (void *)dev->usb_context,
                   (void *)dev->usb_device_handle);

    }

    bf_pltfm_cp2112_initialized = 1;

    return BF_PLTFM_SUCCESS;
}

bf_pltfm_status_t bf_pltfm_cp2112_de_init()
{
    int i;

    fprintf(stdout, "================== Deinit .... %48s ================== \n",
        __func__);

    LOG_DEBUG (
        "%s[%d], "
        "cp2112.deinit(%s)"
        "\n",
        __FILE__, __LINE__,
        "starting");

    fprintf (stdout,
             "%s[%d], "
             "cp2112.deinit(%s)"
             "\n",
             __FILE__, __LINE__,
             "starting");

    /* Not initialized before */
    if (!bf_pltfm_cp2112_initialized) {
        return BF_PLTFM_SUCCESS;
    }

    /* Deley to make sure cp2112 released. */
    bf_sys_sleep (10);

    // Destroy the initialized mutexes
    for (i = 0; i < CP2112_ID_MAX; i++) {
        bf_sys_mutex_del (&cp2112_mutex[i]);
    }

    bf_pltfm_status_t err;
    for (i = 0; i < g_max_cp2112_num; i++) {
        err = bf_pltfm_cp2112_close (&cp2112_dev_arr[i]);
        if (err != BF_PLTFM_SUCCESS) {
            LOG_ERROR ("Error: Unable to close CP2112 devices at %s:%d : <rc=%d>, %d",
                       __func__,
                       __LINE__,
                       err,
                       i);
            /* do not return a FAILURE; continue with the rest of the
             * cleanup nevertheless
             */
        }
    }
    // Deinitialize the USB library
    for (i = 0; i < g_max_cp2112_num; i++) {
        libusb_exit (cp2112_dev_arr[i].usb_context);
        cp2112_dev_arr[i].usb_context = NULL;
    }

    bf_pltfm_cp2112_initialized = 0;

    fprintf(stdout, "================== Deinit done %48s ================== \n",
        __func__);

    return BF_PLTFM_SUCCESS;
}

#if 0  // Do not use curl interface to reset cp2112
static int write_data_cb (void *ptr)
{
    if (ptr == NULL) {
        LOG_ERROR ("CP2112:: Unable to reset the cp2112 devices from BMC\n");
    }
    if (strcmp ("Reset Done", (char *)ptr) == 0) {
        printf ("CP2112:: Devices reset\n");
    } else {
        printf ("CP2112:: Devices could not be reset\n");
    }
    return strlen ((char *)ptr);
}
#endif

bool bf_pltfm_cp2112_addr_scan (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr)
{
    if (addr == 0 || addr == 2) {
        // Don't try scanning address 0 (general call address, which addresses all
        // devices), or address 2 (which is the slave address of the CP2112
        // itself).
        return false;
    }

    uint8_t buf[1];
    if (bf_pltfm_cp2112_read (dev, addr, buf, 1,
                              10) != BF_PLTFM_SUCCESS) {
        return false;
    }
    return true;
}
