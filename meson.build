project('aulang', 'c')

sources = [
    'src/compiler/c_comp.c',
    'src/compiler/c_comp.h',
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

if get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-pg', language : ['c'])
endif
if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-flto', language : ['c'])
endif

compiler = meson.get_compiler('c')
if compiler.has_argument('-Werror')
    add_project_arguments('-Werror', language : ['c'])
endif

executable('aulang', sources, include_directories: include_directories('src'))
