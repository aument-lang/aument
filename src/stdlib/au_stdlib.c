// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"
#include "core/vm/vm.h"

#include "array.h"
#include "gc.h"
#include "io.h"
#include "list.h"
#include "math.h"
#include "str.h"
#include "types.h"

#ifdef AU_TEST
#include "test_fns.h"
#endif

struct std_module_fn {
    const char *name;
    au_extern_func_t func;
    int32_t num_args;
};

struct std_module {
    const char *name;
    const struct std_module_fn *fns;
    const size_t fns_len;
};

// * str.h *
static const struct std_module_fn str_fns[] = {
    (struct std_module_fn){
        .name = "into", .func = au_std_str_into, .num_args = 1},
    (struct std_module_fn){
        .name = "chars", .func = au_std_str_chars, .num_args = 1},
    (struct std_module_fn){
        .name = "char", .func = au_std_str_char, .num_args = 1},
};

// standard library definitions
static const struct std_module au_stdlib_modules_data[] = {
#define LIBRARY(NAME)                                                     \
    (struct std_module) {                                                 \
        .name = #NAME, .fns = NAME##_fns,                                 \
        .fns_len = sizeof(NAME##_fns) / sizeof(struct std_module_fn),     \
    }
    LIBRARY(str),
#undef LIBRARY
};
const size_t au_stdlib_modules_len =
    sizeof(au_stdlib_modules_data) / sizeof(struct std_module);

void au_stdlib_export(struct au_program_data *data) {
    for (size_t i = 0; i < au_stdlib_modules_len; i++) {
        const struct std_module *lib = &au_stdlib_modules_data[i];
        au_hm_var_value_t *old =
            au_hm_vars_add(&data->imported_module_map, lib->name,
                           strlen(lib->name), data->imported_modules.len);
        assert(old == 0);
        struct au_imported_module module = {0};
        au_imported_module_init(&module);
        module.stdlib_module_idx = i;
        au_imported_module_array_add(&data->imported_modules, module);
    }
}

au_extern_module_t au_stdlib_module(size_t idx) {
    if (idx > au_stdlib_modules_len)
        abort();
    const struct std_module *lib = &au_stdlib_modules_data[idx];
    au_extern_module_t module = au_extern_module_new();
    for (size_t i = 0; i < lib->fns_len; i++) {
        const struct std_module_fn *fn = &lib->fns[i];
        au_extern_module_add_fn(module, fn->name, fn->func, fn->num_args);
    }
    return module;
}
