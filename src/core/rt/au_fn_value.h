// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "value.h"
#include <stdlib.h>
#endif

struct au_fn_value;

struct au_fn_value *au_fn_value_new();
void au_fn_value_del(struct au_fn_value *fn_value);

#ifdef _AUMENT_H
struct au_fn_value *au_fn_value_coerce(au_value_t value);
#else
extern struct au_struct_vdata au_fn_value_vdata;
static inline struct au_fn_value *au_fn_value_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_fn_value_vdata)
        return 0;
    return (struct au_fn_value *)au_value_get_struct(value);
}
#endif