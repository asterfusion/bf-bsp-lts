bin_PROGRAMS = uart_util
uart_util_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)
uart_util_CPPFLAGS = -I$(includedir) -I$(top_srcdir)/drivers/include/ -I$(top_srcdir)/platforms/include -I$(top_srcdir)/platforms/asterfusion-bf/include/ -DDEVICE_IS_ASIC

uart_util_SOURCES = \
uart_util.c \
../src/bf_pltfm_uart/bf_pltfm_uart.c

uart_util_LDFLAGS = $(AM_LDFLAGS) -L$(top_builddir)/platforms/asterfusion-bf/src/

uart_util_LDADD = -lbfsys
