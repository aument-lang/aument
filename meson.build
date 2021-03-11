project('aument', 'c')

is_testing = get_option('tests')
is_coverage = get_option('b_coverage')
has_compile_feature = get_option('compiler')
has_delayed_rc_feature = get_option('delayed_rc')
has_leak_mem_feature = get_option('leak_mem')
is_static_exe = get_option('static_exe')
has_math_library = get_option('math_library')
has_io_library = get_option('io_library')

compiler = meson.get_compiler('c')
prog_python = import('python').find_installation('python3')

au_hdr_cflags = []

au_depends = []
au_cflags = []
au_export_dynamic = false

# Flag configs

if target_machine.system() != 'windows'
    code = '''#include <sys/mman.h>
    void main() { mmap(0, 0, 0, 0, 0, 0); }
    '''
    result = compiler.links(code, name : 'mmap')
    if result
        add_project_arguments('-DAU_USE_MMAP', language : ['c'])
    endif

    libdl = compiler.find_library('dl')
    if libdl.found()
        au_cflags += ['-DAU_FEAT_LIBDL']
        au_depends += [libdl]
        au_export_dynamic = true
    endif
endif

m_dep = compiler.find_library('m', required : false)
au_depends += [m_dep]

code = '''#include<stdio.h>
int main(int argc, char **argv) {
  static void *cb[] = { &&a, &&b };
  a: printf("1"); return 0;
  b: printf("0"); return 0;
}
'''
result = compiler.run(code, name : 'dispatch jump')
if result.stdout().strip() == '1'
    add_project_arguments('-DAU_USE_DISPATCH_JMP', language : ['c'])
endif

code = '''#include<float.h>
#include<stdint.h>
#include<stdlib.h>
int main(int argc, char **argv) {
  printf("%d",
#ifdef __STDC_IEC_559__
    1
#else
    0
#endif
    && sizeof(uintptr_t) == sizeof(uint64_t)
    && ((uintptr_t)malloc(128)) < 0xffffffffffff
  );
}
'''
result = compiler.run(code, name : 'nan tagging support')
if result.stdout().strip() == '1'
    add_project_arguments('-DAU_USE_NAN_TAGGING', language : ['c'])
    au_hdr_cflags += ['DAU_USE_NAN_TAGGING']
endif

code = '''#include<stdio.h>
#include <stdlib.h>
#include <inttypes.h>
__attribute__((noinline)) uintptr_t stack_local() {
  char x = '1';
  return (uintptr_t)&x;
}
int main(int argc, char **argv) {
  char x = '1';
  uintptr_t local = (uintptr_t)&x;
  uintptr_t called = stack_local();
  printf("%d", called > local);
}
'''
result = compiler.run(code, name : 'stack grows up')
if result.stdout().strip() == '1'
    add_project_arguments('-DAU_STACK_GROWS_UP', language : ['c'])
endif

code = '''
#ifdef _WIN32
#include <malloc.h>
#ifndef alloca
#define alloca _alloca
#endif
#else
#include <alloca.h>
#endif
int main(int argc, char **argv) {
    alloca(32);
}
'''
result = compiler.links(code, name : 'alloca support')
if result
    add_project_arguments('-DAU_USE_ALLOCA', language : ['c'])
    au_hdr_cflags += ['DAU_USE_ALLOCA']
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
'src/core/rt/malloc.h',
'src/core/rt/struct/coerce.h',
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
'src/core/vm/module.c',
'src/core/vm/module.h',
'src/core/vm/tl.c',
'src/core/vm/tl.h',
'src/core/vm/vm.c',
'src/core/vm/vm.h',
'src/help.h',
'src/platform/cc.h',
'src/platform/mmap.c',
'src/platform/mmap.h',
'src/platform/path.c',
'src/platform/path.h',
'src/platform/platform.h',
'src/platform/spawn.h',
'src/platform/tmpfile.h',
'src/stdlib/au_stdlib.c',
'src/stdlib/au_stdlib.h',
'src/stdlib/collection.c',
'src/stdlib/collection.h',
'src/stdlib/gc.c',
'src/stdlib/gc.h',
'src/stdlib/types.c',
'src/stdlib/types.h',
'src/version.h',
]

