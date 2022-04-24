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

def print_usage():
    print "Usage: sfputil.py <function> <param list>        "
    print "           function: sfp_dump_port <port>        "
    print "                     sfp_dump_all                "
    print "                     sfp_reset <port>            "
    print "                     sfp_get_presence            "
    print "                     get_low_power_mode <port>   "
    print "                     set_low_power_mode <port> <lp_mode: 0/1> "
    
def thriftSetup():
    global thrift_server, transport, pltfm_mgr
    transport = TSocket.TSocket(thrift_server, 9090)

    transport = TTransport.TBufferedTransport(transport)
    bprotocol = TBinaryProtocol.TBinaryProtocol(transport)

    pltfm_mgr_client_module = importlib.import_module(".".join(["pltfm_mgr_rpc", "pltfm_mgr_rpc"]))
    pltfm_mgr_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_mgr_rpc")
    pltfm_mgr = pltfm_mgr_client_module.Client(pltfm_mgr_protocol)

    transport.open()

def pltfm_mgr_sfp_dump(min_port, max_port):
    global pltfm_mgr

    for qsfp_port in range(min_port,max_port+1):
      print ""
      print ""

      qsfp_present = pltfm_mgr.pltfm_mgr_qsfp_presence_get(int(qsfp_port))
      print "SFP # %s : Present : %s" % (qsfp_port, qsfp_present)

      if (qsfp_present == False):
        continue

      sfp_info_r = pltfm_mgr.pltfm_mgr_qsfp_info_get(int(qsfp_port))
      sfp_info_l = re.findall('..?', sfp_info_r)

      print "Power set:  0x%s\tExtended ID:  0x%s\tEthernet Compliance:  0x%s" % (sfp_info_l[93], sfp_info_l[129], sfp_info_l[131])
      print "Extended Ethernet Compliance:  0x%s" % sfp_info_l[192]

      print "TX disable bits: 0x%s" % sfp_info_l[86]
      print "TX rate select bits: 0x%s" % sfp_info_l[88]

      print "Connector: 0x%s" % sfp_info_l[130]
      print "Spec compliance: 0x%s 0x%s 0x%s 0x%s 0x%s 0x%s 0x%s 0x%s" %\
               (sfp_info_l[131],\
               sfp_info_l[132],\
               sfp_info_l[133],\
               sfp_info_l[134],\
               sfp_info_l[135],\
               sfp_info_l[136],\
               sfp_info_l[137],\
               sfp_info_l[138])
      print "Encoding: 0x%s" % sfp_info_l[139]
      print "Nominal Bit Rate: %d MBps" % (int(sfp_info_l[140], 16) * 100)
      print "Ext rate select compliance: 0x%s" % sfp_info_l[141]
      print "Length (SMF): %d km" % int(sfp_info_l[142], 16)
      print "Length (OM3): %d m" % (int(sfp_info_l[143], 16) * 2)
      print "Length (OM2): %d m" % int(sfp_info_l[144], 16)
      print "Length (OM1): %d m" % int(sfp_info_l[145], 16)
      print "Length (Copper): %d m" % int(sfp_info_l[146], 16)
      print "Device Tech: 0x%s" % sfp_info_l[147]
      print "Ext Module: 0x%s" % sfp_info_l[164]
      print "Wavelength tolerance: 0x%s 0x%s" % (sfp_info_l[188], sfp_info_l[189])
      print "Max case temp: %dC" % int(sfp_info_l[190], 16)
      print "CC_BASE: 0x%s" % sfp_info_l[191]
      print "Options: 0x%s 0x%s 0x%s 0x%s" % (sfp_info_l[192], sfp_info_l[193], sfp_info_l[194], sfp_info_l[195])
      print "DOM Type: 0x%s" % sfp_info_l[220]
      print "Enhanced Options: 0x%s" % sfp_info_l[221]
      print "Reserved: 0x%s" % sfp_info_l[222]
      print "CC_EXT: 0x%s" % sfp_info_l[223]
      print "Vendor Specific:"
      print "  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s" %\
               (sfp_info_l[224],\
               sfp_info_l[225],\
               sfp_info_l[226],\
               sfp_info_l[227],\
               sfp_info_l[228],\
               sfp_info_l[229],\
               sfp_info_l[230],\
               sfp_info_l[231],\
               sfp_info_l[232],\
               sfp_info_l[233],\
               sfp_info_l[234],\
               sfp_info_l[235],\
               sfp_info_l[236],\
               sfp_info_l[237],\
               sfp_info_l[238],\
               sfp_info_l[239])
      print "  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s" %\
               (sfp_info_l[240],\
               sfp_info_l[241],\
               sfp_info_l[242],\
               sfp_info_l[243],\
               sfp_info_l[244],\
               sfp_info_l[245],\
               sfp_info_l[246],\
               sfp_info_l[247],\
               sfp_info_l[248],\
               sfp_info_l[249],\
               sfp_info_l[250],\
               sfp_info_l[251],\
               sfp_info_l[252],\
               sfp_info_l[253],\
               sfp_info_l[254],\
               sfp_info_l[255])

      print "vendor: %s" % bytearray.fromhex(sfp_info_r[(148*2):(164*2)]).decode()
      print "Vendor OUI: %s:%s:%s" % (sfp_info_l[165 - 128], sfp_info_l[166 - 128], sfp_info_l[167 - 128])
      print "vendor PN: %s" % bytearray.fromhex(sfp_info_r[(168*2):(184*2)]).decode()
      print "vendor Rev: %s" % bytearray.fromhex(sfp_info_r[(184*2):(186*2)]).decode()
      print "vendor SN: %s" % bytearray.fromhex(sfp_info_r[(196*2):(212*2)]).decode()
      print "Date Code: %s" % bytearray.fromhex(sfp_info_r[(212*2):(220*2)]).decode()


