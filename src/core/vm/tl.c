// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "tl.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "platform/platform.h"
#include "stdlib/au_stdlib.h"

static AU_THREAD_LOCAL struct au_vm_thread_local *current_tl = 0;

struct au_vm_thread_local *au_vm_thread_local_get() {
    return current_tl;
}

void au_vm_thread_local_set(struct au_vm_thread_local *tl) {
    if (current_tl != 0 && tl != 0)
        au_fatal("trying to replace non-null thread local"
                 "(free and set it to null first!)\n");
    current_tl = tl;
}

void au_vm_thread_local_init(struct au_vm_thread_local *tl,
                             const struct au_program_data *p_data) {
    memset(tl, 0, sizeof(struct au_vm_thread_local));
    if (p_data->data_val.len == 0) {
        tl->const_cache = 0;
        tl->const_len = 0;
    } else {
        tl->const_cache = au_value_calloc(p_data->data_val.len);
        tl->const_len = p_data->data_val.len;
    }
    tl->print_fn = au_value_print;
    au_hm_vars_init(&tl->loaded_modules_map);
    tl->stack_max = (size_t)-1;
}

void au_vm_thread_local_del(struct au_vm_thread_local *tl) {
    au_vm_thread_local_del_const_cache(tl);
    au_hm_vars_del(&tl->loaded_modules_map);
    for (size_t i = 0; i < tl->loaded_modules.len; i++) {
        struct au_program_data *ptr = tl->loaded_modules.data[i];
        if (ptr == 0)
            continue;
        au_program_data_del(ptr);
        au_data_free(ptr);
    }
    au_data_free(tl->loaded_modules.data);
    au_data_free(tl->backtrace.data);
    memset(tl, 0, sizeof(struct au_vm_thread_local));
}

void au_vm_thread_local_add_const_cache(struct au_vm_thread_local *tl,
                                        size_t len) {
    if (len == 0)
        return;
    tl->const_cache = au_data_realloc(
        tl->const_cache, sizeof(au_value_t) * (tl->const_len + len));
    au_value_clear(&tl->const_cache[tl->const_len], len);
    tl->const_len += len;
}

void au_vm_thread_local_del_const_cache(struct au_vm_thread_local *tl) {
    if (!tl->const_len)
        return;
    for (size_t i = 0; i < tl->const_len; i++) {
        au_value_deref(tl->const_cache[i]);
    }
    au_data_free(tl->const_cache);
    tl->const_cache = 0;
    tl->const_len = 0;
}

int au_vm_thread_local_reserve_module(struct au_vm_thread_local *tl,
                                      const char *abspath,
                                      uint32_t *retidx) {
    au_hm_var_value_t value = tl->loaded_modules.len;
    if (au_hm_vars_add(&tl->loaded_modules_map, abspath, strlen(abspath),
                       value) != 0) {
        return 0;
    }
    *retidx = value;
    au_program_data_array_add(&tl->loaded_modules, 0);
    return 1;
}

void au_vm_thread_local_add_module(struct au_vm_thread_local *tl,
                                   const uint32_t idx,
                                   struct au_program_data *data) {
    if (au_program_data_array_at(&tl->loaded_modules, idx) != 0)
        abort();
    au_program_data_array_set(&tl->loaded_modules, idx, data);
}

struct au_program_data *
au_vm_thread_local_get_module(const struct au_vm_thread_local *tl,
                              const char *abspath) {
    const au_hm_var_value_t *value =
        au_hm_vars_get(&tl->loaded_modules_map, abspath, strlen(abspath));
    if (value == 0)
        return 0;
    return au_program_data_array_at(&tl->loaded_modules, *value);
}

void au_vm_thread_local_install_stdlib(struct au_vm_thread_local *tl) {
    for (size_t i = 0; i < au_stdlib_modules_len; i++) {
        au_program_data_array_add(&tl->stdlib_modules,
                                  au_stdlib_module(i));
    }
}