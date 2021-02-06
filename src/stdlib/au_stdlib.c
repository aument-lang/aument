// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"

struct au_lib_func au_stdlib_funcs[] = {
    (struct au_lib_func){ .name = "input", .symbol = "au_std_input", .num_args = 0, .func = au_std_input },
};

void au_install_stdlib(struct au_program_data *data) {
    const int stdlib_len = sizeof(au_stdlib_funcs) / sizeof(au_stdlib_funcs[0]);
    for(int i = 0; i < stdlib_len; i++) {
        struct au_lib_func lib_func = au_stdlib_funcs[i];
        struct au_fn fn = (struct au_fn){
            .type = AU_FN_NATIVE,
            .as.native_func = lib_func
        };
        const size_t len = data->fns.len;
        au_fn_array_add(&data->fns, fn);
        struct au_bc_var_value var_value = (struct au_bc_var_value){
            .idx = len,
        };
        au_bc_vars_add(
            &data->fn_map,
            lib_func.name, strlen(lib_func.name),
            &var_value
        );
    }
}
