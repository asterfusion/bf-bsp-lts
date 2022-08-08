#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <dvm/bf_dma_types.h>
#include <dvm/bf_drv_intf.h>
#include <bf_pltfm_types/bf_pltfm_types.h>
#include <bf_pltfm_cp2112_intf.h>
#include <bf_pltfm_chss_mgmt_intf.h>
#include <bf_pltfm_syscpld.h>
#include <bf_pltfm_master_i2c.h>
#include <bf_switchd/bf_switchd.h>
#include <bf_pltfm.h>

#include <pltfm_types.h>

#define DEFAULT_TIMEOUT_MS 500

/* Maximum accessiable syscplds of a platform. */
static int bf_pltfm_max_cplds = 2;
/* For YH and S02 CME. */
static bf_pltfm_cp2112_device_ctx_t *g_cpld_cp2112_hndl;

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

/** read bytes from slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to read
*  @param val
*   value to read into
*  @return
*   0 on success and otherwise in error
*/
int bf_pltfm_cp2112_reg_read_block (
  uint8_t addr,
  uint8_t reg,
  uint8_t *read_buf,
  uint32_t read_buf_size)
{
  uint8_t wr_val;

  wr_val = reg;
  return (bf_pltfm_cp2112_write_read_unsafe (g_cpld_cp2112_hndl,
                      addr << 1, &wr_val, read_buf, 1, read_buf_size, 100));
}

/** write a byte to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_byte (
  uint8_t addr,
  uint8_t reg,
  uint8_t val)
{
  uint8_t wr_val[2] = {0};

  wr_val[0] = reg;
  wr_val[1] = val;
  return (bf_pltfm_cp2112_write (g_cpld_cp2112_hndl,
                      addr << 1, wr_val, 2, 100));
}

/** write bytes to slave's register by cp2112.
*
*  @param hndl
*   cp2112 handler
*  @param reg
*   register to write
*  @param val
*   value to write
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cp2112_reg_write_block (
    uint8_t addr,
    uint8_t reg,
    uint8_t *write_buf,
    uint32_t write_buf_size)
{
    uint32_t tx_len = write_buf_size % 256;
    uint8_t wr_val[256] = {0};

    wr_val[0] = reg;
    //wr_val[1] = val;
    memcpy (wr_val + 1, write_buf, tx_len);
    return (bf_pltfm_cp2112_write (g_cpld_cp2112_hndl,
                        addr << 1, wr_val, tx_len, 100));
}

/** write PCA9548 to select channel which connects to syscpld
*
*  @param cpld_index
*   index of cpld for a given platform
*  @return
*   0 on success otherwise in error
*/
int select_cpld (uint8_t cpld_index)
{
    /* for X312P and X308P, there's no need to select CPLD */
    if (platform_type_equal (X312P) || platform_type_equal (X308P)) {
        return 0;
    }

    int rc = -1;
    int chnl = 0;

    if ((cpld_index <= 0) || (cpld_index > bf_pltfm_max_cplds)) {
        return -2;
    }

    chnl = cpld_index - 1;

    if (g_access_cpld_through_cp2112) {
        rc = bf_pltfm_cp2112_reg_write_byte (
                BF_MAV_MASTER_PCA9548_ADDR, 0x00, 1 << chnl);
    } else {
        /* Switch channel */
        rc = bf_pltfm_master_i2c_write_byte (
                BF_MAV_MASTER_PCA9548_ADDR, 0x00, 1 << chnl);
    }

    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "select cpld(%d : %s : %d)"
            "\n",
            __FILE__, __LINE__, cpld_index,
            "Failed to write PCA9548", rc);
    }

    return rc;
}

/** write PCA9548 back to zero to de-select syscpld
*
*  @return
*   0 on success otherwise in error
*/
int unselect_cpld()
{
    /* for X312P and X308P, there's no need to select CPLD */
    if (platform_type_equal (X312P) || platform_type_equal (X308P)) {
        return 0;
    }

    int rc = -1;

    /* for X5-T and HC */
    if (g_access_cpld_through_cp2112) {
        rc = bf_pltfm_cp2112_reg_write_byte (
                BF_MAV_MASTER_PCA9548_ADDR, 0x00, 0);
    } else {
        /* disable all channel */
        rc = bf_pltfm_master_i2c_write_byte (
                BF_MAV_MASTER_PCA9548_ADDR, 0x00, 0);
    }

    if (rc) {
        LOG_ERROR (
            "%s[%d], "
            "de-select cpld(%s : %d)"
            "\n",
            __FILE__, __LINE__, "Failed to write PCA9548", rc);
    }

    return rc;
}

/** read a byte from syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to read to
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_read_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t *buf)
{
    int rc = -1;
    uint8_t addr = 0;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    if (platform_type_equal (X312P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X312P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X312P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X312P_SYSCPLD3_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD4:
                addr = X312P_SYSCPLD4_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD5:
                addr = X312P_SYSCPLD5_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X308P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X308P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X308P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X564P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X564P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X564P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X564P_SYSCPLD3_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X532P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X532P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X532P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (HC)) {
        /* TBD */
    }

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {
        if (g_access_cpld_through_cp2112) {
            rc = bf_pltfm_cp2112_reg_read_block (
                                            addr, offset, buf, 1);
        } else {
            rc = bf_pltfm_master_i2c_read_byte (addr,
                                            offset, buf);
        }
        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    return rc;
}

