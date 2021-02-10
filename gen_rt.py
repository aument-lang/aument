# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import re
import sys
import subprocess

global_file = sys.argv[1]
output = sys.argv[2]
ident = sys.argv[3]
files = sys.argv[4:]
total = ""
for fn in files:
    with open(fn, "r") as f:
        total += f.read()
        total += '\n'

cpp_output = subprocess.run([
    'cpp',
    '-P'
], input=total.encode('utf-8'), capture_output=True)

total = cpp_output.stdout.decode('utf-8')
total = re.sub(r'^\s+', '', total)
total = re.sub(r'\s+', ' ', total)
if global_file != '-':
    with open(global_file, "r") as f:
        total = f.read() + '\n' + total

total_bytes = total.encode('ascii')
values = ','.join(map(str, total_bytes))
with open(output, "w") as f:
    f.write("#include <stdlib.h>\n")
    f.write("const char %s[] = {%s};\n" % (ident, values))
    f.write("const size_t %s_LEN = %d;\n" % (ident, len(total_bytes)))
