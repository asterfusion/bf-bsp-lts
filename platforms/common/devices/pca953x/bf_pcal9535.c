#include <bf_types/bf_types.h>
#include <bf_pltfm_types/bf_pltfm_types.h>  // LOG_ERROR
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "bf_pcal9535_priv.h"
#include "../include/bf_pcal9535.h"

static pcal9535_info_t pca_info;

static inline void pcal9535_lock (uint8_t device)
{
    if (pca_info.device_lock) {
        pca_info.device_lock (device);
    }
}

static inline void pcal9535_unlock (
    uint8_t device)
{
    if (pca_info.device_unlock) {
        pca_info.device_unlock (device);
    }
}

static bf_pltfm_status_t pcal9535_bf_read_port (
    uint8_t device,
    uint8_t dev_reg,
    uint16_t port_pos,
    uint8_t nbytes,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret = BF_PLTFM_SUCCESS;
    uint16_t port_map;

    if ((!pca_info.read) || (!port_val)) {
        return -1;
    }

    pcal9535_lock (device);
    ret = pca_info.read (device, dev_reg, nbytes,
                         &port_map, user_data);
    pcal9535_unlock (device);
    if (ret != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to read PCAL9535[0x%x] reg %d port_pos:0x%0x\n",
                   device,
                   dev_reg,
                   port_pos);
        return (ret);
    }

    if (port_pos != 0xFFFF) {
        *port_val = (port_map & (1 << port_pos));
    } else {
        *port_val = port_map;
    }

    if (pca_info.debug) {
        LOG_DEBUG (
            "Read PCAL9535[0x%x] reg %d port_pos:0x%0x success. reg-val:0x%0x, "
            "port-val:%d\n",
            device,
            dev_reg,
            port_pos,
            port_map,
            *port_val);
    }
    return (ret);
}

static bf_pltfm_status_t pcal9535_bf_write_port (
    uint8_t device,
    uint8_t dev_reg,
    uint16_t port_pos,
    bool set,
    uint8_t nbytes,
    void *user_data)
{
    uint16_t port_map;
    bf_pltfm_status_t ret = BF_PLTFM_SUCCESS;

    if ((!pca_info.write) || (!pca_info.read)) {
        return -1;
    }

    pcal9535_lock (device);
    if (port_pos != 0xffff) {
        ret = pca_info.read (device, dev_reg, nbytes,
                             &port_map, user_data);
        if (set) {
            port_map |= (1 << port_pos);
        } else {
            port_map &= ~ (1 << port_pos);
        }
        ret |= pca_info.write (device, dev_reg, nbytes,
                               &port_map, user_data);
    } else {
        if (set) {
            port_map = 0xffff;
        } else {
            port_map = 0x0;
        }
        ret = pca_info.write (device, dev_reg, nbytes,
                              &port_map, user_data);
    }
    pcal9535_unlock (device);
    if (ret != BF_PLTFM_SUCCESS) {
        LOG_ERROR ("Failed to write PCAL9535[0x%x] reg %d port_pos:0x%0x\n",
                   device,
                   dev_reg,
                   port_pos);
        return (ret);
    }

    if (pca_info.debug) {
        LOG_DEBUG (
            "write PCAL9535[0x%x] reg %d port-pos:0x%0x port-val:%d success\n",
            device,
            dev_reg,
            port_pos,
            port_map);
    }
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_set_output_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool enable,
    void *user_data)
{
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_OUTPUT_PORT_REG_0;
    bf_pltfm_status_t ret;

    if (grp) {
        dev_reg = PCAL9535_OUTPUT_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_write_port (
              device, dev_reg, port_pos, enable, nbytes,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_enable_input_latch (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool enable,
    void *user_data)
{
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_INPUT_LATCH_PORT_REG_0;
    bf_pltfm_status_t ret;

    if (grp) {
        dev_reg = PCAL9535_INPUT_LATCH_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_write_port (
              device, dev_reg, port_pos, enable, nbytes,
              user_data);
    return (ret);
}

// Pin direction as input or output
bf_pltfm_status_t pcal9535_bf_set_config_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool input,
    void *user_data)
{
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_CONF_PORT_REG_0;
    bf_pltfm_status_t ret;

    if (grp) {
        dev_reg = PCAL9535_CONF_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_write_port (
              device, dev_reg, port_pos, input, nbytes,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_set_pol_inv (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    bool invert,
    void *user_data)
{
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_POL_INV_PORT_REG_0;
    bf_pltfm_status_t ret;

    if (grp) {
        dev_reg = PCAL9535_POL_INV_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_write_port (
              device, dev_reg, port_pos, invert, nbytes,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_get_output_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret;
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_OUTPUT_PORT_REG_0;

    if (grp) {
        dev_reg = PCAL9535_OUTPUT_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_read_port (
              device, dev_reg, port_pos, nbytes, port_val,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_get_config_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret;
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_CONF_PORT_REG_0;

    if (grp) {
        dev_reg = PCAL9535_CONF_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_read_port (
              device, dev_reg, port_pos, nbytes, port_val,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_get_pol_inv_reg (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret;
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_POL_INV_PORT_REG_0;

    if (grp) {
        dev_reg = PCAL9535_POL_INV_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_read_port (
              device, dev_reg, port_pos, nbytes, port_val,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_get_input_port (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret;
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_INPUT_PORT_REG_0;

    if (grp) {
        dev_reg = PCAL9535_INPUT_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_read_port (
              device, dev_reg, port_pos, nbytes, port_val,
              user_data);
    return (ret);
}

bf_pltfm_status_t pcal9535_bf_get_input_latch (
    uint8_t device,
    uint8_t grp,
    uint16_t port_pos,
    uint16_t *port_val,
    void *user_data)
{
    bf_pltfm_status_t ret;
    uint8_t nbytes = 2;
    uint8_t dev_reg = PCAL9535_INPUT_PORT_REG_0;

    if (grp) {
        dev_reg = PCAL9535_INPUT_PORT_REG_1;
        nbytes = 1;
    }

    ret = pcal9535_bf_read_port (
              device, dev_reg, port_pos, nbytes, port_val,
              user_data);
    return (ret);
}

// platform specific initialization
int pcal9535_platform_init (pcal9535_info_t *pca)
{
    if (!pca) {
        return -1;
    }

    if (pca->pca_dev_id != PCAL9535) {
        LOG_ERROR ("Failed to init. Unsupported device:0x%0x\n",
                   pca->pca_dev_id);
        return -1;
    }

    pca_info.pca_dev_id = pca->pca_dev_id;
    pca_info.read = pca->read;
    pca_info.write = pca->write;
    pca_info.device_lock = pca->device_lock;
    pca_info.device_unlock = pca->device_unlock;
    pca_info.debug = pca->debug;

    return 0;
}
