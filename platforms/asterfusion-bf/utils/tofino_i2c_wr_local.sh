#!/bin/sh
# Copyright (c) 2015-2020 Barefoot Networks, Inc.
# SPDX-License-Identifier: BSD-3-Clause
#
# $Id: $
#
usage() {
    echo "Usage: $0 <register offset bits7-0> <bits 15-8> <bits 23-16> <bits<bar 31-28 | bits 27-24> <data offset 7-0> <data 15-8> <data 23-16> <data 31-24>" >&2
}

./cp2112_util 0 write 1 0xe8 1 0x40
sleep 1

if [ "$#" -lt 8 ]; then
    usage
    exit 1
fi

i2c_addr=0xb0

addr_1=$1
addr_2=$2
addr_3=$3
addr_4=$4
data_1=$5
data_2=$6
data_3=$7
data_4=$8

#fill up S_ADDR
./cp2112_util 0 write 1 $i2c_addr 2 0 $addr_1
./cp2112_util 0 write 1 $i2c_addr 2 1 $addr_2
./cp2112_util 0 write 1 $i2c_addr 2 2 $addr_3
./cp2112_util 0 write 1 $i2c_addr 2 3 $addr_4

#fill up S_DATA
./cp2112_util 0 write 1 $i2c_addr 2 4 $data_1
./cp2112_util 0 write 1 $i2c_addr 2 5 $data_2
./cp2112_util 0 write 1 $i2c_addr 2 6 $data_3
./cp2112_util 0 write 1 $i2c_addr 2 7 $data_4

#trigger write execute
./cp2112_util 0 write 1 $i2c_addr 2 8 1 

sleep 1

echo "reading status of write"
cmd=0x28
./cp2112_util 0 addr-read-unsafe 1 $i2c_addr 1 1 $cmd
