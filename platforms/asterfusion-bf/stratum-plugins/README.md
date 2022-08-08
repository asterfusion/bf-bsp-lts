## About this repository

Plugins to port X-T to stratum.

Current supported **X-T switches**:

  - `X564P-T`,  64x 100G and auxiliary 2x 25G.
  - `X532P-T`,  32x 100G and auxiliary 2x 25G.
  - `X312P-T`,  12x 100G, 48x 25G, and  auxiliary 2x 25G.
  - `X308P-T`,  08x 100G, 48x 25G.

Sometimes, we call `X564P-T` and `X532P-T` as `X5-T`, and call `X312P-T` and `X308P-T` as `X3-T`.

## Quick Start

Clone *stratum* source from github.
```
root@localhost:~# git clone [https://github.com/stratum/stratum.git](https://github.com/stratum/stratum.git)
root@localhost:~# cd stratum && git checkout -b 2021-12-13 2021-12-13
```
Compile *stratum* 
```
root@localhost:/stratum# bazel build //stratum/hal/bin/barefoot:stratum_bfrt_deb
root@localhost:/stratum# bazel build //stratum/hal/bin/barefoot:stratum_bf_deb
DEBUG: /stratum/stratum/hal/lib/barefoot/barefoot.bzl:19:14: SDE_INSTALL is deprecated, please use SDE_INSTALL_TAR
DEBUG: /stratum/stratum/hal/lib/barefoot/barefoot.bzl:24:10: SDE with BSP: False.
DEBUG: /stratum/stratum/hal/lib/barefoot/barefoot.bzl:31:10: Detected SDE version: 9.7.0.
INFO: Analyzed target //stratum/hal/bin/barefoot:stratum_bf_deb (182 packages loaded, 11497 targets configured).
……
Target //stratum/hal/bin/barefoot:stratum_bf_deb up-to-date:
bazel-bin/stratum/hal/bin/barefoot/stratum-bf_0.0.1_amd64.deb
INFO: Elapsed time: 165.745s, Critical Path: 80.25s
INFO: 1554 processes: 10 internal, 1544 linux-sandbox.
INFO: Build completed successfully, 1554 total actions

```
After compile done, **stratum-bf_0.0.1_amd64.deb** and **stratum-bfrt_0.0.1_amd64.deb** will be created in path `bazel-bin/stratum/hal/bin/barefoot/`.
```
root@localhost:/stratum# ls bazel-bin/stratum/hal/bin/barefoot/ | grep deb
stratum-bf_0.0.1_amd64.deb
stratum_bf_deb.deb
stratum-bfrt_0.0.1_amd64.deb
stratum_bfrt_deb.deb
root@localhost:/stratum#
```
Copy deb to X-T and install
```
root@localhost:~# dpkg -i stratum-bf_0.0.1_amd64.deb
root@localhost:~# ls /etc/stratum/
barefoot-tofino-model  pipeline_cfg.pb.txt  x86-64-accton-as7716-24xc-r0  x86-64-asterfusion-x312p-48y-t-r0  x86-64-inventec-d10064-r0  x86-64-inventec-d7032q28b-r0  x86-64-netberg-aurora-750-r0
bcm_hardware_specs.pb.txt  x86-64-accton-as6712-32x-r0  x86-64-accton-wedge100bf-32qs-r0  x86-64-dell-z9100-c2538-r0  x86-64-inventec-d5254-r0  x86-64-inventec-d7054q28b-r0  x86-64-quanta-ix1-rangeley-r0
dummy_serdes_db.pb.txt  x86-64-accton-as7712-32x-r0  x86-64-accton-wedge100bf-32x-r0  x86-64-delta-ag9064v1-r0  x86-64-inventec-d5264q28b-r0  x86-64-netberg-aurora-610-r0  x86-64-stordis-bf2556x-1t-r0
gnmi_caps.pb.txt  x86-64-accton-as7716-24sc-r0  x86-64-accton-wedge100bf-65x-r0  x86-64-inventec-d10056-r0  x86-64-inventec-d6254qs-r0  x86-64-netberg-aurora-710-r0  x86-64-stordis-bf6064x-t-r0
root@localhost:~# ls /usr/bin/stratum_bf
/usr/bin/stratum_bf
```
Add X-T chassis metadata (this repo) to *stratum* work dir.
```
root@localhost:~# ln -s stratum-plugins/x86-64-asterfusion-x312p-48y-t-r0 /etc/x86-64-asterfusion-x312p-48y-t-r0
```
Then run.
```
root@localhost:~# /usr/bin/stratum_bf -chassis_config_file=/etc/stratum/x86-64-asterfusion-x312p-48y-t-r0/chassis_config.pb.txt  -bf_switchd_cfg=/usr/share/stratum/tofino_skip_p4.conf -bf_switchd_background=false -bf_sde_install=/usr/local/sde -enable_onlp=false
```
## Q&A

Looking forward your questions by emailing TSIHANG (tsihang@asterfusion.com).

