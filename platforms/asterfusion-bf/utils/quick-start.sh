#!
# @file quick-start.sh
# @date 2023/03/09
#
# Environment setup and config for X-T Programmable Bare Metals.
# This script comes from $BSP/platforms/asterfusion-bf/utils/ and
# would be installed to $SDE_INSTALL/bin/
#
# TSIHANG (tsihang@asterfusion.com)
#

#!/bin/bash

# You could get the version of current ONL via 'cat /etc/onl/rootfs/manifest.json'.
# The following VERSION in quick-start.sh should never be changed by user.
VERSION="24.0117"

# Default Variables
default_iface="ma1"
default_iface_ip="10.240.4.52"
default_gateway="10.240.4.1"
install_dir="/usr/local/sde"

# y/Y, n/N
persistent_cfgnet="y"
persistent_cfgsdk="y"

find_iface()
{
    ifaces=(`ifconfig | grep ^[a-z] | awk -F: '{print $1}'`)
    ifaces_ip=(`ifconfig | grep 'netmask' | sed 's/^.*inet //g' | sed 's/ *netmask.*$//g'`)
    for ((i = 0; i < ${#ifaces[@]}; i++))
    do
        if [ "$1"X == "${ifaces[$i]}"X ] || [ "$1"X == "${ifaces_ip[$i]}"X ]; then
            printf "${GREEN}%-19s%-19s${RES}\n" ${ifaces[$i]} ${ifaces_ip[$i]}
        fi
    done
}

# Diff check
further_check()
{
    thrift_pkg_ver=`dpkg -l | grep thrift | awk '{print $3}'`
    if [ "$thrift_pkg_ver"X == "0.11.0"X ] || [ "$thrift_pkg_ver"X == "0.13.0"X ]; then
        # thrift-0.11.0 and thrift-0.13.0 are installed to /usr/local/
        if [[ -f  /usr/local/lib/libthrift-$thrift_pkg_ver.so ]]; then
            if [[ ! -f  $SDE_INSTALL/lib/libthrift-$thrift_pkg_ver.so ]]; then
                ln -s /usr/local/lib/libthrift-$thrift_pkg_ver.so $SDE_INSTALL/lib/libthrift-$thrift_pkg_ver.so
            fi
        fi
    fi
    if [ "$thrift_pkg_ver"X == "0.14.1"X ]; then
        # thrift-0.14.1 is installed to /usr/local/
        if [[ ! -f  $SDE_INSTALL/lib/libthrift-$thrift_pkg_ver.so ]]; then
            echo "No libthrift-$thrift_pkg_ver.so found."
        fi
    fi

    grpc_pkg_ver=`dpkg -l | grep grpc | awk '{print $3}'`
    protobuf_pkg_ver=`dpkg -l | grep protobuf | awk '{print $3}'`

    if [[ ! -f  $SDE_INSTALL/lib/libbfsys.so.0 ]]; then
        ln -s $SDE_INSTALL/lib/libbfsys.so $SDE_INSTALL/lib/libbfsys.so.0
    fi

    # More Check here.
}

echo "Installing bfnplatform ..."
dpkg -i /root/bfnplatform/*.deb

# Link onl-kernel
ln -s \
 /usr/share/onl/packages/amd64/onl-kernel-4.14-lts-x86-64-all/mbuilds/ \
 /lib/modules/4.14.151-OpenNetworkLinux/build > /dev/null 2>&1

echo "Configure SDE Environment Variables ..."
pkg=`dpkg -l | grep sde- | awk '{print $2}'`

res=$(cat /root/.bashrc | grep SDE_INSTALL)
if [ "$res"X != ""X ] && [ "$pkg"X != ""X ]; then
    if [[ -f  $SDE_INSTALL/bin/xt-setup.sh ]]; then
        source xt-setup.sh
    fi
fi

if [ "$persistent_cfgsdk"X == "n"X  ] || [ "$persistent_cfgsdk"X == "N"X  ]; then
    if [ "$pkg"X != ""X ]; then
        export SDE=$install_dir/bf-$pkg
        export SDE_INSTALL=$install_dir
        export PATH=$SDE_INSTALL/bin:$PATH
        export LD_LIBRARY_PATH=$SDE_INSTALL/lib:$LD_LIBRARY_PATH
        echo -e "${YELLOW}Successfully.${RES}"
	fi
else
    echo -e "${YELLOW}Performing(/root/.bashrc) ...${RES}"
    res=$(cat /root/.bashrc | grep SDE_INSTALL)
    if [ "$res"X == ""X ] && [ "$pkg"X != ""X ]; then
        echo "" >> /root/.bashrc
        echo "" >> /root/.bashrc
        echo "export SDE=$install_dir/bf-$pkg" >> /root/.bashrc
        echo "export SDE_INSTALL=$install_dir" >> /root/.bashrc
        echo 'export PATH=$SDE_INSTALL/bin:${PATH}' >> /root/.bashrc
        echo 'export LD_LIBRARY_PATH=$SDE_INSTALL/lib:$LD_LIBRARY_PATH' >> /root/.bashrc
        echo "" >> /root/.bashrc
        source /root/.bashrc
        echo -e "${YELLOW}Successfully.${RES}"
    else
         echo -e "${RED}Already configured SDK.${RES}"
    fi
fi

# At this time, $SDE_INSTALL is ready so loading xt-setup.sh as soon as possible.
if [[ -f  $SDE_INSTALL/bin/xt-setup.sh ]]; then
    source xt-setup.sh
fi

echo "Configure Network ..."
# SSH
sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/g' /etc/ssh/sshd_config

res=$(find_iface $default_iface)
if [ "$res"X == ""X ]; then
    echo -e "${RED}No $default_iface found.${RES}"
    exit
fi

if [ "$persistent_cfgnet"X == "n"X  ] || [ "$persistent_cfgnet"X == "N"X  ]; then
    # Network
    res=$(find_iface $default_iface_ip)
    if [ "$res"X != ""X ]; then
        echo -e "${RED}Already configured $default_iface $default_iface_ip.${RES}"
    else
        ifconfig $default_iface $default_iface_ip netmask 255.255.255.0
        route add default gw $default_gateway
    fi
else
    echo -e "${YELLOW}Performing Network(/etc/network/interfaces) ...${RES}"
    res=$(cat /etc/network/interfaces | grep address | awk '{print $2}')
    if [ "$res"X == ""X ]; then
        echo "" >> /etc/network/interfaces
        echo "" >> /etc/network/interfaces
        echo "# The primary network interface" >> /etc/network/interfaces
        echo "allow-hotplug $default_iface" >> /etc/network/interfaces
        echo "auto $default_iface" >> /etc/network/interfaces
        echo "iface $default_iface inet static" >> /etc/network/interfaces
        echo "address $default_iface_ip" >> /etc/network/interfaces
        echo "netmask 255.255.255.0" >> /etc/network/interfaces
        echo "gateway $default_gateway" >> /etc/network/interfaces
        echo "dns-nameserver 8.8.8.8" >> /etc/network/interfaces
        echo "" >> /etc/network/interfaces
        echo "" >> /etc/network/interfaces
        /etc/init.d/networking restart
        /etc/init.d/ssh restart
        /etc/init.d/resolvconf restart
    else
         echo -e "${RED}Already configured $default_iface $res.${RES}"
    fi
fi
echo -e "${YELLOW}Successfully.${RES}"

echo ""
echo ""
echo "================================="
echo $SDE
echo $SDE_INSTALL
echo $PATH
echo $LD_LIBRARY_PATH
echo "================================="
echo ""
echo ""

echo ""
echo ""
echo "================================="
echo `dpkg -l | grep bsp`
echo `dpkg -l | grep sde-`
echo `dpkg -l | grep kdrv`
echo `dpkg -l | grep p4c`
echo `dpkg -l | grep p4i`
echo `dpkg -l | grep libboost`
echo `dpkg -l | grep thrift`
echo `dpkg -l | grep protobuf`
echo `dpkg -l | grep grpc`
echo "================================="
echo ""
echo ""

# Remove previous /etc/platform.conf and generated a new one.
if [[ -f  /etc/platform.conf ]]; then
    rm /etc/platform.conf
fi
# Generate /etc/platform.conf
further_check
xt-cfgen.sh

echo ""
echo ""
echo -e "We're almost approaching after this done and quick-start.sh will exit later."
echo -e "Please run following commands on Linux Shell after exit."
echo -e "root@localhost:~#${GREEN} source ~/.bashrc${RES}"
echo -e "root@localhost:~#${GREEN} run_switchd.sh -p diag${RES}"
echo ""
echo ""

friendly_exit

