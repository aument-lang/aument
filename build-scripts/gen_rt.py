# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import re
import sys
import subprocess
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--global-file', type=str)
parser.add_argument('--output', type=str)
parser.add_argument('--ident', type=str)
parser.add_argument('--files', type=str, nargs='*')
parser.add_argument('--cpp', type=str, nargs='*')
args = parser.parse_args()

global_file = args.global_file
output = args.output
ident = args.ident
files = args.files
total = ""
for fn in files:
    with open(fn, "r") as f:
        total += f.read()
        total += '\n'

cpp_args = [
    'cpp',
    '-P'
]
if args.cpp:
    for i in args.cpp:
        cpp_args.append('-' + i)

cpp_output = subprocess.check_output(cpp_args, input=total.encode('utf-8'))
total = cpp_output.decode('utf-8')
total = re.sub(r'^\s+', '', total)
total = re.sub(r'\s+', ' ', total)
if global_file:
    with open(global_file, "r") as f:
        total = f.read() + '\n' + total

total_bytes = total.encode('ascii')
values = ','.join(map(str, total_bytes))
with open(output, "w") as f:
    f.write("#include <stdlib.h>\n")
    f.write("const char %s[] = {%s};\n" % (ident, values))
    f.write("const size_t %s_LEN = %d;\n" % (ident, len(total_bytes)))
