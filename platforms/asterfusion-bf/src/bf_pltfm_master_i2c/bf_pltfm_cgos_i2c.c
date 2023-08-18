#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include "bf_pltfm_cgos_i2c.h"

#include "bf_pltfm_types/bf_pltfm_types.h"


#define debug_dump_open 0
#define EEP_SIZE 128
#define REG_SIZE 256

HCGOS hCgos = 0;
uint32_t ulI2CBusCount = 0;
uint32_t ulPrimaryI2C = 0xFFFFFFFF;
unsigned int ulLastError = 0;

static bf_sys_rmutex_t cgos_i2c_lock;
#define CGOS_LOCK   \
    bf_sys_rmutex_lock(&cgos_i2c_lock)
#define CGOS_UNLOCK \
    bf_sys_rmutex_unlock(&cgos_i2c_lock)

unsigned short CgosOpen (unsigned char bLog)
{
    if (!CgosLibInitialize()) {
        if (!CgosLibInstall (1)) {
            LOG_DEBUG ("ERROR: Failed to install CGOS interface.");
            if (bLog) {
                LOG_DEBUG ("CgosLibInstall FAILED");
            }
            return FALSE;
        }
        if (bLog) {
            LOG_DEBUG ("CgosLibInstall SUCCESS");
        }
        if (!CgosLibInitialize()) {
            LOG_DEBUG ("ERROR: Failed to initialize CGOS interface.");
            if (bLog) {
                LOG_DEBUG ("CgosLibInitialize FAILED");
            }
            return FALSE;
        }

    }
    if (bLog) {
        LOG_DEBUG ("CgosLibInitialize SUCCESS");
    }

    if (!CgosBoardOpen (0, 0, 0, &hCgos)) {
        LOG_DEBUG ("ERROR: Failed to open board interface.");
        if (bLog) {
            LOG_DEBUG ("CgosBoardOpen FAILED");
        }
        return FALSE;
    }
    if (bLog) {
        LOG_DEBUG ("CgosBoardOpen SUCCESS");
    }

    if (CgosLibSetLastErrorAddress (&ulLastError)) {
        if (CgosLibGetLastError() == ulLastError) {
            if (bLog) {
                LOG_DEBUG ("CgosLibGetLastError SUCCESS");
            }
        } else {
            if (bLog) {
                LOG_DEBUG ("CgosLibGetLastError FAILED");
            }
        }
        if (bLog) {
            LOG_DEBUG ("CgosLibSetLastErrorAddress SUCCESS");
        }
    } else {
        if (bLog) {
            LOG_DEBUG ("CgosLibSetLastErrorAddress FAILED");
        }
    }
    return TRUE;
}

unsigned short CgosClose (unsigned char bLog)
{
    if (hCgos) {
        if (!CgosBoardClose (hCgos)) {
            LOG_DEBUG ("ERROR: Failed to close CGOS interface.");
            if (bLog) {
                LOG_DEBUG ("CgosBoardClose FAILED");
            }
        } else {
            if (bLog) {
                LOG_DEBUG ("CgosBoardClose SUCCESS");
            }
        }
        hCgos = 0;
    }

    if (!CgosLibUninitialize()) {
        LOG_DEBUG ("ERROR: Failed to un-initialize CGOS interface.");
        if (bLog) {
            LOG_DEBUG ("CgosLibUninitialize FAILED");
        }
    } else {
        if (bLog) {
            LOG_DEBUG ("CgosLibUninitialize SUCCESS");
        }
    }
    return TRUE;
}

#if debug_dump_open
static void hex_dump (uint8_t *buf,
                      uint32_t len)
{
    uint8_t byte;

    for (byte = 0; byte < len; byte++) {
        if ((byte % 16) == 0) {
            fprintf (stdout, "\n%3d : ", byte);
        }
        fprintf (stdout, "%02x ", buf[byte]);
    }
    fprintf (stdout,  "\n");
}

#endif

