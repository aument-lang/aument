project('aulang', 'c')

is_testing = get_option('tests').enabled()
has_compile_feature = get_option('compiler').enabled()

compiler = meson.get_compiler('c')

au_hdr_cflags = []

# Flag configs

if is_testing
    add_project_arguments('-DTEST', language : ['c'])
endif

code = '''#include <sys/mman.h>
void main() { mmap(0, 0, 0, 0, 0, 0); }
'''
result = compiler.links(code, name : 'mmap')
if result
    add_project_arguments('-DUSE_MMAP', language : ['c'])
endif

code = '''#include<stdio.h>
int main(int argc, char **argv) {
  static void *cb[] = { &&a, &&b };
  a: printf("1"); return 0;
  b: printf("0"); return 0;
}
'''
result = compiler.run(code, name : 'dispatch jump')
if result.stdout().strip() == '1'
    add_project_arguments('-DUSE_DISPATCH_JMP', language : ['c'])
endif

code = '''#include<float.h>
#include<stdint.h>
int main(int argc, char **argv) {
  printf("%d",
#ifdef __STDC_IEC_559__
    1
#else
    0
#endif
    && sizeof(uintptr_t) == sizeof(uint64_t)
  );
}
'''
result = compiler.run(code, name : 'nan tagging support')
if result.stdout().strip() == '1'
    add_project_arguments('-DUSE_NAN_TAGGING', language : ['c'])
    au_hdr_cflags += ['DUSE_NAN_TAGGING']
endif

code = '''#include<alloca.h>
int main(int argc, char **argv) {
    alloca(32);
}
'''
result = compiler.links(code, name : 'alloca support')
if result
    add_project_arguments('-DUSE_ALLOCA', language : ['c'])
    au_hdr_cflags += ['DUSE_ALLOCA']
endif

if compiler.has_argument('-Wextra')
    add_project_arguments('-Wextra', language : ['c'])
endif

if compiler.has_argument('-Werror')
    add_project_arguments('-Werror', language : ['c'])
endif

# Compilation

sources = [
    'src/core/array.h',
    'src/core/bc.c',
    'src/core/bc.h',
    'src/core/bc_dbg.c',
    'src/core/bit_array.h',
    'src/core/char_array.h',
    'src/core/fn.h',
    'src/core/fn/call.h',
    'src/core/fn/main.c',
    'src/core/fn/main.h',
    'src/core/hash.c',
    'src/core/hash.h',
    'src/core/hm_vars.c',
    'src/core/hm_vars.h',
    'src/core/int_error/error_location.h',
    'src/core/int_error/error_printer.c',
    'src/core/int_error/error_printer.h',
    'src/core/parser/exception.h',
    'src/core/parser/lexer.c',
    'src/core/parser/lexer.h',
    'src/core/parser/parser.c',
    'src/core/parser/parser.h',
    'src/core/program.c',
    'src/core/program.h',
    'src/core/rt/au_array.c',
    'src/core/rt/au_array.h',
    'src/core/rt/au_class.c',
    'src/core/rt/au_class.h',
    'src/core/rt/au_string.c',
    'src/core/rt/au_string.h',
    'src/core/rt/au_struct.h',
    'src/core/rt/au_tuple.c',
    'src/core/rt/au_tuple.h',
    'src/core/rt/exception.c',
    'src/core/rt/exception.h',
    'src/core/rt/extern_fn.h',
    'src/core/rt/includes/stdlib_begin.h',
    'src/core/rt/includes/stdlib_end.h',
    'src/core/rt/struct/coerce.h',
    'src/core/rt/struct/helper.c',
    'src/core/rt/struct/helper.h',
    'src/core/rt/struct/main.h',
    'src/core/rt/struct/vdata.h',
    'src/core/rt/value.h',
    'src/core/rt/value/main.h',
    'src/core/rt/value/print.c',
    'src/core/rt/value/ref.h',
    'src/core/str_array.h',
    'src/core/value_array.h',
    'src/core/vm/exception.c',
    'src/core/vm/exception.h',
    'src/core/vm/frame_link.h',
    'src/core/vm/tl.c',
    'src/core/vm/tl.h',
    'src/core/vm/vm.c',
    'src/core/vm/vm.h',
    'src/help.h',
    'src/platform/cc.c',
    'src/platform/cc.h',
    'src/platform/mmap.c',
    'src/platform/mmap.h',
    'src/platform/path.c',
    'src/platform/path.h',
    'src/platform/platform.h',
    'src/platform/spawn.c',
    'src/platform/spawn.h',
    'src/platform/tmpfile.c',
    'src/platform/tmpfile.h',
    'src/stdlib/array.c',
    'src/stdlib/au_stdlib.c',
    'src/stdlib/au_stdlib.h',
    'src/stdlib/io.c',
    'src/stdlib/types.c',
    'src/version.h',
]

