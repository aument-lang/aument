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

au_value_t au_fn_value_from_compiled(au_compiled_func_t fn_ptr,
                                     int32_t num_args) {
    struct au_fn_value *fn_value = au_obj_malloc(
        sizeof(struct au_fn_value), (au_obj_del_fn_t)au_fn_value_del);
    fn_value->rc = 1;
    fn_value->bound_args = (struct au_value_array){0};
    fn_value->fn_ptr = (void *)fn_ptr;
    fn_value->num_args = num_args;
    fn_value->is_native_fn = 0;
    return au_value_fn(fn_value);
}

au_value_t au_fn_value_from_native(au_extern_func_t fn_ptr,
                                   int32_t num_args) {
    struct au_fn_value *fn_value = au_obj_malloc(
        sizeof(struct au_fn_value), (au_obj_del_fn_t)au_fn_value_del);
    fn_value->rc = 1;
    fn_value->bound_args = (struct au_value_array){0};
    fn_value->fn_ptr = (void *)fn_ptr;
    fn_value->num_args = num_args;
    fn_value->is_native_fn = 1;
    return au_value_fn(fn_value);
}

int au_fn_value_add_arg_rt(au_value_t fn_value_, au_value_t arg_value) {
    struct au_fn_value *fn_value = au_fn_value_coerce(fn_value_);
    if (fn_value == 0)
        return 0;
    au_fn_value_add_arg(fn_value, arg_value);
    return 1;
}

au_value_t au_fn_value_call_rt(au_value_t fn_value_,
                               au_value_t *unbound_args,
                               int32_t num_unbound_args) {
    struct au_fn_value *fn_value = au_fn_value_coerce(fn_value_);
    if (fn_value == 0)
        return au_value_op_error();

    const int32_t num_bound_args = fn_value->bound_args.len;
    const int32_t total_args = num_bound_args + num_unbound_args;
    if (total_args != fn_value->num_args) {
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

    au_value_t retval;
    if (fn_value->is_native_fn)
        retval = ((au_extern_func_t)fn_value->fn_ptr)(0, args);
    else
        retval = ((au_compiled_func_t)fn_value->fn_ptr)(args);

    for (int32_t i = 0; i < total_args; i++) {
        au_value_deref(args[i]);
    }
    au_data_free(args);
    return retval;
}
