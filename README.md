## About this repository

Mainline  **ALL-in-ONE** repository for all TF1 based **X-T Bare Metal Switch** powered by Asterfusion with Long Term Support.

Current supported **X-T Bare Metal Switch**:

  - `X564P-T`,  64x 100GbE QSFP28, and auxiliary 2x 25GbE SFP28 which support 1GbE.
  - `X532P-T`,  32x 100GbE QSFP28, and auxiliary 2x 25GbE SFP28 which support 1GbE.
  - `X312P-T`,  12x 100GbE QSFP28, 48x 25GbE SFP28, and auxiliary 2x 25GbE SFP28 which support 1GbE.
  - `X308P-T`,  08x 100GbE QSFP28, 48x 25GbE SFP28 and last 4 of them support 1GbE.

Sometimes, we call `X564P-T` and `X532P-T` as `X5-T`, and call `X312P-T` and `X308P-T` as `X3-T`.

## About the compatibilities to different P4Studio version

Current supported **SDE**:

  - `8.9.x`.
  - `9.1.x`.
  - `9.3.x`.
  - `9.5.x`.
  - `9.7.x`.
  - `9.9.x`.
  - `9.11.x`.

The version number of a SDE consists of three Arabic numbers, `x.y.z`, where `x` is the major version, `y` is the minor version, and `z` is the sub-version under `y`.
It's would be a LTS version when `y` is odd, otherwise it is a non-LTS version. It's worth mentioning that we build and run the code on the top of Debian and here only list the versions which we have adapted and tested, and this does not exclude or deny that the repository does not support other non-LTS SDE versions.

In order to be compatible with different SDE versions and different base systems, we have introduced two macro variables in the BSP, one is `SDE_VERSION` and the other is `OS_VERSION`.
They are defined in `$BSP/drivers/include/bf_pltfm_types/bf_pltfm_type.h`.

You can choose the most suitable mainline branch according to the following principle:
 - `master`.  Mainline branch for Debian `OS_VERSION ≤ 9`.
 - `master+`· Mainline branch for Debian `OS_VERSSON ≥ 10`.

Then modify `SDE_VERSION` to which you're using.


## Special dependency

There're some differences on hardware design between `X5-T` and `X3-T`. To this `all-in-one` repository, the full special dependencies are needed to be compiled and installed.

Here are the special dependencies:

  - `nct6779d`, which needed by `X312P-T`.
  - `cgoslx`, which needed by `X5-T`.

Please install the dependencies first from sources before compiling the **ALL-in-ONE** repository. You can find sources in [asternos.dev](https://asternos.dev/xt).

## Quick Start

All your files and folders are presented as a tree in the file explorer. You can switch from one to another by clicking a file in the tree.
The building of drivers produces a set of libraries that need to be loaded (or linked to) the application.
Here're the steps to build and install the <bf-platforms> package:

Barefoot SDK environment setup:
```
root@localhost:~# vi ~/.bashrc
export SDE=/usr/local/sde/bf-sde-9.3.2
export SDE_INSTALL=/usr/local/sde
export PATH=$PATH:$SDE_INSTALL/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SDE_INSTALL/lib
root@localhost:# source ~/.bashrc
```
Clone and compile `bf-bsp-8.9.x`:
```
root@localhost:~# git clone https://asternos.dev/xt/bf-bsp-8.9.x.git
root@localhost:~# cd bf-bsp-8.9.x
root@localhost:~/bf-bsp-8.9.x# ./autogen.sh
root@localhost:~/bf-bsp-8.9.x# ​./configure --prefix=$SDE_INSTALL [--enable-thrift]
root@localhost:~/bf-bsp-8.9.x# ​make -j7
​root@localhost:~/bf-bsp-8.9.x# make install
```
After make & make install done, the `libasterfusionbf*`, `libplatform_thrift*`, `libpltfm_driver*`, `libpltfm_mgr*` will be installed to `$SDE_INSTALL/lib`, and all the header files exposed by bsp will be installed to `$SDE_INSTALL/include`.

Runtime environment setup:
```
# Generate /etc/platform.conf
root@localhost:~# xt-cfgen.sh
It looks like x532p-t detected.
...
========================== Generate /etc/platform.conf ==========================
CG1508
uart enabled
==========================            Done             ==========================

# Launch ASIC
root@localhost:~# run_switchd.sh -p diag
Using SDE /usr/local/sde/bf-sde-9.3.2
Using SDE_INSTALL /usr/local/sde
Setting up DMA Memory Pool
Using TARGET_CONFIG_FILE /usr/local/sde/share/p4/targets/tofino/diag.conf
...
Install dir: /usr/local/sde (0x564634fa2140)
bf_switchd: system services initialized
bf_switchd: loading conf_file /usr/local/sde/share/p4/targets/tofino/diag.conf...
bf_switchd: processing device configuration...
Configuration for dev_id 0
...
```


## Q&A

Looking forward your questions by emailing TSIHANG (tsihang@asterfusion.com).
