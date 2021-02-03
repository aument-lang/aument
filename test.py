# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import subprocess

for fn in glob.glob('tests/*.out'):
    output = subprocess.check_output([
        './build/aulang',
        'run',
        fn[:-4] + '.au'
    ])
    with open(fn, "r") as f:
        output_chk = f.read().encode('utf-8')
        if output == output_chk:
            print("[OK] %s" % fn)
        else:
            print("[ERR] %s" % fn)
            print(" Expected %s, got %s" %(output_chk, output))