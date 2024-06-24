#!/bin/bash

# Building Environment Checker for X-T Platforms.
# by Hang Tsi (tsihang@asterfusion.com) 2024-01-24

BLINK='\033[05m'
RED='\E[1;31m'
GREEN='\E[1;32m'
YELLOW='\E[1;33m'
BLUE='\E[1;34m'
PINK='\E[1;35m'
RES='\E[0m'

friendly_exit() {
    echo ""
    read -n1 -p "Press any key to exit."
    echo
    exit 0
}

if [ $SDE_INSTALL ];then
    if [[ ! -d $SDE_INSTALL ]];then
        echo  -e "${RED}$SDE_INSTALL: No such file or directory.${RES}"
        friendly_exit
    fi
    echo -e "${GREEN}$SDE_INSTALL${RES}"
    echo -e "${GREEN}$LD_LIBRARY_PATH${RES}"
    rm -rf $SDE_INSTALL/include/bf_led/       \
           $SDE_INSTALL/include/bf_qsfp/      \
           $SDE_INSTALL/include/bf_port_mgmt/ \
           $SDE_INSTALL/include/bf_bd_cfg/    \
           $SDE_INSTALL/include/bf_pltfm*     \
           > /dev/null 2>&1

    echo "Copying bd-map json for tof2 based platforms ..."
    mkdir -p $SDE_INSTALL/share/platforms/board-maps/asterfusion/ \
           > /dev/null 2>&1
    cp ./platforms/asterfusion-bf/src/platform_mgr/pltfm_bd_map_*.json \
       $SDE_INSTALL/share/platforms/board-maps/asterfusion/ \
           > /dev/null 2>&1
else
    echo -e "${RED}SDE_INSTALL: unset${RES}"
    friendly_exit
fi

if [[ ! -f  $SDE_INSTALL/lib/libbfsys.so ]];then
    cd $SDE_INSTALL/lib
    ln -s libtarget_sys.so libbfsys.so
    cd -
fi

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    # Added by tsihang, 2021-06-30
    chmod u+x ./gitver.sh
    ./gitver.sh platforms/asterfusion-bf/include/ version.h
else
    # No git/svn/.git detected, generate platforms/asterfusion-bf/include/version.h
    echo -e "${GREEN}Generating platform/asterfusion-bf/include/version.h${RES}"
    cat <<EOF >platforms/asterfusion-bf/include/version.h
#ifndef VC_VERSION_H
#define VC_VERSION_H

#define VERSION_NUMBER "Git: custom bf-bsp-lts builder @ `date`"

#endif
EOF
fi

#exec autoreconf -fi -v

