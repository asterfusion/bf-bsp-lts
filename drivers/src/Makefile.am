libbd_cfg_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libqsfp_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libled_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
libpm_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI

libbd_cfg_la_SOURCES = \
bf_bd_cfg/bf_bd_cfg_intf.c \
bf_bd_cfg/bf_bd_cfg_intf_ucli.c \
bf_bd_cfg/bf_bd_cfg_porting.c

libqsfp_la_SOURCES = \
bf_qsfp/bf_qsfp_cli.c \
bf_qsfp/bf_qsfp_comm.c \
bf_qsfp/bf_sfp_ucli.c \
bf_qsfp/bf_sfp_comm.c \
bf_qsfp/map.c \
bf_qsfp/dom.c \
bf_qsfp/nonstandard.c \
bf_qsfp/sff.c \
bf_qsfp/sff_db.c \
bf_qsfp/sff_enums.c


libled_la_SOURCES = \
bf_led/bf_led.c

libpm_la_SOURCES = \
bf_port_mgmt/bf_pm_intf.c \
bf_port_mgmt/bf_pm_qsfp_mgmt.c \
bf_port_mgmt/bf_pm_sfp_mgmt.c \
bf_port_mgmt/bf_pm_priv.h \
bf_port_mgmt/bf_pm_porting.c

noinst_LTLIBRARIES = \
libbd_cfg.la \
libqsfp.la \
libled.la \
libpm.la

lib_LTLIBRARIES = \
libpltfm_driver.la

libpltfm_driver_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)

libpltfm_driver_la_SOURCES =

libpltfm_driver_la_LIBADD = \
libbd_cfg.la \
libqsfp.la \
libled.la \
libpm.la
