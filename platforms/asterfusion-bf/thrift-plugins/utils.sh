#!/bin/sh

export PYTHONPATH=./install/bin:./install/lib/python2.7/site-packages/:./install/lib/python2.7/site-packages/p4testutils/:./install/lib/python2.7/site-packages/tofino/:./install/lib/python2.7/site-packages/tofinopd/:$PYTHONPATH

if [ "$1" = "eeprom" ]
then
  ./eeprom.py
fi

if [ "$1" = "sensors" ]
then
  ./sensors.py
fi

