/*******************************************************************************
 * Copyright (c) 2015-2020 Barefoot Networks, Inc.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * $Id: $
 *
 ******************************************************************************/

#include <string.h>
#include <bf_types/bf_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_mav_qsfp_i2c_lock.h>
#include <bf_pltfm_slave_i2c.h>
#include <bf_pltfm_spi.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>

static bf_pltfm_cp2112_device_ctx_t
*tfn_slv_i2c_hdl;

static int raw_read (bf_pltfm_cp2112_device_ctx_t
                     *hndl,
                     uint8_t address,
                     uint8_t *offset,
                     int size_offset,
                     uint8_t *buf,
                     int len)
{
    /* Note that we don't use the writeRead() command, since this
     * locks up the CP2112 chip if it times out.  We perform a separate write,
     * followed by a read.  This releases the I2C bus between operations, but
     * that's okay since there aren't any other master devices on the bus.
     *
     * Also note that we can't read more than 128 bytes at a time.
     */
    int ret;

    /* set the necessary i2c mux after locking the mux access */
    ret = bf_pltfm_i2c_lock_select_misc_chn();
    if (ret) {
        goto raw_read_error;
    }
    ret = bf_pltfm_cp2112_write (hndl, address,
                                 offset, size_offset, 100);
    if (ret) {
        goto raw_read_error;
    }
    ret = bf_pltfm_cp2112_read (hndl, address, buf,
                                len, 100);

    /* unset i2c mux setting and unlock it */
raw_read_error:
    bf_pltfm_unselect_i2c_unlock_misc_chn();
    return ret;
}

static int raw_write (bf_pltfm_cp2112_device_ctx_t
                      *hndl,
                      uint8_t address,
                      uint8_t *buf,
                      int len)
{
    int ret;

    /* set the necessary i2c mux after locking the mux access */
    ret = bf_pltfm_i2c_lock_select_misc_chn();
    if (ret) {
        goto raw_write_error;
    }
    ret = bf_pltfm_cp2112_write (hndl, address, buf,
                                 len, 100);

    /* unset i2c mux setting and unlock it */
raw_write_error:
    bf_pltfm_unselect_i2c_unlock_misc_chn();
    return ret;
}

/* returns the i2c-addr of the ASIC */
static uint8_t get_i2c_addr (int chip_id)
{
    /* for now, we have only single chip platforms */
    return I2C_ADDR_TOFINO;
}

static int bf_pltfm_i2c_init (int chip_id)
{
    tfn_slv_i2c_hdl = bf_pltfm_cp2112_get_handle (
                          CP2112_ID_1);
    if (!tfn_slv_i2c_hdl) {
        LOG_ERROR ("error in bf_pltfm_slave_i2c_init\n");
        return -1;
    }
    return 0;
}

/** Tofino slave mode i2c read with "local read" command
 *
 *  @param chip_id
 *    chip_id
 *  @param local_reg
 *    one of the 32 local (8 bit) debug registers to access
 *  @param data
 *    data to read from local_reg
 *  @return
 *    0 on success and  any other value on error
 */
int bf_pltfm_loc_i2c_rd (int chip_id,
                         uint8_t local_reg, uint8_t *data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;

    local_reg &= ~TOFINO_I2C_CMD_OPCODE_MASK;
    local_reg |= TOFINO_I2C_OPCODE_LOCAL_RD;
    return (raw_read (upper, get_i2c_addr (chip_id),
                      &local_reg, 1, data, 1));
}

/** Tofino slave mode i2c write with "local write" command
 *
 *  @param chip_id
 *    chip_id
 *  @param local_reg
 *    one of the 32 local (8 bit) debug registers to access
 *  @param data
 *    data to write to local_reg
 *  @return
 *    0 on success and any other value on error
 */
int bf_pltfm_loc_i2c_wr (int chip_id,
                         uint8_t local_reg, uint8_t data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;
    uint8_t out[7];

    local_reg &= ~TOFINO_I2C_CMD_OPCODE_MASK;
    local_reg |= TOFINO_I2C_OPCODE_LOCAL_WR;
    out[0] = local_reg;
    out[1] = data;
    return (raw_write (upper, get_i2c_addr (chip_id),
                       &out[0], 2));
}

/** Tofino slave mode i2c read with "direct read" command
 *
 *  @param chip_id
 *    chip_id
 *  @param seg
 *    BAR segment (0: regular PCIe space, 1: MSIX cap space,
 *                 2: 4K pcie cap/cfg space)
 *  @param dir_reg
 *     direct address space register to access
 *  @param data
 *    data to read from dir_addr
 *  @return
 *    0 on success and any other value on error
 */
