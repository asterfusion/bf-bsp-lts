ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

SUBDIRS =
if IS_THRIFT_ENABLED
SUBDIRS += thrift
SUBDIRS += include src cp2112_util uart_util tofino_spi_util tofino_pci_util
else
SUBDIRS += src
endif
