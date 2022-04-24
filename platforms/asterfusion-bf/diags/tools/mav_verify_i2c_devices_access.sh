#!/bin/bash
echo "Must setup the environment to run cp2112_util"
echo "Verifying Upper Board PCA 9548 Devices"
DEVICES="0xe0 0xe2 0xe4 0xe6 0xe8 0x64"

for dev in $DEVICES; do
  output=$(./cp2112_util read 1 $dev 1 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

echo "Verifying Upper Board PCA9535 Devices"
./cp2112_util write 1 0xe8 1 0x7F
DEVICES="0x40 0x42 0x44 0x46 0x48 0x4A"
for dev in $DEVICES; do
  output=$(./cp2112_util addr-read-unsafe 1 $dev 2 1 0 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

echo "Verifying Upper Board PCA9535 Channel-6 Devices"
DEVICES="0x70 0x7e 0xa0 0xb0 0xd6 0xde"
for dev in $DEVICES; do
  output=$(./cp2112_util read 1 $dev 2 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

#reset 0xe8 to its default setting
./cp2112_util write 1 0xe8 1 0x3F

echo "Verifying Lower Board PCA 9548 Devices"
DEVICES="0xe0 0xe2 0xe4 0xe6 0xe8 0xea 0x64"

for dev in $DEVICES; do
  output=$(./cp2112_util read 0 $dev 1 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

echo "Verifying Lower Board PCA9535 Devices"
./cp2112_util write 0 0xe8 1 0x7F
DEVICES="0x40 0x42 0x44 0x46 0x48 0x4A"
for dev in $DEVICES; do
  output=$(./cp2112_util addr-read-unsafe 0 $dev 2 1 0 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

echo "Verifying Lower Board PCA9535 Channel-6 Devices"
DEVICES="0xA0"
for dev in $DEVICES; do
  output=$(./cp2112_util read 0 $dev 2 >/dev/null 2>&1)
  ret=$?
  if [ $ret == 0 ]; then
    echo "$dev Ok"
  else
    echo "$dev not Ok"
  fi
done

#reset 0xe8 to its default setting
./cp2112_util write 0 0xe8 1 0x00


echo "Verifying Lower Board Repeater Devices"
DEVICES="0x30 0x36 0x48 0x4E"
CHN[1]="0"
CHN[2]="1"
CHN[3]="3"
CHN[4]="4"
CHN[5]="6"
CHN[6]="7"
CHN_MASK[1]="0x01"
CHN_MASK[2]="0x02"
CHN_MASK[3]="0x08"
CHN_MASK[4]="0x10"
CHN_MASK[5]="0x40"
CHN_MASK[6]="0x80"
for index in 1 2 3 4 5 6; do

  echo "Channel-${CHN[index]} Repeaters"
  ./cp2112_util write 0 0xea 1 ${CHN_MASK[index]}

  for dev in $DEVICES; do
    output=$(./cp2112_util read 0 $dev 1 >/dev/null 2>&1)
    ret=$?
    if [ $ret == 0 ]; then
      echo "chn ${CHN[index]} $dev Ok"
    else
      echo "chn ${CHN[index]} $dev not Ok"
    fi
  done
done

#reset 0xea to its default setting
./cp2112_util write 0 0xea 1 0x00
