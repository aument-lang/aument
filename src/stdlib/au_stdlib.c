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

// * array.h *
static const struct std_module_fn array_fns[] = {
    (struct std_module_fn){
        .name = "repeat", .func = au_std_array_repeat, .num_args = 2},
    (struct std_module_fn){
        .name = "push", .func = au_std_array_push, .num_args = 2},
    (struct std_module_fn){
        .name = "pop", .func = au_std_array_pop, .num_args = 1},
};

// * gc.h *
static const struct std_module_fn gc_fns[] = {
    (struct std_module_fn){
        .name = "heap_size", .func = au_std_gc_heap_size, .num_args = 0},
};

#ifdef AU_FEAT_IO_LIB
// * io.h *
static const struct std_module_fn io_fns[] = {
    (struct std_module_fn){
        .name = "stdout", .func = au_std_io_stdout, .num_args = 0},
    (struct std_module_fn){
        .name = "stdin", .func = au_std_io_stdin, .num_args = 0},
    (struct std_module_fn){
        .name = "stderr", .func = au_std_io_stderr, .num_args = 0},
    (struct std_module_fn){
        .name = "open", .func = au_std_io_open, .num_args = 2},
    (struct std_module_fn){
        .name = "close", .func = au_std_io_close, .num_args = 1},
    (struct std_module_fn){
        .name = "read", .func = au_std_io_read, .num_args = 1},
    (struct std_module_fn){
        .name = "read_up_to", .func = au_std_io_read_up_to, .num_args = 2},
    (struct std_module_fn){
        .name = "write", .func = au_std_io_write, .num_args = 2},
    (struct std_module_fn){
        .name = "flush", .func = au_std_io_flush, .num_args = 1},
};
#endif

// * list.h *
static const struct std_module_fn list_fns[] = {
    (struct std_module_fn){
        .name = "len", .func = au_std_list_len, .num_args = 1},
};

#ifdef AU_FEAT_MATH_LIB
// * math.h *
static const struct std_module_fn math_fns[] = {
    (struct std_module_fn){
        .name = "abs", .func = au_std_math_abs, .num_args = 1},
    (struct std_module_fn){
        .name = "max", .func = au_std_math_max, .num_args = 2},
    (struct std_module_fn){
        .name = "min", .func = au_std_math_min, .num_args = 2},
    (struct std_module_fn){
        .name = "exp", .func = au_std_math_exp, .num_args = 1},
    (struct std_module_fn){
        .name = "ln", .func = au_std_math_ln, .num_args = 1},
    (struct std_module_fn){
        .name = "log2", .func = au_std_math_log2, .num_args = 1},
    (struct std_module_fn){
        .name = "log10", .func = au_std_math_log10, .num_args = 1},
    (struct std_module_fn){
        .name = "sqrt", .func = au_std_math_sqrt, .num_args = 1},
    (struct std_module_fn){
        .name = "cbrt", .func = au_std_math_cbrt, .num_args = 1},
    (struct std_module_fn){
        .name = "hypot", .func = au_std_math_hypot, .num_args = 2},
    (struct std_module_fn){
        .name = "pow", .func = au_std_math_pow, .num_args = 2},
    (struct std_module_fn){
        .name = "sin", .func = au_std_math_sin, .num_args = 1},
    (struct std_module_fn){
        .name = "cos", .func = au_std_math_cos, .num_args = 1},
    (struct std_module_fn){
        .name = "tan", .func = au_std_math_tan, .num_args = 1},
    (struct std_module_fn){
        .name = "asin", .func = au_std_math_asin, .num_args = 1},
    (struct std_module_fn){
        .name = "acos", .func = au_std_math_acos, .num_args = 1},
    (struct std_module_fn){
        .name = "atan", .func = au_std_math_atan, .num_args = 1},
    (struct std_module_fn){
        .name = "atan2", .func = au_std_math_atan2, .num_args = 2},
    (struct std_module_fn){
        .name = "sinh", .func = au_std_math_sinh, .num_args = 1},
    (struct std_module_fn){
        .name = "cosh", .func = au_std_math_cosh, .num_args = 1},
    (struct std_module_fn){
        .name = "tanh", .func = au_std_math_tanh, .num_args = 1},
    (struct std_module_fn){
        .name = "asinh", .func = au_std_math_asinh, .num_args = 1},
    (struct std_module_fn){
        .name = "acosh", .func = au_std_math_acosh, .num_args = 1},
    (struct std_module_fn){
        .name = "atanh", .func = au_std_math_atanh, .num_args = 1},
    (struct std_module_fn){
        .name = "erf", .func = au_std_math_erf, .num_args = 1},
    (struct std_module_fn){
        .name = "erfc", .func = au_std_math_erfc, .num_args = 1},
    (struct std_module_fn){
        .name = "lgamma", .func = au_std_math_lgamma, .num_args = 1},
    (struct std_module_fn){
        .name = "tgamma", .func = au_std_math_tgamma, .num_args = 1},
    (struct std_module_fn){
        .name = "ceil", .func = au_std_math_ceil, .num_args = 1},
    (struct std_module_fn){
        .name = "floor", .func = au_std_math_floor, .num_args = 1},
    (struct std_module_fn){
        .name = "trunc", .func = au_std_math_trunc, .num_args = 1},
    (struct std_module_fn){
        .name = "round", .func = au_std_math_round, .num_args = 1},
    (struct std_module_fn){
        .name = "is_finite", .func = au_std_math_is_finite, .num_args = 1},
    (struct std_module_fn){.name = "is_infinite",
                           .func = au_std_math_is_infinite,
                           .num_args = 1},
    (struct std_module_fn){
        .name = "is_nan", .func = au_std_math_is_nan, .num_args = 1},
    (struct std_module_fn){
        .name = "is_normal", .func = au_std_math_is_normal, .num_args = 1},
};
#endif

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
    LIBRARY(array), LIBRARY(gc),
#ifdef AU_FEAT_IO_LIB
    LIBRARY(io),
#endif
    LIBRARY(list),
#ifdef AU_FEAT_MATH_LIB
    LIBRARY(math),
#endif
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
