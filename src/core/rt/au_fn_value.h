// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "core/value_array.h"
#include "value.h"
#include <stdlib.h>
#endif

struct au_fn;
struct au_program_data;

struct au_fn_value {
    uint32_t rc;
    const struct au_fn *fn;
    const struct au_program_data *p_data;
    struct au_value_array bound_args;
};

struct au_fn_value *au_fn_value_new(const struct au_fn *fn,
                                    const struct au_program_data *p_data);
void au_fn_value_del(struct au_fn_value *fn_value);

void au_fn_value_add_arg(struct au_fn_value *fn_value, au_value_t value);

static inline struct au_fn_value *au_fn_value_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_FN)
        return 0;
    return au_value_get_fn(value);
}