def pltfm_mgr_sfp_dump_all():
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port();
    print "SFP Max Port %s" % qsfp_max_port
    pltfm_mgr_sfp_dump(1, qsfp_max_port)

def pltfm_mgr_sfp_dump_port(port_num):
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port();
    if ((port_num > qsfp_max_port) | (port_num < 1)):
      print "Invalid port: range: 1-%d" % qsfp_max_port
      print ""
      print_usage()
      exit()

    pltfm_mgr_sfp_dump(port_num, port_num)

def sfp_reset(port_num):
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port();
    if ((port_num > qsfp_max_port) | (port_num < 1)):
      print "Invalid port: range: 1-%d" % qsfp_max_port
      print ""
      print_usage()
      exit()

    status = pltfm_mgr.pltfm_mgr_qsfp_reset(port_num, True)
    status = pltfm_mgr.pltfm_mgr_qsfp_reset(port_num, False)


def sfp_get_presence():
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port()
    print "SFP Max Port %s" % qsfp_max_port

    for qsfp_port in range(1,qsfp_max_port+1):

      qsfp_present = pltfm_mgr.pltfm_mgr_qsfp_presence_get(int(qsfp_port))
      print "SFP # %s : Present : %s" % (qsfp_port, qsfp_present)


def get_low_power_mode(port_num):             
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port();
    if ((port_num > qsfp_max_port) | (port_num < 1)):
      print "Invalid port: range: 1-%d" % qsfp_max_port
      print ""
      print_usage()
      exit()

    lpmode = pltfm_mgr.pltfm_mgr_qsfp_lpmode_get(port_num)
    return lpmode


def set_low_power_mode(port_num, lpmode):                
    global pltfm_mgr

    qsfp_max_port = pltfm_mgr.pltfm_mgr_qsfp_get_max_port();
    if ((port_num > qsfp_max_port) | (port_num < 1)):
      print "Invalid port: range: 1-%d" % qsfp_max_port
      print ""
      print_usage()
      exit()

    if ((lpmode > 1) | (lpmode < 0)):
      print "Invalid mode: 0/1"
      print ""
      print_usage()
      exit()

    sts = pltfm_mgr.pltfm_mgr_qsfp_lpmode_set(port_num, lpmode)
    return sts


def thriftTeardown():
    global transport
    transport.close()


thriftSetup()
argc = len(sys.argv)
if (argc == 1):
  print_usage()
  exit()

if (sys.argv[1] == "sfp_dump_port"):
  if (argc < 3):
    print_usage()
    exit()

  port_num = int(sys.argv[2])
  pltfm_mgr_sfp_dump_port(port_num)
  exit()

if (sys.argv[1] == "sfp_dump_all"):
  pltfm_mgr_sfp_dump_all()
  exit()

if (sys.argv[1] == "sfp_reset"):
  if (argc < 3):
    print_usage()
    exit()

  port_num = int(sys.argv[2])
  sfp_reset(port_num)
  exit()

if (sys.argv[1] == "sfp_get_presence"):
  sfp_get_presence()
  exit()

if (sys.argv[1] == "get_low_power_mode"):
  if (argc < 3):
    print_usage()
    exit()

  port_num = int(sys.argv[2])
  mode = get_low_power_mode(port_num)
  print "Port: %d lp mode: %s" % (port_num, mode)
  exit()

if (sys.argv[1] == "set_low_power_mode"):
  if (argc < 4):
    print_usage()
    exit()

  port_num = int(sys.argv[2])
  lp_mode = int(sys.argv[3])
  set_low_power_mode(port_num, lp_mode)
  exit()

print_usage()

thriftTeardown()
