#!/usr/bin/env python3

#
# Copyright(c) 2012-2021 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#
import os
import sys
import random

if not os.path.exists('config'):
    os.mkdir('config')

with open("config/random.cfg", "w") as f:
    f.write(str(random.randint(0, sys.maxsize)))
