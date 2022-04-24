lib_LTLIBRARIES = libasterfusion_mav_diags.la

AM_CPPFLAGS += -I$(includedir) -I$(srcdir)/../../include

libasterfusion_mav_diags_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS)
libasterfusion_mav_diags_la_CPPFLAGS = -I$(top_builddir)/p4-build $(AM_CPPFLAGS)
if IS_ASIC
libasterfusion_mav_diags_la_CPPFLAGS += -DDEVICE_IS_ASIC
else
libasterfusion_mav_diags_la_CPPFLAGS += -DDEVICE_IS_SW_MODEL
endif

libasterfusion_mav_diags_la_SOURCES = \
diag_server.c \
diag_handler.c \
diag_ucli.c

libasterfusion_mav_diags_la_LIBADD =

bin_SCRIPTS = \
swutil

SUBDIRS = targets
