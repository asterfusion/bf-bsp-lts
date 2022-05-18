#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_cp2112_intf.h>

#include "bf_pltfm_cp2112.h"

#define DEFAULT_TIMEOUT_MS 500

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_read__ (
    ucli_context_t *uc)
{
    unsigned int i;
    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (
        uc, "read", 3,
        "<dev(Lower->0, Upper->1)> <i2c_addr> <length>");

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);
    i2c_addr = (uint8_t)strtol (uc->pargs->args[1],
                                NULL, 16);
    byte_buf_size = (uint8_t)strtol (
                        uc->pargs->args[2], NULL, 16);

    aim_printf (&uc->pvs,
                "CP2112:: read <dev=%d> <i2c_addr=%02x> <length=%d>\n",
                cp2112_id_,
                (unsigned)i2c_addr,
                byte_buf_size);
    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (&uc->pvs,
                    "CP2112:: Read failed. \nUsage: read <dev(Lower->0, Upper->1)> "
                    "<i2c_addr> <length>\n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    byte_buf = (uint8_t *)malloc (byte_buf_size *
                                  sizeof (uint8_t));

    bf_pltfm_status_t sts = bf_pltfm_cp2112_read (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: cp2112 read failed for dev %d addr %02x \n",
                    cp2112_id_,
                    (unsigned)i2c_addr);
    } else {
        aim_printf (&uc->pvs,
                    "SUCCESS: cp2112 read succeeded \n");
        aim_printf (&uc->pvs, "The read bytes are :\n");
        for (i = 0; i < byte_buf_size; i++) {
            aim_printf (&uc->pvs, "%02x ",
                        (unsigned)byte_buf[i]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    free (byte_buf);

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_write__ (
    ucli_context_t *uc)
{
    uint8_t i2c_addr;
    uint32_t byte_buf_size;
    uint8_t *byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (
        uc,
        "write",
        -1,
        "<dev(Lower->0, Upper->1)> <i2c_addr> <length> <byte1> [<byte2> ...]");

    if (uc->pargs->count < 3) {
        aim_printf (&uc->pvs,
                    "CP2112:: Insufficient arguments\n");
        return 0;
    }

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);
    i2c_addr = (uint8_t)strtol (uc->pargs->args[1],
                                NULL, 16);
    byte_buf_size = (uint8_t)strtol (
                        uc->pargs->args[2], NULL, 16);

    if (uc->pargs->count < (int) (3 +
                                  byte_buf_size)) {
        aim_printf (&uc->pvs,
                    "CP2112:: Insufficient arguments\n");
        return 0;
    }

    aim_printf (&uc->pvs,
                "CP2112:: write <dev=%d> <i2c_addr=%02x> <length=%d> "
                "<byte1=%02x> ...\n",
                cp2112_id_,
                (unsigned)i2c_addr,
                byte_buf_size,
                (uint8_t)strtol (uc->pargs->args[3], NULL, 16));

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (
            &uc->pvs,
            "CP2112:: Write failed. \nUsage: write <dev(Lower->0, Upper->1)> "
            "<i2c_addr> <length> <byte1> [<byte2> ...]\n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    byte_buf = (uint8_t *)malloc (byte_buf_size *
                                  sizeof (uint8_t));

    unsigned int i;
    for (i = 0; i < byte_buf_size; i++) {
        byte_buf[i] = strtol (uc->pargs->args[3 + i],
                              NULL, 16);
    }

    bf_pltfm_status_t sts = bf_pltfm_cp2112_write (
                                cp2112_dev, i2c_addr, byte_buf, byte_buf_size,
                                DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: cp2112 write failed for dev %d addr %02x \n",
                    cp2112_id_,
                    (unsigned)i2c_addr);
    } else {
        aim_printf (&uc->pvs,
                    "SUCCESS: cp2112 write succeeded \n");
    }

    free (byte_buf);

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_write_byte__ (
    ucli_context_t *uc)
{
    uint8_t i2c_addr;
    uint8_t byte;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (
        uc, "write-byte", 3,
        "<dev(Lower->0, Upper->1)> <i2c_addr> <byte>");

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);
    i2c_addr = (uint8_t)strtol (uc->pargs->args[1],
                                NULL, 16);
    byte = (uint8_t)strtol (uc->pargs->args[2], NULL,
                            16);

    aim_printf (&uc->pvs,
                "CP2112:: write byte <dev=%d> <i2c_addr=%02x> <byte=%02x> "
                "...\n",
                cp2112_id_,
                (unsigned)i2c_addr,
                byte);

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (&uc->pvs,
                    "CP2112:: Write Byte failed. \nUsage: write byte <dev(Lower->0, "
                    "Upper->1)> <i2c_addr> <byte>\n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    bf_pltfm_status_t sts =
        bf_pltfm_cp2112_write_byte (
            cp2112_dev, i2c_addr, byte, DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: cp2112 write byte failed for dev %d addr %02x \n",
                    cp2112_id_,
                    (unsigned)i2c_addr);
    } else {
        aim_printf (&uc->pvs,
                    "SUCCESS: cp2112 write byte succeeded \n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_write_read_unsafe__
(
    ucli_context_t *uc)
{
    uint8_t i2c_addr;
    uint32_t read_byte_buf_size, write_byte_buf_size;
    uint8_t *read_byte_buf, *write_byte_buf;
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (uc,
                       "write-read-unsafe",
                       -1,
                       "<dev(Lower->0, Upper->1)> <i2c_addr> <read_length> "
                       "<write_length> <byte1> [<byte2> ...]");

    if (uc->pargs->count < 4) {
        aim_printf (&uc->pvs,
                    "CP2112:: Insufficient arguments\n");
        return 0;
    }

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);
    i2c_addr = (uint8_t)strtol (uc->pargs->args[1],
                                NULL, 16);
    read_byte_buf_size = (uint8_t)strtol (
                             uc->pargs->args[2], NULL, 16);
    write_byte_buf_size = (uint8_t)strtol (
                              uc->pargs->args[3], NULL, 16);

    if (uc->pargs->count < (int) (4 +
                                  write_byte_buf_size)) {
        aim_printf (&uc->pvs,
                    "CP2112:: Insufficient arguments\n");
        return 0;
    }

    aim_printf (&uc->pvs,
                "CP2112:: write read unsafe <dev=%d> <i2c_addr=%02x> "
                "<read_length=%d> "
                "<write_length=%d> <byte1=%02x> ...\n",
                cp2112_id_,
                (unsigned)i2c_addr,
                read_byte_buf_size,
                write_byte_buf_size,
                (uint8_t)strtol (uc->pargs->args[4], NULL, 16));

    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (
            &uc->pvs,
            "CP2112:: Write Read Unsafe failed. \nUsage: write read unsafe "
            "<dev(Lower->0, Upper->1)> <i2c_addr> <read_length> <write_length> "
            "<byte1> [<byte2> ...]\n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    read_byte_buf = (uint8_t *)malloc (
                        read_byte_buf_size * sizeof (uint8_t));
    write_byte_buf = (uint8_t *)malloc (
                         write_byte_buf_size * sizeof (uint8_t));

    unsigned int i;
    for (i = 0; i < write_byte_buf_size; i++) {
        write_byte_buf[i] = strtol (uc->pargs->args[4 +
                                    i], NULL, 16);
    }

    bf_pltfm_status_t sts =
        bf_pltfm_cp2112_write_read_unsafe (cp2112_dev,
                                           i2c_addr,
                                           write_byte_buf,
                                           read_byte_buf,
                                           write_byte_buf_size,
                                           read_byte_buf_size,
                                           DEFAULT_TIMEOUT_MS);
    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "ERROR: cp2112 write read unsafe failed for dev %d addr %02x\n",
                    cp2112_id_,
                    (unsigned)i2c_addr);
    } else {
        aim_printf (&uc->pvs,
                    "SUCCESS: cp2112 write read succeeded \n");
        aim_printf (&uc->pvs, "The read bytes are :\n");
        for (i = 0; i < read_byte_buf_size; i++) {
            aim_printf (&uc->pvs, "%02x ",
                        (unsigned)read_byte_buf[i]);
        }
        aim_printf (&uc->pvs, "\n");
    }

    free (read_byte_buf);
    free (write_byte_buf);

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_detect__ (
    ucli_context_t *uc)
{
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (uc, "detect", 1,
                       "<dev(Lower->0, Upper->1)>");

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);

    aim_printf (&uc->pvs,
                "CP2112:: detect <dev=%d>\n", cp2112_id_);
    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (
            &uc->pvs,
            "CP2112:: Detect failed. \nUsage: detect <dev(Lower->0, Upper->1)> \n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    uint8_t i, j, addr;

    aim_printf (&uc->pvs,
                "      00 02 04 06 08 0a 0c 0e\n");
    for (i = 0; i < 128; i += 8) {
        aim_printf (&uc->pvs, "0x%02x: ", i << 1);
        for (j = 0; j < 8; j++) {
            addr = (i + j) << 1;
            if (bf_pltfm_cp2112_addr_scan (cp2112_dev,
                                           addr)) {
                aim_printf (&uc->pvs, "%02x ", addr);
            } else {
                aim_printf (&uc->pvs, "-- ");
            }
        }
        aim_printf (&uc->pvs, "\n");
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_soft_reset__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "soft-reset", 0, "");

    aim_printf (&uc->pvs,
                "CP2112 live soft reset not supported\n");
#if 0 /* can be turned on when nothing else is using cp2112 */
    bf_pltfm_status_t sts =
        bf_pltfm_cp2112_soft_reset();

    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "CP2112:: Unable to soft reset the device(s) \n");
        return 0;
    }
#endif
    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_hard_reset__ (
    ucli_context_t *uc)
{
    UCLI_COMMAND_INFO (uc, "hard-reset", 0, "");

    aim_printf (&uc->pvs,
                "CP2112 live hard reset not supported\n");
#if 0 /* can be turned on when nothing else is using cp2112 */
    bf_pltfm_status_t sts =
        bf_pltfm_cp2112_hard_reset();

    if (sts != BF_PLTFM_SUCCESS) {
        aim_printf (&uc->pvs,
                    "CP2112:: Unable to hard reset the device(s) \n");
        return 0;
    }
#endif
    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_cancel_transfers__
(
    ucli_context_t *uc)
{
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (uc, "cancel-transfers", 1,
                       "<dev(Lower->0, Upper->1)>");

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);

    aim_printf (&uc->pvs,
                "CP2112:: cancel transfers <dev=%d>\n",
                cp2112_id_);
    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (&uc->pvs,
                    "CP2112:: Cancel transfers failed. \nUsage: cancel-transfers "
                    "<dev(Lower->0, Upper->1)> \n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    cp2112_cancel_transfers (cp2112_dev);

    return 0;
}

static ucli_status_t
bf_pltfm_cp2112_ucli_ucli__cp2112_flush_transfers__
(
    ucli_context_t *uc)
{
    uint8_t cp2112_id_;
    bf_pltfm_cp2112_id_t cp2112_id;
    bf_pltfm_cp2112_device_ctx_t *cp2112_dev;

    UCLI_COMMAND_INFO (uc, "flush-transfers", 1,
                       "<dev(Lower->0, Upper->1)>");

    cp2112_id_ = (uint8_t)strtol (uc->pargs->args[0],
                                  NULL, 16);

    aim_printf (&uc->pvs,
                "CP2112:: flush transfers <dev=%d>\n",
                cp2112_id_);
    if (cp2112_id_ == 0) {
        cp2112_id = CP2112_ID_2;
    } else if (cp2112_id_ == 1) {
        cp2112_id = CP2112_ID_1;
    } else {
        aim_printf (&uc->pvs,
                    "CP2112:: Flush transfers failed. \nUsage: flush-transfers "
                    "<dev(Lower->0, Upper->1)> \n");
        return 0;
    }

    cp2112_dev = bf_pltfm_cp2112_get_handle (
                     cp2112_id);
    if (cp2112_dev == NULL) {
        aim_printf (
            &uc->pvs, "ERROR: Unable to get the handle for dev %d\n",
            cp2112_id_);
        return 0;
    }

    cp2112_flush_transfers (cp2112_dev);

    return 0;
}

static ucli_command_handler_f
bf_pltfm_cp2112_ucli_ucli_handlers__[] = {
    bf_pltfm_cp2112_ucli_ucli__cp2112_read__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_write__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_write_byte__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_write_read_unsafe__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_detect__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_soft_reset__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_hard_reset__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_flush_transfers__,
    bf_pltfm_cp2112_ucli_ucli__cp2112_cancel_transfers__,
    NULL
};

static ucli_module_t bf_pltfm_cp2112_ucli_module__
= {
    "bf_pltfm_cp2112_ucli",
    NULL,
    bf_pltfm_cp2112_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_pltfm_cp2112_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_cp2112_ucli_module__);
    n = ucli_node_create ("cp2112", m,
                          &bf_pltfm_cp2112_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("cp2112"));
    return n;
}
