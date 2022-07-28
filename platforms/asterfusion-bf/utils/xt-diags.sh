#!/bin/bash

LOG_DIR_PREFIX="/var/asterfusion"
log="$LOG_DIR_PREFIX/diagnose-report.log"

if [[ -f  $log ]];then
    rm -rf $log
fi

if [[ ! -d  $LOG_DIR_PREFIX ]];then
    mkdir $LOG_DIR_PREFIX
fi

echo "==============================================================================================================================================" | tee -a $log
echo "1. Platform OS" | tee -a $log
echo ""  | tee -a $log

uname -a  | tee -a $log
disinfo=`fdisk -l | grep "Disk identifier"`
echo $disinfo | tee -a $log
disinfo=`fdisk -l | grep "Disk /dev/sda"`
echo $disinfo | tee -a $log
meminfo=`cat /proc/meminfo  | grep MemTotal`
echo $meminfo | tee -a $log
cpuinfo=`lscpu | grep "Model name:"`
echo $cpuinfo | tee -a $log

echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "2. The list of installed packages" | tee -a $log
echo "" | tee -a $log
dpkg -l | grep kdrv | tee -a $log
dpkg -l | grep sde | tee -a $log
dpkg -l | grep p4c | tee -a $log
dpkg -l | grep bsp | tee -a $log
dpkg -l | grep cgos | tee -a $log
dpkg -l | grep nct6779d | tee -a $log
dpkg -l | grep dependencies | tee -a $log
dpkg -l | grep thrift | tee -a $log
dpkg -l | grep grpc | tee -a $log
dpkg -l | grep protobuf | tee -a $log
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "3. /etc/platform.conf" | tee -a $log
echo "" | tee -a $log
if [[ -f /etc/platform.conf ]];then
    echo "`awk '{if(!NF || /^#/){next}}1' /etc/platform.conf`" | tee -a $log
else
    echo "No /etc/platform.conf detected" | tee -a $log
fi
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "4. i2cdetect" | tee -a $log
echo "" | tee -a $log
i2cdetect -l | tee -a $log
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "5. eeprom" | tee -a $log
echo "" | tee -a $log
if [[ -f $LOG_DIR_PREFIX/eeprom ]];then
    cat $LOG_DIR_PREFIX/eeprom | tee -a $log
else
    echo "No $LOG_DIR_PREFIX/eeprom detected" | tee -a $log
fi


echo "" | tee -a $log

