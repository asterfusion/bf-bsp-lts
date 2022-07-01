/*******************************************************************************
BSD License

For fboss software

Copyright (c) 2013-2015, Facebook, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name Facebook nor the names of its contributors may be used to
   endorse or promote products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef _CP2112_I2C_BUS_INCLUDED
#define _CP2112_I2C_BUS_INCLUDED

#include <stdint.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <libusb-1.0/libusb.h> /* cor cp2112 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The standard vendor and product IDs reported by cp2112 devices
 * (unless overridden in the customizable PROM).
 *
 * Using command 'lsusb' to find out how many CP2112(s)
 * our platform support.
 * Edit by tsihang, 2019.7.20
 */
#define CP2112_VENDOR_ID    0x10c4
#define CP2112_PRODUCT_ID   0xea90

/*
 * The part number reported by the get version call
 */
#define CP2112_PART_NUMBER 0x0c

/*
 * Identifies the CP2112 chip on the board
 */
typedef enum bf_pltfm_cp2112_id_e {
    /* Make sure that the cp2112 hndl in CP2112_ID_1 for those platforms
     * which has two CP2112 but only one on duty to access QSFP.
     * See differentiate_cp2112_devices for reference.
     * by tsihang, 2021-07-14. */
    CP2112_ID_1 = 0,
    /* It could used to access CPLD/SFP for a part of CME.
     * by tsihang, 2022-06-23. */
    CP2112_ID_2 = 1,
    CP2112_ID_MAX = 2,
    CP2112_ID_INVALID = -1
} bf_pltfm_cp2112_id_t;


/*
 * Holds the entire context of a CP2112 device
 */
typedef struct bf_pltfm_cp2112_device_ctx_t {
    libusb_device *usb_device;
    libusb_device_handle *usb_device_handle;
    libusb_context *usb_context;
    int selected_port;
    int selected_repeater;
    bool bus_good;
    uint16_t usb_bus_num;
} bf_pltfm_cp2112_device_ctx_t;

/*
 * Read from the SMBus.
 *
 * The least significant bit of the address must be 0, since SMBus addresses
 * are only 7 bits.
 *
 * The length must be between 1 and 512 bytes.
 */
bf_pltfm_status_t bf_pltfm_cp2112_read (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *byte_buf,
    uint32_t byte_buf_size,
    int timeout_ms);

/*
 * Write to the SMBus.
 *
 * The length must be between 1 and 61 bytes.
 */
bf_pltfm_status_t bf_pltfm_cp2112_write (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *byte_buf,
    uint32_t byte_buf_size,
    int timeout_ms);

bf_pltfm_status_t bf_pltfm_cp2112_write_byte (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t val,
    int timeout_ms);

/*
   * An atomic write followed by read, without releasing the I2C bus.
   *
   * This uses a repeated start, without a stop condition between the
   * write and read operations.
   *
   * The writeLength must be between 1 and 16 bytes.
   * The readLength must be between 1 and 512 bytes.
   *
   * WARNING: This command appears to cause the CP2112 chip to lock
   * up if it times out.  The only way to recover is to reset the chip using
   * the reset pin.  (A USB reset command is not sufficient.)
   */
bf_pltfm_status_t
bf_pltfm_cp2112_write_read_unsafe (
    bf_pltfm_cp2112_device_ctx_t *dev,
    uint8_t addr,
    uint8_t *write_buf,
    uint8_t *read_buf,
    uint32_t write_buf_size,
    uint32_t read_buf_size,
    int timeout_ms);

bf_pltfm_cp2112_device_ctx_t
*bf_pltfm_cp2112_get_handle (
    bf_pltfm_cp2112_id_t cp2112_id);

bf_pltfm_cp2112_id_t cp2112_dev_id_get (
    bf_pltfm_cp2112_device_ctx_t *dev);

bf_pltfm_status_t bf_pltfm_cp2112_init();

bf_pltfm_status_t bf_pltfm_cp2112_de_init();

bool bf_pltfm_cp2112_addr_scan (
    bf_pltfm_cp2112_device_ctx_t *dev, uint8_t addr);

#ifdef INC_PLTFM_UCLI
ucli_node_t *bf_pltfm_cp2112_ucli_node_create (
    ucli_node_t *m);
#endif

bf_pltfm_status_t bf_pltfm_cp2112_util_init (
    bf_pltfm_board_id_t board_id);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
