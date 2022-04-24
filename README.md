## About this repository

Mainline  **all-in-one** repository for all TF1 based **X-T Switch** powered by Asterfusion with Long Term Support.
Current supported X-T switches:

  - `X564P-T`,  64x 100G and auxiliary 2x 25G.
  - `X532P-T`,  32x 100G and auxiliary 2x 25G.
  - `X312P-T`,  12x 100G, 48x 25G, and  auxiliary 2x 25G.
  - `X308P-T`,  08x 100G, 48x 25G, comming soon.

Sometimes, we call `X564P-T` and `X532P-T` as `X5-T`, and call `X312P-T` and `X308P-T` as `X3-T`.

## About the compatibilities to different P4Studio version

A few APIs, which exposed to and called by BSP, are changed between lower version (< 9.3) of P4Studio and higher version (>= 9.3) of P4Studio.

For this repository, default to support higher version of P4Studio.

If you want to compile with P4Studio under `9.3`, please let BSP know your working P4Studio version by editing macro `SDE_VERSION`, which you can find in `bf-bsp-8.9.x/drivers/include/bf_pltfm_types/bf_pltfm_types.h`.


## Special dependency.

There're some differents between `X5-T` and `X3-T`. To this `all-in-one` repository, the full special dependencies are needed to compile with.

Here are the special dependencies:

  - `nct6779d`, which needed by `X312P-T`.
  - `cgoslx`, which needed by `X5-T`.

Please install the dependencies first from sources before compile this repository. You can find the sources in [asternos.dev](https://asternos.dev/xt).

## Quick Start

All your files and folders are presented as a tree in the file explorer. You can switch from one to another by clicking a file in the tree.
The building of drivers produces a set of libraries that need to be loaded (or linked to) the application.
Here're the steps to build and install the <bf-platforms> package:
```
​cd bf-bsp-8.9.x
./autogen.sh
​./configure --prefix=$SDE_INSTALL [--enable-thrift]
​make -j7
​make install
```
After make & make install done, the `libasterfusionbf*`, `libplatform_thrift*`, `libpltfm_driver*`, `libpltfm_mgr*` and `libtcl_server*` will be installed to `$SDE_INSTALL/lib`, and all the header files exposed by bsp will be installed to `$SDE_INSTALL/include`.


## Q&A

Looking forward your questions by emailing TSIHANG (tsihang@asterfusion.com).
