#!/bin/bash

BLINK='\033[05m'
RED='\E[1;31m'
GREEN='\E[1;32m'
YELLOW='\E[1;33m'
BLUE='\E[1;34m'
PINK='\E[1;35m'
RES='\E[0m'

cfgfile=/etc/platform.conf
LOG_DIR_PREFIX="/var/asterfusion"
BFNSDK_INSTDIR="/usr/local/sde"

friendly_exit()
{
    echo ""
    read -n1 -p "Press any key to exit."
    echo
    exit 0
}

install_cgosdrv()
{
    cgosdrv="/lib/modules/`uname -r`/kernel/drivers/misc/cgosdrv.ko"
    mod=`lsmod | grep cgosdrv`
    mod=${mod:0:7}

    if [ "$mod"X = ""X ]; then
        if [ ! -e $cgosdrv ]; then
            echo -e "${RED}$cgosdrv.${RES}"
            echo -e "${RED}Critical: No cgosdrv detected, please install it first.${RES}"
            echo -e "${RED}You can install it from a package or from source.${RES}"
            friendly_exit
        fi

        echo -e "Loading cgosdrv ..."
        insmod $cgosdrv
    fi
}

uninstall_cgosdrv()
{
    mod=`lsmod | grep cgosdrv`
    mod=${mod:0:7}

    if [ "$mod"X = "cgosdrv"X ]; then
        echo -e "Off loading cgosdrv ..."
        rmmod $mod
    fi
}

install_nct6779d()
{
    nct6779drv="/lib/modules/`uname -r`/kernel/drivers/misc/nct6779d.ko"
    mod=`lsmod | grep nct6779d`
    mod=${mod:0:8}

    if [ "$mod"X = ""X ]; then
        if [ ! -e $nct6779drv ]; then
            echo -e "${RED}$nct6779drv.${RES}"
            echo -e "${RED}Critical: No nct6779drv detected, please install it first.${RES}"
            echo -e "${RED}You can install it from a package or from source.${RES}"
            friendly_exit
        fi

        echo -e "Loading nct6779d ..."
        insmod $nct6779drv
    fi
}

uninstall_nct6779d()
{
    mod=`lsmod | grep nct6779d`
    mod=${mod:0:8}

    if [ "$mod"X = "nct6779d"X ]; then
        echo -e "Off loading nct6779d ..."
        rmmod $mod
    fi
}

install_bfnkdrv()
{
    hugepages=`cat /proc/meminfo  | grep HugePages_Total | awk '{print $2}'`
    if [ $hugepages -ne 128 ]; then
        echo "DMA setup $SDE_INSTALL/bin/dma_setup.sh"
        $SDE_INSTALL/bin/dma_setup.sh
    fi

    bfnkdrv="$SDE_INSTALL/lib/modules/bf_kdrv.ko"
    mod=`lsmod | grep bf_kdrv`
    mod=${mod:0:7}

    if [ "$mod"X = ""X ]; then
        if [ ! -e $bfnkdrv ]; then
            echo -e "${RED}$bfnkdrv.${RES}"
            echo -e "${RED}Critical: No bfnkdrv detected, please install it first.${RES}"
            echo -e "${RED}You can install it from a package or from source.${RES}"
            friendly_exit
        fi

        echo -e "Loading bf_kdrv ..."
        $SDE_INSTALL/bin/bf_kdrv_mod_load $SDE_INSTALL
    fi
}

dump_eeprom()
{
    #TBD
    echo ""
}

