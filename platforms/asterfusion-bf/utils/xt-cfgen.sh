# Generate /etc/platform.conf and output configure entries to /etc/platform.conf.
# which is strongly required when launch X-T Programmable Bare Metal Switches.
#
# by tsihang <tsihang@asterfusion.com> on 20 May, 2022.
#

#!/bin/bash

cfgfile=/etc/platform.conf
default_cme='CME3000'
default_i2c="127"
hw_platform='N/A'
hw_version="0"
enable_uart=0
enable_iic=1

BLINK='\033[05m'
RED='\E[1;31m'
GREEN='\E[1;32m'
YELLOW='\E[1;33m'
BLUE='\E[1;34m'
PINK='\E[1;35m'
RES='\E[0m'

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

echo -e "${YELLOW}Notice: Start detecting and make sure that the switchd is not running${RES}"
sleep 1
install_bfnkdrv

# Find uart_util which created by bsp and installed to $SDE_INSTALL/bin.
# When found return its absolute path.
# For a given X-T with different OS (such as ubuntu), the working tty may differ from /dev/ttyS1 on Debian.
# Please help to test with all possible /dev/ttySx.
# uart_util=`find / | grep uart_util`
uart_util="$SDE_INSTALL/bin/uart_util"
if [ ! -e $uart_util ]; then
    # For SONiC, uart_util is installed in /opt/bfn/install/bin
    uart_util="/opt/bfn/install/bin/uart_util"
fi

if [ ! -e $uart_util ]; then
    echo -e "${RED}Critical: No uart_util detected, please install it first.${RES}"
    friendly_exit
fi

# 1st, Let us read eeprom ASAP.
# Standard EEPROM
#================== EEPROM ==================
#0x21         Product Name:                          X532P-T
#0x22          Part Number:                   ONBP1U-2Y32C-E
#0x23        Serial Number:                      F01970CA007
#0x24       Base MAC Addre:                60:EB:5A:00:7D:1E
#0x25     Manufacture Date:              30/05/2022 14:49:48
#0x26      Product Version:                                1
#0x27   Product Subversion:                                0
#0x28        Platform Name: x86_64-asterfusion_congad1519-r0
#0x29         Onie Version:        master-202008111723-dirty
#0x2a        MAC Addresses:                                1
#0x2b         Manufacturer:                      Asterfusion
#0x2c         Country Code:                               CN
#0x2d          Vendor Name:                      Asterfusion
#0x2e         Diag Version:                              1.0
#0x2f          Service Tag:                                X
#0x30   Switch ASIC Vendor:                            Intel
#0x31   Main Board Version:                 APNS320T-A1-V1.1
#0x32         COME Version:              CME5027-16GB-HV-CGT
#0x33  GHC-0 Board Version:                                 
#0x34  GHC-1 Board Version:                                 

xt_platform=''
xt_hwver=''
xt_hwsubver=''

# For most part of X-T Programmable Bare Metal, we can get eeprom by uart.
# The most smart thing we should do first is get the platform ASAP.
bmc_version_10hex=$($uart_util /dev/ttyS1 0xd 0xaa 0xaa)
if [ $? -ne 0 ]; then
    echo -e "${YELLOW}Warning: The reason why this error occured is either the environment is incorrect or the command is not recognized.${RES}"
    exit 0
