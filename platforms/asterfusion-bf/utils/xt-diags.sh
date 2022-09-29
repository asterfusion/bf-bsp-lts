#!/bin/bash

source xt-setup.sh
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
dpkg -l | grep sde- | tee -a $log
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
echo "3. $cfgfile" | tee -a $log
echo "" | tee -a $log
if [[ -f $cfgfile ]];then
    echo "`awk '{if(!NF || /^#/){next}}1' $cfgfile`" | tee -a $log
else
    echo "No $cfgfile detected" | tee -a $log
fi
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "4. i2cdetect" | tee -a $log
echo "" | tee -a $log
i2cdetect -l | tee -a $log
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "5. Check the ASIC" | tee -a $log
echo "" | tee -a $log
lspci | grep 1d1c | tee -a $log
echo "" | tee -a $log
echo "SDE=$SDE" | tee -a $log
echo "SDE_INSTALL=$SDE_INSTALL" | tee -a $log
echo "PATH=$PATH" | tee -a $log
echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH" | tee -a $log
echo "" | tee -a $log
ps -ef | grep switchd | tee -a $log
echo "" | tee -a $log

echo "==============================================================================================================================================" | tee -a $log
echo "6. eeprom" | tee -a $log
echo "" | tee -a $log
if [[ -f $LOG_DIR_PREFIX/eeprom ]];then
    cat $LOG_DIR_PREFIX/eeprom | tee -a $log
else
    echo "No $LOG_DIR_PREFIX/eeprom detected" | tee -a $log
fi


echo "" | tee -a $log
echo -e "${YELLOW}Done${RES}"

