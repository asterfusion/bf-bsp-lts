/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#ifndef _BF_PLTFM_SFP_H
#define _BF_PLTFM_SFP_H

/* Allow the use in C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

/* SFP platforms specific intialization */

/* initialize the entire qsfp subsystem
 * it is presently invoked with arg = NULL
 * @return
 *   0 on success and -1 on error
 */
int bf_pltfm_sfp_init (void *arg);

/** detect a sfp module by attempting to read its memory offste zero
 *
 *  @param module
 *   module (1 based)
 *  @return
 *   true if present and false if not present
 */
bool bf_pltfm_detect_sfp (    uint32_t module);

/** read a sfp module memory
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
int bf_pltfm_sfp_read_module (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf);

/** write a sfp module memory
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
int bf_pltfm_sfp_write_module (
    uint32_t module,
    uint8_t offset,
    uint8_t len,
    uint8_t *buf);

/* read a sfp  module register
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
int bf_pltfm_sfp_read_reg (
    uint32_t module,
    uint8_t page,
    int offset,
    uint8_t *val);

/* write a sfp  module register
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
int bf_pltfm_sfp_write_reg (
    uint32_t module,
    uint8_t page,
    int offset,
    uint8_t val);

/** get sfp presence mask status
*
*  @param port_1_32_pres
*   mask for lower 32 ports (1-32) 0: present, 1:not-present
*  @param port_32_64_pres
*   mask for upper 32 ports (33-64) 0: present, 1:not-present
*  @return
*   0 on success and -1 in error
*/
int bf_pltfm_sfp_get_presence_mask (
    uint32_t *port_1_32_pres,
    uint32_t *port_32_64_pres);


/** get sfp interrupt status
*
*  @param port_1_32_ints
*   interrupt from lower 32 ports (1-32) 0: int-present, 1:not-present
*  @param port_32_64_ints
*   interrupt from upper 32 ports (33-64) 0: int-present, 1:not-present
*/
void bf_pltfm_sfp_get_int_mask (uint32_t
                                *port_1_32_ints,
                                uint32_t *port_32_64_ints);

/** get sfp lpmode status
*
*  @param port_1_32_lpmode_
*   lpmode of lower 32 ports (1-32) 0: no-lpmod 1: lpmode
*  @param port_32_64_ints
*   lpmode of upper 32 ports (33-64) 0: no-lpmod 1: lpmode
*  @return
*   0 on success and -1 in error
*/
int bf_pltfm_sfp_get_lpmode_mask (
    uint32_t *port_1_32_ints,
    uint32_t *port_32_64_ints);

/** set sfp lpmode (hardware pins)
*
*  @param port
*   port
*  @param lpmode
*   true : set lpmode, false : set no-lpmode
*  @return
*   0: success, -1: failure
*/
int bf_pltfm_sfp_set_lpmode (uint32_t module,
                             bool lpmode);

/** reset sfp module (hardware pins)
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
int bf_pltfm_sfp_module_reset (uint32_t module,
                               bool reset);

int bf_pltfm_sfp_module_tx_disable (
    uint32_t module,
    bool disable);

int bf_pltfm_sfp_los_read (uint32_t module,
                           bool *los);

int bf_pltfm_sfp_lookup_by_module (
    uint32_t module,
    uint32_t *conn, uint32_t *chnl);
int bf_pltfm_sfp_lookup_by_conn (uint32_t conn,
                                 uint32_t chnl, uint32_t *module);
bool is_panel_sfp (uint32_t conn, uint32_t chnl);

/* common part for a give sfp. */
struct sfp_ctx_info_t {
    const char *desc;
    uint32_t conn;
    uint32_t chnl;
    uint8_t i2c_chnl_addr;
    uint32_t rev; /* moudle -> rev, by tsihang, 2021-07-13.
                * for HC, use rev to store address of PCA9548
                */
};

/* User defined sfp ctx. */
struct sfp_ctx_st_t {
    struct st_ctx_t tx_dis;
    struct st_ctx_t pres;
    struct st_ctx_t rx_los;
    //struct st_ctx_t tx_fault;
};

typedef struct sfp_ctx_st_t sfp_ctx_st_x5_t;
typedef struct sfp_ctx_st_t sfp_ctx_st_x3_t;
typedef struct sfp_ctx_st_t sfp_ctx_st_hc_t;

/* access is 0 based index. */
struct sfp_ctx_t {
    struct sfp_ctx_info_t info;
    void *st;
};

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* _BF_PLTFM_QSFP_H */

