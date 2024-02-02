ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS =
SUBDIRS += platforms
SUBDIRS += drivers

libpltfm_mgr_la_CFLAGS = $(AM_CFLAGS) $(BFPLATFORMS_CFLAGS) -DINC_PLTFM_UCLI
if IS_THRIFT_ENABLED
libpltfm_mgr_la_CFLAGS += -DTHRIFT_ENABLED
endif

if IS_ASIC
libpltfm_mgr_la_CPPFLAGS = $(AM_CPPFLAGS) -DDEVICE_IS_ASIC
else
libpltfm_mgr_la_CPPFLAGS = $(AM_CPPFLAGS) -DDEVICE_IS_SW_MODEL
endif

if IS_STATIC_LINKED_LIB
libpltfm_mgr_la_CPPFLAGS += -DSTATIC_LINK_LIB
endif

libpltfm_mgr_la_SOURCES = \
drivers/src/bf_pltfm_mgr/pltfm_mgr_init.c \
drivers/src/bf_pltfm_mgr/pltfm_mgr_handlers.c


libpltfm_mgr_la_LIBADD =
if BUILD_ASTERFUSIONBF_PLAT
libpltfm_mgr_la_LIBADD += $(builddir)/platforms/asterfusion-bf/src/libasterfusionbf_driver.la
endif

libpltfm_mgr_la_LIBADD += $(builddir)/drivers/src/libpltfm_driver.la

lib_LTLIBRARIES = libpltfm_mgr.la
