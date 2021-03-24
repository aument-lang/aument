// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "main.h"
#include "core/program.h"

void au_imported_func_del(struct au_imported_func *fn) {
    au_data_free(fn->name);
}

void au_dispatch_func_del(struct au_dispatch_func *fn) {
    au_data_free(fn->data.data);
}

void au_fn_del(struct au_fn *fn) {
    switch (fn->type) {
    case AU_FN_BC: {
        au_bc_storage_del(&fn->as.bc_func);
        break;
    }
    case AU_FN_IMPORTER: {
        au_imported_func_del(&fn->as.imported_func);
        break;
    }
    case AU_FN_DISPATCH: {
        au_dispatch_func_del(&fn->as.dispatch_func);
        break;
    }
    default:
        break;
    }
    memset(fn, 0, sizeof(struct au_fn));
}

void au_fn_fill_import_cache_unsafe(
    struct au_fn *fn, const size_t fn_idx_cached,
    const struct au_program_data *p_data_cached) {
    fn->as.imported_func.fn_idx_cached = fn_idx_cached;
    fn->as.imported_func.p_data_cached = p_data_cached;
}

void au_fn_fill_class_cache_unsafe(
    struct au_fn *fn, const struct au_program_data *current_p_data) {
    switch (fn->type) {
    case AU_FN_BC: {
        if (fn->as.bc_func.class_interface_cache == 0) {
            fn->as.bc_func.class_interface_cache =
                current_p_data->classes.data[fn->as.bc_func.class_idx];
        }
        break;
    }
    case AU_FN_DISPATCH: {
        for (size_t i = 0; i < fn->as.dispatch_func.data.len; i++) {
            struct au_dispatch_func_instance *instance =
                &fn->as.dispatch_func.data.data[i];
            if (instance->class_interface_cache == 0) {
                instance->class_interface_cache =
                    current_p_data->classes.data[instance->class_idx];
            }
        }
        break;
    }
    default: {
        break;
    }
    }
}