/** write a byte to syscpld register
*
*  @param cpld_index
*   index of cpld for a given platform
*  @param offset
*   syscpld register to read
*  @param buf
*   buffer to write from
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_cpld_write_byte (
    uint8_t cpld_index,
    uint8_t offset,
    uint8_t val)
{
    int rc = -1;
    uint8_t addr = 0;

    /* It is quite dangerous to access CPLD though Master PCA9548
     * in different thread without protection.
     * Added by tsihang, 20210616. */

    if (platform_type_equal (X312P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X312P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X312P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X312P_SYSCPLD3_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD4:
                addr = X312P_SYSCPLD4_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD5:
                addr = X312P_SYSCPLD5_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X308P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X308P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X308P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X564P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X564P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X564P_SYSCPLD2_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD3:
                addr = X564P_SYSCPLD3_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (X532P)) {
        /* cpld_index to cpld_addr */
        switch (cpld_index) {
            case BF_MAV_SYSCPLD1:
                addr = X532P_SYSCPLD1_I2C_ADDR;
                break;
            case BF_MAV_SYSCPLD2:
                addr = X532P_SYSCPLD2_I2C_ADDR;
                break;
            default:
                return -1;
        }
    } else if (platform_type_equal (HC)) {
        /* TBD */
    }

    MASTER_I2C_LOCK;
    if (!select_cpld (cpld_index)) {
        if (g_access_cpld_through_cp2112) {
            rc = bf_pltfm_cp2112_reg_write_byte (
                                    addr, offset, val);
        } else {
            rc = bf_pltfm_master_i2c_write_byte (
                                    addr, offset, val);
        }
        unselect_cpld();
    }
    MASTER_I2C_UNLOCK;

    return rc;
}

/** reset syscpld for a given platform
*
*  @param none
*  @return
*   0 on success otherwise in error
*/
int bf_pltfm_syscpld_reset()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    uint8_t offset = 0x2;
    int usec = 2 * 1000 * 1000;

    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val0);
    if (rc) {
        goto end;
    }
    bf_sys_usleep (usec);

    val = val0;
    if (platform_type_equal (X532P)) {
        /* offset = 2 and bit[2] to reset cpld2.
         * by tsihang, 2020-05-25 */
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 2);
    } else if (platform_type_equal (X564P)) {
        /* offset = 2 and bit[4], bit[5] to reset cpld2 and cpld3.
         * by tsihang, 2020-05-25 */
        offset = 0x2;
        /* reset cpld2 */
        val |= (1 << 4);
        /* reset cpld3 */
        val |= (1 << 5);

    } else if (platform_type_equal (HC)) {
        /* offset = 3 and bit[3], bit[4] to reset cpld2 and cpld3.
         * by tsihang, 2020-05-25 */
        offset = 0x3;
        /* reset cpld2 */
        val |= (1 << 3);
        /* reset cpld3 */
        val |= (1 << 4);
    } else if (platform_type_equal (X312P)) {
        offset = 0x0C;
        val = 0x5f;
        // CPLD3,CPLD4,CPLD5 reset timing=1000ms
    }

    rc = bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                    offset, val);
    if (rc) {
        goto end;
    }
    bf_sys_usleep (usec);

    /* read back for check and log. */
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val1);
    if (rc) {
        goto end;
    }

    /* there's no need to check for X312P as it will auto de-assert after 1000ms, so return here. */
    if (platform_type_equal (X312P)) {
        fprintf (stdout,
                 "CPLD3-5  RST(auto de-reset) : (0x%02x -> 0x%02x =? 0x%02x) : %s\n",
                 val0, val, val1, rc ? "failed" : "success");
        LOG_DEBUG ("CPLD3-5 RST(auto de-reset) : (0x%02x -> 0x%02x =? 0x%02x) : %s",
                   val0, val, val1, rc ? "failed" : "success");

        return 0;
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

    rc = bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                    offset, val);
    if (rc) {
        goto end;
    }

    bf_sys_usleep (usec);

    /* read back for check and log. */
    rc = bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                    offset, &val0);
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

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                14, &val0);
    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);

    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                14, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                14, &val1);
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

    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                14, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                14, &val0);
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

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val1);
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
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);
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

/* Reset PCA9548 for QSFP/SFP control.
 * CPLD1 offset = 2, and
 * bit[2] reset PCA9548 for Y33~Y48, C1~C8
 * bit[1] reset PCA9548 for Y1~Y32
 * by yanghuachen, 2022-02-25 */
