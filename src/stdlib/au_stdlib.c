// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"
#include "core/vm/vm.h"

#define STDLIB_FUNC(NAME, ARGS)                                           \
    (struct au_lib_func) {                                                \
        .name = #NAME, .symbol = "au_std_" #NAME, .num_args = ARGS,       \
        .func = au_std_##NAME                                             \
    }

#define MODULE_FUNC(NAME, SYMBOL, ARGS)                                           \
    (struct au_lib_func) {                                                \
        .name = #NAME, .symbol = SYMBOL, .num_args = ARGS,       \
        .func = au_std_##NAME                                             \
    }

static void add_module(struct au_program_data *data, const char *name,
                       struct au_lib_func *funcs, int len) {
    struct au_hm_var_value *old = au_hm_vars_add(&data->imported_module_map, name, strlen(name),
                   AU_HM_VAR_VALUE(data->imported_modules.len));
    assert(old == 0);
    struct au_imported_module module = {0};
    au_imported_module_init(&module, 1);
    for (int i = 0; i < len; i++) {
        struct au_lib_func lib_func = funcs[i];
        struct au_fn fn = (struct au_fn){
            .flags = 0, .type = AU_FN_NATIVE, .as.native_func = lib_func};
        const size_t fn_len = data->fns.len;
        au_fn_array_add(&data->fns, fn);
        au_hm_vars_add(&module.fn_map, lib_func.symbol,
                       strlen(lib_func.symbol), AU_HM_VAR_VALUE(fn_len));
    }
    au_imported_module_array_add(&data->imported_modules, module);
}

void au_install_stdlib(struct au_program_data *data) {
    {
        struct au_lib_func stdlib_funcs[] = {
            // *io.c*
            STDLIB_FUNC(input, 0),
            // *types.c*
            STDLIB_FUNC(int, 1),
            STDLIB_FUNC(str, 1),
            STDLIB_FUNC(bool, 1),
            // *array.c*
            STDLIB_FUNC(len, 1),
        };
        const int stdlib_len =
            sizeof(stdlib_funcs) / sizeof(stdlib_funcs[0]);
        for (int i = 0; i < stdlib_len; i++) {
            struct au_lib_func lib_func = stdlib_funcs[i];
            struct au_fn fn = (struct au_fn){.flags = 0,
                                             .type = AU_FN_NATIVE,
                                             .as.native_func = lib_func};
            const size_t len = data->fns.len;
            au_fn_array_add(&data->fns, fn);
            au_hm_vars_add(&data->fn_map, lib_func.name,
                           strlen(lib_func.name), AU_HM_VAR_VALUE(len));
        }
    }
    {
        struct au_lib_func gc_funcs[] = {
            // *gc.c*
            MODULE_FUNC(gc_heap_size, "heap_size", 0),
        };
        const int gc_funcs_len = sizeof(gc_funcs) / sizeof(gc_funcs[0]);
        add_module(data, "gc", gc_funcs, gc_funcs_len);
    }
}
