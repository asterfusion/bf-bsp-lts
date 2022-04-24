#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bfutils/uCli/ucli.h>
#include <bfutils/uCli/ucli_argparse.h>
#include <bfutils/uCli/ucli_handler_macros.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

#define DEFAULT_TIMEOUT_MS 500

int bf_pltfm_max_cplds = 2;

static void hex_dump (ucli_context_t *uc,
                      uint8_t *buf, uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            aim_printf (&uc->pvs, "\n%3d : ", byte);
        }
        aim_printf (&uc->pvs, "%02x ", buf[byte]);
    }
    aim_printf (&uc->pvs, "\n");

}


int select_cpld (int cpld)
{
    /* for X312P, there's no need to select CPLD */
    if (platform_type_equal (X312P)) {
        return 0;
    }

    int rc;
    int chnl = 0;

    if ((cpld <= 0) || (cpld > bf_pltfm_max_cplds)) {
        return -1;
    }

    chnl = cpld - 1;

    /* Switch channel */
    rc = bf_pltfm_master_i2c_write_byte (
             BF_MAV_MASTER_PCA9548_ADDR, 0x00, 1 << chnl);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "i2c_write(%d : %s)"
            "\n",
            __FILE__, __LINE__, cpld,
            "Failed to write PCA9548");
        return -1;
    }
    return 0;
}

int unselect_cpld()
{
    /* for X312P, there's no need to select CPLD */
    if (platform_type_equal (X312P)) {
        return 0;
    }

    int rc;

    /* disable all channel */
    rc = bf_pltfm_master_i2c_write_byte (
             BF_MAV_MASTER_PCA9548_ADDR, 0x00, 0);
    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "i2c_write(%s)"
            "\n",
            __FILE__, __LINE__, "Failed to write PCA9548");
        return -1;
    }
    return 0;
}

int bf_pltfm_cpld_read_bytes_x3 (
    int cpld_index,
    uint8_t *buf,
    uint8_t rdlen)
{
    cpld_index = cpld_index;
    buf = buf;
    rdlen = rdlen;
    return 0;
}

int bf_pltfm_cpld_read_byte_x3 (
    int i2c_addr,
    uint8_t offset,
    uint8_t *buf)
{
    int rc = 0;
    //uint8_t addr = ((uint8_t)i2c_addr) << 1;
    //bf_pltfm_cp2112_device_ctx_t *hndl = bf_pltfm_cp2112_get_handle(CP2112_ID_1);

    MASTER_I2C_LOCK;
    rc = bf_pltfm_master_i2c_read_byte (i2c_addr,
                                        offset, buf);
    if (rc) {
        goto end;
    }

    //rc = bf_pltfm_cp2112_write_byte(hndl, addr, offset, DEFAULT_TIMEOUT_MS);
    //if (rc) {
    //    goto end;
    //}

    //rc = bf_pltfm_cp2112_read(hndl, addr, buf, 1, DEFAULT_TIMEOUT_MS);
    //if (rc) {
    //    goto end;
    //}

end:
    MASTER_I2C_UNLOCK;
    return rc;
}

static int bf_pltfm_cpld_write_bytes_x3 (
    int i2c_addr,
    uint8_t offset,
    uint8_t *buf,
    uint8_t wrlen)
{
    i2c_addr = i2c_addr;
    offset = offset;
    buf = buf;
    wrlen = wrlen;
    return 0;
}

