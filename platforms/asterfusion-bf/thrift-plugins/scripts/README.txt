root@localhost:~# tree thrift-plugins
thrift-plugins
├── eeprom.py
├── fancontrol.py
├── ps_info.py
├── scripts
│   ├── eeprom
│   ├── fancontrol
│   ├── ps_info
│   ├── sensors
│   └── sfputil
├── sensors.py
├── sfputil.py
└── utils.sh

1 directory, 11 files
root@localhost:~# cd thrift-plugins
root@localhost:~/thrift-plugins#
root@localhost:~/thrift-plugins# ln -s `pwd`/eeprom.py $SDE_INSTALL/lib/python2.7/site-packages/eeprom.py
root@localhost:~/thrift-plugins# ln -s `pwd`/ps_info.py $SDE_INSTALL/lib/python2.7/site-packages/ps_info.py
root@localhost:~/thrift-plugins# ln -s `pwd`/fancontrol.py $SDE_INSTALL/lib/python2.7/site-packages/fancontrol.py
root@localhost:~/thrift-plugins# ln -s `pwd`/sensors.py $SDE_INSTALL/lib/python2.7/site-packages/sensors.py
root@localhost:~/thrift-plugins# ln -s `pwd`/sfputil.py $SDE_INSTALL/lib/python2.7/site-packages/sfputil.py

root@localhost:~/thrift-plugins# . ./scripts/fancontrol  fan_speed_info_get
fan number: 1 front rpm: 9200 rear rpm: 10100 percent: 39% 
fan number: 2 front rpm: 9200 rear rpm: 10200 percent: 39% 
fan number: 3 front rpm: 9100 rear rpm: 10200 percent: 39% 
fan number: 4 front rpm: 9200 rear rpm: 10200 percent: 39% 
fan number: 5 front rpm: 9100 rear rpm: 10200 percent: 39%

root@localhost:~/thrift-plugins# . ./scripts/eeprom
name: X732Q-T
part #: CME3000
product serial #: 18120001
ext mac address: 00:11:22:33:44:55
system mfg date: 12/31/2018 23:59:59
product version: 0
product subversion: 
arch: x86_64-asterfusion_x732q_t
onie: 2018.07.31
MAC ext size: 1
system mfger: Asterfusion
country: CN
vendor: Asterfusion
diag ver: 1.0
serv tag: X
ASIC: Intel-BF
crc32: -750367c3

root@localhost:~/thrift-plugins# . ./scripts/ps_info [1|2]
Supplier: AC
Presence: true
Power OK: true
VIN: 233
VOUT: 12
IIN: 0.51
IOUT: 8
PIN: 112
POUT: 62.000000
SN:219218513
Model:ERA85-CPA-BF
REV: FA0002913001
Rota: 13000

root@localhost:~/thrift-plugins# . ./scripts/sensors
No sensors info available
acpitz-virtual-0
Adapter: Virtual device
temp1:        +26.8°C (crit = +100.0°C)

coretemp-isa-0000
Adapter: ISA adapter
Package id 0:  +28.0°C (high = +71.0°C, crit = +91.0°C)
Core 2:        +28.0°C (high = +71.0°C, crit = +91.0°C)
Core 6:        +27.0°C (high = +71.0°C, crit = +91.0°C)
Core 8:        +25.0°C (high = +71.0°C, crit = +91.0°C)
Core 12:       +24.0°C (high = +71.0°C, crit = +91.0°C)

root@localhost:~/thrift-plugins#

root@localhost:~/thrift-plugins# . ./scripts/sfputils
