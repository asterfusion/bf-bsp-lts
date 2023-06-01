# Generate /etc/platform.conf and write required entries(i.e, platform,hwver,i2c,...) to /etc/platform.conf.
# /etc/platform.conf is a pre-installed configuration file which holds amount of entries that strongly required
# when launching X-T Programmable Bare Metal Switch.
#
# by tsihang <tsihang@asterfusion.com> on 20 May, 2022.
#

#!/bin/bash

source xt-setup.sh

default_cme='CME3000'
default_i2c="255"
hw_platform='N/A'
hw_version="0"
enable_uart=0
enable_iic=1

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
    if [[ "$bmc_version_10hex" =~ "read failed" ]];then
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
        num=`echo "$xt_hwver" | awk -F"-" '{print NF-1}'`
        if [[ $num = 3 ]]; then
            xt_hwver=${xt_hwver%-*}
        fi
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

# Detect X308P-T. Two-way cp2112
# BMC  <- UART
# CPLD <- cp2112 or nct6779d
# SFP  <- cp2112
if [[ $xt_platform =~ "308" ]]; then
    enable_iic=0
    hw_platform="X308P-T"
    echo -e "${YELLOW}${BLINK}It looks like x308p-t detected.${RES}${RES}"
fi

# Detect X312P-T.
# v1.x
    # BMC  <- nct6779d
    # CPLD <- nct6779d
    # SFP  <- nct6779d
# v2.x
    # BMC  <- cp2112
    # CPLD <- cp2112
    # SFP  <- cp2112
# v3.x and later
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
# SFP  <- cp2112 or cgosdrv
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
# SFP  <- cp2112 or cgosdrv
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

echo "========================== Generate $cfgfile ========================== "

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
echo "# X-T Bare Metal Hardware Version." >> $cfgfile
echo "hwver:"$hw_version >> $cfgfile
echo "" >> $cfgfile

echo "# COM-Express (X86)." >> $cfgfile
echo "# Currently supported COM-Express listed below:" >> $cfgfile
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

echo "# Master I2C which is used to access CPLD and/or BMC." >> $cfgfile
echo "#" >> $cfgfile
echo "#             [X312P-T V3.0 and later]      [X308P-T]           [X532P-T/X564P-T]" >> $cfgfile
echo "#                      |                       |                        |" >> $cfgfile
echo "#  BMC          <---- nct6779d          <---- UART               <---- UART" >> $cfgfile
echo "#  CPLD         <---- nct6779d          <---- cp2112             <---- cp2112" >> $cfgfile
echo "#  Transceiver  <---- cp2112            <---- cp2112             <---- cp2112" >> $cfgfile
echo "#" >> $cfgfile
echo "# Details" >> $cfgfile
echo "# For X532P-T/X564P-T with ComExpress CG15xx serials, i2c-127 means CPLD forcely accessed through cgosdrv (transition scenarios)." >> $cfgfile
echo "# For X532P-T V1.0 and V1.1, CPLD can be accessed by cgosdrv and as well by cp2112 (default by cp2112)." >> $cfgfile
echo "# For X532P-T V2.0, CPLD can be accessed by cgosdrv and as well by cp2112 (default by cp2112)." >> $cfgfile
echo "# For X564P-T V1.0 and V1.1, CPLD can be and only can be accessed by cgosdrv." >> $cfgfile
echo "# For X564P-T V1.2, CPLD can be accessed by cgosdrv and as well by cp2112 (default by cp2112)." >> $cfgfile
echo "# For X564P-T V2.0, CPLD can be and only can be accessed by cp2112." >> $cfgfile
echo "# For X312P-T V1.0, nct6779d is used to access BMC/CPLD/Transceiver." >> $cfgfile
echo "# For X312P-T V2.0, cp2112 is used to access BMC/CPLD/Transceiver." >> $cfgfile
echo "#" >> $cfgfile
if [ $enable_iic = 1 ]; then
    echo "i2c-"$default_i2c
    echo "i2c:"$default_i2c >> $cfgfile
    echo "" >> $cfgfile
else
    echo "#i2c:"$default_i2c >> $cfgfile
    echo "" >> $cfgfile
fi

echo "# An internal console which is used to access BMC." >> $cfgfile
echo "# If given, BSP will access BMC through this console, otherwise, use i2c instead." >> $cfgfile
echo "# by tsihang, 2021-07-05" >> $cfgfile
if [ $enable_uart = 1 ];then
    echo "uart enabled"
    echo "uart:/dev/ttyS1" >> $cfgfile
else
    echo "uart disabled"
    echo "#uart:/dev/ttyS1" >> $cfgfile
fi

echo "==========================            Done             ========================== "
