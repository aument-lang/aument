// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"
#include "core/vm/vm.h"

#include "array.h"
#include "bool.h"
#include "float.h"
#include "gc.h"
#include "int.h"
#include "io.h"
#include "list.h"
#include "math.h"
#include "str.h"
#include "sys.h"

#ifdef AU_TEST
#include "test_fns.h"
#endif

struct std_module_fn {
    const char *name;
    const char *symbol;
    au_extern_func_t func;
    int32_t num_args;
};

struct std_module {
    const char *name;
    const struct std_module_fn *fns;
    const size_t fns_len;
};

#define AU_MODULE_FN(NAME, SYMBOL, NUM_ARGS)                              \
    (struct std_module_fn) {                                              \
        .name = NAME, .symbol = #SYMBOL, .func = SYMBOL,                  \
        .num_args = NUM_ARGS                                              \
    }

// * array.h *
static const struct std_module_fn array_fns[] = {
    AU_MODULE_FN("is", au_std_array_is, 1),
    AU_MODULE_FN("repeat", au_std_array_repeat, 2),
    AU_MODULE_FN("push", au_std_array_push, 2),
    AU_MODULE_FN("pop", au_std_array_pop, 1),
    AU_MODULE_FN("insert", au_std_array_insert, 3),
};

// * array.h *
static const struct std_module_fn bool_fns[] = {
    AU_MODULE_FN("is", au_std_bool_is, 1),
    AU_MODULE_FN("into", au_std_bool_into, 1),
};

// * float.h *
static const struct std_module_fn float_fns[] = {
    AU_MODULE_FN("is", au_std_float_is, 1),
    AU_MODULE_FN("into", au_std_float_into, 1),
};

// * gc.h *
static const struct std_module_fn gc_fns[] = {
    AU_MODULE_FN("heap_size", au_std_gc_heap_size, 0),
};

// * int.h *
static const struct std_module_fn int_fns[] = {
    AU_MODULE_FN("is", au_std_int_is, 1),
    AU_MODULE_FN("into", au_std_int_into, 1),
};

#ifdef AU_FEAT_IO_LIB
// * io.h *
static const struct std_module_fn io_fns[] = {
    AU_MODULE_FN("input", au_std_io_input, 0),
    AU_MODULE_FN("stdout", au_std_io_stdout, 0),
    AU_MODULE_FN("stdin", au_std_io_stdin, 0),
    AU_MODULE_FN("stderr", au_std_io_stderr, 0),
    AU_MODULE_FN("open", au_std_io_open, 2),
    AU_MODULE_FN("close", au_std_io_close, 1),
    AU_MODULE_FN("read", au_std_io_read, 1),
    AU_MODULE_FN("read_up_to", au_std_io_read_up_to, 2),
    AU_MODULE_FN("write", au_std_io_write, 2),
    AU_MODULE_FN("flush", au_std_io_flush, 1),
};
#endif

// * list.h *
static const struct std_module_fn list_fns[] = {
    AU_MODULE_FN("len", au_std_list_len, 1),
};

#ifdef AU_FEAT_MATH_LIB
// * math.h *
static const struct std_module_fn math_fns[] = {
    AU_MODULE_FN("abs", au_std_math_abs, 1),
    AU_MODULE_FN("max", au_std_math_max, 2),
    AU_MODULE_FN("min", au_std_math_min, 2),
    AU_MODULE_FN("exp", au_std_math_exp, 1),
    AU_MODULE_FN("ln", au_std_math_ln, 1),
    AU_MODULE_FN("log2", au_std_math_log2, 1),
    AU_MODULE_FN("log10", au_std_math_log10, 1),
    AU_MODULE_FN("sqrt", au_std_math_sqrt, 1),
    AU_MODULE_FN("cbrt", au_std_math_cbrt, 1),
    AU_MODULE_FN("hypot", au_std_math_hypot, 2),
    AU_MODULE_FN("pow", au_std_math_pow, 2),
    AU_MODULE_FN("sin", au_std_math_sin, 1),
    AU_MODULE_FN("cos", au_std_math_cos, 1),
    AU_MODULE_FN("tan", au_std_math_tan, 1),
    AU_MODULE_FN("asin", au_std_math_asin, 1),
    AU_MODULE_FN("acos", au_std_math_acos, 1),
    AU_MODULE_FN("atan", au_std_math_atan, 1),
    AU_MODULE_FN("atan2", au_std_math_atan2, 2),
    AU_MODULE_FN("sinh", au_std_math_sinh, 1),
    AU_MODULE_FN("cosh", au_std_math_cosh, 1),
    AU_MODULE_FN("tanh", au_std_math_tanh, 1),
    AU_MODULE_FN("asinh", au_std_math_asinh, 1),
    AU_MODULE_FN("acosh", au_std_math_acosh, 1),
    AU_MODULE_FN("atanh", au_std_math_atanh, 1),
    AU_MODULE_FN("erf", au_std_math_erf, 1),
    AU_MODULE_FN("erfc", au_std_math_erfc, 1),
    AU_MODULE_FN("lgamma", au_std_math_lgamma, 1),
    AU_MODULE_FN("tgamma", au_std_math_tgamma, 1),
    AU_MODULE_FN("ceil", au_std_math_ceil, 1),
    AU_MODULE_FN("floor", au_std_math_floor, 1),
    AU_MODULE_FN("trunc", au_std_math_trunc, 1),
    AU_MODULE_FN("round", au_std_math_round, 1),
    AU_MODULE_FN("is_finite", au_std_math_is_finite, 1),
    AU_MODULE_FN("is_infinite", au_std_math_is_infinite, 1),
    AU_MODULE_FN("is_nan", au_std_math_is_nan, 1),
    AU_MODULE_FN("is_normal", au_std_math_is_normal, 1),
};
#endif

