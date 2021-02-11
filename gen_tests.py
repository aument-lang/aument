# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import subprocess

files = glob.glob('tests/features/*.out')
n_tests = len(files)
c_src = """
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/bc.h"
#include "core/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/rt/au_array.h"
#include "core/vm.h"
"""

def gen_check_value(item, val_type, val_contents):
    if val_type == 'int':
        return f"assert(au_value_get_type({item}) == VALUE_INT && au_value_get_int({item}) == {val_contents});"
    elif val_type == 'float':
        return f"assert(au_value_get_type({item}) == VALUE_DOUBLE && au_value_get_double({item}) == {val_contents});"
    elif val_type == 'bool':
        return f"assert(au_value_get_type({item}) == VALUE_BOOL && au_value_get_bool({item}) == {'1' if val_contents == 'true' else '0'});"
    elif val_type == 'str':
        return f"assert(au_value_get_type({item}) == VALUE_STR && au_value_get_string({item})->len == {len(val_contents)} && memcmp(au_value_get_string({item})->data, \"{val_contents}\", {len(val_contents)}) == 0);\n"
    raise NotImplementedError(val_type)

for (i, fn) in enumerate(files):
    with open(fn[:-4] + '.au', 'r') as f:
        input_cont = f.read()
    with open(fn, 'r') as f:
        output_lines = map(lambda s: s.rstrip(), f.readlines())
    input_bytes = input_cont.encode('utf-8')
    input_len = len(input_bytes)
    input_array = ','.join(map(str, map(int, input_bytes)))

    c_src += f"""\
static int test_{i}_idx = 0;
static void test_{i}_check(au_value_t value) {{ switch(test_{i}_idx) {{
"""
    for val_idx, value in enumerate(output_lines):
        if value.startswith("array;"):
            array_contents = value.split(';')[1:]
            c_src += f"  case {val_idx}: {{\n"
            c_src += """\
    assert(au_value_get_type(value) == VALUE_STRUCT && au_value_get_struct(value)->vdata == &au_obj_array_vdata);
    struct au_obj_array *array = (struct au_obj_array *)au_value_get_struct(value);
"""
            for item_idx, item in enumerate(array_contents):
                val_type, val_contents = item.split(',')
                item = f"au_obj_array_get(array, au_value_int({item_idx}))"
                c_src += f"    {gen_check_value(item, val_type, val_contents)}\n"
            c_src += f"  }}\n"
            continue
        val_type, val_contents = value.split(';')
        c_src += f"  case {val_idx}: {{"
        c_src += gen_check_value("value", val_type, val_contents)
        c_src += f" break; }}\n"
    c_src += f"""\
    }}
    test_{i}_idx++;
}}
static void test_{i}() {{
    const char source[] = {{{input_array}}};
    struct au_program program;
    assert(au_parse(source, {input_len}, &program) != 0);
    struct au_vm_thread_local tl;
    au_vm_thread_local_init(&tl, &program.data);
    tl.print_fn = test_{i}_check;
    au_vm_exec_unverified_main(&tl, &program);
    au_program_del(&program);
    au_vm_thread_local_del(&tl);
}}\n\n"""

c_src += """
int main(int argc, char **argv) {
"""

for (i, fn) in enumerate(files):
    c_src += f"  printf(\"[{i+1}/{len(files)}] {fn}\\n\"); test_{i}();\n"

c_src += """
    return 0;
}
"""

with open("build/tests.c", "w") as f:
    f.write(c_src)