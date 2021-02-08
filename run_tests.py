# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import subprocess

OK = "  [\033[1;32mOK\033[0m] %s"
ERR = "  [\033[1;31mERR\033[0m] %s"
oks, errs = 0, 0
files = glob.glob('tests/*.out')
print("Executing %d tests" % len(files))
for fn in files:
    try:
        output = subprocess.check_output([
            './build/aulang',
            'run',
            fn[:-4] + '.au'
        ])
    except subprocess.CalledProcessError as e:
        print(ERR % fn)
        print("    Error: %s", e)
        errs += 1
        continue
    with open(fn, "r") as f:
        output_chk = f.read().encode('utf-8')
        if output == output_chk:
            print(OK % fn)
            oks += 1
        else:
            print(ERR % fn)
            print("    Expected %s, got %s" %(output_chk, output))
            errs += 1
print("Result: %d ok, %d errors" % (oks, errs))