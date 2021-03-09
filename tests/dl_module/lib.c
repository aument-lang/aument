#include <aument.h>
#include <stdio.h>

struct au_program_data *au_module_load() {
    fprintf(stderr, "module load\n");
    return 0;
}