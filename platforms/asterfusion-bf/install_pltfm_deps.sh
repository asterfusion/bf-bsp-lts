#!/bin/bash

# Install additional pkgs needed to build Barefoot Networks SDE (BF-SDE)
# bf-platforms software

trap 'exit' ERR

# Download third-party pkg sources to SDE
[ -z ${SDE} ] && echo "Environment variable SDE not set" && exit 1
echo "Using SDE ${SDE}"

osinfo=`cat /etc/issue.net`

if [[ $osinfo =~ "Fedora" ]]; then
    sudo yum install -y libusb-devel
    sudo yum install -y libcurl-devel
else
    sudo apt-get install -y libusb-1.0-0-dev
    sudo apt-get install -y libcurl4-gnutls-dev
fi
