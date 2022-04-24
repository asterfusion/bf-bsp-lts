#ifndef _BF_CGOS_H_
#define _BF_CGOS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int HCGOS;

#define CGOSDLLAPI
#define CGOSAPI

#define cgosret_bool CGOSDLLAPI unsigned int CGOSAPI
#define cgosret_ulong CGOSDLLAPI unsigned int CGOSAPI

#define TRUE    1
#define FALSE   0

//
// I2C bus types returned by CgosI2CType()
//

#define CGOS_I2C_TYPE_UNKNOWN 0           // I2C bus for unknown or special purposes
#define CGOS_I2C_TYPE_PRIMARY 0x00010000  // primary I2C bus
#define CGOS_I2C_TYPE_SMB     0x00020000  // system management bus
#define CGOS_I2C_TYPE_DDC     0x00030000  // I2C bus of the DDC interface

/* from libcgos */
cgosret_bool CgosLibInitialize (void);
cgosret_bool CgosLibUninitialize (void);
cgosret_bool CgosLibInstall (unsigned int
                             install);

cgosret_bool CgosBoardOpen (unsigned int dwClass,
                            unsigned int dwNum, unsigned int dwFlags,
                            HCGOS *phCgos);
cgosret_bool CgosBoardClose (HCGOS hCgos);

cgosret_bool CgosLibSetLastErrorAddress (
    unsigned int *pErrNo); // 1.2
cgosret_ulong CgosLibGetLastError (void); // 1.2

cgosret_ulong CgosI2CCount (HCGOS hCgos);
cgosret_ulong CgosI2CType (HCGOS hCgos,
                           unsigned int dwUnit);
cgosret_bool CgosI2CIsAvailable (HCGOS hCgos,
                                 unsigned int dwUnit);
cgosret_bool CgosI2CRead (HCGOS hCgos,
                          unsigned int dwUnit, unsigned char bAddr,
                          unsigned char *pBytes, unsigned int dwLen);
cgosret_bool CgosI2CWrite (HCGOS hCgos,
                           unsigned int dwUnit, unsigned char bAddr,
                           unsigned char *pBytes, unsigned int dwLen);

cgosret_bool CgosI2CReadRegister (HCGOS hCgos,
                                  unsigned int dwUnit, unsigned char bAddr,
                                  unsigned short wReg, unsigned char *pDataByte);
cgosret_bool CgosI2CWriteReadCombined (
    HCGOS hCgos, unsigned int dwUnit,
    unsigned char bAddr, unsigned char *pBytesWrite,
    unsigned int dwLenWrite,
    unsigned char *pBytesRead,
    unsigned int dwLenRead);

cgosret_bool CgosI2CGetFrequency (HCGOS hCgos,
                                  unsigned int dwUnit,
                                  unsigned int *pdwSetting); // 1.3
cgosret_bool CgosI2CSetFrequency (HCGOS hCgos,
                                  unsigned int dwUnit,
                                  unsigned int dwSetting); // 1.3


/* pltfm def */
int bf_cgos_i2c_write_byte (uint8_t addr,
                            uint8_t offset, uint8_t value);
int bf_cgos_i2c_read_byte (uint8_t addr,
                           uint8_t offset, uint8_t *value);
int bf_cgos_i2c_write_block (uint8_t addr,
                             uint8_t offset, uint8_t *value, uint32_t wr_len);
int bf_cgos_i2c_read_block (uint8_t addr,
                            uint8_t offset, uint8_t *value, uint32_t rd_len);
int bf_cgos_i2c_bmc_read (uint8_t addr,
                          uint8_t cmd, uint8_t *sub_cmd, uint8_t sub_num,
                          uint8_t *buf, int usec);
int bf_cgos_i2c_bmc_write (uint8_t addr,
                           uint8_t cmd, uint8_t *sub_cmd, uint8_t sub_num);

unsigned short CgosOpen (unsigned char bLog);
unsigned short CgosClose (unsigned char bLog);
int bf_cgos_init();
int bf_cgos_de_init();


#ifdef __cplusplus
}
#endif

#endif // _BF_CGOS_H_
