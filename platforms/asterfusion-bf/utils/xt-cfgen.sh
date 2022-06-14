# Generate /etc/platform.conf and output configure entries to /etc/platform.conf.
# The /etc/platform.conf is strongly required when launch X-T Programmable Bare Metal Switches.
#
# Both nct6779d and cgosdrv must be loaded before running this script.
# After that, run xt-cfgen.sh to generate the configure file (/etc/platform.conf)
# and then run asic-setup.sh to setup more for launching X-T Programmable Bare Metal Switches.
#
# by tsihang <tsihang@asterfusion.com> on 20 May, 2022.
#

#!/bin/bash

cfgfile=/etc/platform.conf
default_cme='CME3000'
default_i2c="i2c-0"
enable_uart=0

cpu=`cat /proc/cpuinfo | grep Intel\(R\)\ Pentium\(R\)\ CPU\ D15`
if [[ $cpu =~ "D15" ]]
then
    default_cme='CG1508'
fi
sleep 1

# Find uart_util which created by bsp and installed to $SDE_INSTALL/bin.
# When found return its absolute path.
# For a given X-T with different OS (such as ubuntu), the working tty may differ from /dev/ttyS1 on Debian.
# Please help to test with all possible /dev/ttySx.
uart_util=`find /usr | grep uart_util`
echo $uart_util
uart_util=`find /opt | grep uart_util`
echo $uart_util

# Detect X308P-T first.
if [ "$default_cme"x = "CME3000"x ];then
    bmc_version_10hex=$(uart_util /dev/ttyS1 0xd 0xaa 0xaa)
    if [ $? -eq 0 ]; then
        if [ "$bmc_version_10hex"x = "read failed"x ];then
            echo "Maybe not x308p-t."
        else
            # Maybe x308p-t.
            echo "Maybe x308p-t."
            # uart is used for x308pp-t with newer BMC version.
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
            xt_platform=$(uart_util /dev/ttyS1 0x1 0x21 0xaa)
            echo $xt_platform
            enable_uart=1
        fi
    fi
fi

# Detect X564P-T and X532P-T.
if [ "$default_cme"x = "CG1508"x ] || [ "$default_cme"x = "CG1527"x ]; then
    # Maybe x532p-t or x564p-t.
    echo "Maybe x532p-t or x564p-t."
    bmc_version_10hex=$(uart_util /dev/ttyS1 0xd 0xaa 0xaa)
    if [ $? -eq 0 ]; then
        if [ "$bmc_version_10hex"x = "read failed"x ];then
            # At this moment, cgosdrv is only used for X5-T with an earlier BMC version.
            cgosi2c 0x3e 0x01 0x21 0xaa
            sleep 1
            xt_platform=`cgosi2c 0x3e`
        else
            # uart is used for x532p-t and x564p-t with newer BMC version.
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
            xt_platform=$(uart_util /dev/ttyS1 0x1 0x21 0xaa)
            echo $xt_platform
            enable_uart=1
        fi
    else
        # At this moment, cgosdrv is only used for X5-T with an earlier BMC version.
        cgosi2c 0x3e 0x01 0x21 0xaa
        sleep 1
        xt_platform=`cgosi2c 0x3e`
    fi
fi

# Detect X312P-T
if [ "$default_cme"x = "CME3000"x ] && [ $enable_uart = 0 ];then
    # Maybe x312p-t
    echo "Maybe x312p-t."
    default_i2c=`i2cdetect -l | grep sio_smbus | awk '{print $1}'`
    if [ ! $default_i2c ]; then
        # CP2112 is used for access BMC by x312p-t v2.0.
        default_i2c=`i2cdetect -l | grep CP2112 | awk '{print $1}'`
        if [ ! $default_i2c ]; then
            echo "No CP2112 kernel driver."
            exit 0
        fi

        i2cset -y ${default_i2c:4:1} 0x3e 0x01 0x21 0xaa s
        sleep 1
        xt_platform=`i2cdump -y ${default_i2c:4:1} 0x3e s 0xff`

        if [ $? -ne 0 ]; then
            echo "Get X-T Programmable Bare Metal failed."
            exit 0
        fi
    else
        # SuperIO is used for access BMC by x312p-t v1.0 and x312p-t v3.0.
        i2cset -y ${default_i2c:4:1} 0x3e 0x01 0x21 0xaa s
        sleep 1
        xt_platform=`i2cdump -y ${default_i2c:4:1} 0x3e s 0xff`

        if [ $? -ne 0 ]; then
            default_i2c=`i2cdetect -l | grep CP2112 | awk '{print $1}'`
            if [ ! $default_i2c ]; then
                echo "No CP2112 kernel driver."
                exit 0
            fi

            i2cset -y ${default_i2c:4:1} 0x3e 0x01 0x21 0xaa s
            sleep 1
            xt_platform=`i2cdump -y ${default_i2c:4:1} 0x3e s 0xff`

            if [ $? -ne 0 ]; then
                echo "Get X-T Programmable Bare Metal failed"
                exit 0
            fi
        fi
    fi
fi


echo ""
echo ""
echo ""
echo ""

echo "========================== Generate /etc/platform.conf ========================== "

# Clear existing /etc/platform.conf
echo "" > $cfgfile
echo "# /etc/platform.conf" >> $cfgfile
echo "" >> $cfgfile
echo "" >> $cfgfile

echo "# If CG1508 or CG1527 selected, next configuration, here meaning 'i2c:X' will not make an impact." >> $cfgfile
echo "# Currently supported CME like below:" >> $cfgfile
echo "#   1. CG1508 (Default)" >> $cfgfile
echo "#   2. CG1527" >> $cfgfile
echo "#   3. CME3000" >> $cfgfile
echo "#   4. CME7000" >> $cfgfile
echo $default_cme
echo "com-e:"$default_cme >> $cfgfile
echo "" >> $cfgfile

echo "# Master I2C used to access BMC and CPLD." >> $cfgfile
echo "# Here we set channel 0 by default channel for ComExpress except CG15xx serials." >> $cfgfile
echo "# Any other ComExpress like ComExpress 3000 may use different one." >> $cfgfile
echo "i2c-"${default_i2c:4:1}
echo "i2c:"${default_i2c:4:1} >> $cfgfile
echo "" >> $cfgfile

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

