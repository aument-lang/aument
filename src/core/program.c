// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <string.h>

#include "program.h"
#include "stdlib/au_stdlib.h"
#include "vm/vm.h"

void au_fn_del(struct au_fn *fn) {
    if (fn->type == AU_FN_BC) {
        au_bc_storage_del(&fn->as.bc_func);
    }
    memset(fn, 0, sizeof(struct au_fn));
}

void au_program_import_del(struct au_program_import *data) {
    free(data->path);
    memset(data, 0, sizeof(struct au_program_import));
}

void au_imported_module_init(struct au_imported_module *data) {
    memset(data, 0, sizeof(struct au_imported_module));
    au_hm_vars_init(&data->fn_map);
    au_hm_vars_init(&data->vars_map);
}

void au_imported_module_del(struct au_imported_module *data) {
    au_hm_vars_del(&data->fn_map);
    au_hm_vars_del(&data->vars_map);
}

void au_program_data_init(struct au_program_data *data) {
    memset(data, 0, sizeof(struct au_program_data));
    au_hm_vars_init(&data->fn_map);
    au_install_stdlib(data);
}

void au_program_data_del(struct au_program_data *data) {
    au_hm_vars_del(&data->fn_map);
    for (size_t i = 0; i < data->fns.len; i++)
        au_fn_del(&data->fns.data[i]);
    free(data->fns.data);
    free(data->data_val.data);
    free(data->data_buf);
    for (size_t i = 0; i < data->imports.len; i++)
        au_program_import_del(&data->imports.data[i]);
    free(data->imports.data);
    au_hm_vars_del(&data->imported_module_map);
    for (size_t i = 0; i < data->imported_modules.len; i++)
        au_imported_module_del(&data->imported_modules.data[i]);
    free(data->imported_modules.data);
    free(data->cwd);
    free(data->file);
    free(data->source_map.data);
    memset(data, 0, sizeof(struct au_program_data));
}

int au_program_data_add_data(struct au_program_data *p_data,
                             au_value_t value, uint8_t *v_data,
                             size_t v_len) {
    size_t buf_idx = 0;
    if (v_len != 0) {
        buf_idx = p_data->data_buf_len;
        p_data->data_buf =
            realloc(p_data->data_buf, p_data->data_buf_len + v_len);
        memcpy(&p_data->data_buf[buf_idx], v_data, v_len);
        p_data->data_buf_len += v_len;
    }

    const size_t idx = p_data->data_val.len;
    struct au_program_data_val new_val = (struct au_program_data_val){
        .real_value = value,
        .buf_idx = buf_idx,
        .buf_len = v_len,
    };
    au_program_data_vals_add(&p_data->data_val, new_val);
    return idx;
}

void au_program_del(struct au_program *p) {
    au_bc_storage_del(&p->main);
    au_program_data_del(&p->data);
    memset(p, 0, sizeof(struct au_program));
}

au_value_t au_fn_call(const struct au_fn *fn,
                      struct au_vm_thread_local *tl,
                      const struct au_program_data *p_data,
                      const au_value_t *args) {
    switch (fn->type) {
    case AU_FN_NATIVE: {
        return fn->as.native_func.func(tl, p_data, args);
    }
    case AU_FN_BC: {
        return au_vm_exec_unverified(tl, &fn->as.bc_func, p_data, args,
                                     (struct au_vm_frame_link){0});
    }
    case AU_FN_IMPORTER: {
        if (!fn->as.import_func.is_cached) {
            // FIXME: use atomic locking
            const struct au_program_data *func_p_data =
                &tl->module_data.data[p_data->tl_imported_modules_start +
                                      fn->as.import_func.num_args];
            const size_t func_idx_in_p_data =
                au_hm_vars_get(&func_p_data->fn_map,
                               fn->as.import_func.name,
                               fn->as.import_func.name_len)
                    ->idx;
            *(const struct au_fn **)(&fn->as.import_func.au_fn_cached) =
                &func_p_data->fns.data[func_idx_in_p_data];
            *(const struct au_program_data **)(&fn->as.import_func
                                                    .p_data_cached) =
                func_p_data;
            *(int *)(&fn->as.import_func.is_cached) = 1;
        }
        return au_fn_call(fn->as.import_func.au_fn_cached, tl,
                          fn->as.import_func.p_data_cached, args);
    }
    default: {
        _Unreachable;
        return au_value_none();
    }
    }
}