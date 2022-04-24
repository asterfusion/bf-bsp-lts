/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PLTFM_QSFP_H
#define _BF_PLTFM_QSFP_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* QSFP platforms specific intialization */

/* initialize the entire qsfp subsystem
 * it is presently invoked with arg = NULL
 * @return
 *   0 on success and -1 on error
 */
int bf_pltfm_qsfp_init (void *arg);

/** detect a qsfp module by attempting to read its memory offste zero
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if present and false if not present
 */
bool bf_pltfm_detect_qsfp (unsigned int module);

/** read a qsfp module memory
 *
 *  @param module
 *   module (1 based)
 *  @param offset
 *   offset into qsfp memory space
 *  @param len
 *   len num of bytes to read
 *  @param buf
 *   buf to read into
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_read_module (unsigned int
                               module,
                               int offset,
                               int len,
                               uint8_t *buf);

/** write a qsfp module memory
 *
 *  @param module
 *   module (1 based)
 *  @param offset
 *   offset into qsfp memory space
 *  @param len
 *   len num of bytes to write
 *  @param buf
 *   buf to read from
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_write_module (unsigned int
                                module,
                                int offset,
                                int len,
                                uint8_t *buf);

/* read a qsfp  module register
 *  @param module
 *   module (1 based)
 *  @param page
 *   page that the register belongs to
 *  @param offset
 *   offset into qsfp memory space
 *  @param val
 *   val to read into
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_read_reg (unsigned int module,
                            uint8_t page,
                            int offset,
                            uint8_t *val);

/* write a qsfp  module register
 *  @param module
 *   module (1 based)
 *  @param page
 *   page that the register belongs to
 *  @param offset
 *   offset into qsfp memory space
 *  @param val
 *   val to write
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_write_reg (unsigned int module,
                             uint8_t page,
                             int offset,
                             uint8_t val);

/** get qsfp presence mask status
 *
 *  @param port_1_32_pres
 *   mask for lower 32 ports (1-32) 0: present, 1:not-present
 *  @param port_32_64_pres
 *   mask for upper 32 ports (33-64) 0: present, 1:not-present
 *  @param port_cpu_pres
 *   mask for cu port presence
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_get_presence_mask (
    uint32_t *port_1_32_pres,
    uint32_t *port_32_64_pres,
    uint32_t *port_cpu_pres);


/** get qsfp interrupt status
 *
 *  @param port_1_32_ints
 *   interrupt from lower 32 ports (1-32) 0: int-present, 1:not-present
 *  @param port_32_64_ints
 *   interrupt from upper 32 ports (33-64) 0: int-present, 1:not-present
 *  @param port_cpu_ints
 *   mask for cpu port interrupt
 */
void bf_pltfm_qsfp_get_int_mask (uint32_t
                                 *port_1_32_ints,
                                 uint32_t *port_32_64_ints,
                                 uint32_t *port_cpu_ints);

/** get qsfp lpmode status
 *
 *  @param port_1_32_lpmode_
 *   lpmode of lower 32 ports (1-32) 0: no-lpmod 1: lpmode
 *  @param port_32_64_ints
 *   lpmode of upper 32 ports (33-64) 0: no-lpmod 1: lpmode
 *  @param port_cpu_ints
 *   lpmode of cpu port
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_get_lpmode_mask (
    uint32_t *port_1_32_ints,
    uint32_t *port_32_64_ints,
    uint32_t *port_cpu_ints);

/** set qsfp lpmode (hardware pins)
 *
 *  @param port
 *   port
 *  @param lpmode
 *   true : set lpmode, false : set no-lpmode
 *  @return
 *   0: success, -1: failure
 */
int bf_pltfm_qsfp_set_lpmode (unsigned int module,
                              bool lpmode);

/** reset qsfp module (hardware pins)
 *
 *  @param module
 *   module (1 based)
 *  @param reg
 *   syscpld register to write
 *  @param val
 *   value to write
 *  @return
 *   0 on success and -1 in error
 */
int bf_pltfm_qsfp_module_reset (unsigned int
                                module,
                                bool reset);

struct qsfp_ctx_t   {
    /* common part for a give qsfp. */
    const char *desc;
    uint32_t conn_id;   /* Added conn_id for qsfp map dump,
                         * Please help fixing X312P/HC.
                         * by tsihang 2021-08-02. */
    uint8_t pca9548;
    uint8_t channel;
    uint32_t rev;   /* eth_id -> rev for 2nd pca9548 map,
                     * unused when it set as INVALID.
                     * rev = pca9548 << 8 | channel.
                     * eg, pca9548 addr = 0x70, and take channel 1 as an example,
                     * so, rev = 0x70 << 8 | 0x01 = 0x00007001.
                     * by tsihang, 2021-07-13. */

    /* User defined sfp ctx. */
    struct st_ctx_t pres;
    struct st_ctx_t rst;

    /* struct st_ctx_t tx_dis; */
    /* struct st_ctx_t rx_los; */
    /* struct st_ctx_t tx_fault; */
    /* struct st_ctx_t int; */
    /* struct st_ctx_t lpmode; */
};

bool is_panel_qsfp_module (unsigned int  module);
bool is_panel_sfp_module (unsigned int  module);
bool is_panel_cpu_module (unsigned int  module);

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_QSFP_H */
