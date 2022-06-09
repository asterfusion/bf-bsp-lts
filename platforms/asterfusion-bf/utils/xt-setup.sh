#!/bin/bash

hugepages=`cat /proc/meminfo  | grep HugePages_Total | awk '{print $2}'`
if [ $hugepages -ne 128 ]; then
   echo "Running through $SDE_INSTALL/bin/dma_setup.sh"
   $SDE_INSTALL/bin/dma_setup.sh
fi

bf_kdrv=`lsmod | grep bf_kdrv | awk '{print $1}'`
if [ "$bf_kdrv"X == ""X ]; then
   echo "Loading Barefoot Kernel driver"
   $SDE_INSTALL/bin/bf_kdrv_mod_load $SDE_INSTALL
fi

bsp=`dpkg -l | grep bsp | awk '{print $2}'`
if [ "$bsp"X = "bsp-8.9.x"X ];then
   echo "Loading CGOS kernel driver"
   insmod /lib/modules/`uname -r`/kernel/drivers/misc/cgosdrv.ko
   echo "Loading NCT6779D kernel driver"
   insmod /lib/modules/`uname -r`/kernel/drivers/misc/nct6779d.ko
   # Generate /etc/platform.conf
   #$SDE_INSTALL/bin/xt-cfgen.sh
else
   # install kernel drivers for CME
   cme=`cat /etc/platform.conf  | grep "com-e:" | awk '{print $1}'`
   type=${cme:6}

   if [ "$type"X == "CG1508"X ] || [ "$type"X == "CG1527"X ]; then
      cgos_kdrv=`lsmod | grep cgosdrv | awk '{print $1}'`
      if [ "$cgos_kdrv"X == ""X ]; then
         echo "Loading CGOS kernel driver"
         insmod /lib/modules/`uname -r`/kernel/drivers/misc/cgosdrv.ko
      fi
   fi
   if [ "$type"X == "CME3000"X ]; then
      nct6779d_kdrv=`lsmod | grep nct6779d | awk '{print $1}'`
      if [ "$nct6779d_kdrv"X == ""X ]; then
         echo "Loading NCT6779D kernel driver"
	 insmod /lib/modules/`uname -r`/kernel/drivers/misc/nct6779d.ko
      fi
   fi
   #  do nothing.
   #if [ "$type"X == "CME7000"X ]; then
   #fi
fi