if has_compile_feature
    prog_python = import('python').find_installation('python3')

    rt_hdr_depends = files(
        'src/platform/platform.h',
        'src/core/array.h',
        'src/core/rt/exception.h',
        'src/core/rt/au_string.h',
        'src/core/rt/struct/main.h',
        'src/core/rt/value/main.h',
        'src/core/rt/struct/vdata.h',
        'src/core/rt/struct/coerce.h',
        'src/core/rt/struct/helper.h',
        'src/core/rt/value/ref.h',
        'src/core/rt/au_array.h',
        'src/core/rt/au_tuple.h',
        'src/core/rt/extern_fn.h',
        'src/core/vm/frame_link.h',
        'src/core/rt/includes/stdlib_end.h',
    )
    stdlib_begin_hdr = files('src/core/rt/includes/stdlib_begin.h')
    rt_hdr_src = custom_target(
        'comp_hdr.c',
        output : 'comp_hdr.c',
        input : 'gen_rt.py',
        depend_files : rt_hdr_depends + stdlib_begin_hdr,
        command : [
            prog_python,
            '@INPUT@',
            '--global-file', stdlib_begin_hdr,
            '--output', '@OUTPUT@',
            '--ident', 'AU_RT_HDR',
            '--files'] + rt_hdr_depends + ['--cpp'] + au_hdr_cflags,
    )

    rt_code_depends = files(
        'src/core/rt/au_string.c',
        'src/core/rt/au_array.c',
        'src/core/rt/au_tuple.c',
        'src/core/rt/struct/helper.c',
    )
    if not is_testing
        rt_code_depends += files('src/core/rt/value/print.c')
    endif
    rt_code_src = custom_target(
        'comp_code.c',
        output : 'comp_code.c',
        input : 'gen_rt.py',
        depend_files : rt_code_depends,
        command : [
            prog_python, '@INPUT@',
            '--output', '@OUTPUT@',
            '--ident', 'AU_RT_CODE',
            '--files'
        ] + rt_code_depends + ['--cpp'] + au_hdr_cflags,
    )

    sources += [
        'src/platform/cc.c',
        'src/platform/cc.h',
        'src/platform/tmpfile.c',
        'src/platform/tmpfile.h',
        'src/platform/spawn.c',
        'src/platform/spawn.h',
        'src/compiler/c_comp.c',
        'src/compiler/c_comp.h',
        rt_hdr_src,
        rt_code_src,
    ]
    add_project_arguments('-DFEAT_COMPILER', language : ['c'])
endif

au_runtime_sources = [
    'src/core/rt/exception.c',
    'src/stdlib/io.c',
    'src/stdlib/types.c',
    'src/stdlib/array.c',
    'src/core/rt/struct/helper.c',
]

# Build types
add_project_arguments('-DAU_IS_INTERPRETER', language : ['c'])

if get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-pg', language : ['c'])
endif
if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
    if compiler.has_argument('-flto')
        add_project_arguments('-flto', language : ['c'])
    endif
endif

incs = include_directories('src')

if has_compile_feature
    static_library('au_runtime', au_runtime_sources, include_directories: incs)
endif

if is_testing
    e = executable('aulang-tests-feature', sources + ['build/tests.c'], include_directories: incs)
    test('language feature test', e)
    if has_compile_feature
        e = executable('aulang-tests-comp', sources + ['build/tests_comp.c'], include_directories: incs)
        test('c compiler test', e)
    endif
else
    executable('aulang', sources + ['src/main.c'], include_directories: incs)
endif
