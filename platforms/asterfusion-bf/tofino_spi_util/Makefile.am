bin_PROGRAMS = tofino_spi_util
tofino_spi_util_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)
tofino_spi_util_CPPFLAGS = -I$(includedir) -I$(top_srcdir)/drivers/include/ -I$(top_srcdir)/platforms/include -I$(top_srcdir)/platforms/asterfusion-bf/include/ -DDEVICE_IS_ASIC

tofino_spi_util_SOURCES = \
tofino_spi_if.c 
#../src/bf_pltfm_spi/bf_pltfm_spi.c

tofino_spi_util_LDFLAGS = $(AM_LDFLAGS) -L$(top_builddir)/platforms/asterfusion-bf/src/

# Different SDE on different OS.
# Default to OS <= 9 and SDE < 9.9.x. by tsihang, 2022-11-09.
# For SDE version <  9.9.x && OS <= 9
tofino_spi_util_LDADD = -lbfsys -lusb-1.0 -lcurl -lcgos
# For SDE version >= 9.9.x && OS <= 9
#tofino_spi_util_LDADD =  -ltarget_sys -ldriver -lusb-1.0 -lcurl -lcgos
# For SDE version <  9.9.x && OS >= 10
#tofino_spi_util_LDADD =  -lbfsys -ldriver -lusb-1.0 -lcurl -lcgos -li2c
# For SDE version >= 9.9.x && OS >= 10
#tofino_spi_util_LDADD =  -ltarget_sys -ldriver -lusb-1.0 -lcurl -lcgos -li2c

# For SDE version <  9.7.x
#tofino_spi_util_LDADD += -lbf_switchd_lib
