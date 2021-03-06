// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "core/vm/vm.h"
#include "main.h"
#include "platform/platform.h"

/// [func] Calls another aulang function. If the called function
///     lies in another module, it will recursively search
///     for the real module that holds the function.
/// @param fn function to be called
/// @param tl current thread-local object
/// @param p_data the program data containing the function
/// @param args the arguments passed to the function
/// @return the return value of the function
static au_value_t au_fn_call(const struct au_fn *fn,
                             struct au_vm_thread_local *tl,
                             const struct au_program_data *p_data,
                             const au_value_t *args);

_Unused static au_value_t
au_fn_call_internal(const struct au_fn *fn, struct au_vm_thread_local *tl,
                    const struct au_program_data *p_data,
                    const au_value_t *args, int *is_native) {
self_call:
    switch (fn->type) {
    case AU_FN_NATIVE: {
        if (is_native) {
            *is_native = 1;
        }
        return fn->as.native_func.func(tl, args);
    }
    case AU_FN_BC: {
        if ((fn->flags & AU_FN_FLAG_HAS_CLASS) != 0) {
            struct au_obj_class *obj_class = au_obj_class_coerce(args[0]);
            if (_Unlikely((obj_class == 0) |
                          (fn->as.bc_func.class_interface_cache !=
                           obj_class->interface))) {
                return au_value_op_error();
            }
            return au_vm_exec_unverified(tl, &fn->as.bc_func, p_data,
                                         args);
        }
        return au_vm_exec_unverified(tl, &fn->as.bc_func, p_data, args);
    }
    case AU_FN_IMPORTER: {
        const struct au_fn *old_fn = fn;
        fn = old_fn->as.import_func.fn_cached;
        p_data = old_fn->as.import_func.p_data_cached;
        goto self_call;
    }
    case AU_FN_DISPATCH: {
        struct au_obj_class *obj_class = au_obj_class_coerce(args[0]);
        if (_Unlikely(obj_class == 0)) {
            return au_value_op_error();
        }
        const struct au_class_interface *class_interface =
            obj_class->interface;
        for (size_t i = 0; i < fn->as.dispatch_func.data.len; i++) {
            const struct au_dispatch_func_instance *inst =
                &fn->as.dispatch_func.data.data[i];
            if (inst->class_interface_cache == class_interface) {
                fn = &p_data->fns.data[inst->function_idx];
                goto self_call;
            }
        }
        const size_t fallback_fn = fn->as.dispatch_func.fallback_fn;
        if (fallback_fn == AU_DISPATCH_FUNC_NO_FALLBACK) {
            return au_value_op_error();
        } else {
            fn = &p_data->fns.data[fallback_fn];
            goto self_call;
        }
    }
    default: {
        _Unreachable;
        return au_value_none();
    }
    }
}

_Unused au_value_t au_fn_call(const struct au_fn *fn,
                              struct au_vm_thread_local *tl,
                              const struct au_program_data *p_data,
                              const au_value_t *args) {
    return au_fn_call_internal(fn, tl, p_data, args, 0);
}
