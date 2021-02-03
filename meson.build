project('aulang', 'c')

sources = [
    'src/program.c',
    'src/au_string.c',
    'src/hash.c',
    'src/exception.c',
    'src/bc.c',
    'src/bc_dbg.c',
    'src/bc_vars.c',
    'src/parser.c',
    'src/vm.c',
    'src/value.c',
    'src/c_comp.c',
    'src/main.c',
    'src/mmap.c',
]

if get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-pg', language : ['c'])
endif
if get_option('buildtype') == 'release' or get_option('buildtype') == 'debugoptimized'
    add_project_arguments('-flto', language : ['c'])
endif

add_project_arguments('-Werror', language : ['c'])

executable('aulang', sources)
