#include <aument.h>
#include <stdio.h>

au_extern_module_t
au_extern_module_load(struct au_extern_module_options *options) {
    (void)options;
    printf("module load\n");
    return 0;
}