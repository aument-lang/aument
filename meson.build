project('aulang', 'c')

compiler = meson.get_compiler('c')
sources = [
    'src/core/array.h',
    'src/core/au_string.c',
    'src/core/au_string.h',
    'src/core/bc.c',
    'src/core/bc.h',
    'src/core/bc_dbg.c',
    'src/core/bc_vars.c',
    'src/core/bc_vars.h',
    'src/core/bit_array.h',
    'src/core/exception.c',
    'src/core/exception.h',
    'src/core/hash.c',
    'src/core/hash.h',
    'src/core/parser.c',
    'src/core/program.c',
    'src/core/program.h',
    'src/core/str_array.h',
    'src/core/value.c',
    'src/core/value.h',
    'src/core/vm.c',
    'src/core/vm.h',
    'src/platform/mmap.c',
    'src/platform/mmap.h',
    'src/platform/platform.h',
    'src/platform/tmpfile.h',
    'src/main.c',
]

if get_option('compiler').enabled()
    src += [
        'src/compiler/c_comp.c',
        'src/compiler/c_comp.h'
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

executable('aulang', sources, include_directories: include_directories('src'))
