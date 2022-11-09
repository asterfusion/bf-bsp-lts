bin_PROGRAMS = uart_util
uart_util_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)
uart_util_CPPFLAGS = -I$(includedir) -I$(top_srcdir)/drivers/include/ -I$(top_srcdir)/platforms/include -I$(top_srcdir)/platforms/asterfusion-bf/include/ -DDEVICE_IS_ASIC

uart_util_SOURCES = \
uart_util.c \
../src/bf_pltfm_uart/bf_pltfm_uart.c \
../src/bf_pltfm_cp2112/bf_pltfm_cp2112_intf.c \
../src/bf_pltfm_master_i2c/bf_pltfm_master_i2c.c \
../src/bf_pltfm_master_i2c/bf_pltfm_cgos_i2c.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_intf.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.c \
../src/bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.h

uart_util_LDFLAGS = $(AM_LDFLAGS) -L$(top_builddir)/platforms/asterfusion-bf/src/

uart_util_LDADD = -lbfsys -lusb-1.0 -lcurl -lcgos
