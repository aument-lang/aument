// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "au_fn_value.h"
#include "au_array.h"
#include "value.h"
#endif

struct au_fn_value *au_fn_value_new(const struct au_fn *fn,
                                    const struct au_program_data *p_data) {
    struct au_fn_value *fn_value = au_obj_malloc(
        sizeof(struct au_fn_value), (au_obj_del_fn_t)au_fn_value_del);
    fn_value->rc = 1;
    fn_value->fn = fn;
    fn_value->p_data = p_data;
    fn_value->bound_args = (struct au_value_array){0};
    return fn_value;
}

void au_fn_value_del(struct au_fn_value *fn_value) {
    au_data_free(fn_value->bound_args.data);
}

void au_fn_value_add_arg(struct au_fn_value *fn_value, au_value_t value) {
    au_value_ref(value);
    au_value_array_add(&fn_value->bound_args, value);
}