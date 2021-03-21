#include <aument.h>
#include <stdio.h>
#include <string.h>

au_extern_module_t
au_extern_module_load(struct au_extern_module_options *options) {
    printf("%s\n", options->subpath);
    return 0;
}