static int bf_pltfm_cpld_write_byte_x3 (
    int i2c_addr,
    uint8_t offset,
    uint8_t val)
{
    int rc = 0;
    //uint8_t addr = ((uint8_t)cpld_index) << 1;
    //uint8_t tmp[2];
    //bf_pltfm_cp2112_device_ctx_t *hndl = bf_pltfm_cp2112_get_handle(CP2112_ID_1);
    //tmp[0] = offset;
    //tmp[1] = val;

    MASTER_I2C_LOCK;
    //rc = bf_pltfm_cp2112_write(hndl, addr, tmp, 2, DEFAULT_TIMEOUT_MS);
    rc = bf_pltfm_master_i2c_write_byte (i2c_addr,
                                         offset, val);
    if (rc) {
        goto end;
    }

end:
    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_cpld_read_bytes (
    int cpld_index,
    uint8_t *buf,
    uint8_t rdlen)
{
    if (platform_type_equal (X312P)) {
        return bf_pltfm_cpld_read_bytes_x3 (cpld_index,
                                            buf, rdlen);
    }
    int rv = -1;
    int i = 0;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {

        for (i = 0; i < rdlen; i ++) {
            if (!bf_pltfm_master_i2c_read_byte (0x40, i,
                                                &buf[i])) {
                continue;
            }
        }

        if (i == rdlen) {
            rv = 0;
        }

        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    /* If the channel selected error, return false immediately. */
    return rv;
}


int bf_pltfm_cpld_read_byte (
    int cpld_index,
    uint8_t offset,
    uint8_t *buf)
{
    if (platform_type_equal (X312P)) {
        return bf_pltfm_cpld_read_byte_x3 (cpld_index,
                                           offset, buf);
    }

    int rv = -1;
    int i = 0;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {

        if (!bf_pltfm_master_i2c_read_byte (0x40, offset,
                                            &buf[i])) {
            rv = 0;
        }

        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    /* If the channel selected error, return false immediately. */
    return rv;
}

int bf_pltfm_cpld_write_bytes (
    int cpld_index,
    uint8_t offset,
    uint8_t *buf,
    uint8_t wrlen)
{
    if (platform_type_equal (X312P)) {
        return bf_pltfm_cpld_write_bytes_x3 (cpld_index,
                                             offset, buf, wrlen);
    }
    int rv = -1;
    int i = 0;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {

        for (i = 0; i < wrlen; i ++) {
            if (!bf_pltfm_master_i2c_write_byte (0x40, i,
                                                 buf[i])) {
                continue;
            }
        }

        if (i == wrlen) {
            rv = 0;
        }

        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    /* If the channel selected error, return false immediately. */
    return rv;
}

int bf_pltfm_cpld_write_byte (
    int cpld_index,
    uint8_t offset,
    uint8_t val)
{
    if (platform_type_equal (X312P)) {
        return bf_pltfm_cpld_write_byte_x3 (cpld_index,
                                            offset, val);
    }
    int rv = -1;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {

        if (!bf_pltfm_master_i2c_write_byte (0x40, offset,
                                             val)) {
            rv = 0;
        }

        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    /* If the channel selected error, return false immediately. */
    return rv;
}

static int bf_pltfm_syscpld_reset_x312p()
{
    int rc = 0;
    uint8_t val, val0;
    uint8_t offset;

    // reset CPLD
    offset = 0x0C;
    val = 0x5f;
    rc |= bf_pltfm_cpld_read_byte (
              BF_MON_SYSCPLD1_I2C_ADDR, offset, &val0);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              BF_MON_SYSCPLD1_I2C_ADDR, offset, val);
    // sleep 0.2 s
    bf_sys_usleep (0.2 * 1000 * 1000);
    fprintf (stdout,
             "CPLD3-5  RST(auto de-reset) : (0x%02x -> 0x%02x)\n",
             val0, val);
    LOG_DEBUG ("CPLD3-5 RST(auto de-reset) : (0x%02x -> 0x%02x)",
               val0, val);

    return rc;
}

/* Reset CPLD2 for QSFP/SFP control.
 * offset = 2, and
 * bit[2] reset cpld2
 * by tsihang, 2020-05-25 */
int bf_pltfm_syscpld_reset()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    uint8_t offset = 0x2;
    int usec = 500;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, &val0);

    val = val0;
    if (platform_type_equal (X532P)) {
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 2);
    } else if (platform_type_equal (X564P)) {
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 4);
        /* reset cpld3 */
        val |= (1 << 5);

    } else if (platform_type_equal (HC)) {
        offset = 0x3;
        /* reset cpld2 */
        val |= (1 << 3);
        /* reset cpld3 */
        val |= (1 << 4);
    }

    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, offset, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, offset, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "CPLD  +RST : (0x%02x -> 0x%02x)\n", val0, val1);
    LOG_DEBUG ("CPLD +RST : (0x%02x -> 0x%02x)", val0,
               val1);


    val = val1;
    if (platform_type_equal (X532P)) {
        /* !reset cpld2 */
        val &= ~ (1 << 2);
    } else if (platform_type_equal (X564P)) {
        /* !reset cpld2 */
        val &= ~ (1 << 4);
        /* !reset cpld3 */
        val &= ~ (1 << 5);
    } else if (platform_type_equal (HC)) {
        /* !reset cpld2 */
        val &= ~ (1 << 3);
        /* !reset cpld3 */
        val &= ~ (1 << 4);
    } else { }

    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, offset, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, offset, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "CPLD  -RST : (0x%02x -> 0x%02x)\n", val1, val0);
    LOG_DEBUG ("CPLD -RST : (0x%02x -> 0x%02x)", val1,
               val0);

end:
    return rc;
}

/* Reset CP2112 for QSFP/SFP control.
 * offset = 2, and
 * bit[3] reset CP2112
 * by tsihang, 2020-05-25 */
