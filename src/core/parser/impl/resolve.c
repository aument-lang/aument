// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "def.h"

const au_hm_var_value_t *
au_parser_resolve_identifier(const struct au_parser *p,
                             struct au_token id_tok) {
    for (size_t i = p->vars.len; i-- > 0;) {
        struct au_hm_vars *vars = &p->vars.data[i];
        const au_hm_var_value_t *retval = 0;
        if ((retval = au_hm_vars_get(vars, id_tok.src, id_tok.len)) != 0) {
            return retval;
        }
    }
    return 0;
}

struct au_imported_module *
au_parser_resolve_module(struct au_parser *p, struct au_token module_tok,
                         size_t *module_idx_out) {
    const au_hm_var_value_t *module_val = au_hm_vars_get(
        &p->p_data->imported_module_map, module_tok.src, module_tok.len);
    if (module_val == 0) {
        return 0;
    }
    const au_hm_var_value_t module_idx = *module_val;
    if (module_idx_out != 0)
        *module_idx_out = module_idx;
    return &p->p_data->imported_modules.data[module_idx];
}

int au_parser_resolve_fn(struct au_parser *p, struct au_token module_tok,
                         struct au_token id_tok, int num_args_in,
                         // output
                         size_t *func_idx_out, int *execute_self_out,
                         int *create_function_out) {
    *create_function_out = 0;
    size_t func_idx = 0;
    int func_idx_found = 0;
    int execute_self = 0;
    if (module_tok.type != AU_TOK_EOF) {
        size_t module_idx = 0;
        struct au_imported_module *module =
            au_parser_resolve_module(p, module_tok, &module_idx);
        if (module == 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_MODULE,
                .data.unknown_id.name_token = module_tok,
            };
            return 0;
        }
        const au_hm_var_value_t *val =
            au_hm_vars_get(&module->fn_map, id_tok.src, id_tok.len);
        if (val == 0) {
            *create_function_out = 1;
            au_hm_var_value_t value = p->p_data->fns.len;
            char *import_name = au_data_malloc(id_tok.len);
            memcpy(import_name, id_tok.src, id_tok.len);
            struct au_fn fn = (struct au_fn){
                .type = AU_FN_IMPORTER,
                .flags = 0,
                .as.imported_func.num_args = num_args_in,
                .as.imported_func.module_idx = module_idx,
                .as.imported_func.name = import_name,
                .as.imported_func.name_len = id_tok.len,
                .as.imported_func.fn_idx_cached = 0,
                .as.imported_func.p_data_cached = 0,
            };
            au_fn_array_add(&p->p_data->fns, fn);
            func_idx = value;
            au_hm_var_value_t *old = au_hm_vars_add(
                &module->fn_map, id_tok.src, id_tok.len, value);
            EXPECT_BYTECODE(old == 0);
            val = au_hm_vars_get(&module->fn_map, id_tok.src, id_tok.len);
        } else {
            func_idx = *val;
        }
        func_idx_found = 1;
    } else if (p->self_name && id_tok.len == p->self_len &&
               memcmp(p->self_name, id_tok.src, p->self_len) == 0) {
        execute_self = 1;
        func_idx_found = 1;
    } else {
        const au_hm_var_value_t *val =
            au_hm_vars_get(&p->p_data->fn_map, id_tok.src, id_tok.len);
        if (val) {
            func_idx = *val;
            func_idx_found = 1;
        }
    }

    if (!func_idx_found) {
        *create_function_out = 1;
        au_hm_var_value_t func_value = p->p_data->fns.len;
        au_hm_vars_add(&p->p_data->fn_map, id_tok.src, id_tok.len,
                       func_value);
        struct au_fn none_func = (struct au_fn){
            .type = AU_FN_NONE,
            .as.none_func.num_args = num_args_in,
            .as.none_func.name_token = id_tok,
        };
        au_fn_array_add(&p->p_data->fns, none_func);
        au_str_array_add(&p->p_data->fn_names,
                         au_data_strndup(id_tok.src, id_tok.len));
        func_idx = func_value;
    }

    *func_idx_out = func_idx;
    *execute_self_out = execute_self;
    return 1;
}
