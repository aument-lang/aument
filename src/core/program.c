// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <string.h>

#include "program.h"

#include "stdlib/au_stdlib.h"

void au_program_data_init(struct au_program_data *data) {
    memset(data, 0, sizeof(struct au_program_data));
    au_bc_vars_init(&data->fn_map);
    au_install_stdlib(data);
}

void au_fn_del(struct au_fn *fn) {
    if (fn->type == AU_FN_BC) {
        au_bc_storage_del(&fn->as.bc_func);
    }
    memset(fn, 0, sizeof(struct au_fn));
}

void au_program_data_del(struct au_program_data *data) {
    au_bc_vars_del(&data->fn_map);
    for (size_t i = 0; i < data->fns.len; i++)
        au_fn_del(&data->fns.data[i]);
    free(data->fns.data);
    free(data->data_val.data);
    free(data->data_buf);
    for (size_t i = 0; i < data->imports.len; i++)
        free(data->imports.data[i]);
    free(data->imports.data);
    free(data->cwd);
    memset(data, 0, sizeof(struct au_program_data));
}

int au_program_data_add_data(struct au_program_data *p_data,
                             au_value_t value, uint8_t *v_data,
                             size_t len) {
    size_t buf_idx = 0;
    if (len != 0) {
        buf_idx = p_data->data_buf_len;
        p_data->data_buf =
            realloc(p_data->data_buf, p_data->data_buf_len + len);
        memcpy(&p_data->data_buf[buf_idx], v_data, len);
        p_data->data_buf_len += len;
    }

    const size_t idx = p_data->data_val.len;
    struct au_program_data_val new_val = (struct au_program_data_val){
        .real_value = value,
        .buf_idx = buf_idx,
        .buf_len = len,
    };
    au_program_data_vals_add(&p_data->data_val, new_val);
    return idx;
}

void au_program_del(struct au_program *p) {
    au_bc_storage_del(&p->main);
    au_program_data_del(&p->data);
    memset(p, 0, sizeof(struct au_program));
}