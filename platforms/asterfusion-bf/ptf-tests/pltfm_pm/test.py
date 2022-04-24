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
Platform-Port Manager Thrift interface basic tests
"""

from collections import OrderedDict

import time
import sys
import logging
import unittest
import random
import os
import pltfm_pm_rpc
import random
import pdb
import api_base_tests
from pltfm_pm_rpc.ttypes import *

from ptf import config
from ptf.testutils import *
from ptf.thriftutils import *


class TestPlatformPm(api_base_tests.ThriftInterfaceDataPlane):
    def setUp(self):
	api_base_tests.ThriftInterfaceDataPlane.setUp(self)

    """ Basic test """
    def runTest(self):
	bd_id = self.pltfm_pm_rpc.pltfm_pm_board_type_get();
	print "Board id is ", bd_id
	ret = 0
