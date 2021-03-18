// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "core/rt/extern_fn.h"
#include "core/value_array.h"
#include "value.h"
#include <stdlib.h>
#endif

struct au_fn;
struct au_program_data;
struct au_fn_value;

_Public void au_fn_value_del(struct au_fn_value *fn_value);

_Public void au_fn_value_add_arg(struct au_fn_value *fn_value,
                                 au_value_t value);

#ifdef AU_IS_STDLIB
au_value_t au_fn_value_from_compiled(au_compiled_func_t fn_ptr,
                                     int32_t num_args);

au_value_t au_fn_value_from_native(au_extern_func_t fn_ptr,
                                   int32_t num_args);

int au_fn_value_add_arg_rt(au_value_t func_value, au_value_t arg_value);

au_value_t au_fn_value_call_rt(au_value_t func_value,
                               au_value_t *unbound_args,
                               int32_t num_unbound_args);
#else
struct au_fn_value *
au_fn_value_from_vm(const struct au_fn *fn,
                    const struct au_program_data *p_data);
#endif

struct au_vm_thread_local;
au_value_t au_fn_value_call_vm(const struct au_fn_value *fn_value,
                               struct au_vm_thread_local *tl,
                               au_value_t *unbound_args,
                               int num_unbound_args, int *is_native_out);

static inline struct au_fn_value *au_fn_value_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_FN)
        return 0;
    return au_value_get_fn(value);
}
