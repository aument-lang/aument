// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "tl.h"
#include "core/program.h"
#include "core/rt/exception.h"

void au_vm_thread_local_init(struct au_vm_thread_local *tl,
                             const struct au_program_data *p_data) {
    memset(tl, 0, sizeof(struct au_vm_thread_local));
    tl->const_cache = au_value_calloc(p_data->data_val.len);
    tl->const_len = p_data->data_val.len;
    tl->print_fn = au_value_print;
    au_hm_vars_init(&tl->loaded_modules_map);
}

void au_vm_thread_local_del(struct au_vm_thread_local *tl) {
    for (size_t i = 0; i < tl->const_len; i++) {
        au_value_deref(tl->const_cache[i]);
    }
    free(tl->const_cache);
    au_hm_vars_del(&tl->loaded_modules_map);
    for (size_t i = 0; i < tl->loaded_modules.len; i++) {
        struct au_program_data *ptr = tl->loaded_modules.data[i];
        if (ptr == 0)
            continue;
        au_program_data_del(ptr);
        free(ptr);
    }
    free(tl->loaded_modules.data);
    memset(tl, 0, sizeof(struct au_vm_thread_local));
}

void au_vm_thread_local_add_const_cache(struct au_vm_thread_local *tl,
                                        size_t len) {
    tl->const_cache = realloc(tl->const_cache,
                              sizeof(au_value_t) * (tl->const_len + len));
    au_value_clear(&tl->const_cache[tl->const_len], len);
    tl->const_len += len;
}

enum au_tl_reserve_mod_retval
au_vm_thread_local_reserve_import_only(struct au_vm_thread_local *tl,
                                       const char *abspath) {
    struct au_hm_var_value value = (struct au_hm_var_value){
        .idx = AU_HM_VAR_VALUE_NONE,
    };
    struct au_hm_var_value *old_value;
    if ((old_value = au_hm_vars_add(&tl->loaded_modules_map, abspath,
                                    strlen(abspath), &value)) != 0) {
        if (old_value->idx == AU_HM_VAR_VALUE_NONE) {
            return AU_TL_RESMOD_RETVAL_OK_MAIN_CALLED;
        }
        return AU_TL_RESMOD_RETVAL_FAIL;
    }
    return AU_TL_RESMOD_RETVAL_OK;
}

enum au_tl_reserve_mod_retval
au_vm_thread_local_reserve_module(struct au_vm_thread_local *tl,
                                  const char *abspath, uint32_t *retidx) {
    struct au_hm_var_value value = (struct au_hm_var_value){
        .idx = tl->loaded_modules.len,
    };
    struct au_hm_var_value *old_value;
    if ((old_value = au_hm_vars_add(&tl->loaded_modules_map, abspath,
                                    strlen(abspath), &value)) != 0) {
        if (old_value->idx == AU_HM_VAR_VALUE_NONE) {
            old_value->idx = value.idx;
            *retidx = value.idx;
            au_program_data_array_add(&tl->loaded_modules, 0);
            return AU_TL_RESMOD_RETVAL_OK_MAIN_CALLED;
        }
        return AU_TL_RESMOD_RETVAL_FAIL;
    }
    *retidx = value.idx;
    au_program_data_array_add(&tl->loaded_modules, 0);
    return AU_TL_RESMOD_RETVAL_OK;
}

void au_vm_thread_local_add_module(struct au_vm_thread_local *tl,
                                   const uint32_t idx,
                                   struct au_program_data *data) {
    if (idx >= tl->loaded_modules.len)
        au_fatal_index(&tl->loaded_modules, idx, tl->loaded_modules.len);
    if (tl->loaded_modules.data[idx] != 0)
        au_fatal("module is already loaded\n");
    tl->loaded_modules.data[idx] = data;
}

struct au_program_data *
au_vm_thread_local_get_module(const struct au_vm_thread_local *tl,
                              const char *abspath) {
    const struct au_hm_var_value *value =
        au_hm_vars_get(&tl->loaded_modules_map, abspath, strlen(abspath));
    if (!value || value->idx == AU_HM_VAR_VALUE_NONE)
        return 0;
    if (value->idx >= tl->loaded_modules.len)
        au_fatal_index(&tl->loaded_modules, value->idx,
                       tl->loaded_modules.len);
    return tl->loaded_modules.data[value->idx];
}
