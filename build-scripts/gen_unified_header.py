# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import re
import sys
import subprocess
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str)
parser.add_argument('--files', type=str, nargs='*')
parser.add_argument('--cpp', type=str, nargs='*')
args = parser.parse_args()

output = args.output
files = args.files
cpp = args.cpp
total = ""

for arg in cpp:
    if arg[0] == 'D':
        total += f"""\
#ifndef {arg[1:]}
#define {arg[1:]}
#endif"""

total += """\
#ifdef __cplusplus
extern "C" {
#endif
#define AU_IS_INTERPRETER
"""
for fn in files:
    with open("../"+fn, "r") as f:
        total += f.read()
        total += '\n'
total = total.replace("#pragma once", "")
total = re.sub(r'^#include "[^"]+"', '', total, flags=re.MULTILINE)
total = re.sub(r'^//.*', '', total, flags=re.MULTILINE)
total = re.sub(r'\n+','\n',total)
total += """
#ifdef __cplusplus
}
#endif
"""

total = """\
#ifndef _AUMENT_H
#define _AUMENT_H
%s
#endif /* _AUMENT_H */
""" % total

with open(output, "w") as f:
    f.write(total)
