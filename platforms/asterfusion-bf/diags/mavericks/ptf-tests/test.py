# Copyright 2013-present Barefoot Networks, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Thrift PD interface basic tests
"""

from collections import OrderedDict

import time
import sys
import logging

import unittest
import random

import pd_base_tests
import pltfm_pm_rpc

from ptf import config
from ptf.testutils import *
from ptf.thriftutils import *

import os

from diag.p4_pd_rpc.ttypes import *
from res_pd_rpc.ttypes import *
from pltfm_pm_rpc.ttypes import *
from port_mapping import *

import random
import pdb
import subprocess

this_dir = os.path.dirname(os.path.abspath(__file__))

nop_mbr_hdl_set = False
nop_mbr_hdl = {'ipv4_routing_select':0,
               'tcam_indirect_action':0}

dev_id = 0
MAX_PORT_COUNT = 456

swports = []
for device, port, ifname in config["interfaces"]:
    swports.append(port)
    swports.sort()

if swports == []:
    for count in range(1,11):
        swports.append(count*4)

def tofLocalPortToOfPort(port, dev_id):
    assert port < MAX_PORT_COUNT
    return (dev_id * MAX_PORT_COUNT) + port

def check_and_execute_program(command):
    program = command[0]
    for path in os.environ["PATH"].split(os.pathsep):
        path = path.strip('"')
        exe_file = os.path.join(path, program)
        if os.path.isfile(exe_file) and os.access(exe_file, os.X_OK):
            print "swutil file path is ", exe_file
            return subprocess.call(command)
    print >> sys.stderr, "Cannot find %s in PATH(%s)" % (program,
                                                         os.environ["PATH"])
    sys.exit(1)

def get_exe_path(command):
    # look for the exe file
    program = command[0]
    for path in os.environ["PATH"].split(os.pathsep):
        path = path.strip('"')
        exe_file = os.path.join(path, program)
        if os.path.isfile(exe_file) and os.access(exe_file, os.X_OK):
            print "bf_switchd file path is ", exe_file
            return path
    print >> sys.stderr, "Cannot find %s in PATH(%s)" % (program,
                                                         os.environ["PATH"])
    sys.exit(1)

def portToPipe(port):
    return port >> 7

def portToPipeLocalId(port):
    return port & 0x7F

def portToBitIdx(port):
    pipe = portToPipe(port)
    index = portToPipeLocalId(port)
    return 72 * pipe + index

def set_port_map(indicies):
    bit_map = [0] * ((288+7)/8)
    for i in indicies:
        index = portToBitIdx(i)
        bit_map[index/8] = (bit_map[index/8] | (1 << (index%8))) & 0xFF
    return bytes_to_string(bit_map)

def check_for_swutil_output():
    searchfile = open("/tmp/mav_diag_result", "r")
    count = 0
    for line in searchfile:
        count = count + 1
    searchfile.close()
    assert(count > 0)

def check_for_swutil_success():
    print
    result = 0
    searchfile = open("/tmp/mav_diag_result", "r")
    for line in searchfile:
        if "Success" in line:
            result = 1
            return 0
    searchfile.close()
    assert(result == 1)

def set_lag_map(indicies):
    bit_map = [0] * ((256+7)/8)
    for i in indicies:
        bit_map[i/8] = (bit_map[i/8] | (1 << (i%8))) & 0xFF
    return bytes_to_string(bit_map)

def UntagVlanFlowPkt(self, vlan_id, tag_ports, untag_ports):
    print
    first_port = untag_ports[0]
    second_port = untag_ports[1]
    print "Sending packet port first -> second (1.1.1.1 -> 10.0.0.1 [id = 101])"
    pkt1 = simple_tcp_packet(eth_dst='00:00:00:44:44:44',
                             eth_src='00:00:00:55:55:55',
                             ip_dst='10.2.0.1',
                             ip_ttl=64,
                              pktlen=100)
    pkt1_tag = simple_tcp_packet(eth_dst='00:00:00:44:44:44',
                                 eth_src='00:00:00:55:55:55',
                                 ip_dst='10.2.0.1',
                                 ip_ttl=64,
                                 dl_vlan_enable=True,
                                 vlan_vid=vlan_id,
                                 pktlen=104)
    print "Sending on port ", first_port
    send_packet(self, tofLocalPortToOfPort(first_port, dev_id), str(pkt1))
    egress_ports = []
    egress_pkts = []
    for p in tag_ports:
        egress_ports.append(p)
        egress_pkts.append(pkt1_tag)
    for p in untag_ports:
        if p != first_port:
            egress_ports.append(p)
            egress_pkts.append(pkt1)
    print "Egress ports are ", egress_ports
    verify_each_packet_on_each_port(self, egress_pkts, egress_ports)
    # Allow learn to happen sleep for sometime
    time.sleep(4)
    print "Sending packet second -> first (10.0.0.1 -> 1.1.1.1 [id = 101])"
    pkt2 = simple_tcp_packet(eth_dst='00:00:00:55:55:55',
                            eth_src='00:00:00:44:44:44',
                            ip_src='10.0.0.1',
                            ip_dst='1.1.1.1',
                            ip_id=101,
                            ip_ttl=64)
    send_packet(self, tofLocalPortToOfPort(second_port, dev_id), str(pkt2))
    verify_packets(self, pkt2, [first_port])
    # Allow learn to happen sleep for sometime
    time.sleep(4)
    # Send unicast pkt now
    print "Sending packet first -> second (1.1.1.1 -> 10.0.0.1 [id = 101])"
    send_packet(self, tofLocalPortToOfPort(first_port, dev_id), str(pkt1))
    verify_packets(self, pkt1, [second_port])
    verify_no_other_packets(self)
    time.sleep(25)

def TagVlanFlowPkt(self, vlan_id, tag_ports, untag_ports):
    print
    first_port = tag_ports[0]
    second_port = tag_ports[1]
    print "Sending packet port first -> second (1.1.1.1 -> 10.0.0.1 [id = 101])"
    pkt1 = simple_tcp_packet(eth_dst='00:00:00:66:66:66',
                            eth_src='00:00:00:77:77:77',
                            dl_vlan_enable=True,
                            vlan_vid=vlan_id,
                            ip_dst='10.0.0.1',
                            ip_id=101,
                            ip_ttl=64,
                            pktlen=110)
    pkt1_untag = simple_tcp_packet(eth_dst='00:00:00:66:66:66',
                                eth_src='00:00:00:77:77:77',
                                ip_dst='10.0.0.1',
                                ip_id=101,
                                ip_ttl=64,
                                pktlen=106)

    print "Sending on port ", first_port
    send_packet(self, tofLocalPortToOfPort(first_port, dev_id), str(pkt1))
    egress_ports = []
    egress_pkts = []
    for p in tag_ports:
        if p != first_port:
            egress_ports.append(p)
            egress_pkts.append(pkt1)
    for p in untag_ports:
        egress_ports.append(p)
        egress_pkts.append(pkt1_untag)
    print "Egress ports are ", egress_ports
    verify_each_packet_on_each_port(self, egress_pkts, egress_ports)

    # Allow learn to happen sleep for sometime
    time.sleep(4)
    print "Sending packet second -> first (10.0.0.1 -> 1.1.1.1 [id = 101])"
    pkt2 = simple_tcp_packet(eth_dst='00:00:00:77:77:77',
                            eth_src='00:00:00:66:66:66',
                            dl_vlan_enable=True,
                            vlan_vid=vlan_id,
                            ip_src='10.0.0.1',
                            ip_dst='1.1.1.1',
                            ip_id=101,
                            ip_ttl=64)
    send_packet(self, tofLocalPortToOfPort(second_port, dev_id), str(pkt2))
    verify_packets(self, pkt2, [first_port])
    # Allow learn to happen sleep for sometime
    time.sleep(4)
    # Send unicast pkt now
    print "Sending packet first -> second (1.1.1.1 -> 10.0.0.1 [id = 101])"
    send_packet(self, tofLocalPortToOfPort(first_port, dev_id), str(pkt1))
    verify_packets(self, pkt1, [second_port])
    verify_no_other_packets(self)
    time.sleep(25)

@group('vlan')
class TestSwutilUnTagVlanFlow(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        vlan_id = 21
        # dev-ports
        tag_ports = [swports[0],swports[1],swports[2]]
        untag_ports = [swports[3],swports[4],swports[5]]
        swutilscript = 'swutil'
        # 1,2,3 are tagged and 4,5,6 are untagged
        # Dev-port to front port mapping is different on hw and harlyn-model
        if test_param_get('target') == "hw":
            # dev-port 4 maps to conn 18 channel 0
            check_and_execute_program([swutilscript, 'vlan create 21 pbm=18-20 ubm=21-23'])
        else:
            check_and_execute_program([swutilscript, 'vlan create 21 pbm=2-4 ubm=5-7'])
        check_and_execute_program([swutilscript, 'vlan show 21'])
 
        try:
            UntagVlanFlowPkt(self, vlan_id, tag_ports, untag_ports)

        finally:
            print "deleting entries"
            check_and_execute_program([swutilscript, 'vlan destroy 21'])


@group('vlan')
class TestSwutilTagVlanFlow(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        vlan_id = 31
        swutilscript = 'swutil'
        tag_ports = [swports[0],swports[1],swports[2]]
        untag_ports = [swports[3],swports[4],swports[5]]
        # 1,2,3 are tagged and 4,5,6 are untagged
        # Dev-port to front port mapping is different on hw and harlyn-model
        if test_param_get('target') == "hw":
            check_and_execute_program([swutilscript, 'vlan create 31 pbm=18-20 ubm=21-23'])
        else:
            check_and_execute_program([swutilscript, 'vlan create 31 pbm=2-4 ubm=5-7'])
        check_and_execute_program([swutilscript, 'vlan show 31'])

        try:
            TagVlanFlowPkt(self, vlan_id, tag_ports, untag_ports)

        finally:
            print "deleting entries"
            check_and_execute_program([swutilscript, 'vlan destroy 31'])


@group('notext')
class TestSwutilShowCmds(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        check_and_execute_program([swutilscript, 'ps'])
        check_for_swutil_output()
        check_and_execute_program([swutilscript, 'phy info'])
        check_for_swutil_output()
        check_and_execute_program([swutilscript, 'show pmap'])
        check_for_swutil_output()
        check_and_execute_program([swutilscript, 'show temp'])
        check_for_swutil_output()
        check_and_execute_program([swutilscript,
                'phy diag 1 feyescan u=1 if=line lane=0'])
        check_for_swutil_output()
        check_and_execute_program([swutilscript, 'version'])
        check_for_swutil_output()


@group('notext')
class TestSwutilLed(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        check_and_execute_program([swutilscript, 'led red'])
        check_and_execute_program([swutilscript, 'led_port'])
        check_and_execute_program([swutilscript, 'led off'])
        check_and_execute_program([swutilscript, 'led blue'])
        check_and_execute_program([swutilscript, 'led green'])
        check_and_execute_program([swutilscript, 'led red 24'])
        check_and_execute_program([swutilscript, 'led_port 1'])


@group('notext')
class TestSwutilExtLoopback(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        num_ports=4
        if test_param_get('target') == "hw":
	    self.platform_type = "mavericks"
	    board_type = self.pltfm_pm.pltfm_pm_board_type_get()
	    if re.search("0x0234|0x1234|0x4234", hex(board_type)):
		self.platform_type = "mavericks"
	    elif re.search("0x2234|0x3234", hex(board_type)):
		self.platform_type = "montara"
            start_port = getFrontPanelPort(self.platform_type, swports[13])
            num_ports=2
        else:
            # Dev-port to front port mapping is different on hw and harlyn-model
            # getFrontPanelPort API does not work for harlyn-model
            start_port=8
            num_ports=4

        print "Front panel Start port: ", start_port
        print "Num ports: ", num_ports
        # specify front-port to swutil. Ports.json has dev ports
        check_and_execute_program([swutilscript, 'linespeed init ' + str(start_port) + ' ' + str(num_ports) + ' EXT2'])
        check_for_swutil_success()
        # test-time=6 seconds, pkt size = 300, num_pkts=1
        check_and_execute_program([swutilscript, 'linespeed run 6 300 1'])
        check_for_swutil_success()
        check_and_execute_program([swutilscript, 'linespeed stop'])
        check_for_swutil_success()

        try:
            ret = check_and_execute_program([swutilscript, 'linespeed show'])
            assert (ret == 0)

        finally:
            print "Cleanup linespeed command"
            check_and_execute_program([swutilscript, 'linespeed end'])
            check_for_swutil_success()

@group('notext')
class TestSwutilDrvReload(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        # ptf-tests are run remotely in real HW and this test will not work
        if test_param_get('target') == "hw":
            return
        # Skip this test as creating/destroying process has issues through ptf
        return
        print
        swutilscript = 'swutil'
        switchd = 'bf_switchd'
        exe_path = get_exe_path([switchd]);
        print "Exe location is ", exe_path
        print
        install_path = '--install-path=' + exe_path + '/../'
        # shutdown drivers
        print "Shutdown drivers"
        check_and_execute_program([swutilscript, '-C', '-s'])
        time.sleep(5)
        # start drivers
        print "Start drivers"
        check_and_execute_program([swutilscript, '-C', '-S', install_path])
        time.sleep(100)
        # restart drivers
        print "Re-start drivers"
        check_and_execute_program([swutilscript, '-C', '-r', install_path])
        time.sleep(100)


@group('notext')
class TestSwutilPortCmds(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        # Speed
        check_and_execute_program([swutilscript, 'port 1 speed=40000'])
        check_and_execute_program([swutilscript, 'port 1 speed=100000'])
        # Autoneg
        check_and_execute_program([swutilscript, 'xport autoneg 1 ON'])
        check_and_execute_program([swutilscript, 'xport autoneg 1 DEFAULT'])
        # FEC
        check_and_execute_program([swutilscript, 'xport FEC 2 ON'])
        check_and_execute_program([swutilscript, 'xport FEC 2 OFF'])
 

@group('notext')
class TestSwutilPreEmphasis(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        # Pre-emphasis
        check_and_execute_program([swutilscript, 'phy 6 CL93N72_UT_CTL2r.1 CL93N72_TXFIR_PRE=1'])
        check_and_execute_program([swutilscript, 'phy 6 CL93N72_UT_CTL3r.1 CL93N72_TXFIR_MAIN=2'])
        check_and_execute_program([swutilscript, 'phy 6 CL93N72_UT_CTL2r.1 CL93N72_TXFIR_POST=3'])


@group('notext')
class TestSwutilPrbs(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, ["diag"])

    """ Basic test """
    def runTest(self):
        print
        swutilscript = 'swutil'
        # Prbs
        check_and_execute_program([swutilscript, 'phy diag 5 prbs set p=0'])
        check_and_execute_program([swutilscript, 'phy diag 5 prbs show'])
        check_and_execute_program([swutilscript, 'phy diag 5 prbs clean'])

