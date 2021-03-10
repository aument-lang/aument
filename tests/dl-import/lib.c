#include <aument.h>
#include <stdio.h>

struct au_program_data *au_module_load() {
    printf("module load\n");
    return 0;
}