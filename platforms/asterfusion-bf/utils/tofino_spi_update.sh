#!/bin/bash

# This is a shell script that helps to update SPI ROM Image for PCIe Gen2 CPU Link.
# PCIe will only advertise Gen2 capable.
# by tsihang, 2022-11-16.

# by tsihang, 2022-10-31.
source xt-setup.sh
slot_name=`lspci -d 1d1c: | awk '{print $1}'`

usage() {
    echo "$0 <X523P-T/X564P-T/X312P-T/X308P-T>"
}

if [ $# -ne 1 ]
then
    usage
    friendly_exit
fi

if [ $1 != "X532P-T" ] && [ $1 != "X564P-T" ] && [ $1 != "X312P-T" ] && [ $1 != "X308P-T" ]; then
    usage
    friendly_exit
fi

index=0
tofino_spi_util 0 up $index $SDE_INSTALL/bin/tofino_B0_spi_gen2_rev09_corrected_220815.bin

friendly_exit
