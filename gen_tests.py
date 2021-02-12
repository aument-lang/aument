# This source file is part of the aulang project
# Copyright (c) 2021 the aulang contributors
#
# Licensed under Apache License v2.0 with Runtime Library Exception
# See LICENSE.txt for license information

import glob
import subprocess
import os

source_path = os.path.dirname(os.path.abspath(__file__))

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

c_src_comp_test = c_src + """
#include <libgen.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "platform/tmpfile.h"

#include "compiler/c_comp.h"

#include <libgen.h>
#include <signal.h>
#include <unistd.h>

extern char *TEST_RT_CODE;
extern size_t TEST_RT_CODE_LEN;
"""

def gen_check_value(item, val_type, val_contents):
    if val_type == 'int':
        return f"assert(au_value_get_type({item}) == VALUE_INT && au_value_get_int({item}) == {val_contents});"
    elif val_type == 'float':
        return f"assert(au_value_get_type({item}) == VALUE_DOUBLE && au_value_get_double({item}) == {val_contents});"
    elif val_type == 'bool':
        return f"assert(au_value_get_type({item}) == VALUE_BOOL && au_value_get_bool({item}) == {'1' if val_contents == 'true' else '0'});"
    elif val_type == 'str':
        return f"char *s={val_contents};assert(au_value_get_type({item}) == VALUE_STR && au_value_get_string({item})->len == strlen(s) && memcmp(au_value_get_string({item})->data, s, strlen(s)) == 0);\n"
    raise NotImplementedError(val_type)

def bytes_to_c_array(x):
    return ','.join(map(str, map(int, x))), len(x)

test_srcs = []
input_srcs = []

for (i, fn) in enumerate(files):
    with open(fn[:-4] + '.au', 'r') as f:
        input_cont = f.read()
    with open(fn, 'r') as f:
        output_lines = map(lambda s: s.rstrip(), f.readlines())
    input_bytes = input_cont.encode('utf-8')
    input_array, input_len = bytes_to_c_array(input_bytes)
    input_srcs.append((input_array, input_len))

    test_src = f"""\
static int test_{i}_idx = 0;
static void test_{i}_check(au_value_t value) {{ switch(test_{i}_idx) {{
"""
    for val_idx, value in enumerate(output_lines):
        if value.startswith("array;"):
            array_contents = value.split(';')[1:]
            test_src += f"  case {val_idx}: {{\n"
            test_src += """\
    assert(au_value_get_type(value) == VALUE_STRUCT && au_value_get_struct(value)->vdata == &au_obj_array_vdata);
    struct au_obj_array *array = (struct au_obj_array *)au_value_get_struct(value);
"""
            for item_idx, item in enumerate(array_contents):
                val_type, val_contents = item.split(',')
                item = f"au_obj_array_get(array, au_value_int({item_idx}))"
                test_src += f"    {gen_check_value(item, val_type, val_contents)}\n"
            test_src += f"  }}\n"
            continue
        val_type, val_contents = value.split(';')
        test_src += f"  case {val_idx}: {{"
        test_src += gen_check_value("value", val_type, val_contents)
        test_src += f" break; }}\n"
    test_src += f"""\
    }}
    test_{i}_idx++;
}}
"""
    c_src += test_src

    test_src = f"""
#include <assert.h>
{test_src}
void au_value_print(au_value_t v) {{ test_{i}_check(v); }}
"""
    test_srcs.append(test_src)

# Interpreter test

for (i, (input_src_array, input_src_len)) in enumerate(input_srcs):
    c_src += f"""
static void test_{i}() {{
    const char source[] = {{{input_src_array}}};
    struct au_program program;
    assert(au_parse(source, {input_src_len}, &program) != 0);
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

# Compiler test
# TODO: put this code in an actual source file
c_src_comp_test += f"""
void run_gcc(const char *source, const size_t source_len) {{
    char c_file[] = TMPFILE_TEMPLATE;
    int fd;
    if ((fd = mkstemps(c_file, 2)) == -1)
        au_perror("cannot generate tmpnam");

    char c_file_out[] = TMPFILE_TEMPLATE;
    int fd_out;
    if ((fd_out = mkstemps(c_file_out, 2)) == -1)
        au_perror("cannot generate tmpnam");

    struct au_c_comp_state c_state = {{
        .f = fdopen(fd, "w"),
    }};
    struct au_program program;
    assert(au_parse(source, source_len, &program) != 0);
    au_c_comp(&c_state, &program);
    au_c_comp_state_del(&c_state);
    au_program_del(&program);

    char *cc = getenv("CC");
    if (cc == 0)
        cc = "gcc";

    char *args[] = {{
        cc, "-flto", "-O2", "-o", c_file_out, c_file, NULL,
    }};
    pid_t pid = fork();
    if (pid == -1) {{
        au_perror("fork");
    }} else if (pid > 0) {{
        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }} else {{
        execvp(args[0], args);
    }}

    pid = fork();
    if (pid == -1) {{
        au_perror("fork");
    }} else if (pid > 0) {{
        int status;
        waitpid(pid, &status, 0);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }} else {{
        char *args[] = {{ c_file_out, NULL }};
        execvp(c_file_out, args);
    }}
    unlink(c_file);
    unlink(c_file_out);
}}"""
for (i, ((input_src_array, input_src_len), test_src)) in enumerate(zip(input_srcs, test_srcs)):
    test_src_array, test_src_len = bytes_to_c_array(test_src.encode('utf-8'))
    c_src_comp_test += f"""
static void test_{i}() {{
    char rt_code[] = {{{test_src_array}}};
    TEST_RT_CODE = rt_code;
    TEST_RT_CODE_LEN = {test_src_len};
    const char source[] = {{{input_src_array}}};
    run_gcc(source, {input_src_len});
}}\n\n"""

c_src_comp_test += """
int main(int argc, char **argv) {
"""

for (i, fn) in enumerate(files):
    c_src_comp_test += f"  printf(\"[{i+1}/{len(files)}] {fn}\\n\"); test_{i}();\n"

c_src_comp_test += """
    return 0;
}
"""

with open("build/tests_comp.c", "w") as f:
    f.write(c_src_comp_test)
