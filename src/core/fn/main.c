// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "main.h"

void au_imported_func_del(struct au_imported_func *fn) { free(fn->name); }

void au_fn_del(struct au_fn *fn) {
    if (fn->type == AU_FN_BC) {
        au_bc_storage_del(&fn->as.bc_func);
    } else if (fn->type == AU_FN_IMPORTER) {
        au_imported_func_del(&fn->as.import_func);
    }
    memset(fn, 0, sizeof(struct au_fn));
}

void au_fn_fill_import_cache_unsafe(
    const struct au_fn *fn, const struct au_fn *fn_cached,
    const struct au_program_data *p_data_cached) {
    *(const struct au_fn **)(&fn->as.import_func.fn_cached) = fn_cached;
    *(const struct au_program_data **)(&fn->as.import_func.p_data_cached) =
        p_data_cached;
}
