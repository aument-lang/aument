project(
    'aument', 'c',
    license: 'Apache-2.0',
    default_options: [
        'warning_level=2',
        'werror=true',
    ]
)

is_testing = get_option('tests')
is_coverage = get_option('b_coverage')

has_sanitizer = get_option('b_sanitize') != 'none'
has_compile_feature = get_option('compiler')

has_delayed_rc_feature = get_option('delayed_rc')
has_leak_mem_feature = get_option('leak_mem')
has_prefetch_insn_feature = get_option('prefetch_insn')
has_dispatch_jump_feature = get_option('dispatch_jump')

is_static_exe = get_option('static_exe')

has_math_library = get_option('math_library')
has_io_library = get_option('io_library')

compiler = meson.get_compiler('c')
prog_python = import('python').find_installation('python3')

au_hdr_cflags = []
au_depends = []
au_supports_dll_import = false

# Preconfigs
if compiler.has_argument('-Wno-unused-command-line-argument')
    add_project_arguments('-Wno-unused-command-line-argument', language : ['c'])
endif

if compiler.has_argument('-fvisibility=hidden')
    add_project_arguments('-fvisibility=hidden', language : ['c'])
endif

if is_testing
    add_project_arguments('-DAU_TEST', language : ['c'])
    au_hdr_cflags += ['DAU_TEST']
endif

# Flag configs

if target_machine.system() != 'windows'
    code = '''#include <sys/mman.h>
    void main() { mmap(0, 0, 0, 0, 0, 0); }
    '''
    result = compiler.links(code, name : 'mmap')
    if result
        add_project_arguments('-DAU_USE_MMAP', language : ['c'])
    endif
endif

if target_machine.system() == 'windows'
    au_supports_dll_import = true
else
    libdl = compiler.find_library('dl', required: false)
    if libdl.found()
        add_project_arguments('-DAU_FEAT_LIBDL', language : ['c'])
        au_depends += [libdl]
        au_supports_dll_import = true
    endif
endif

if has_dispatch_jump_feature
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
endif

