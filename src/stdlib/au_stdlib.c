// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"
#include "core/vm/vm.h"

#include "collection.h"
#include "gc.h"
#include "io.h"
#include "math.h"
#include "types.h"

#define STDLIB_FUNC(NAME, ARGS)                                           \
    (struct au_lib_func) {                                                \
        .name = #NAME, .symbol = "au_std_" #NAME, .num_args = ARGS,       \
        .func = au_std_##NAME                                             \
    }

#define MODULE_FUNC(SYMBOL, NAME, ARGS)                                   \
    (struct au_lib_func) {                                                \
        .name = NAME, .symbol = "au_std_" #SYMBOL, .num_args = ARGS,      \
        .func = au_std_##SYMBOL                                           \
    }

static void add_module(struct au_program_data *data, const char *name,
                       struct au_lib_func *funcs, int len) {
    au_hm_var_value_t *old =
        au_hm_vars_add(&data->imported_module_map, name, strlen(name),
                       data->imported_modules.len);
    assert(old == 0);
    struct au_imported_module module = {0};
    au_imported_module_init(&module, 1);
    for (int i = 0; i < len; i++) {
        struct au_lib_func lib_func = funcs[i];
        struct au_fn fn = (struct au_fn){
            .flags = 0, .type = AU_FN_LIB, .as.lib_func = lib_func};
        const size_t fn_len = data->fns.len;
        au_fn_array_add(&data->fns, fn);
        au_hm_vars_add(&module.fn_map, lib_func.name,
                       strlen(lib_func.name), fn_len);
    }
    au_imported_module_array_add(&data->imported_modules, module);
}

void au_install_stdlib(struct au_program_data *data) {
    {
        struct au_lib_func mod_funcs[] = {
#ifdef AU_FEAT_IO_LIB
            // *io.c*
            STDLIB_FUNC(input, 0),
#endif
            // *types.c*
            STDLIB_FUNC(int, 1),
            STDLIB_FUNC(float, 1),
            STDLIB_FUNC(str, 1),
            STDLIB_FUNC(bool, 1),
            // *array.c*
            STDLIB_FUNC(len, 1),
        };
        const int mod_funcs_len = sizeof(mod_funcs) / sizeof(mod_funcs[0]);

        for (int i = 0; i < mod_funcs_len; i++) {
            struct au_lib_func lib_func = mod_funcs[i];
            struct au_fn fn = (struct au_fn){
                .flags = 0, .type = AU_FN_LIB, .as.lib_func = lib_func};
            const size_t fn_len = data->fns.len;
            au_fn_array_add(&data->fns, fn);
            au_hm_vars_add(&data->fn_map, lib_func.name,
                           strlen(lib_func.name), fn_len);
        }

        add_module(data, "std", mod_funcs, mod_funcs_len);
    }
    {
        struct au_lib_func mod_funcs[] = {
            // *gc.c*
            MODULE_FUNC(gc_heap_size, "heap_size", 0),
        };
        const int mod_funcs_len = sizeof(mod_funcs) / sizeof(mod_funcs[0]);
        add_module(data, "gc", mod_funcs, mod_funcs_len);
    }
#ifdef AU_FEAT_IO_LIB
    {
        struct au_lib_func mod_funcs[] = {
            // *io.c*
            MODULE_FUNC(io_stdout, "stdout", 0),
            MODULE_FUNC(io_stdin, "stdin", 0),
            MODULE_FUNC(io_stderr, "stderr", 0),
            MODULE_FUNC(io_open, "open", 2),
            MODULE_FUNC(io_close, "close", 1),
            MODULE_FUNC(io_read, "read", 1),
            MODULE_FUNC(io_read_up_to, "read_up_to", 1),
            MODULE_FUNC(io_write, "write", 2),
            MODULE_FUNC(io_flush, "flush", 1),
        };
        const int mod_funcs_len = sizeof(mod_funcs) / sizeof(mod_funcs[0]);
        add_module(data, "io", mod_funcs, mod_funcs_len);
    }
#endif
#ifdef AU_FEAT_MATH_LIB
    {
        struct au_lib_func mod_funcs[] = {
            // *math.c*
            MODULE_FUNC(math_abs, "abs", 1),
            MODULE_FUNC(math_max, "max", 2),
            MODULE_FUNC(math_min, "min", 2),
            MODULE_FUNC(math_exp, "exp", 1),
            MODULE_FUNC(math_ln, "ln", 1),
            MODULE_FUNC(math_log2, "log2", 1),
            MODULE_FUNC(math_log10, "log10", 1),
            MODULE_FUNC(math_sqrt, "sqrt", 1),
            MODULE_FUNC(math_cbrt, "cbrt", 1),
            MODULE_FUNC(math_hypot, "hypot", 2),
            MODULE_FUNC(math_pow, "pow", 2),
            MODULE_FUNC(math_sin, "sin", 1),
            MODULE_FUNC(math_cos, "cos", 1),
            MODULE_FUNC(math_tan, "tan", 1),
            MODULE_FUNC(math_asin, "asin", 1),
            MODULE_FUNC(math_acos, "acos", 1),
            MODULE_FUNC(math_atan, "atan", 1),
            MODULE_FUNC(math_atan2, "atan2", 2),
            MODULE_FUNC(math_sinh, "sinh", 1),
            MODULE_FUNC(math_cosh, "cosh", 1),
            MODULE_FUNC(math_tanh, "tanh", 1),
            MODULE_FUNC(math_asinh, "asinh", 1),
            MODULE_FUNC(math_acosh, "acosh", 1),
            MODULE_FUNC(math_atanh, "atanh", 1),
            MODULE_FUNC(math_erf, "erf", 1),
            MODULE_FUNC(math_erfc, "erfc", 1),
            MODULE_FUNC(math_lgamma, "lgamma", 1),
            MODULE_FUNC(math_tgamma, "tgamma", 1),
            MODULE_FUNC(math_ceil, "ceil", 1),
            MODULE_FUNC(math_floor, "floor", 1),
            MODULE_FUNC(math_trunc, "trunc", 1),
            MODULE_FUNC(math_round, "round", 1),
            MODULE_FUNC(math_is_finite, "is_finite", 1),
            MODULE_FUNC(math_is_infinite, "is_infinite", 1),
            MODULE_FUNC(math_is_nan, "is_nan", 1),
            MODULE_FUNC(math_is_normal, "is_normal", 1),
        };
        const int mod_funcs_len = sizeof(mod_funcs) / sizeof(mod_funcs[0]);
        add_module(data, "math", mod_funcs, mod_funcs_len);
    }
#endif
}
