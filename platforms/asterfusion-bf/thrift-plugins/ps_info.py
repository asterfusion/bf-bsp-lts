#!/usr/bin/python

import importlib
import time
import os
import re
import sys
import pltfm_pm_rpc
import pltfm_mgr_rpc
#from pltfm_pm_rpc.ttypes import *
from pltfm_mgr_rpc.ttypes import *

from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from thrift.protocol import TMultiplexedProtocol

from argparse import ArgumentParser

thrift_server = 'localhost'
transport = None
pltfm_mgr = None

MAX_PS_NUM = 2

def thriftSetup():
    global thrift_server, transport, pltfm_mgr
    transport = TSocket.TSocket(thrift_server, 9090)

    transport = TTransport.TBufferedTransport(transport)
    bprotocol = TBinaryProtocol.TBinaryProtocol(transport)

    pltfm_mgr_client_module = importlib.import_module(".".join(["pltfm_mgr_rpc", "pltfm_mgr_rpc"]))
    pltfm_mgr_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_mgr_rpc")
    pltfm_mgr = pltfm_mgr_client_module.Client(pltfm_mgr_protocol)

    transport.open()

def pltfm_mgr_ps_info_get(ps_num):
    global pltfm_mgr

    ps_info = pltfm_mgr.pltfm_mgr_pwr_supply_info_get(ps_num)
    # DO NOT supported, by tsihang, 2022-03-28.
    #print "PS Model: %s" % ps_info.model
    #print "PS Serial #: %s" % ps_info.serial
    #print "PS revision: %s" % ps_info.rev
    print "VIN: %d.%d" % (ps_info.vin >> 8, ps_info.vin & 0x00FF)
    print "VOUT: %d.%d" % (ps_info.vout >> 8, ps_info.vout & 0x00FF)
    print "IIN: %d.%d" % (ps_info.iin >> 8, ps_info.iin & 0x00FF)
    print "IOUT: %d.%d" % (ps_info.iout >> 8, ps_info.iout & 0x00FF)
    print "PIN: %f" % ps_info.pwr_in
    print "POUT: %f" % ps_info.pwr_out

def print_usage():
    print "Usage: ps_info.py <power supply #>         "

def thriftTeardown():
    global transport
    transport.close()


thriftSetup()

argc = len(sys.argv)
if (argc == 1):
  print_usage()
  exit()

ps_num = int(sys.argv[1])

if ((ps_num > MAX_PS_NUM) | (ps_num <= 0)):
  print "Invalid Power supply #. Max %d" % MAX_PS_NUM
  print ""
  print_usage()
  exit()

pltfm_mgr_ps_info_get(ps_num)
thriftTeardown()
