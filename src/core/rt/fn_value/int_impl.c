// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "../au_array.h"
#include "../au_fn_value.h"
#include "../value.h"

#include "core/fn.h"
#include "core/vm/tl.h"
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

au_value_t au_fn_value_call(const struct au_fn_value *fn_value,
                            struct au_vm_thread_local *tl,
                            const struct au_program_data *p_data,
                            au_value_t *unbound_args,
                            int32_t num_unbound_args, int *is_native_out) {
    const int32_t num_bound_args = fn_value->bound_args.len;
    const int32_t total_args = num_bound_args + num_unbound_args;
    if (total_args != au_fn_num_args(fn_value->fn)) {
        return au_value_op_error();
    }
    au_value_t *args = au_value_calloc(total_args);
    for (int32_t i = 0; i < (int32_t)fn_value->bound_args.len; i++) {
        if (i == num_bound_args)
            break;
        au_value_ref(fn_value->bound_args.data[i]);
        args[i] = fn_value->bound_args.data[i];
    }
    for (int32_t i = 0; i < num_unbound_args; i++) {
        args[num_bound_args + i] = unbound_args[i];
        unbound_args[i] = au_value_none();
    }
    au_value_t retval =
        au_fn_call_internal(fn_value->fn, tl, p_data, args, is_native_out);
    for (int32_t i = 0; i < total_args; i++) {
        au_value_deref(args[i]);
    }
    au_data_free(args);
    return retval;
}
