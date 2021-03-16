// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/program.h"
#include "core/rt/extern_fn.h"
#include "core/rt/malloc.h"
#include "core/fn/main.h"

#include "platform/platform.h"

/// [func] Creates a new external module
/// @return a reference to the module
static _Unused inline struct au_program_data *au_module_new() {
    return (struct au_program_data *)au_data_calloc(
        sizeof(struct au_program_data));
}

/// [func] Declares an external function in the external module
/// @param p_data the module
/// @param name null-terminated string representing the name of the
/// function
/// @param func the pointer to the external function
/// @param num_args the number of arguments the external function takes
/// @return 0 if a function by that name already exists, 1 if successful
static _Unused inline int au_module_add_fn(struct au_program_data *p_data,
                                           const char *name,
                                           au_extern_func_t func,
                                           int num_args) {
    struct au_fn fn;
    fn.flags = AU_FN_FLAG_EXPORTED;
    fn.type = AU_FN_LIB;
    fn.as.lib_func = (struct au_lib_func){
        .num_args = num_args,
        .func = func,
        .name = name,
        .symbol = name,
    };
    const size_t fn_len = p_data->fns.len;
    if (au_hm_vars_add(&p_data->fn_map, name, strlen(name),
                       AU_HM_VAR_VALUE(fn_len)) == 0) {
        au_fn_array_add(&p_data->fns, fn);
        return 1;
    } else {
        return 0;
    }
}