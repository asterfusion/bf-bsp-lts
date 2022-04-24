#!/bin/sh

# Added by tsihang, 2021-06-30
chmod u+x ./gitver.sh
./gitver.sh platforms/asterfusion-bf/include/ version.h

exec autoreconf -fi -v
