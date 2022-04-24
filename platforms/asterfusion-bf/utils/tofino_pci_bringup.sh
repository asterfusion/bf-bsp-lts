#!/bin/bash

# This is a shell script that checks if Tofino is enumerated as Gen2/Gen3-x4.
# The script removes the device from PCI bus, resets it and
# reenumerates the bus upto 5 times, if not.
# The script uses hardcoded pci location bus/fun/dev for the ASIC.
# The procedure to reset the ASIC is specific to Barefoot switches.

usage() {
  echo "$0 <Mavericks or Montara>"
}

if [ $# -ne 1 ]
then
  usage
  exit 1
fi

if [ $1 = "Mavericks" ]; then
  cp2112_id=0
  mavericks=1
elif [ $1 = "Montara" ]; then
  cp2112_id=1
  mavericks=0
else
  usage
  exit 1
fi


for i in 1 2 3 4 5
do
  # check if Tofino came up as x4
  output=$(sudo setpci -s 05:00.0 92.b)
  echo $
  if [[ $output == "43" ]]
  then
    echo "PCI device enumerated as Gen3/x4"
    exit 0
  fi
  if [[ $output == "42" ]]
  then
    echo "PCI device enumerated as Gen2/x4"
    exit 0
    break
  fi
  # Now remove the device from pci, reset the device and rescan
  echo "PCI DEVICE REMOVE"
  sudo sh -c "echo 1 > /sys/bus/pci/devices/0000\:05\:00.0/remove"
  sleep 1
  #/* Write 0x9 to offset 0x32 on device 0x64 */
  #/* ASIC in PCIE reset and power on reset (if implemented by CPLD) */
  echo "WRITE 0x9"
  ./cp2112_util $mavericks write $cp2112_id 0x64 2 0x32 0x9
  sleep 1
  #/* remove power-on-reset of ASIC */
  echo "WRITE 0xB"
  ./cp2112_util $maverick write $cp2112_id 0x64 2 0x32 0xB
  sleep 1
  #/* remove pci-reset of ASIC */
  echo "WRITE 0xF"
  ./cp2112_util $maverick write $cp2112_id 0x64 2 0x32 0xF
  sleep 1
  echo "PCI RESCAN"
  sudo sh -c "echo 1 > /sys/bus/pci/rescan"
  sleep 2
done

echo "Failed to bring up the device in x4" >&2
exit 1

