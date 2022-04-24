#!/bin/bash
# Start running Asterfusion Diag compile

function print_help() {
  echo "USAGE: $(basename ""$0"") -s <bf-sde-path> -r <bf-reference-bsp-path>"
  echo "Options for running Asterfusion Diag compile:"
  echo "  -s bf-sde-path"
  echo "    Absolute path to bf-sde tgz file"
  echo "  -r bf-reference-bsp-path"
  echo "    Absolute path to bf-reference-bsp tgz file"
  echo "  -h"
  echo "    Print this message"
  exit 0
}

opts=`getopt -o s:r:h -- "$@"`
if [ $? != 0 ]; then
  exit 1
fi
eval set -- "$opts"

BF_SDE_PATH=""
BF_REFERENCE_BSP_PATH=""
HELP=false
while true; do
    case "$1" in
      -s) BF_SDE_PATH=$2; shift 2;;
      -r) BF_REFERENCE_BSP_PATH=$2; shift 2;;
      -h) HELP=true; shift 1;;
      --) shift; break;;
    esac
done

if [ $HELP = true ]; then
  print_help
fi

if [ -z $BF_SDE_PATH ] ; then
  echo "BF-SDE file does not exist"
  exit
fi

if [ -z $BF_REFERENCE_BSP_PATH ] ; then
  echo "BF-PLATFORMS file does not exist"
  exit
fi

echo "Using BF-SDE at ${BF_SDE_PATH}"
echo "Using BF-REFERENCE-BSP at ${BF_REFERENCE_BSP_PATH}"
# Extract build number from bf-sde file name
temp=$(echo ${BF_SDE_PATH} | sed 's/.*bf-sde-//')
buildNumber=$(echo ${temp} | sed 's/\(.*\).tgz/\1/')
echo "Using SDE build number ${buildNumber}"

# Create new directory for doing all operations
rm -rf bf_sde_pkg
mkdir bf_sde_pkg
cd bf_sde_pkg
export WORKSPACE=$(pwd)

# Extract SDE packages
cp $BF_SDE_PATH .
tar xvzf bf-sde*.tgz
cp $BF_REFERENCE_BSP_PATH .
tar xvzf bf-reference-bsp*.tgz
cd $WORKSPACE/bf-sde*
cd p4studio_build
./p4studio_build.py --use-profile profiles/diags_profile.yaml --bsp-path $WORKSPACE/bf-reference-bsp-$buildNumber --bf-diags-configure-options='--with-tofino P4PPFLAGS=-DDIAG_POWER_ENABLE' --bf-platforms-configure-options='--with-asterfusion-diags' --tofino-architecture='tofino'

export SDE=$WORKSPACE/bf-sde-$buildNumber

echo "Compile completed successfully"

# building tar file for Asterfusion
cd $SDE
mkdir -p bf_kdrv
cp $SDE/pkgsrc/bf-drivers/kdrv/bf_kdrv/bf_kdrv.c bf_kdrv/.
cp $SDE/pkgsrc/bf-drivers/kdrv/bf_kdrv/bf_kdrv.h bf_kdrv/.
cp $SDE/pkgsrc/bf-drivers/kdrv/bf_kdrv/bf_ioctl.h bf_kdrv/.
cp $SDE/pkgsrc/bf-drivers/kdrv/bf_kdrv/Makefile.standalone bf_kdrv/Makefile
tar cf bf-diag-image.tar ./install ./bf_kdrv --exclude='./install/bin/tofino-model' --exclude='./install/lib/python3.4'
cd $SDE/pkgsrc/bf-platforms/platforms/asterfusion-bf/diags/mavericks
tar -uf $SDE/bf-diag-image.tar swutil.c
tar -uf $SDE/bf-diag-image.tar README
cd $SDE
gzip bf-diag-image.tar
cp bf-diag-image.tar.gz $WORKSPACE/..
echo "Built tar.gz file successfully"
echo "Asterfusion diag image bf-diag-image.tar.gz is available in current directory"

