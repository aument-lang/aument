#include <aument.h>
#include <stdio.h>

au_extern_module_t au_extern_module_load() {
    printf("module load\n");
    return 0;
}