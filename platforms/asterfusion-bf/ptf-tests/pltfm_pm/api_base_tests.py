"""
Base classes for test cases

Tests will usually inherit from one of these classes to have the controller
and/or dataplane automatically set up.
"""

import importlib
import os
import logging
import unittest

import ptf
from ptf.base_tests import BaseTest
from ptf import config
import ptf.dataplane as dataplane

################################################################
#
# Thrift interface base tests
#
################################################################

#import pltfm_pm_rpc.pltfm_pm_rpc as pltfm_pm_rpc
#from pltfm_pm_rpc.ttypes import *
#from pltfm_mgr_rpc.ttypes import *
from thrift.transport import TSocket
from thrift.transport import TTransport
from thrift.protocol import TBinaryProtocol
from thrift.protocol import TMultiplexedProtocol

class ThriftInterface(BaseTest):

    def setUp(self):
        BaseTest.setUp(self)

        # Set up thrift client and contact server
        self.transport = TSocket.TSocket('localhost', 9095)
        self.transport = TTransport.TBufferedTransport(self.transport)
        bprotocol = TBinaryProtocol.TBinaryProtocol(self.transport)

  	try:
	    self.pltfm_pm_rpc_client_module = importlib.import_module(".".join(["pltfm_pm_rpc", "pltfm_pm_rpc"]))
	except:
	    self.pltfm_pm_rpc_client_module = None
	try:
            self.pltfm_mgr_rpc_client_module = importlib.import_module(".".join(["pltfm_mgr_rpc", "pltfm_mgr_rpc"]))
	except:
	    self.pltfm_mgr_rpc_client_module = None

	self.pltfm_pm_rpc_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_pm_rpc")
	self.pltfm_mgr_rpc_protocol = TMultiplexedProtocol.TMultiplexedProtocol(bprotocol, "pltfm_mgr_rpc")
        
	self.pltfm_pm_rpc = self.pltfm_pm_rpc_client_module.Client(self.pltfm_pm_rpc_protocol) 
	self.pltfm_mgr_rpc = self.pltfm_mgr_rpc_client_module.Client(self.pltfm_mgr_rpc_protocol) 
  
        self.transport.open()

    def tearDown(self):
        if config["log_dir"] != None:
            self.dataplane.stop_pcap()
        BaseTest.tearDown(self)
        self.transport.close()

class ThriftInterfaceDataPlane(ThriftInterface):
    """
    Root class that sets up the thrift interface and dataplane
    """
    def setUp(self):
        self.client = ThriftInterface.setUp(self)
        self.dataplane = ptf.dataplane_instance
        self.dataplane.flush()
        if config["log_dir"] != None:
            filename = os.path.join(config["log_dir"], str(self)) + ".pcap"
            self.dataplane.start_pcap(filename)
	return self.client

    def tearDown(self):
        if config["log_dir"] != None:
            self.dataplane.stop_pcap()
        ThriftInterface.tearDown(self)