int bf_pltfm_dir_i2c_rd (int chip_id,
                         Tofino_I2C_BAR_SEG seg,
                         uint32_t dir_addr,
                         uint32_t *data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;
    uint8_t out[7];

    out[0] = TOFINO_I2C_OPCODE_DIR_RD;
    dir_addr |= (seg << TOFINO_I2C_BAR_SEG_SHFT);
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &dir_addr, 4);
    return (
               raw_read (upper, get_i2c_addr (chip_id), &out[0],
                         5, (uint8_t *)data, 4));
}

int bf_pltfm_switchd_dir_i2c_rd (int chip_id,
                                 uint32_t dir_addr,
                                 uint32_t *data)
{
    return bf_pltfm_dir_i2c_rd (chip_id,
                                TOFINO_I2c_BAR0, dir_addr, data);
}

/** Tofino slave mode i2c write with "direct write" command
 *
 *  @param chip_id
 *    chip_id
 *  @param seg
 *    BAR segment (0: regular PCIe space, 1: MSIX cap space,
 *                 2: 4K pcie cap/cfg space)
 *  @param dir_reg
 *     direct address space register to access
 *  @param data
 *    data to write to dir_addr
 *  @return
 *    0 on success and any other value on error
 */
int bf_pltfm_dir_i2c_wr (int chip_id,
                         Tofino_I2C_BAR_SEG seg,
                         uint32_t dir_addr,
                         uint32_t data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;
    uint8_t out[16];

    out[0] = TOFINO_I2C_OPCODE_DIR_WR;
    dir_addr |= (seg << TOFINO_I2C_BAR_SEG_SHFT);
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &dir_addr, 4);
    memcpy (&out[5], &data, 4);

    return (raw_write (upper, get_i2c_addr (chip_id),
                       &out[0], 9));
}

int bf_pltfm_switchd_dir_i2c_wr (int chip_id,
                                 uint32_t dir_addr, uint32_t data)
{
    return bf_pltfm_dir_i2c_wr (chip_id,
                                TOFINO_I2c_BAR0, dir_addr, data);
}

/** Tofino slave mode i2c read with "indirect read" command
 *
 *  @param chip_id
 *    chip_id
 *  @param indir_reg
 *     indirect address space register to access (40 least significant  bit)
 *  @param data
 *    data to read from indir_addr (128 bits = 16 bytes array)
 *  @return
 *    0 on success and any other value on error
 */
int bf_pltfm_ind_i2c_rd (int chip_id,
                         uint64_t indir_addr, uint8_t *data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;
    uint8_t out[7];

    out[0] = TOFINO_I2C_OPCODE_DIR_RD;
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &indir_addr, 5);
    return (
               raw_read (upper, get_i2c_addr (chip_id), &out[0],
                         6, (uint8_t *)data, 16));
}

/** Tofino slave mode i2c write with "indirect write" command
 *
 *  @param chip_id
 *    chip_id
 *  @param indir_reg
 *     indirect address space register to access (40 least significant  bit)
 *  @param data
 *    data to write to indir_addr (128 bits = 16 bytes array)
 *  @return
 *    0 on success and any other value on error
 */
int bf_pltfm_ind_i2c_wr (int chip_id,
                         uint64_t indir_addr, uint8_t *data)
{
    bf_pltfm_cp2112_device_ctx_t *upper =
        tfn_slv_i2c_hdl;
    uint8_t out[32];

    out[0] = TOFINO_I2C_OPCODE_INDIR_WR;
    /* TBD : check endianness with tofino specs */
    memcpy (&out[1], &indir_addr, 5);
    memcpy (&out[6], data, 16);

    return (raw_write (upper, get_i2c_addr (chip_id),
                       &out[0], 22));
}

