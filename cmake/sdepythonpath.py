#!/usr/bin/env python3
#  INTEL CONFIDENTIAL
#
#  Copyright (c) 2022 Intel Corporation
#  All Rights Reserved.
#
#  This software and the related documents are Intel copyrighted materials,
#  and your use of them is governed by the express license under which they
#  were provided to you ("License"). Unless the License provides otherwise,
#  you may not use, modify, copy, publish, distribute, disclose or transmit this
#  software or the related documents without Intel's prior written permission.
#
#  This software and the related documents are provided as is, with no express or
#  implied warranties, other than those that are expressly stated in the License.

import sys
from distutils.sysconfig import get_python_lib
from pathlib import Path
from typing import Optional

_MY_DIR = Path(__file__).parent.resolve()
_IS_INSTALLED = _MY_DIR.name == 'bin'


def get_sde_python_dependencies(sde_install: Optional[Path], sde_dependencies: Optional[Path]) -> Path:
    if sde_install is None:
        sde_install = _MY_DIR.parent
    if sde_dependencies is None:
        environment_file = (sde_install / 'share/environment').read_text()
        variables = {k: v for k, v in (line.split('=', 1) for line in environment_file.splitlines())}
        sde_dependencies = variables['SDE_DEPENDENCIES']

    sde_dependencies = sde_install.joinpath(sde_dependencies)
    python_lib = get_python_lib(prefix='', standard_lib=True, plat_specific=True)
    return (sde_dependencies / python_lib / 'site-packages')


if __name__ != '__main__' and not _IS_INSTALLED:
    raise ImportError("only installed version of sdepythonpath can be imported")

if _IS_INSTALLED:
    SDE_PYTHON_DEPENDENCIES = get_sde_python_dependencies(None, None).as_posix()
    sys.path.insert(0, SDE_PYTHON_DEPENDENCIES)

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='Print path to python third-party dependencies')
    if _IS_INSTALLED:
        args = parser.parse_args()
        print(sys.path[0])
    else:
        parser.add_argument(
            '--sde-install',
            type=Path,
            metavar='DIR',
            required=True,
            help='Path to SDE Installation directory'
        )
        parser.add_argument(
            '--sde-dependencies',
            metavar='DIR',
            type=Path,
            required=True,
            help='Path to SDE Dependencies directory (can be relative to --sde-install)'
        )
        args = parser.parse_args()
        path = get_sde_python_dependencies(args.sde_install, args.sde_dependencies)
        print(path)
