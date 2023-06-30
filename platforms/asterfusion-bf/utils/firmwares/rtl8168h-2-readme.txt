1. Install ONL.
2. Create directory "/lib/firmware/rtl_nic" and copy "rtl8168h-2.fw" to "/lib/firmware/rtl_nic/".
3. Restart network or reboot.
4. Find "enp8s0" via command <ip a> or <ifconfig>.
5. Configure network, change the management interface from "ma1" to "enp8s0" in "/etc/network/interfaces".

Note: 
1. The firmware works on CME3000 and only ONL8 requires it.
2. There could be a way to rename the mangement interface from "enp8s0" to "ma1".