else
    if [ "$bmc_version_10hex"x = "read failed"x ];then
        # This means we could not get eeprom through uart. There could be two reason for this case:
        # 1. Wrong /dev/ttySx
        # 2. Get eeprom through I2C.
        enable_uart=0
    else
        bmc_version_10hex=${bmc_version_10hex:2}

        # echo $bmc_version_10hex
        var1=`echo "$bmc_version_10hex"|awk -F ' ' '{print $1}'`
        var2=`echo "$bmc_version_10hex"|awk -F ' ' '{print $2}'`
        var3=`echo "$bmc_version_10hex"|awk -F ' ' '{print $3}'`

        #change 16hex to 10hex
        var1=$((16#$var1))
        var2=$((16#$var2))
        var3=$((16#$var3))
        echo "bmc_version is "$var1.$var2.$var3
        sleep 1

        xt_platform=$($uart_util /dev/ttyS1 0x1 0x21 0xaa)
        enable_uart=1
        # Read HW version
        xt_hwver=$($uart_util /dev/ttyS1 0x1 0x31 0xaa)
        hw_version=${xt_hwver: -3}
        echo "hwver:"$hw_version
    fi
fi

# Unfortunately, we do not get platform so far. Try another way.
if [ $enable_uart = 0 ] && [ "$xt_platform"X = ""X ]; then
    # Try to read platform with nct6779d or cp2112 for X312P-T detection.
    # nct6779d is used for access bmc for X312P-T v1.0 and v3.0.
    # cp2112 is used for access bmc for X312P-T v2.0.
    install_nct6779d

    i2c=`i2cdetect -l | awk -F '[- ]' '/sio_smbus/{print $2}'`
    default_i2c=${i2c:0:1}
    if [ ! $default_i2c ]; then
        default_i2c=`i2cdetect -l | grep CP2112 | awk '{print $1}'`
        if [ ! $default_i2c ]; then
            echo -e "${RED}Critical: No CP2112 kernel driver.${RES}"
            friendly_exit
        fi

        echo -e "${YELLOW}superio i2c-$default_i2c${RES}"
        i2cset -y $default_i2c 0x3e 0x01 0x21 0xaa s
        sleep 1
        rc=`i2cdump -y $default_i2c 0x3e s 0xff`

        if [ $? -ne 0 ]; then
            echo -e "${RED}Critical: Get X-T Programmable Bare Metal failed.${RES}"
            friendly_exit
        fi
    else
        i2cset -y $default_i2c 0x3e 0x01 0x21 0xaa s
        sleep 1
        rc=`i2cdump -y $default_i2c 0x3e s 0xff`

        if [ $? -ne 0 ]; then
            i2c=`i2cdetect -l | awk -F '[- ]' '/CP2112/{print $2}'`
            default_i2c=${i2c:0:1}
            if [ ! $default_i2c ]; then
                echo -e "${RED}Critical: No CP2112 kernel driver.${RES}"
                echo -e "cp2112 hang on and a cold reboot required."
                friendly_exit
            fi
            
            echo -e "${YELLOW}cp2112 i2c-$default_i2c${RES}"
            i2cset -y $default_i2c 0x3e 0x01 0x21 0xaa s
            sleep 1
            rc=`i2cdump -y $default_i2c 0x3e s 0xff`

            if [ $? -ne 0 ]; then
                echo -e "${RED}Critical: Get X-T Programmable Bare Metal failed.${RES}"
                friendly_exit
            fi
        fi
    fi
    if [[ $rc =~ "312" ]]; then
        # default x312p-t
        xt_platform="X312P-T"
        # Read HW version
        i2cset -y $default_i2c 0x3e 0x01 0x26 0xaa s
        sleep 1
        xt_hwver=`i2cdump -y $default_i2c 0x3e s 0xff`
        i2cset -y $default_i2c 0x3e 0x01 0x27 0xaa s
        sleep 1
        xt_hwsubver=`i2cdump -y $default_i2c 0x3e s 0xff`
        hw_version="${xt_hwver: -1}.${xt_hwsubver: -1}"
        echo "hwver:"$hw_version
    fi
fi

# Unfortunately, we do not get platform so far. Try another way.
# Almost not happen because only old X5 could reach here at this moment.
if [ $enable_uart = 0 ] && [ "$xt_platform"X = ""X ]; then
    # Try to read platform with cgosdrv
    # cgosdrv may be used for X5-T with an earlier BMC access.
    install_cgosdrv

    rc=`cgosi2c 0x3e 0x01 0x21 0xaa`
    if [ "$rc"X = "ERROR: Failed to write to EEPROM."X ];then
        echo ""
    else
        sleep 1
        xt_platform=`cgosi2c 0x3e`
    fi
fi

# Unfortunately, we still did not get what we want after many attempts at last.
# Exit
if [ "$xt_platform"X = ""X ]; then
    echo -e "${RED}Critical: Detect X-T Programmable Bare Metal failed.${RES}"
    friendly_exit
fi

# What should we do if an unrecognized platform detected ?
echo -e "Platform : $xt_platform"

# We have got the platform. Then try to get CME type by uart.
if [ "$xt_platform"X != ""X ] && [ $enable_uart = 1 ]; then
    # Read eeprom offset of 0x32.
    xt_cme=$($uart_util /dev/ttyS1 0x1 0x32 0xaa)
    if [[ $xt_cme =~ "CGT" ]]; then
        # Detect X564P-T and X532P-T with S02
        default_cme='CG1527'
        if [[ $xt_cme =~ "08" ]]; then
            default_cme='CG1508'
        fi
    fi
    if [[ $xt_cme =~ "ADV" ]]; then
        # Detect X564P-T and X532P-T with YH
        default_cme='ADV1527'
        if [[ $xt_cme =~ "08" ]]; then
            default_cme='ADV1508'
        fi
    fi
    if [[ $xt_cme =~ "S02" ]]; then
        # Detect X564P-T and X532P-T with S02
        default_cme='S021527'
        if [[ $xt_cme =~ "08" ]]; then
            default_cme='S021508'
        fi
    fi
    if [[ $xt_cme =~ "CME3000" ]]; then
        # Detect X308P-T, X312P-T and X532P-T with S01
        default_cme='CME3000'
    fi
fi

# We have got the platform. Then try to get CME type by i2c.
# This may not happen because we only have X312P-T with CME3000 need this routine.
# As well as some old hw such X532P-T v1.0 and X564P-T v1.1.
if [ "$xt_platform"X != ""X ] && [ $enable_uart = 0 ]; then
    echo ""
    # x532p-t
    # x564p-t
    # x312p-t
    # TBD
fi

# Detect X308P-T.
# BMC  <- UART
# CPLD <- nct6779d
# SFP  <- CP2112
if [ "$default_cme"X = "CME3000"X ] && [[ $xt_platform =~ "308" ]]; then
    install_nct6779d
    i2c=`i2cdetect -l | awk -F '[- ]' '/sio_smbus/{print $2}'`
    enable_iic=1
    default_i2c=${i2c:0:1}
    hw_platform="X308P-T"
    echo -e "${YELLOW}${BLINK}It looks like x308p-t detected.${RES}${RES}"
fi

# Detect X312P-T.
# v1.0 and v1.1
    # BMC  <- nct6779d
    # CPLD <- nct6779d
    # SFP  <- nct6779d
# v1.2
    # BMC  <- cp2112
    # CPLD <- cp2112
    # SFP  <- cp2112
# v1.3
    # BMC  <- nct6779d
    # CPLD <- nct6779d
    # SFP  <- cp2112
if [ "$default_cme"X = "CME3000"X ] && [[ $xt_platform =~ "312" ]]; then
    enable_iic=1
    hw_platform="X312P-T"
    echo -e "${YELLOW}${BLINK}It looks like x312p-t detected.${RES}${RES}"
    # i2c has been detected during xt_platform detect routine, no need to detect it again.
fi

# Detect X532P-T. Two-way cp2112
# BMC  <- UART or cgosdrv
# CPLD <- cp2112 or cgosdrv
# SFP  <- cp2112 or cogsdrv
# QSFP <- cp2112
if [[ $xt_platform =~ "532" ]]; then
    enable_iic=0
    if [[ $default_cme =~ "CG" ]]; then
        # At least, cgosdrv is forced required to access cpld and sfp under CG15xx.
        install_cgosdrv
    fi
    if [[ $default_cme =~ "CME3000" ]]; then
        install_nct6779d
        i2c=`i2cdetect -l | awk -F '[- ]' '/sio_smbus/{print $2}'`
        default_i2c=${i2c:0:1}
        enable_iic=1
    fi
    hw_platform="X532P-T"
    echo -e "${YELLOW}${BLINK}It looks like x532p-t detected.${RES}${RES}"
fi

# Detect X564P-T. Two-way cp2112
# BMC  <- UART or cgosdrv
# CPLD <- cp2112 or cgosdrv
# SFP  <- cp2112 or cogsdrv
# QSFP <- cp2112
if [[ $xt_platform =~ "564" ]]; then
    enable_iic=0
    if [[ $default_cme =~ "CG" ]]; then
        # At least, cgosdrv is forced required to access cpld and sfp under CG15xx.
        install_cgosdrv
    fi
    if [[ $default_cme =~ "CME3000" ]]; then
        install_nct6779d
        i2c=`i2cdetect -l | awk -F '[- ]' '/sio_smbus/{print $2}'`
        default_i2c=${i2c:0:1}
        enable_iic=1
    fi
    hw_platform="X564P-T"
    echo -e "${YELLOW}${BLINK}It looks like x564p-t detected.${RES}${RES}"
fi

echo "COMe     : $default_cme"

if [ -f $cfgfile ]; then
    echo ""
    echo ""
    echo -e "${RED} We found that the $cfgfile already exists and no new configuration file will be generated.${RES}"
    echo -e "${RED} The purpose of this prompt is to take into account that your system may have worked fine in the past.${RES}"
    echo -e "${RED} If you want to generate a new configuration file, delete the old one first.${RES}"
    friendly_exit
fi

echo ""
echo ""
echo ""
echo ""

echo "========================== Generate /etc/platform.conf ========================== "

# Clear existing /etc/platform.conf
echo "" > $cfgfile
echo "# /etc/platform.conf" >> $cfgfile
echo "# Generated by xt-cfgen.sh" >> $cfgfile
echo "" >> $cfgfile
echo "" >> $cfgfile

echo "# X-T Bare Metal Hardware Platform." >> $cfgfile
echo "# Currently supported X-T Bare Metal like below:" >> $cfgfile
echo "#   1. X532P-T (Default)" >> $cfgfile
echo "#   2. X564P-T"  >> $cfgfile
echo "#   3. X308P-T" >> $cfgfile
echo "#   4. X312P-T" >> $cfgfile
echo $xt_platform $hw_platform
echo "platform:"$hw_platform >> $cfgfile
echo "" >> $cfgfile
echo "hwver:"$hw_version >> $cfgfile
echo "" >> $cfgfile


echo "# If CG15xx,ADV15xx or S0215xx selected, 'i2c:X' will not make an impact." >> $cfgfile
echo "# Currently supported CME like below:" >> $cfgfile
echo "#   1. CG1508 (Default)" >> $cfgfile
echo "#   2. CG1527"  >> $cfgfile
echo "#   3. ADV1508" >> $cfgfile
echo "#   4. ADV1527" >> $cfgfile
echo "#   5. S021508" >> $cfgfile
echo "#   6. S021527" >> $cfgfile
echo "#   7. CME3000" >> $cfgfile
echo "#   8. CME7000" >> $cfgfile
echo $default_cme
echo "com-e:"$default_cme >> $cfgfile
echo "" >> $cfgfile

echo "# Master I2C used to access BMC and/or CPLD." >> $cfgfile
echo "# Here we set channel 0 by default channel for ComExpress except CG15xx/ADV15xx/S0215xx serials." >> $cfgfile
echo "# Any other ComExpress like ComExpress 3000 may use different one." >> $cfgfile
if [ $enable_iic = 1 ]; then
    echo "i2c-"$default_i2c
    echo "i2c:"$default_i2c >> $cfgfile
    echo "" >> $cfgfile
else
    echo "#i2c:"$default_i2c >> $cfgfile
    echo "" >> $cfgfile
fi

echo "# Default uart to access BMC." >> $cfgfile
echo "# If a given uart set, BSP will access BMC through it, otherwise, using I2C instead." >> $cfgfile
echo "# Only works on CG15xx and need a new BMC version." >> $cfgfile
echo "# by tsihang, 2021-07-05" >> $cfgfile
if [ $enable_uart = 1 ];then
    echo "uart enabled"
    echo "uart:/dev/ttyS1" >> $cfgfile
else
    echo "uart disabled"
    echo "#uart:/dev/ttyS1" >> $cfgfile
fi

echo "==========================            Done             ========================== "