static ucli_status_t
bf_pltfm_ucli_ucli__loc_i2c_rd (ucli_context_t
                                *uc)
{
    bf_dev_id_t dev = 0;
    uint8_t addr;
    uint8_t data;

    UCLI_COMMAND_INFO (uc, "loc-i2c-rd", 2,
                       "loc-i2c-rd <dev> 0x<addr>");

    dev = atoi (uc->pargs->args[0]);
    addr = strtol (uc->pargs->args[1], NULL, 16);

    aim_printf (&uc->pvs,
                "bf_pltfm_slave_i2c: loc-i2c-rd <dev=%d> <addr=0x%x>\n",
                dev,
                addr);
    if (bf_pltfm_loc_i2c_rd (dev, addr, &data)) {
        aim_printf (&uc->pvs, "error I2C read\n");
        return 0;
    } else {
        aim_printf (&uc->pvs, "addr: data 0x%x: 0x%x\n",
                    addr, data);
    }
    return 0;
}
static ucli_status_t
bf_pltfm_ucli_ucli__loc_i2c_wr (ucli_context_t
                                *uc)
{
    bf_dev_id_t dev = 0;
    uint8_t addr;
    uint8_t data;

    UCLI_COMMAND_INFO (uc, "loc-i2c-wr", 3,
                       "loc-i2c-wr <dev> 0x<addr> 0x<data>");

    dev = atoi (uc->pargs->args[0]);
    addr = strtol (uc->pargs->args[1], NULL, 16);
    data = strtol (uc->pargs->args[2], NULL, 16);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_slave_i2c: loc-i2c-wr <dev=%d> <addr=0x%x> <data=0x%x>\n",
        dev,
        addr,
        data);
    if (bf_pltfm_loc_i2c_wr (dev, addr, data)) {
        aim_printf (&uc->pvs, "error I2C write\n");
    }
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__dir_i2c_rd (ucli_context_t
                                *uc)
{
    bf_dev_id_t dev = 0;
    int bar;
    int addr;
    int data;
    int i, cnt;

    UCLI_COMMAND_INFO (uc,
                       "dir-i2c-rd",
                       4,
                       "dir-i2c-rd <dev> <bar-seg> 0x<addr> <num-32bit-words>");

    dev = atoi (uc->pargs->args[0]);
    bar = atoi (uc->pargs->args[1]);
    addr = strtol (uc->pargs->args[2], NULL, 16);
    cnt = atoi (uc->pargs->args[3]);

    aim_printf (
        &uc->pvs,
        "bf_pltfm_slave_i2c: dir-i2c-rd <dev=%d> <bar=%d> <addr=0x%x> <cnt=%d\n",
        dev,
        bar,
        addr,
        cnt);
    for (i = 0; i < cnt; i++) {
        if (bf_pltfm_dir_i2c_rd (dev, bar, addr + (i * 4),
                                 (uint32_t *)&data)) {
            aim_printf (&uc->pvs, "error I2C read\n");
            return 0;
        } else {
            aim_printf (&uc->pvs, "addr: data 0x%x: 0x%x\n",
                        addr + (i * 4), data);
        }
    }
    aim_printf (&uc->pvs, "I2c Read OK\n");
    return 0;
}

static ucli_status_t
bf_pltfm_ucli_ucli__dir_i2c_wr (ucli_context_t
                                *uc)
{
    bf_dev_id_t dev = 0;
    int bar;
    int addr;
    int data;

    UCLI_COMMAND_INFO (
        uc, "dir-i2c-wr", 4,
        "dir-i2c-wr <dev> <bar-seg> 0x<addr> 0x<data>");

    dev = atoi (uc->pargs->args[0]);
    bar = atoi (uc->pargs->args[1]);
    addr = strtol (uc->pargs->args[2], NULL, 16);
    data = strtol (uc->pargs->args[3], NULL, 16);

    aim_printf (&uc->pvs,
                "bf_pltfm_slave_i2c: dir-i2c-wr <dev=%d> <bar=%d> <addr=0x%x> "
                "<data=0x%x>\n",
                dev,
                bar,
                addr,
                data);

    if (bf_pltfm_dir_i2c_wr (dev, bar, addr, data)) {
        aim_printf (&uc->pvs, "error I2C write\n");
    } else {
        aim_printf (&uc->pvs, "I2c write OK\n");
    }
    return 0;
}

/* <auto.ucli.handlers.start> */
static ucli_command_handler_f
bf_pltfm_slave_i2c_ucli_ucli_handlers__[] = {
    bf_pltfm_ucli_ucli__dir_i2c_rd,
    bf_pltfm_ucli_ucli__dir_i2c_wr,
    bf_pltfm_ucli_ucli__loc_i2c_rd,
    bf_pltfm_ucli_ucli__loc_i2c_wr,
    NULL,
};

/* <auto.ucli.handlers.end> */
static ucli_module_t
bf_pltfm_slave_i2c_ucli_module__ = {
    "slave_i2c_ucli", NULL, bf_pltfm_slave_i2c_ucli_ucli_handlers__, NULL, NULL,
};

ucli_node_t *bf_pltfm_slave_i2c_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (
        &bf_pltfm_slave_i2c_ucli_module__);
    n = ucli_node_create ("slave_i2c", m,
                          &bf_pltfm_slave_i2c_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("slave_i2c"));
    return n;
}

/* Init function  */
int bf_pltfm_slave_i2c_init (void *arg)
{
    (void)arg;
    bf_pltfm_i2c_init (0);
    return 0;
}
