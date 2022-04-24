bin_PROGRAMS = spi_i2c_util
spi_i2c_util_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)
spi_i2c_util_CPPFLAGS = -I$(includedir) -I$(top_srcdir)/drivers/include/ -I$(top_srcdir)/platforms/include -I$(top_srcdir)/platforms/asterfusion-bf/include/ -DDEVICE_IS_ASIC

spi_i2c_util_SOURCES = \
tofino_spi_if.c \
tofino_porting_spi.c \
../src/bf_pltfm_bmc_tty/bmc_tty.c \
../src/bf_pltfm_cp2112/bf_pltfm_cp2112_intf.c \
../src/bf_pltfm_master_i2c/bf_pltfm_master_i2c.c \
../src/bf_pltfm_uart/bf_pltfm_uart.c \
../src/bf_pltfm_master_i2c/bf_pltfm_cgos_i2c.c \
../../../drivers/src/bf_bd_cfg/bf_bd_cfg_intf.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_intf.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.h

spi_i2c_util_LDFLAGS = $(AM_LDFLAGS) -L$(top_builddir)/platforms/asterfusion-bf/src/

spi_i2c_util_LDADD = -lbfsys -lusb-1.0 -lcurl -lcgos
