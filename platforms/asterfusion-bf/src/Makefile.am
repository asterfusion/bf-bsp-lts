AM_CPPFLAGS += -I$(srcdir)/../include

libasterfusionbf_platform_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI -lcgos
if IS_THRIFT_ENABLED
libasterfusionbf_platform_la_CFLAGS += -DTHRIFT_ENABLED
endif

libasterfusionbf_qsfp_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libasterfusionbf_cpld_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libasterfusionbf_cp2112_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libasterfusionbf_chss_mgmt_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI

if IS_ASIC
libasterfusionbf_chss_mgmt_la_CPPFLAGS = $(AM_CPPFLAGS) -DDEVICE_IS_ASIC
else
libasterfusionbf_chss_mgmt_la_CPPFLAGS = $(AM_CPPFLAGS) -DDEVICE_IS_SW_MODEL
endif

libasterfusionbf_platform_la_SOURCES = \
bf_pltfm_bmc_tty/bmc_tty.c \
platform_mgr/platform.c \
platform_mgr/platform_board.c \
platform_mgr/platform_health_mntr.c \
bf_pltfm_led/bf_pltfm_mav_led.c \
bf_pltfm_led/bf_pltfm_mav_led_x732q.c \
bf_pltfm_led/bf_pltfm_mav_led_x564p.c \
bf_pltfm_led/bf_pltfm_mav_led_x532p.c \
bf_pltfm_led/bf_pltfm_mav_led_x308p.c \
bf_pltfm_led/bf_pltfm_mav_led_x312p.c \
bf_pltfm_led/bf_pltfm_mav_led_hc36y24c.c \
bf_pltfm_led/lq.c \
bf_pltfm_led/bf_pltfm_mav_cpld_led.c \
bf_pltfm_master_i2c/bf_pltfm_master_i2c.c \
bf_pltfm_master_i2c/bf_pltfm_cgos_i2c.c \
bf_pltfm_spi/bf_pltfm_spi.c \
bf_pltfm_uart/bf_pltfm_uart.c

libasterfusionbf_qsfp_la_SOURCES = \
qsfp/bf_mav_qsfp_module.c \
qsfp/bf_mav_qsfp_sub_module.c \
qsfp/bf_mav_qsfp_sub_module_x312p.c \
qsfp/bf_mav_qsfp_sub_module_x308p.c \
qsfp/bf_mav_qsfp_sub_module_x532p.c \
qsfp/bf_mav_qsfp_sub_module_x564p.c \
qsfp/bf_mav_qsfp_sub_module_x732q.c \
qsfp/bf_mav_qsfp_sub_module_hc36y24c.c \
qsfp/bf_mav_sfp_module.c \
qsfp/bf_mav_sfp_module_x312p.c \
qsfp/bf_mav_sfp_module_x308p.c \
qsfp/bf_mav_sfp_module_x532p.c \
qsfp/bf_mav_sfp_module_x564p.c \
qsfp/bf_mav_sfp_module_x732q.c \
qsfp/bf_mav_sfp_module_hc36y24c.c


libasterfusionbf_cpld_la_SOURCES = \
bf_pltfm_cpld/bf_pltfm_cpld_ucli.c

libasterfusionbf_chss_mgmt_la_SOURCES = \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_intf.c \
bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.c \
bf_pltfm_chss_mgmt/bf_pltfm_bd_eeprom.h \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_tmp.c \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_vrail.c \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_ps.c \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_fan.c \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_mac.c \
bf_pltfm_chss_mgmt/bf_pltfm_chss_mgmt_ucli.c

libasterfusionbf_chss_mgmt_la_LIBADD = -lcurl

libasterfusionbf_cp2112_la_SOURCES = \
bf_pltfm_cp2112/bf_pltfm_cp2112_intf.c \
bf_pltfm_cp2112/bf_pltfm_cp2112_intf_ucli.c

libasterfusionbf_cp2112_la_LIBADD = -lusb-1.0 -lm


noinst_LTLIBRARIES = \
libasterfusionbf_platform.la \
libasterfusionbf_cp2112.la \
libasterfusionbf_chss_mgmt.la \
libasterfusionbf_qsfp.la \
libasterfusionbf_cpld.la

lib_LTLIBRARIES = \
libasterfusionbf_driver.la

libasterfusionbf_driver_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)

libasterfusionbf_driver_la_SOURCES =

libasterfusionbf_driver_la_LIBADD = \
libasterfusionbf_platform.la \
libasterfusionbf_cp2112.la \
libasterfusionbf_chss_mgmt.la \
libasterfusionbf_qsfp.la \
libasterfusionbf_cpld.la

if IS_THRIFT_ENABLED
libasterfusionbf_driver_la_LIBADD += $(builddir)/../thrift/libplatform_thrift.la
endif

bin_SCRIPTS = \
../utils/tofino_i2c_rd.sh \
../utils/tofino_i2c_wr.sh \
../utils/tofino_i2c_rd_local.sh \
../utils/tofino_i2c_wr_local.sh \
../utils/tofino_pci_bringup.sh \
../utils/tofino_spi_update.sh \
../utils/quick-start.sh \
../utils/xt-cfgen.sh \
../utils/xt-diags.sh \
../utils/xt-clean.sh \
../utils/xt-setup.sh \
../utils/bmc_tool.py \
../utils/firmwares/tofino_B0_spi_gen2_rev07.bin \
../utils/firmwares/tofino_B0_spi_gen2_rev09_corrected_220815.bin \
../utils/firmwares/rtl8168h-2.fw \
../utils/firmwares/credo_firmware.bin