int bf_pltfm_pca9548_reset_x308p()
{
    int rc = 0;
    uint8_t val = 0, val0 = 0, val1 = 0;
    int usec = 500;

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val0);

    val = val0;
    val |= (1 << 1);
    val |= (1 << 2);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val1);
    if (rc != 0) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-6)  +RST : (0x%02x -> 0x%02x)\n", val0,
             val1);
    LOG_DEBUG ("PCA9548(0-6) +RST : (0x%02x -> 0x%02x)",
               val0, val1);

    val = val1;
    val &= ~ (1 << 1);
    val &= ~ (1 << 2);
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD1,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD1,
                2, &val0);
    if (rc) {
        goto end;
    }

    fprintf (stdout,
             "PCA9548(0-6)  -RST : (0x%02x -> 0x%02x)\n", val1,
             val0);
    LOG_DEBUG ("PCA9548(0-6) -RST : (0x%02x -> 0x%02x)",
               val1, val0);

end:
    return rc;
}

int bf_pltfm_pca9548_reset_x312p()
{
    int rc = 0;
    uint8_t ret_value;

    // select to level1 pca9548 ch1, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 1);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    fprintf(stdout, "L1[1]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x74, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x74, 0x0, &ret_value);
    fprintf(stdout, "L1[1]: L2_0x74: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch2, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 2);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    fprintf(stdout, "L1[2]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    fprintf(stdout, "L1[2]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch3, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 3);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    fprintf(stdout, "L1[3]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    fprintf(stdout, "L1[3]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch4, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 4);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    fprintf(stdout, "L1[4]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x73, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x73, 0x0, &ret_value);
    fprintf(stdout, "L1[4]: L2_0x73: %02x\n", ret_value);
    bf_sys_usleep (500);

    // select to level1 pca9548 ch5, and unselect sub pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 1 << 5);
    bf_sys_usleep (500);
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L2_0x72, 0x0, 0x0);
    rc |= bf_pltfm_master_i2c_read_byte (
              X312P_PCA9548_L2_0x72, 0x0, &ret_value);
    fprintf(stdout, "L1[5]: L2_0x72: %02x\n", ret_value);
    bf_sys_usleep (500);

    // unselect level1 pca9548
    rc |= bf_pltfm_master_i2c_write_byte (
              X312P_PCA9548_L1_0x71, 0x0, 0x0);
    bf_sys_usleep (500);

    fprintf (stdout,
             "Reset all PCA9548 : %s!\n", rc ? "failed" : "success");
    LOG_DEBUG ("Reset all PCA9548 : %s!",
               rc ? "failed" : "success");

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

    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);

    val = val0;
    val |= (1 << 0);
    val |= (1 << 1);
    val |= (1 << 2);
    val |= (1 << 3);
    val |= (1 << 4);
    val |= (1 << 5);
    val |= (1 << 6);
    val |= (1 << 7);

    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val1);
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
    rc |= bf_pltfm_cpld_write_byte (BF_MAV_SYSCPLD2,
                2, val);
    bf_sys_usleep (usec);

    /* read back for log. */
    rc |= bf_pltfm_cpld_read_byte (BF_MAV_SYSCPLD2,
                2, &val0);
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

int bf_pltfm_syscpld_init()
{
    fprintf (stdout,
             "\n\n================== CPLDs INIT ==================\n");

    if (g_access_cpld_through_cp2112) {
        /* get cp2112 handler. */
        g_cpld_cp2112_hndl =
         bf_pltfm_cp2112_get_handle (CP2112_ID_2);
        BUG_ON (g_cpld_cp2112_hndl == NULL);
        fprintf (stdout, "The USB dev dev %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_device);
        fprintf (stdout, "The USB dev contex %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_context);
        fprintf (stdout, "The USB dev handle %p.\n",
                (void *)g_cpld_cp2112_hndl->usb_device_handle);
    }

    if (platform_type_equal (X564P)) {
        bf_pltfm_max_cplds = 3;
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_47();
        bf_pltfm_pca9548_reset_03();
    } else if (platform_type_equal (X532P)) {
        bf_pltfm_max_cplds = 2;
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_03();
    } else if (platform_type_equal (X308P)) {
        bf_pltfm_max_cplds = 2;
        bf_pltfm_pca9548_reset_x308p();
    } else if (platform_type_equal (HC)) {
        bf_pltfm_max_cplds = 3;
        bf_pltfm_syscpld_reset();
        bf_pltfm_pca9548_reset_hc();
    } else if (platform_type_equal (X312P)) {
        bf_pltfm_max_cplds = 5;
        /* reset cpld */
        bf_pltfm_syscpld_reset();
        /* reset PCA9548 */
        bf_pltfm_pca9548_reset_x312p();
    }

    return 0;
}

static ucli_status_t
bf_pltfm_cpld_ucli_ucli__read_cpld (
    ucli_context_t *uc)
{
    int i = 0;
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

    for (i = 0; i < cpld_page_size; i ++) {
        if (bf_pltfm_cpld_read_byte (cpld_index, i,
                                        &buf[i])) {
            break;
        }
    }
    /* Check total */
    if (i == cpld_page_size) {
        aim_printf (&uc->pvs, "\ncpld %d:\n",
                    cpld_index);
        hex_dump (uc, buf, cpld_page_size);

    }
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
