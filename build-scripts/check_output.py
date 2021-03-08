# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import argparse
import glob
import subprocess
import os

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=str)
parser.add_argument('--path', type=str)
args = parser.parse_args()

out_extension = '.out'
out_extension_len = len(out_extension)

for out_path in glob.glob(args.path + '/*' + out_extension):
    program_path = out_path[:-out_extension_len] + '.au'
    print(f"Checking {program_path}")
    with open(out_path, "rb") as fout:
        expected_output = fout.read()
    output = subprocess.check_output([
        args.binary,
        'run',
        program_path
    ])
    assert(output == expected_output)