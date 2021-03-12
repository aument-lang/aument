# This source file is part of the Aument language
# Copyright (c) 2021 the aument contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information
import argparse
import glob
import subprocess
import multiprocessing
import os
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=str)
parser.add_argument('--path', type=str)
parser.add_argument('--file', type=str)
parser.add_argument('--check', type=str)
parser.add_argument('--param', type=str)
args = parser.parse_args()

out_extension = '.out'
out_extension_len = len(out_extension)

def check_output(out_path):
    global out_extension, out_extension_len
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

def check_with_input(out_path):
    global out_extension, out_extension_len
    program_path = out_path[:-out_extension_len] + '.au'
    input_path = out_path[:-out_extension_len] + '.in'
    print(f"Checking {program_path}")
    with open(out_path, "rb") as fout:
        expected_output = fout.read()
    with open(input_path, "rb") as finp:
        read_input = finp.read()
    output = subprocess.check_output([
        args.binary,
        'run',
        program_path
    ], input=read_input)
    assert(output == expected_output)

def check_comp(out_path):
    global out_extension, out_extension_len
    program_path = out_path[:-out_extension_len] + '.au'
    print(f"Checking {program_path}")
    with open(out_path, "rb") as fout:
        expected_output = fout.read()
    exe_out = tempfile.NamedTemporaryFile(mode='r', delete=False)
    exe_name = exe_out.name
    exe_out.close()
    subprocess.check_output([
        args.binary,
        'build',
        program_path,
        exe_name,
    ])
    output = subprocess.check_output([ exe_name ])
    assert(output == expected_output)
    try:
        os.remove(exe_name)
    except:
        pass

def check_comp_to_path(out_path):
    global out_extension, out_extension_len, args
    program_path = out_path[:-out_extension_len] + '.au'
    print(f"Checking {program_path}")
    with open(out_path, "rb") as fout:
        expected_output = fout.read()
    exe_name = args.param
    subprocess.check_output([
        args.binary,
        'build',
        program_path,
        exe_name,
    ])
    output = subprocess.check_output([ exe_name ])
    assert(output == expected_output)
    try:
        os.remove(exe_name)
    except:
        pass

def check_errors(out_path):
    program_path = out_path[:-out_extension_len] + '.au'
    print(f"Checking {program_path}")
    with open(out_path, "rb") as fout:
        expected_output = fout.read()
    try:
        subprocess.check_output([
            args.binary,
            'run',
            program_path
        ], stderr=subprocess.STDOUT)
        print(f"File {program_path} succeeded")
        exit(1)
    except subprocess.CalledProcessError as e:
        assert(e.output == expected_output)

check_fn = {
    "output": check_output,
    "with_input": check_with_input,
    "comp": check_comp,
    "errors": check_errors,
    "comp_to_path": check_comp_to_path,
}[args.check]

if args.file:
    check_fn(args.file)
else:
    with multiprocessing.Pool() as p: 
        threads = []
        for out_path in glob.glob(args.path + '/*' + out_extension):
            threads.append(
                p.apply_async(check_fn, (out_path,))
            )
        for thread in threads:
            thread.get()
    