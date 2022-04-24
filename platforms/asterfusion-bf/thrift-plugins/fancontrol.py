#!/usr/bin/python

import importlib
import time
import os
import re
import sys
import pltfm_pm_rpc
import pltfm_mgr_rpc
from pltfm_pm_rpc.ttypes import *
from pltfm_mgr_rpc.ttypes import *

from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from thrift.protocol import TMultiplexedProtocol

from argparse import ArgumentParser

thrift_server = 'localhost'
transport = None
pltfm_mgr = None

MAX_FAN = 10
def thriftSetup():
    global thrift_server, transport, pltfm_mgr
    transport = TSocket.TSocket(thrift_server, 9090)

    transport = TTransport.TBufferedTransport(transport)
    bprotocol = TBinaryProtocol.TBinaryProtocol(transport)

    pltfm_mgr_client_module = importlib.import_module(".".join(["pltfm_mgr_rpc", "pltfm_mgr_rpc"]))
    pltfm_mgr_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_mgr_rpc")
    pltfm_mgr = pltfm_mgr_client_module.Client(pltfm_mgr_protocol)

    transport.open()

def fan_speed_set(fan, percent):
    global pltfm_mgr
    pltfm_mgr.pltfm_mgr_fan_speed_set(fan, percent)


def fan_speed_info_get():
    global pltfm_mgr

    for fan_num in range(1,MAX_FAN+1):
      fan_info = pltfm_mgr.pltfm_mgr_fan_info_get(fan_num);
      if (fan_info.fan_num == fan_num):
        print "fan number: %d front rpm: %d rear rpm: %d percent: %d%% " % (fan_info.fan_num, fan_info.front_rpm, fan_info.rear_rpm, fan_info.percent)

def print_usage():
    print "Usage: fancontrol.py <function> <param list>         "
    print "           function: fan_speed_set <fan #> <percent> "
    print "                     fan_speed_info_get              "


def thriftTeardown():
    global transport
    transport.close()


thriftSetup()

argc = len(sys.argv)
if (argc == 1):
  print_usage()
  exit()

if (sys.argv[1] == "fan_speed_set"):
  if (argc != 4):
    print_usage()
    exit()

  fan = int(sys.argv[2])
  percent = int(sys.argv[3])

  if ((fan > MAX_FAN) | (fan < 0)):
    print "Invalid value for fan #."
    print ""
    print_usage()
    exit()

  if ((percent > 100) | (percent < 0)):
    print "Invalid value for precent"
    print ""
    print_usage()
    exit()

  fan_speed_set(fan, percent)
  exit()

if (sys.argv[1] == "fan_speed_info_get"):
  fan_speed_info_get()
  exit()

print_usage()

thriftTeardown()
