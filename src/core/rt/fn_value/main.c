// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "../au_array.h"
#include "../au_fn_value.h"
#include "../value.h"

#include "core/vm/tl.h"
#endif

void au_fn_value_del(struct au_fn_value *fn_value) {
    for (size_t i = 0; i < fn_value->bound_args.len; i++)
        au_value_deref(fn_value->bound_args.data[i]);
    au_data_free(fn_value->bound_args.data);
}

void au_fn_value_add_arg(struct au_fn_value *fn_value, au_value_t value) {
    au_value_ref(value);
    au_value_array_add(&fn_value->bound_args, value);
}
