# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import subprocess
import multiprocessing
import os
import sys

binary = sys.argv[1]

def test(n):
    global binary
    subprocess.run([binary, str(n)])

num_tests = int(subprocess.check_output([binary, "-1"]).decode("utf-8"))
with multiprocessing.Pool() as p:
    threads = []
    for i in range(num_tests):
        threads.append(p.apply_async(test, (i,)))
    for thread in threads:
        thread.get()
