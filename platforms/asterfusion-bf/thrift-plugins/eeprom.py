#!/usr/bin/python

import importlib
import time
import os
import re
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

def thriftSetup():
    global thrift_server, transport, pltfm_mgr
    transport = TSocket.TSocket(thrift_server, 9090)

    transport = TTransport.TBufferedTransport(transport)
    bprotocol = TBinaryProtocol.TBinaryProtocol(transport)

    pltfm_mgr_client_module = importlib.import_module(".".join(["pltfm_mgr_rpc", "pltfm_mgr_rpc"]))
    pltfm_mgr_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_mgr_rpc")
    pltfm_mgr = pltfm_mgr_client_module.Client(pltfm_mgr_protocol)

    transport.open()
    #transport_diag.open()

def pltfm_mgr_eeprom_show():
    global pltfm_mgr
    '''
    '''
    eeprom = pltfm_mgr.pltfm_mgr_sys_eeprom_get()
    print "name: %s" % eeprom.prod_name
    print "part #: %s" % eeprom.prod_part_num
    print "product serial #: %s" % eeprom.prod_ser_num
    print "ext mac address: %s" % eeprom.ext_mac_addr
    print "system mfg date: %s" % eeprom.sys_mfg_date
    print "product version: %s" % eeprom.prod_ver
    print "product subversion: %s" % eeprom.prod_sub_ver
    print "arch: %s" % eeprom.prod_arch
    print "onie: %s" % eeprom.onie_version
    print "MAC ext size: %s" % eeprom.ext_mac_addr_size
    print "system mfger: %s" % eeprom.sys_mfger
    print "country: %s" % eeprom.country_code
    print "vendor: %s" % eeprom.vendor_name
    print "diag ver: %s" % eeprom.diag_version
    print "serv tag: %s" % eeprom.serv_tag
    print "ASIC: %s" % eeprom.asic_vendor
    print "main board: %s" % eeprom.main_bd_version
    print "DH board: %s" % eeprom.come_version
    print "ghc_bd0: %s" % eeprom.ghc_bd0
    print "ghc_bd1: %s" % eeprom.ghc_bd1
    print "crc32: %x" % eeprom.crc32

def thriftTeardown():
    global transport
    transport.close()


thriftSetup()
pltfm_mgr_eeprom_show()
thriftTeardown()