if has_io_library
    sources += [
        'src/stdlib/io.c',
        'src/stdlib/io.h',
    ]
    add_project_arguments('-DAU_FEAT_IO_LIB', language : ['c'])
endif

if has_math_library
    sources += [
        'src/stdlib/math.c',
        'src/stdlib/math.h',
    ]
    add_project_arguments('-DAU_FEAT_MATH_LIB', language : ['c'])
endif

if has_compile_feature
    rt_hdr_depends = files(
        'src/platform/platform.h',
        'src/core/rt/malloc.h',
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
        input : './build-scripts/gen_rt.py',
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
        'src/core/rt/value/print.c',
    )
    rt_code_src = custom_target(
        'comp_code.c',
        output : 'comp_code.c',
        input : './build-scripts/gen_rt.py',
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
    add_project_arguments('-DAU_FEAT_COMPILER', language : ['c'])
endif

if has_delayed_rc_feature
    sources += ['src/core/rt/malloc.c']
    add_project_arguments('-DAU_FEAT_DELAYED_RC', language : ['c'])
endif

if has_leak_mem_feature
    add_project_arguments('-DAU_FEAT_LEAK_MEM', language : ['c'])
endif

if is_coverage
    add_project_arguments('-DAU_COVERAGE', language : ['c'])
endif

au_runtime_sources = [
    'src/core/rt/exception.c',
    'src/stdlib/gc.c',
    'src/stdlib/types.c',
    'src/stdlib/collection.c',
    'src/core/rt/struct/helper.c',
]
if has_io_library
    au_runtime_sources += ['src/stdlib/io.c']
endif
if has_math_library
    au_runtime_sources += ['src/stdlib/math.c']
endif

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


# These header files must be ordered by hand
aument_header_files = [
# Platform-specific
'src/platform/platform.h',
'src/core/rt/exception.h',
'src/core/rt/malloc.h',

# Data structure defs
'src/core/array.h',
'src/core/str_array.h',
'src/core/char_array.h',
'src/core/bit_array.h',
'src/core/hash.h',
'src/core/hm_vars.h',

# Token/bc defs
'src/core/parser/lexer.h',
'src/core/bc.h',

# Runtime part
'src/core/rt/struct/main.h',
'src/core/rt/au_struct.h',
'src/core/rt/au_string.h',
'src/core/rt/value/main.h',
'src/core/value_array.h',
'src/core/rt/struct/coerce.h',
'src/core/rt/au_array.h',
'src/core/rt/au_class.h',
'src/core/rt/au_tuple.h',
'src/core/rt/struct/vdata.h',
'src/core/rt/value/ref.h',
'src/core/rt/extern_fn.h',
'src/core/fn.h',
'src/core/fn/main.h',
'src/core/rt/struct/helper.h',

# Everything else
'src/core/parser/parser.h',
'src/core/int_error/error_location.h',
'src/core/vm/exception.h',
'src/core/int_error/error_printer.h',
'src/core/parser/exception.h',
'src/core/program.h',
'src/core/vm/frame_link.h',
'src/core/vm/tl.h',
'src/core/vm/vm.h',
'src/core/fn/call.h',
'src/platform/cc.h',
'src/platform/mmap.h',
'src/platform/path.h',
'src/platform/spawn.h',
'src/platform/tmpfile.h',
'src/stdlib/au_stdlib.h',
'src/version.h',

# Libraries
'src/lib/module.h',
    ]
aument_header = custom_target(
    'aument.h',
    output : 'aument.h',
    input : './build-scripts/gen_unified_header.py',
    depend_files : aument_header_files,
    command : [
        prog_python, '@INPUT@',
        '--output', '@OUTPUT@',
        '--files'
    ] + aument_header_files + ['--cpp'] + au_hdr_cflags,
    install: true,
    install_dir: get_option('includedir')
)

if is_testing
    if has_compile_feature
        static_library('au_runtime', au_runtime_sources,
            include_directories: incs,
            c_args: ['-DAU_IS_STDLIB', '-DAU_TEST_RT_CODE'])
    endif

    aument_exe = executable('aument',
        sources + ['src/main.c'],
        include_directories: incs,
        c_args: ['-DAU_TEST_EXE'] + au_cflags,
        dependencies: au_depends,
        export_dynamic: au_export_dynamic)

    e = executable('aument-tests-feature',
       sources + [join_paths(meson.build_root(), 'tests.c')],
       include_directories: incs,
       c_args: ['-DAU_TEST_RT_CODE'])
    test('language feature test', e)

    if has_compile_feature
        e = executable('aument-tests-comp',
           sources + [join_paths(meson.build_root(), 'tests_comp.c')],
           include_directories: incs,
           c_args: ['-DAU_TEST_RT_CODE'])
        test('c compiler test', prog_python,
            args: files('./build-scripts/compile_runner.py') + [
                join_paths(meson.build_root(), 'aument-tests-comp'),
            ])
    endif

    test('features with input', prog_python,
        args: files('./build-scripts/check_output.py') + [
            '--check', 'with_input',
            '--binary', join_paths(meson.build_root(), 'aument'),
            '--path', join_paths(meson.source_root(), 'tests/features-with-input'),
        ],
        depends: [aument_exe])

    test('imports', prog_python,
        args: files('./build-scripts/check_output.py') + [
            '--check', 'output',
            '--binary', join_paths(meson.build_root(), 'aument'),
            '--path', join_paths(meson.source_root(), 'tests/imports'),
        ],
        depends: [aument_exe])

    if has_compile_feature
        test('imports compiled', prog_python,
            args: files('./build-scripts/check_output.py') + [
                '--check', 'comp',
                '--binary', join_paths(meson.build_root(), 'aument'),
                '--path', join_paths(meson.source_root(), 'tests/imports'),
            ],
            depends: [aument_exe])

    test('error output', prog_python,
        args: files('./build-scripts/check_output.py') + [
            '--check', 'errors',
            '--binary', join_paths(meson.build_root(), 'aument'),
            '--path', join_paths(meson.source_root(), 'tests/errors'),
        ],
        depends: [aument_exe])

    endif

    foreach import_lib_info : [
        ['dl_import_test', 'tests/dl-import'],
        ['dl_module_test', 'tests/dl-module'],
    ]
        import_lib_test = shared_module(import_lib_info[0],
            files(import_lib_info[1] + '/lib.c'),
            include_directories: incs)
        if target_machine.system() == 'windows'
            # TODO: import_lib_test for windows
        else
            test(import_lib_info[0], prog_python,
                args: files('./build-scripts/check_output.py') + [
                    '--check', 'output',
                    '--binary', join_paths(meson.build_root(), 'aument'),
                    '--path', join_paths(meson.source_root(), import_lib_info[1]),
                ],
                depends: [aument_exe, import_lib_test])
        endif
    endforeach
else
    if has_compile_feature
        static_library('au_runtime',
            au_runtime_sources,
            include_directories: incs,
            c_args: ['-DAU_IS_STDLIB'],
            install: true,
            install_dir: get_option('bindir'))
    endif

    link_args = []
    if is_static_exe and get_option('b_sanitize') == 'none'
        link_args += ['-static']
    endif
    executable('aument',
        sources + ['src/main.c'],
        include_directories: incs,
        link_args: link_args,
        install: true,
        dependencies: au_depends,
        export_dynamic: au_export_dynamic)
endif
