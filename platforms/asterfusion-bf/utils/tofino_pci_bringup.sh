#!/bin/bash

# This is a shell script that checks if Tofino is enumerated as Gen2/Gen3-x4.
# The script removes the device from PCI bus, resets it and
# reenumerates the bus upto 5 times, if not.
# The script uses hardcoded pci location bus/fun/dev for the ASIC.
# The procedure to reset the ASIC is specific to Barefoot switches.

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

echo -e "${YELLOW}Notice: SLOT_NAME : $slot_name"
if [ "$slot_name"X = ""X ]; then
    echo -e "${YELLOW}Warning: No ASIC found.${RES}"
    friendly_exit
fi

if [ $1 = "X312P-T" ]; then
    echo -e "${YELLOW}Warning: Not support PCIe reset from cpld.${RES}"
    friendly_exit
fi

echo -e "${YELLOW}Notice: This is a shell script that checks whether Tofino is enumerated as Gen2/Gen3-x4 or not.${RES}"
echo -e "${YELLOW}        If not the script removes the device from PCI bus, resets it and reenumerates the bus upto 5 times.${RES}"

for i in 1 2 3 4 5
do
# check if Tofino came up as x4
    output=$(sudo setpci -s $slot_name 92.b)
    if [[ $output == "43" ]]
    then
        echo -e "${YELLOW}${BLINK}PCI device $slot_name enumerated as Gen3/x4.${RES}${RES}"
        friendly_exit
    fi
    if [[ $output == "42" ]]
    then
        echo -e "${YELLOW}${BLINK}PCI device $slot_name enumerated as Gen2/x4.${RES}${RES}"
        friendly_exit
    fi

    # Now remove the device from pci, reset the device and rescan
    echo "PCI DEVICE REMOVE"
    sudo sh -c "echo 1 > /sys/bus/pci/devices/0000\:$slot_name/remove"
    sleep 1

    #
    # Mainly reset routine on a given platform.
    #
    #/* Write 0x9 to offset 0x32 on device 0x64 */
    #/* ASIC in PCIE reset and power on reset (if implemented by CPLD) */
    echo "WRITE 0x9"
    #cp2112_util 0 read 0 0x40 1 0x00
    sleep 1
    #/* remove power-on-reset of ASIC */
    echo "WRITE 0xB"
    #./cp2112_util $maverick write $cp2112_id 0x64 2 0x32 0xB
    sleep 1
    #/* remove pci-reset of ASIC */
    echo "WRITE 0xF"
    #./cp2112_util $maverick write $cp2112_id 0x64 2 0x32 0xF
    sleep 1

    echo "PCI RESCAN"
    sudo sh -c "echo 1 > /sys/bus/pci/rescan"
    sleep 2
done

echo -e "${RED}Fatal: Failed to bring up the device in x4.${RES}" >&2
friendly_exit
