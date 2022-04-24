ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS =
SUBDIRS += tcl_server
if !TCL_ONLY
if IS_THRIFT_ENABLED
SUBDIRS += thrift
endif
SUBDIRS += include src cp2112_util tofino_spi_util
if BUILD_ASTERFUSIONBF_DIAGS
SUBDIRS += diags
endif
else
SUBDIRS += src
endif