int bf_pltfm_cp2112_reset()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, &val0);

    val = val0;
    if (platform_type_equal (X532P)) {
        val |= (1 << 3);
    } else if (platform_type_equal (X564P)) {
        val |= (1 << 6);
    } else { }

    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "CP2112  +RST : (0x%02x -> 0x%02x)\n", val0,
             val1);
    LOG_DEBUG ("CP2112 +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    if (platform_type_equal (X532P)) {
        val &= ~ (1 << 3);
    } else if (platform_type_equal (X564P)) {
        val &= ~ (1 << 6);
    } else { }

    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD1_I2C_ADDR, 2, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "CP2112  -RST : (0x%02x -> 0x%02x)\n", val1,
             val0);
    LOG_DEBUG ("CP2112 -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * offset = 14, and
 * bit[0] reset PCA9548 for C8~C1
 * bit[1] reset PCA9548 for C16~C9
 * bit[2] reset PCA9548 for C24~C17
 * bit[3] reset PCA9548 for C32~C25
 * by tsihang, 2020-05-25 */
int bf_pltfm_pca9548_reset_03()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-3)  +RST : (0x%02x -> 0x%02x)\n", val0,
             val1);
    LOG_DEBUG ("PCA9548(0-3) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 0);
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    val &= ~ (1 << 3);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 14, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-3)  -RST : (0x%02x -> 0x%02x)\n", val1,
             val0);
    LOG_DEBUG ("PCA9548(0-3) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2, and
 * bit[3] reset PCA9548 for C8~C1
 * bit[2] reset PCA9548 for C16~C9
 * bit[1] reset PCA9548 for C24~C17
 * bit[0] reset PCA9548 for C32~C25
 * by tsihang, 2020-05-25 */
int bf_pltfm_pca9548_reset_47()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(4-7)  +RST : (0x%02x -> 0x%02x)\n", val0,
             val1);
    LOG_DEBUG ("PCA9548(4-7) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 0);
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    val &= ~ (1 << 3);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(4-7)  -RST : (0x%02x -> 0x%02x)\n", val1,
             val0);
    LOG_DEBUG ("PCA9548(4-7) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

int bf_pltfm_pca9548_reset_x312p()
{
    int rc = 0;
    // select to level1 pca9548 ch1, and unselect sub pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 1 << 1);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x74, 0x0, 0x0);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch2, and unselect sub pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 1 << 2);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch3, and unselect sub pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 1 << 3);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch4, and unselect sub pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 1 << 4);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch5, and unselect sub pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 1 << 5);
    bf_sys_usleep (500);
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    bf_sys_usleep (500);

    // unselect level1 pca9548
    rc |= bf_pltfm_cpld_write_byte (
              X312P_PCA9548_L1_0X71, 0x0, 0x0);
    bf_sys_usleep (500);

    if (rc) {
        fprintf (stdout,
                 "unselect all PCA9548 failed!\n");
    } else {
        fprintf (stdout,
                 "unselect all PCA9548 success!\n");
    }

    return rc;
}

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2
 */
int bf_pltfm_pca9548_reset_hc()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    val |= (1 << 4);
    val |= (1 << 5);
    val |= (1 << 6);
    val |= (1 << 7);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-7)  +RST : (0x%02x -> 0x%02x)\n", val0,
             val1);
    LOG_DEBUG ("PCA9548(0-7) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 0);
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    val &= ~ (1 << 3);
    val &= ~ (1 << 4);
    val &= ~ (1 << 5);
    val &= ~ (1 << 6);
    val &= ~ (1 << 7);
    rc |= bf_pltfm_master_i2c_write_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_master_i2c_read_byte (
              BF_MAV_SYSCPLD2_I2C_ADDR, 2, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-7)  -RST : (0x%02x -> 0x%02x)\n", val1,
             val0);
    LOG_DEBUG ("PCA9548(0-7) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

int bf_pltfm_syscpld_init_x564p()
{
    int rc;
    MASTER_I2C_LOCK;
    if (((rc = select_cpld (1)) != 0)) {
        goto end;
    } else {
        bf_pltfm_syscpld_reset();
        /* Never try to reset CP2112. */
        //bf_pltfm_cp2112_reset();
        if (platform_type_equal (X564P)) {
            bf_pltfm_pca9548_reset_47();
        }
    }
    if (((rc = select_cpld (2)) != 0)) {
        goto end;
    } else {
        bf_pltfm_pca9548_reset_03();
    }
end:
    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_syscpld_init_x532p()
{
    int rc;
    MASTER_I2C_LOCK;
    if (((rc = select_cpld (1)) != 0)) {
        goto end;
    } else {
        bf_pltfm_syscpld_reset();
        /* Never try to reset CP2112. */
        //bf_pltfm_cp2112_reset();
    }
    if (((rc = select_cpld (2)) != 0)) {
        goto end;
    } else {
        bf_pltfm_pca9548_reset_03();
    }
end:
    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_syscpld_init_hc()
{
    int rc;
    MASTER_I2C_LOCK;

    do {
        if (((rc = select_cpld (1)) != 0)) {
            break;
        } else {
            bf_pltfm_syscpld_reset();
            /* Never try to reset CP2112. */
            //bf_pltfm_cp2112_reset();
            /* reset PCA9548 */
            bf_pltfm_pca9548_reset_hc();
        }
    } while (0);

    MASTER_I2C_UNLOCK;
    return rc;
}

int bf_pltfm_syscpld_init_x312p()
{
    MASTER_I2C_LOCK;

    /* reset cpld */
    bf_pltfm_syscpld_reset_x312p();
    /* reset PCA9548 */
    bf_pltfm_pca9548_reset_x312p();

    MASTER_I2C_UNLOCK;

    return 0;
}

int bf_pltfm_syscpld_init()
{
    if (platform_type_equal (X564P)) {
        bf_pltfm_max_cplds = 3;
        return bf_pltfm_syscpld_init_x564p();
    } else if (platform_type_equal (X532P)) {
        bf_pltfm_max_cplds = 2;
        return bf_pltfm_syscpld_init_x532p();
    } else if (platform_type_equal (HC)) {
        bf_pltfm_max_cplds = 3;
        return bf_pltfm_syscpld_init_hc();
    } else if (platform_type_equal (X312P)) {
        bf_pltfm_max_cplds = 5;
        return bf_pltfm_syscpld_init_x312p();
    }
    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__read_cpld (
    ucli_context_t *uc)
{
    uint8_t buf[BUFSIZ];
    int cpld_index;
    uint8_t cpld_page_size = 128;

    UCLI_COMMAND_INFO (uc, "read-cpld", 1,
                       "read-cpld <cpld_idx>");

    cpld_index = strtol (uc->pargs->args[0], NULL, 0);

    if ((cpld_index <= 0) ||
        (cpld_index > bf_pltfm_max_cplds)) {
        aim_printf (&uc->pvs,
                    "invalid cpld_idx:%d, should be 1~%d\n",
                    cpld_index, bf_pltfm_max_cplds);
        return -1;
    }

    if (bf_pltfm_cpld_read_bytes (cpld_index, buf,
                                  cpld_page_size)) {
        return -1;
    }

    aim_printf (&uc->pvs, "\ncpld %d:\n",
                cpld_index);
    hex_dump (uc, buf, cpld_page_size);

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__write_cpld (
    ucli_context_t *uc)
{
    int cpld_index;
    uint8_t offset;
    uint8_t val_wr, val_rd;

    UCLI_COMMAND_INFO (uc, "write-cpld", 3,
                       "write-cpld <cpld_idx> <offset> <val>");

    cpld_index = strtol (uc->pargs->args[0], NULL, 0);
    offset     = strtol (uc->pargs->args[1], NULL, 0);
    val_wr     = strtol (uc->pargs->args[2], NULL, 0);

    if ((cpld_index <= 0) ||
        (cpld_index > bf_pltfm_max_cplds)) {
        aim_printf (&uc->pvs,
                    "invalid cpld_idx:%d, should be 1~%d\n",
                    cpld_index, bf_pltfm_max_cplds);
        return -1;
    }

    if (bf_pltfm_cpld_write_byte (cpld_index, offset,
                                  val_wr)) {
        return -1;
    }

    if (bf_pltfm_cpld_read_byte (cpld_index, offset,
                                 &val_rd)) {
        return -1;
    }

    if (offset == 1) {
        if ((val_wr ^ val_rd) == 0xFF) {
            aim_printf (&uc->pvs,
                        "cpld%d write test register successfully\n",
                        cpld_index);
            return 0;
        } else {
            aim_printf (&uc->pvs,
                        "cpld%d write test register failure\n",
                        cpld_index);
            return -1;
        }
    } else {
        if (val_wr == val_rd) {
            aim_printf (&uc->pvs,
                        "cpld write successfully\n");
            return 0;
        } else {
            aim_printf (&uc->pvs,
                        "cpld write failure, maybe read-only or invalid address\n");
            return -1;
        }
    }
}

static ucli_command_handler_f
bf_pltfm_cpld_ucli_ucli_handlers__[] = {
    bf_pltfm_cpld_ucli_ucli__read_cpld,
    bf_pltfm_cpld_ucli_ucli__write_cpld,
    NULL
};

static ucli_module_t bf_pltfm_cpld_ucli_module__
= {
    "bf_pltfm_cpld_ucli",
    NULL,
    bf_pltfm_cpld_ucli_ucli_handlers__,
    NULL,
    NULL,
};

ucli_node_t *bf_pltfm_cpld_ucli_node_create (
    ucli_node_t *m)
{
    ucli_node_t *n;
    ucli_module_init (&bf_pltfm_cpld_ucli_module__);
    n = ucli_node_create ("cpld", m,
                          &bf_pltfm_cpld_ucli_module__);
    ucli_node_subnode_add (n,
                           ucli_module_log_node_create ("cpld"));
    return n;
}
