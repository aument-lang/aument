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
/// @param frame the caller's frame
/// @return the return value of the function
static _AlwaysInline au_value_t
au_fn_call(const struct au_fn *fn, struct au_vm_thread_local *tl,
           const struct au_program_data *p_data, const au_value_t *args,
           const struct au_vm_frame_link frame);

au_value_t au_fn_call(const struct au_fn *fn,
                      struct au_vm_thread_local *tl,
                      const struct au_program_data *p_data,
                      const au_value_t *args,
                      const struct au_vm_frame_link frame) {
self_call:
    switch (fn->type) {
    case AU_FN_NATIVE: {
        return fn->as.native_func.func(tl, args, frame);
    }
    case AU_FN_BC: {
        return au_vm_exec_unverified(tl, &fn->as.bc_func, p_data, args,
                                     frame);
    }
    case AU_FN_IMPORTER: {
        const struct au_fn *old_fn = fn;
        fn = old_fn->as.import_func.fn_cached;
        p_data = old_fn->as.import_func.p_data_cached;
        goto self_call;
    }
    default: {
        _Unreachable;
        return au_value_none();
    }
    }
}