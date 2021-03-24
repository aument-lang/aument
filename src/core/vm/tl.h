// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdlib.h>

#include "core/array.h"
#include "core/hm_vars.h"
#include "core/rt/value.h"
#include "core/vm/frame_link.h"
#include "platform/platform.h"

typedef void (*au_vm_print_fn_t)(au_value_t);

struct au_program_data;
AU_ARRAY_COPY(struct au_program_data *, au_program_data_array, 1)

struct au_vm_thread_local {
    au_vm_print_fn_t print_fn;
    au_value_t *const_cache;
    size_t const_len;
    struct au_hm_vars loaded_modules_map;
    struct au_program_data_array loaded_modules;
    struct au_program_data_array stdlib_modules;
    struct au_vm_frame_link current_frame;
    uintptr_t stack_start;
    size_t stack_max;
};

/// [func] Gets the current thread's au_vm_thread_local instance
/// @return the current thread's au_vm_thread_local instance
AU_PUBLIC struct au_vm_thread_local *au_vm_thread_local_get();

/// [func] Sets the current thread's au_vm_thread_local instance.
///     The caller must ensure that tl lasts for the lifetime
///     of an executed aument program.
/// @param tl the current thread's au_vm_thread_local instance
AU_PUBLIC void au_vm_thread_local_set(struct au_vm_thread_local *tl);

/// [func] Initializes an au_vm_thread_local instance
/// @param tl instance to be initialized
/// @param p_data global au_program_data instance
AU_PUBLIC void
au_vm_thread_local_init(struct au_vm_thread_local *tl,
                        const struct au_program_data *p_data);

/// [func] Deinitializes an au_vm_thread_local instance
/// @param tl instance to be deinitialized
AU_PUBLIC void au_vm_thread_local_del(struct au_vm_thread_local *tl);

AU_PRIVATE void
au_vm_thread_local_add_const_cache(struct au_vm_thread_local *tl,
                                   size_t len);

AU_PRIVATE void
au_vm_thread_local_del_const_cache(struct au_vm_thread_local *tl);

enum au_tl_reserve_mod_retval {
#define X(NAME) AU_TL_RESMOD_RETVAL_##NAME
    X(FAIL),
    X(OK),
    X(OK_MAIN_CALLED),
#undef X
};

AU_PRIVATE enum au_tl_reserve_mod_retval
au_vm_thread_local_reserve_module(struct au_vm_thread_local *tl,
                                  const char *abspath, uint32_t *retidx);

AU_PRIVATE enum au_tl_reserve_mod_retval
au_vm_thread_local_reserve_import_only(struct au_vm_thread_local *tl,
                                       const char *abspath);

AU_PRIVATE void
au_vm_thread_local_add_module(struct au_vm_thread_local *tl,
                              const uint32_t idx,
                              struct au_program_data *data);

AU_PRIVATE struct au_program_data *
au_vm_thread_local_get_module(const struct au_vm_thread_local *tl,
                              const char *abspath);

AU_PUBLIC void
au_vm_thread_local_install_stdlib(struct au_vm_thread_local *tl);
