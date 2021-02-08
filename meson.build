project('aulang', 'c')

compiler = meson.get_compiler('c')
sources = [
    'src/core/array.h',
    'src/core/bc.c',
    'src/core/bc.h',
    'src/core/bc_dbg.c',
    'src/core/bc_vars.c',
    'src/core/bc_vars.h',
    'src/core/bit_array.h',
    'src/core/hash.c',
    'src/core/hash.h',
    'src/core/parser.c',
    'src/core/parser.h',
    'src/core/program.c',
    'src/core/program.h',
    'src/core/rt/au_string.c',
    'src/core/rt/au_string.h',
    'src/core/rt/exception.c',
    'src/core/rt/exception.h',
    'src/core/rt/extern_fn.h',
    'src/core/rt/value.c',
    'src/core/rt/value.h',
    'src/core/str_array.h',
    'src/core/lexer.c',
    'src/core/lexer.h',
    'src/core/vm.c',
    'src/core/vm.h',
    'src/help.h',
    'src/main.c',
    'src/platform/mmap.c',
    'src/platform/mmap.h',
    'src/platform/platform.h',
    'src/platform/tmpfile.h',
    'src/stdlib/au_stdlib.c',
    'src/stdlib/au_stdlib.h',
    'src/stdlib/io.c',
    'src/stdlib/types.c',
]

if get_option('compiler').enabled() and host_machine.system() != 'windows'
    prog_python = import('python').find_installation('python3')

    rt_hdr_depends = files(
        'src/platform/platform.h',
        'src/core/array.h',
        'src/core/rt/exception.h',
        'src/core/rt/au_string.h',
        'src/core/rt/value.h',
        'src/core/rt/extern_fn.h',
        'src/core/rt/includes/stdlib_end.h',
    )
    stdlib_begin_hdr = files('src/core/rt/includes/stdlib_begin.h')
    rt_hdr_src = custom_target(
        'comp_hdr.c',
        output : 'comp_hdr.c',
        input : 'gen_rt.py',
        depend_files : rt_hdr_depends + stdlib_begin_hdr,
        command : [prog_python, '@INPUT@', stdlib_begin_hdr, '@OUTPUT@', 'AU_RT_HDR'] + rt_hdr_depends,
    )

    rt_code_depends = files(
        'src/core/rt/au_string.c',
        'src/core/rt/value.c',
    )
    rt_code_src = custom_target(
        'comp_code.c',
        output : 'comp_code.c',
        input : 'gen_rt.py',
        depend_files : rt_code_depends,
        command : [prog_python, '@INPUT@', '-', '@OUTPUT@', 'AU_RT_CODE'] + rt_code_depends,
    )

    sources += [
        'src/compiler/c_comp.c',
        'src/compiler/c_comp.h',
        rt_hdr_src,
        rt_code_src,
    ]
    add_project_arguments('-DFEAT_COMPILER', language : ['c'])
endif

# Flag configs

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
endif

if compiler.has_argument('-Werror')
    add_project_arguments('-Werror', language : ['c'])
endif

# Build types

if get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-pg', language : ['c'])
endif
if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-flto', language : ['c'])
endif

add_project_arguments('-DAU_IS_INTERPRETER', language : ['c'])

executable('aulang', sources, include_directories: include_directories('src'))
