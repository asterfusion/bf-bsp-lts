Table of Contents
- [About This Repository](#about-this-repository)
- [About the Compatibilities to SDE](#about-the-compatibilities-to-sde)
- [Special Dependency](#special-dependency)
- [Quick Start](#quick-start)
  - [Environment Variables](#environment-variables)
  - [Build BSP](#build-bsp)
    - [Clone repo](#clone-repo)
    - [Build BSP via CMAKE](#build-bsp-via-cmake)
    - [Build BSP via an Outdated Way](#build-bsp-via-an-outdated-way)
  - [Launch](#launch)
    - [Generate Launching Variables](#generate-launching-variables)
    - [Launch X-T Platforms](#launch-x-t-platforms)
    - [Launch X-T Platforms (Tofino2 based)](#launch-x-t-platforms-tofino2-based)
- [State Machine](#state-machine)
- [Q\&A](#qa)

## <a name="about-this-repository"></a>About This Repository

Mainline  **ALL-in-ONE** repository for all Intel Tofino based **X-T Programmable Bare Metal Switch** powered by Asterfusion with Long Term Support.

Disclaimer: The SDE for the Intel Tofino series of P4-programmable ASICs is currently only available under NDA from Intel. The users of this repository are assumed to be authorized to download and use the SDE. And also, we assume the users have build SDE successfully before working with this repository.

Current supported **X-T Programmable Bare Metal Switch**:
  - `X308P-T`,  08x 100GbE QSFP28, 48x 25GbE SFP28 and last 4 of them can be configured as 1GbE.
  - `X564P-T`,  64x 100GbE QSFP28, and auxiliary 2x 25GbE SFP28 which can be configured as 1GbE.
  - `X532P-T`,  32x 100GbE QSFP28, and auxiliary 2x 25GbE SFP28 which can be configured as 1GbE.
  - `X732Q-T`,  32x 400GbE QSFP56-DD, and auxiliary 2x 25GbE SFP28 which can be configured as 1GbE.
  - `X312P-T`,  12x 100GbE QSFP28, 48x 25GbE SFP28, and auxiliary 2x 25GbE SFP28 which can be configured as 1GbE.

![X-T](docs/programmable-bare-metals.jpg "Figure 1: X-T Programmable Bare Metal Switch Family")

__Figure 1: X-T Programmable Bare Metal Switch Family__

Sometimes, we call `X564P-T` and `X532P-T` as `X5-T`, and call `X312P-T` and `X308P-T` as `X3-T`.

## <a name="about-the-compatibilities-to-sde"></a>About the Compatibilities to SDE

Current supported **SDE**:

  - `8.9.x`.
  - `9.1.x`.
  - `9.3.x`.
  - `9.5.x`.
  - `9.7.x`.
  - `9.9.x`.
  - `9.11.x`.
  - `9.13.x`.

The version number of a SDE consists of three Arabic numbers, `x.y.z`, where `x` is the major version, `y` is the minor version, and `z` is the sub-version under `y`.
It's would be a LTS version when `y` is odd, otherwise it is a non-LTS version. It's worth mentioning that we build and run the code on the top of Debian and here only list the versions which we have adapted and tested, and this does not exclude or deny that the repository does not support other non-LTS SDE versions.


## <a name="special-dependency"></a>Special Dependency

There're some differences on hardware design between `X5-T` and `X3-T`. To this `all-in-one` repository, the full special dependencies are needed to be compiled and installed.

Here are the special dependencies:

  - `nct6779d`, which only required by `X312P-T`.
  - `cgoslx`, which required by `X5-T` (earlier HW).

Please install the dependencies from sources before trying bsp. You can find sources in [github](https://github.com/asterfusion).

## <a name="quick-start"></a>Quick Start

### <a name="environment-variables"></a>Environment Variables
Intel Tofino SDK Variables

*x and y are appropriate values to fit your SDE.*
```
root@localhost:~# vi ~/.bashrc
export SDE=/root/bf-sde-9.x.y
export SDE_INSTALL=$SDE/install
export PATH=$PATH:$SDE_INSTALL/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SDE_INSTALL/lib
root@localhost:# source ~/.bashrc
```

### <a name="build-bsp"></a>Build BSP

#### <a name="clone-repo"></a>Clone repo

```
root@localhost:~# git clone https://github.com/asterfusion/bf-bsp-lts.git
root@localhost:~# cd bf-bsp-lts
root@localhost:~/bf-bsp-lts#
```

There're `2 ways` to build BSP.

#### <a name="build-bsp-via-cmake"></a>Build BSP via CMAKE
The `1st` (also recommended) one is by cmake:
```
root@localhost:~/bf-bsp-lts# ./autogen.sh
root@localhost:~/bf-bsp-lts# mkdir build && cd build/
root@localhost:~/bf-bsp-lts/build# cmake .. \
                      -DCMAKE_MODULE_PATH=`pwd`/../cmake  \
                      -DCMAKE_INSTALL_PREFIX=$SDE_INSTALL \
                      -DOS_NAME=Debian                    \
                      -DOS_VERSION=9                      \
                      -DSDE_VERSION=9130
root@localhost:~/bf-bsp-lts/build# make -j15 install
```
The defaut value of variables listed below if none of them passed by cmake CLI:

`OS_NAME=Debian`, `OS_VERSION=9`, `SDE_VERSION=9130`

#### <a name="build-bsp-via-an-outdated-way"></a>Build BSP via an Outdated Way

Skip to `Generate Configuration Variables` if `1st` building method is choosed. Otherwise, the `2nd` one, which will be disabled at `the end of July 2024`, is:

Optionally changes to fit your SDE manually:
```
root@localhost:~/bf-bsp-lts# vi drivers/include/bf_pltfm_types/bf_pltfm_types.h +34
Modify SDE_VERSION  to value '990','9110','9120'or '9130' ...
```
TIPS: In order to be compatible with different SDE versions and different base systems, we have introduced two macro variables in the BSP, one is `SDE_VERSION` and the other is `OS_VERSION`.
They are defined in `$BSP/drivers/include/bf_pltfm_types/bf_pltfm_type.h`. Amend `SDE_VERSION` to the one you're using.


Build and Install
```
root@localhost:~/bf-bsp-lts# ./autogen.sh
root@localhost:~/bf-bsp-lts# ./configure --prefix=$SDE_INSTALL --enable-thrift
root@localhost:~/bf-bsp-lts# make -j7 install
```

To be highlighted, during the evolution of SDE, the installation paths of the third party dependencies have been changed, so did the generated dependencies. Therefore, a minor changes in bsp sources must be done to accommodate those changes. For SDE version equal with  or higher than `9.9.x`, the changes as following:
```
root@localhost:~# mkdir /usr/local/include
root@localhost:~# ln -s $SDE_INSTALL/include/thrift/ /usr/local/include/thrift
```


Finally, `libasterfusionbf*`, `libplatform_thrift*`, `libpltfm_driver*`, `libpltfm_mgr*` will be installed to `$SDE_INSTALL/lib`, and all headers exposed by bsp will be installed to `$SDE_INSTALL/include`.

### <a name="launch"></a>Launch

#### <a name="generate-launching-variables"></a>Generate Launching Variables

BSP requires a set of variable entries before launch. Those entries are generated by `xt-cfgen.sh` and written to `/etc/platform.conf`.

```
root@localhost:~# xt-cfgen.sh
Notice: Start detecting and make sure that the switchd is not running
Loading bf_kdrv ...
...
It looks like x532p-t detected.
...
Generate /etc/platform.conf
Done
...
```
*xt-cfgen.sh will do 2 things: 1) Load all required kernel drivers, including bf_kdrv and i2c_kdrv; 2) Generate /etc/platform.conf which required by bsp. If /etc/platform.conf already existed, xt-cfgen.sh will skip generate it but only load required kernel drivers. It is recommended to run xt-cfgen.sh at least once after every boot.*

#### <a name="launch-x-t-platforms"></a>Launch X-T Platforms
A p4 prog named `diags.p4` is intergrated by default, you can launch it freely.

```
root@localhost:~# run_switchd.sh -p diag
Using SDE /usr/local/sde/bf-sde-9.x.y
Using SDE_INSTALL /usr/local/sde
Setting up DMA Memory Pool
Using TARGET_CONFIG_FILE /usr/local/sde/share/p4/targets/tofino/diag.conf
...
```

#### <a name="launch-x-t-platforms-tofino2-based"></a>Launch X-T Platforms (Tofino2 based)
```
root@localhost:~# run_switchd.sh -p diag --arch tf2
...
Using TARGET_CONFIG_FILE /usr/local/sde/share/p4/targets/tofino2/diag.conf
...
```
*For Third-party integration, please copy $BSP/platforms/asterfusion-bf/src/platform_mgr/pltfm_bd_map_xxx.json to your $SDE_INSTALL/share/platforms/board-maps/asterfusion/ before running.*

## <a name="state-machine"></a>State Machine

![fsm](docs/sfp-fsm.jpg "Figure 2: QSFP/SFP State Machine")

__Figure 2: QSFP/SFP State Machine__


## <a name="qa"></a>Q&A

More information or helps, please visit [Asterfusion](https://help.cloudswit.ch/portal/en/home).