// * str.h *
static const struct std_module_fn str_fns[] = {
    AU_MODULE_FN("into", au_std_str_into, 1),
    AU_MODULE_FN("char", au_std_str_char, 1),
    AU_MODULE_FN("ord", au_std_str_ord, 1),
    AU_MODULE_FN("bytes", au_std_str_bytes, 1),
    AU_MODULE_FN("code_points", au_std_str_code_points, 1),
    AU_MODULE_FN("index_of", au_std_str_index_of, 2),
    AU_MODULE_FN("contains", au_std_str_contains, 2),
    AU_MODULE_FN("starts_with", au_std_str_starts_with, 2),
    AU_MODULE_FN("ends_with", au_std_str_ends_with, 2),
    AU_MODULE_FN("is_space", au_std_str_is_space, 1),
    AU_MODULE_FN("is_digit", au_std_str_is_digit, 1),
};

// * sys.h *
static const struct std_module_fn sys_fns[] = {
    AU_MODULE_FN("abort", au_std_sys_abort, 0),
};

#ifdef AU_TEST
static const struct std_module_fn test_fns[] = {
    AU_MODULE_FN("test1", au_std_test_1, 1),
    AU_MODULE_FN("test2", au_std_test_2, 2),
};
#endif

// standard library definitions
static const struct std_module au_stdlib_modules_data[] = {
#define LIBRARY(NAME)                                                     \
    (struct std_module) {                                                 \
        .name = #NAME, .fns = NAME##_fns,                                 \
        .fns_len = sizeof(NAME##_fns) / sizeof(struct std_module_fn),     \
    }
    // clang-format off
    LIBRARY(array),
    LIBRARY(bool),
    LIBRARY(float),
    LIBRARY(gc),
    LIBRARY(int),
#ifdef AU_FEAT_IO_LIB
    LIBRARY(io),
#endif
    LIBRARY(list),
#ifdef AU_FEAT_MATH_LIB
    LIBRARY(math),
#endif
    LIBRARY(str),
    LIBRARY(sys),
#ifdef AU_TEST
    LIBRARY(test),
#endif
// clang-format on
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
    if (idx >= au_stdlib_modules_len)
        abort();
    const struct std_module *lib = &au_stdlib_modules_data[idx];
    au_extern_module_t module = au_extern_module_new();
    for (size_t i = 0; i < lib->fns_len; i++) {
        const struct std_module_fn *std_fn = &lib->fns[i];
        struct au_fn fn;
        fn.flags = AU_FN_FLAG_EXPORTED;
        fn.type = AU_FN_LIB;
        fn.as.lib_func = (struct au_lib_func){
            .num_args = std_fn->num_args,
            .func = std_fn->func,
            .name = std_fn->name,
            .symbol = std_fn->symbol,
        };
        const au_hm_var_value_t fn_idx = module->fns.len;
        if (au_hm_vars_add(&module->fn_map, std_fn->name,
                           strlen(std_fn->name), fn_idx) == 0) {
            au_fn_array_add(&module->fns, fn);
        } else {
            abort();
        }
    }
    return module;
}