int bf_cgos_i2c_write_byte (uint8_t addr,
                            uint8_t offset, uint8_t value)
{
    /* lock */
    CGOS_LOCK;

    /* left shift for cgos */
    addr = addr << 1;
    /* starts from 1 */
    uint8_t buf[REG_SIZE + 1];
    memset (buf, 0, REG_SIZE + 1);

    if (addr == 0x40 << 1) {
        /* read first */
        if (!CgosI2CRead (hCgos, ulPrimaryI2C,
                          addr | 0x01, buf + 1, REG_SIZE)) {
            LOG_ERROR ("cgoslx i2c: Failed to read before write.");
            CGOS_UNLOCK;
            return -1;
        }
#if debug_dump_open
        fprintf (stdout, "\n%02x:\n", addr);
        hex_dump (buf + 1, 256);
#endif

        /* change value */
        buf[offset + 1] = value;

        /* write back */
        if (!CgosI2CWrite (hCgos, ulPrimaryI2C, addr, buf,
                           REG_SIZE + 1)) {
            LOG_ERROR ("cgoslx i2c: Failed to write.");
            CGOS_UNLOCK;
            return -2;
        }
    } else {
        buf[0] = offset;
        buf[1] = value;
        if (!CgosI2CWrite (hCgos, ulPrimaryI2C, addr, buf,
                           1 + 1)) {
            LOG_ERROR ("cgoslx i2c: Failed to write.");
            CGOS_UNLOCK;
            return -2;
        }
    }

    /* unlock */
    CGOS_UNLOCK;
    return 0;
}

int bf_cgos_i2c_write_block (uint8_t addr,
                             uint8_t offset, uint8_t *value, uint32_t wr_len)
{
    /* lock */
    CGOS_LOCK;

    /* left shift for cgos */
    addr = addr << 1;
    /* starts from 1 */
    uint8_t buf[REG_SIZE + 1];
    memset (buf, 0, REG_SIZE + 1);

    buf[0] = offset;
    memcpy (&buf[1], value, wr_len);
    if (!CgosI2CWrite (hCgos, ulPrimaryI2C, addr, buf,
                       wr_len + 1)) {
        LOG_ERROR ("cgoslx i2c: Failed to write.");
        CGOS_UNLOCK;
        return -1;
    }

    /* unlock */
    CGOS_UNLOCK;
    return 0;
}

/* Read REG_SIZE bytes one time and return one byte specified by @offset. */
int bf_cgos_i2c_read_byte (uint8_t addr,
                           uint8_t offset, uint8_t *value)
{
    CGOS_LOCK;
    int rc;
    addr = addr << 1;

    if (CgosI2CReadRegister (hCgos, ulPrimaryI2C,
                             addr | 0x01,
                             offset, value)) {
        rc = 0;
    } else {
        LOG_ERROR ("cgoslx i2c: Failed to read addr %x offset %d.",
                   addr, offset);
        rc = -1;
    }

    CGOS_UNLOCK;
    return rc;
}

int bf_cgos_i2c_read_block (uint8_t addr,
                            uint8_t offset, uint8_t *value, uint32_t rd_len)
{
    if (!value) {
        return -1;
    }
    addr = addr << 1;
    CGOS_LOCK;
    uint32_t i;

    for (i = 0; i < rd_len; i++) {
        if (!CgosI2CReadRegister (hCgos, ulPrimaryI2C,
                                  addr | 0x01, offset + i, value + i)) {
            LOG_WARNING ("cgoslx i2c: Failed to read addr %x offset %d.",
                       addr, offset);
            CGOS_UNLOCK;
            return -1;
        }
    }
    CGOS_UNLOCK;
    return 0;
}

int bf_cgos_i2c_write_read (uint8_t addr,
                            uint8_t offset, uint8_t *value, uint32_t rd_len)
{
    if (!value) {
        return -1;
    }
    addr = addr << 1;
    CGOS_LOCK;
    uint32_t i;

    for (i = 0; i < rd_len; i++) {
        if (!CgosI2CReadRegister (hCgos, ulPrimaryI2C,
                                  addr | 0x01, offset + i, value + offset + i)) {
            LOG_ERROR ("cgoslx i2c: Failed to read addr %x offset %d.",
                       addr, offset);
            CGOS_UNLOCK;
            return -1;
        }
    }
    CGOS_UNLOCK;
    return 0;
}

// this func returns read_len if read success, -1 when fail
int bf_cgos_i2c_bmc_read (uint8_t addr,
                          uint8_t cmd, uint8_t *sub_cmd, uint8_t sub_num,
                          uint8_t *buf, int usec)
{
    uint8_t wr_buf[128];
    uint8_t rd_buf[128];

    addr = addr << 1;
    wr_buf[0] = cmd;
    wr_buf[1] = sub_num;
    memcpy (&wr_buf[2], sub_cmd, sub_num);

    CGOS_LOCK;

    if (!CgosI2CWrite (hCgos, ulPrimaryI2C, addr,
                       wr_buf, sub_num + 2)) {
        LOG_ERROR ("ERROR: cgos i2c write failed");
        CGOS_UNLOCK;
        return -1;
    }

    usleep (usec);

    wr_buf[0] = 0xFF;
    if (!CgosI2CWriteReadCombined (hCgos,
                                   ulPrimaryI2C, addr, wr_buf, 1, rd_buf, 50)) {
        LOG_ERROR ("ERROR: cgos i2c write read failed");
        CGOS_UNLOCK;
        return -1;
    }

    CGOS_UNLOCK;

    memcpy (buf, &rd_buf[1], rd_buf[0]);
    return rd_buf[0];
}