code = '''#include<float.h>
#include<stdint.h>
#include<stdlib.h>
#include<stdio.h>
int supports_ieee754() {
#ifdef __STDC_IEC_559__
    return 1;
#else
#if __GCC_IEC_559 > 0
	return 1;
#else
	return 0;
#endif
#endif
}
int main(int argc, char **argv) {
  printf("%d",
    supports_ieee754()
    && sizeof(double) == sizeof(uint64_t)
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

# Error flags

if meson.version().version_compare('<0.54.0')
    if compiler.has_argument('-Werror')
        add_project_arguments('-Werror', language : ['c'])
    endif
endif

# Compilation

sources = run_command(prog_python, [
    './build-scripts/meson_files.py'
]).stdout().split('\n')

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
    libm = compiler.find_library('m', required : true)
    au_depends += [libm]
endif

if has_compile_feature
    rt_hdr_depends = files(
        'src/platform/platform.h',
        'src/platform/arithmetic.h',
        'src/core/rt/malloc.h',
        'src/core/array.h',
        'src/core/rt/exception.h',
        'src/core/rt/au_string.h',
        'src/core/rt/struct/main.h',
        'src/core/rt/value/main.h',
        'src/core/value_array.h',
        'src/core/rt/struct/vdata.h',
        'src/core/rt/struct/coerce.h',
        'src/core/rt/value/ref.h',
        'src/core/hash.h',
        'src/core/hm_vars.h',
        'src/core/rt/au_class.h',
        'src/core/rt/au_array.h',
        'src/core/rt/au_tuple.h',
        'src/core/rt/extern_fn.h',
        'src/core/rt/au_fn_value.h',
        'src/core/vm/frame_link.h',
        'src/core/rt/struct/helper.h',
        'src/os/mmap.h',
        'src/core/vm/module.h',

        'src/core/rt/value/print.c',
        'src/core/rt/malloc/static.c',
        'src/core/rt/exception.c',
        'src/core/hash.c',
        'src/core/hm_vars.c',
        'src/core/rt/au_class.c',
        'src/core/rt/au_string.c',
        'src/core/rt/au_array.c',
        'src/core/rt/au_tuple.c',
        'src/core/rt/fn_value/comp_impl.c',
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
            '--files'] + rt_hdr_depends + ['--cpp', 'DAU_IS_STDLIB'] + au_hdr_cflags,
    )

    sources += [
        'src/os/cc.c',
        'src/os/cc.h',
        'src/os/tmpfile.c',
        'src/os/tmpfile.h',
        'src/os/spawn.c',
        'src/os/spawn.h',
        'src/compiler/c_comp.c',
        'src/compiler/c_comp.h',
        rt_hdr_src,
    ]
    add_project_arguments('-DAU_FEAT_COMPILER', language : ['c'])
endif

if has_delayed_rc_feature
    sources += ['src/core/rt/malloc/gc.c']
    add_project_arguments('-DAU_FEAT_DELAYED_RC', language : ['c'])
endif

if has_leak_mem_feature
    add_project_arguments('-DAU_FEAT_LEAK_MEM', language : ['c'])
endif

if has_prefetch_insn_feature
    add_project_arguments('-DAU_FEAT_PREFETCH_INSN', language : ['c'])
endif

if is_coverage
    add_project_arguments('-DAU_COVERAGE', language : ['c'])
endif

if get_option('debug_gc')
    add_project_arguments('-DAU_DEBUG_GC', language : ['c'])
endif

if has_sanitizer
    add_project_arguments('-DAU_SANITIZER="' + get_option('b_sanitize') + '"',
        language : ['c'])
endif

au_runtime_sources = [
    'src/core/vm/module.c',

    'src/stdlib/array.c',
    'src/stdlib/bool.c',
    'src/stdlib/float.c',
    'src/stdlib/gc.c',
    'src/stdlib/int.c',
    'src/stdlib/list.c',
    'src/stdlib/str.c',
    'src/stdlib/sys.c',
]
if has_io_library
    au_runtime_sources += ['src/stdlib/io.c']
endif
if has_math_library
    au_runtime_sources += ['src/stdlib/math.c']
endif
if is_testing
    au_runtime_sources += ['src/stdlib/test_fns.c']
endif

au_runtime_c_args = ['-DAU_IS_STDLIB']
if compiler.has_link_argument('-Wl,--export-all-symbols')
    au_runtime_c_args += ['-Wl,--export-all-symbols']
elif compiler.has_link_argument('-Wl,--export-dynamic')
    au_runtime_c_args += ['-Wl,--export-dynamic']
endif

add_project_arguments('-DAU_BINDIR="' + join_paths(get_option('prefix'), get_option('bindir')) + '"', language : ['c'])
add_project_arguments('-DAU_INCLUDEDIR="' + join_paths(get_option('prefix'), get_option('includedir')) + '"', language : ['c'])
add_project_arguments('-DAU_LIBDIR="' + join_paths(get_option('prefix'), get_option('libdir')) + '"', language : ['c'])

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
'src/lib/begin.h',

# Platform-specific
'src/platform/platform.h',
'src/platform/arithmetic.h',
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
'src/core/rt/au_fn_value.h',
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
'src/os/cc.h',
'src/os/mmap.h',
'src/os/path.h',
'src/os/spawn.h',
'src/os/tmpfile.h',
'src/lib/module.h',
'src/core/vm/module.h',
'src/stdlib/au_stdlib.h',
'src/version.h',

# Libraries
'src/lib/string_builder.h',
'src/lib/fn_value.h',
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
        au_runtime = static_library('au_runtime', au_runtime_sources,
            include_directories: incs,
            c_args: au_runtime_c_args + ['-DAU_TEST_RT_CODE'],
            dependencies: au_depends)
    endif

    aument_exe = executable('aument',
        sources + ['src/main.c'],
        include_directories: incs,
        c_args: ['-DAU_TEST_EXE'],
        dependencies: au_depends,
        export_dynamic: au_supports_dll_import,
        implib: au_supports_dll_import)

    e = executable('aument-tests-feature',
            sources + [join_paths(meson.build_root(), 'tests.c')],
            include_directories: incs,
            c_args: ['-DAU_TEST_RT_CODE'],
            dependencies: au_depends)
    test('language feature test', e)

    if has_compile_feature
        e = executable('aument-tests-comp',
                sources + [join_paths(meson.build_root(), 'tests_comp.c')],
                include_directories: incs,
                c_args: ['-DAU_TEST_RT_CODE'],
                dependencies: au_depends)
        test('c compiler test', prog_python,
            args: files('./build-scripts/compile_runner.py') + [
                join_paths(meson.build_root(), 'aument-tests-comp'),
            ],
            depends: [au_runtime])
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
            depends: [aument_exe, au_runtime])

        test('error output', prog_python,
            args: files('./build-scripts/check_output.py') + [
                '--check', 'errors',
                '--binary', join_paths(meson.build_root(), 'aument'),
                '--path', join_paths(meson.source_root(), 'tests/errors'),
            ],
            depends: [aument_exe])
    endif

    if au_supports_dll_import
    foreach import_lib_info : [
        ['dl_import_test', 'tests/dl-import'],
        ['dl_module_test', 'tests/dl-module'],
        ['dl_subpath_test', 'tests/dl-subpath'],
        ['dl_univ_lib_test', 'tests/dl-univ-lib'],
    ]
        import_lib_test = shared_module(import_lib_info[0],
            files(import_lib_info[1] + '/lib.c'),
            include_directories: incs,
            link_with: aument_exe)

        if target_machine.system() == 'windows'
            test_file = join_paths(meson.source_root(), import_lib_info[1], 'win.out')
        else
            test_file = join_paths(meson.source_root(), import_lib_info[1], 'unix.out')
        endif

        test(import_lib_info[0], prog_python,
            args: files('./build-scripts/check_output.py') + [
                '--check', 'output',
                '--binary', join_paths(meson.build_root(), 'aument'),
                '--file', test_file,
            ],
            depends: [aument_exe, import_lib_test])

        test(import_lib_info[0] + ' (compiled)',
            prog_python,
            args: files('./build-scripts/check_output.py') + [
                '--check', 'comp_to_path',
                '--binary', join_paths(meson.build_root(), 'aument'),
                '--file', test_file,
                '--param', join_paths(meson.build_root(), import_lib_info[0] + '_bin'),
            ],
            depends: [aument_exe, import_lib_test])
    endforeach
    endif

    test('io module', prog_python,
        args: files('./build-scripts/check_output.py') + [
            '--check', 'output_stderr',
            '--binary', join_paths(meson.build_root(), 'aument'),
            '--path', join_paths(meson.source_root(), 'tests/io'),
        ],
        depends: [aument_exe])
else
    if has_compile_feature
        static_library('au_runtime',
            au_runtime_sources,
            include_directories: incs,
            c_args: au_runtime_c_args,
            dependencies: au_depends,
            install: true,
            install_dir: get_option('bindir'))
    endif

    link_args = []
    if is_static_exe and not has_sanitizer
        link_args += ['-static']
    endif
    executable('aument',
        sources + ['src/main.c'],
        include_directories: incs,
        link_args: link_args,
        install: true,
        dependencies: au_depends,
        export_dynamic: au_supports_dll_import,
        implib: au_supports_dll_import)
endif
