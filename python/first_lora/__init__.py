#
# Copyright 2008,2009 Free Software Foundation, Inc.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

# The presence of this file turns this directory into a Python package

'''
This is the GNU Radio FIRST_LORA module. Place your Python package
description here (python/__init__.py).
'''
import os

# import pybind11 generated symbols into the first_lora namespace
try:
    # this might fail if the module is python-only
    from .first_lora_python import *
except ModuleNotFoundError:
    pass

# import any pure python here
#
