// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "../value/main.h"
#include "./main.h"
#endif

typedef void (*au_struct_del_fn_t)(void *self);
typedef au_value_t (*au_struct_idx_get_fn_t)(struct au_struct *self,
                                             au_value_t idx);
typedef int (*au_struct_idx_set_fn_t)(struct au_struct *self,
                                      au_value_t idx, au_value_t value);
typedef int32_t (*au_struct_len_fn_t)(struct au_struct *self);

struct au_struct_vdata {
    au_struct_del_fn_t del_fn;
    au_struct_idx_get_fn_t idx_get_fn;
    au_struct_idx_set_fn_t idx_set_fn;
    au_struct_len_fn_t len_fn;
};

/// Increases the reference count of an au_struct
/// @param header the au_struct instance
static inline void au_struct_ref(struct au_struct *header) {
    header->rc++;
#ifdef DEBUG_RC
    printf("[%p]: [ref] rc now %d\n", header, header->rc);
#endif
}

/// Decreases the reference count of an au_struct.
///     Automatically frees the au_struct if the
///     reference count reaches 0.
/// @param header the au_struct instance
static inline void au_struct_deref(struct au_struct *header) {
    header->rc--;
#ifdef DEBUG_RC
    printf("[%p]: [ref] rc now %d\n", header, header->rc);
#endif
    if (header->rc == 0) {
        header->vdata->del_fn(header);
    }
}