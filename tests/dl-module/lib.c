#include <aument.h>
#include <stdio.h>
#include <string.h>

const char *string = "string from module\n";

AU_EXTERN_FUNC_DECL(lib_str) {
    return au_value_string(au_string_from_const(string, strlen(string)));
}

struct au_program_data *au_module_load() {
    struct au_program_data *data = au_module_new();
    au_module_add_fn(data, "str", lib_str, 0);
    return data;
}