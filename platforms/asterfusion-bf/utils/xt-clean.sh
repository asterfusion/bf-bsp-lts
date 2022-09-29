#! /bin/bash

source xt-setup.sh

echo "Uninstall SDE"
sde=`dpkg -l | grep sde- | awk '{print $2}'`
if [ "$sde"X != ""X ];then
	echo -e "${RED}Removing $sde${RES}"
	dpkg -r $sde
fi
unset sde

echo "Uninstall P4 compiler"
p4c=`dpkg -l | grep p4c | awk '{print $2}'`
if [ "$p4c"X != ""X ];then
	echo -e "${RED}Removing $p4c${RES}"
	dpkg -r $p4c
fi
unset p4c

echo "Uninstall P4 Insight"
p4i=`dpkg -l | grep p4i | awk '{print $2}'`
if [ "$p4i"X != ""X ];then
	echo -e "${RED}Removing $p4i${RES}"
	dpkg -r $p4i
fi
unset p4i

echo "Uninstall kdrv"
kdrv=`dpkg -l | grep kdrv | awk '{print $2}'`
if [ "$kdrv"X != ""X ];then
	echo -e "${RED}Removing $kdrv${RES}"
	dpkg -r $kdrv
fi
unset kdrv

echo "Uninstall i2cdrv NCT6779D"
i2cdrv=`dpkg -l | grep nct6779d | awk '{print $2}'`
if [ "$i2cdrv"X != ""X ];then
	echo -e "${RED}Removing $i2cdrv${RES}"
	dpkg -r $i2cdrv
fi
unset i2cdrv

echo "Uninstall i2cdrv CGOS"
i2cdrv=`dpkg -l | grep cgos | awk '{print $2}'`
if [ "$i2cdrv"X != ""X ];then
	echo -e "${RED}Removing $i2cdrv${RES}"
	dpkg -r $i2cdrv
fi
unset i2cdrv

echo "Uninstall BSP"
bsp=`dpkg -l | grep bsp | awk '{print $2}'`
if [ "$bsp"X != ""X ];then
	echo -e "${RED}Removing $bsp${RES}"
	dpkg -r $bsp
fi
unset bsp

echo "Uninstall thrift"
thrift=`dpkg -l | grep thrift | awk '{print $2}'`
if [ "$thrift"X != ""X ];then
	echo -e "${RED}Removing $thrift${RES}"
	dpkg -r $thrift
fi
unset thrift

echo "Uninstall protobuf"
protobuf=`dpkg -l | grep protobuf | awk '{print $2}'`
if [ "$protobuf"X != ""X ];then
	echo -e "${RED}Removing $protobuf${RES}"
	dpkg -r $protobuf
fi
unset protobuf

echo "Uninstall grpc"
grpc=`dpkg -l | grep grpc | awk '{print $2}'`
if [ "$grpc"X != ""X ];then
	echo -e "${RED}Removing $grpc${RES}"
	dpkg -r $grpc
fi
unset grpc

# deps will drop support and replace to 3 standalone debian packages:
# thrift
# grpc
# protobuf
# should libboost here too ?
# by tsihang, 2022-03-28.
echo "Uninstall deps"
deps=`dpkg -l | grep dependencies | awk '{print $2}'`
if [ "$deps"X != ""X ];then
	echo -e "${RED}Removing $deps${RES}"
	dpkg -r $deps
fi
unset deps

if [[ -d  $BFNSDK_INSTDIR ]];then
    echo -e "${RED}Removing $BFNSDK_INSTDIR${RES}"
    rm -rf $BFNSDK_INSTDIR
fi

if [[ -d  $LOG_DIR_PREFIX ]];then
    echo -e "${RED}Removing $LOG_DIR_PREFIX${RES}"
    rm -rf $LOG_DIR_PREFIX
fi

echo -e "${YELLOW}Done${RES}"