int bf_cgos_i2c_bmc_write (uint8_t addr,
                           uint8_t cmd, uint8_t *sub_cmd, uint8_t sub_num)
{
    uint8_t wr_buf[128];

    addr = addr << 1;
    wr_buf[0] = cmd;
    wr_buf[1] = sub_num;
    memcpy (&wr_buf[2], sub_cmd, sub_num);

    CGOS_LOCK;

    if (!CgosI2CWrite (hCgos, ulPrimaryI2C, addr,
                       wr_buf, sub_num + 2)) {
        LOG_ERROR ("ERROR: cgos i2c write failed");
        CGOS_UNLOCK;
        return -1;
    }

    CGOS_UNLOCK;

    return 0;
}


int bf_cgos_init()
{
    uint32_t ulUnit;
    uint32_t ulI2CBusType;
    uint32_t ulCurrentFreq;
    uint32_t ulMaxFreq;
    ulMaxFreq = 50000;

    /* init lock */
    /* thread lock init */
    if (bf_sys_rmutex_init (&cgos_i2c_lock) != 0) {
        LOG_ERROR ("cgoslx i2c: Failed to init lock.");
        return -1;
    }
    /* open */
    if (!CgosOpen (1)) {
        LOG_ERROR ("cgoslx i2c: failed to open cgoslx.");
        return -1;
    }
    /* Get number of available I2C busses */
    if ((ulI2CBusCount = CgosI2CCount (hCgos)) != 0) {
        LOG_DEBUG ("cgoslx i2c: Success get number of available I2C busses: %u",
                   ulI2CBusCount);
    } else {
        LOG_ERROR ("cgoslx i2c: Failed to get number of I2C busses.");
        return -1;
    }

    /* Check type and availability */
    for (ulUnit = 0; ulUnit < ulI2CBusCount;
         ulUnit++) {
        ulI2CBusType = CgosI2CType (hCgos, ulUnit);
        LOG_DEBUG ("unit number : %u , I2C bus type: %08Xh",
                   ulUnit, ulI2CBusType);
        /* choose PrimaryI2C */
        if (ulI2CBusType == CGOS_I2C_TYPE_PRIMARY) {
            ulPrimaryI2C = ulUnit;
        }
        if (CgosI2CIsAvailable (hCgos, ulI2CBusType)) {
        }
    }
    /* set to MaxFerq */
    if (CgosI2CGetFrequency (hCgos, ulPrimaryI2C,
                             &ulCurrentFreq)) {
        if (ulCurrentFreq != ulMaxFreq) {
            if (CgosI2CSetFrequency (hCgos, ulPrimaryI2C,
                                     ulMaxFreq)) {
                if (CgosI2CGetFrequency (hCgos, ulPrimaryI2C,
                                         &ulCurrentFreq)) {
                    if (ulCurrentFreq != ulMaxFreq) {
                        LOG_ERROR ("cgoslx i2c: Failed to set frequency of primary I2C bus to %d",
                                   ulMaxFreq);
                        return -1;
                    } else {
                        LOG_DEBUG ("cgoslx i2c: Success to set frequency of primary I2C bus from %d to %d",
                                   ulCurrentFreq, ulMaxFreq);
                    }
                } else {
                    LOG_ERROR ("cgoslx i2c: Failed to get frequency of primary I2C bus");
                    return -1;
                }
            } else {
                LOG_ERROR ("cgoslx i2c: Failed to set frequency of primary I2C bus to %d",
                           ulMaxFreq);
                return -1;
            }
        } else {
            LOG_DEBUG ("cgoslx i2c: Success to read frequency of primary I2C bus: %d, no need to change",
                       ulCurrentFreq);
        }
    } else {
        LOG_ERROR ("cgoslx i2c: Failed to get frequency of primary I2C bus");
        return -1;
    }
    return 0;
}

int bf_cgos_de_init()
{
    if (!CgosClose (1)) {
        LOG_ERROR ("cgoslx i2c: failed to open cgoslx.");
        return -1;
    }
    bf_sys_rmutex_del (&cgos_i2c_lock);
    return 0;
}

