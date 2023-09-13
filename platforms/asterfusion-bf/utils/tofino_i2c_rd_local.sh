#!/bin/sh
# Copyright (c) 2015-2020 Barefoot Networks, Inc.
# SPDX-License-Identifier: BSD-3-Clause
#
# $Id: $
#
usage() {
  echo "Usage: $0 <register offset bits7-0> <bits 15-8> <bits 23-16> <bits<bar 31-28 | bits 27-24>" >&2
}

./cp2112_util 0 write 1 0xe8 1 0x40
sleep 1

if [ "$#" -lt 4 ]; then
    usage
    exit 1
fi

i2c_addr=0xb0

addr_1=$1
addr_2=$2
addr_3=$3
addr_4=$4

#fill up S_ADDR
./cp2112_util 0 write 1 $i2c_addr 2 0 $addr_1
./cp2112_util 0 write 1 $i2c_addr 2 1 $addr_2
./cp2112_util 0 write 1 $i2c_addr 2 2 $addr_3
./cp2112_util 0 write 1 $i2c_addr 2 3 $addr_4

#read and execute
cmd=0x08
data_1=0x21
./cp2112_util 0 write 1 $i2c_addr 2 $cmd $data_1

sleep 1

echo "reading status of write"
cmd=0x28
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd

#read the data back
cmd=0x24
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd

cmd=0x25
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd

cmd=0x26
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd

cmd=0x27
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd

