// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "../malloc.h"
#include "../value/main.h"
#include "./main.h"
#endif

typedef au_obj_del_fn_t au_struct_del_fn_t;
typedef int (*au_struct_idx_get_fn_t)(struct au_struct *self,
                                      au_value_t idx, au_value_t *value);
typedef int (*au_struct_idx_set_fn_t)(struct au_struct *self,
                                      au_value_t idx, au_value_t value);
typedef int32_t (*au_struct_len_fn_t)(struct au_struct *self);

struct au_struct_vdata {
    /// Deinitialization function to call when the structure is deleted.
    /// The function does not need to call free() or equivalents on itself
    au_obj_del_fn_t del_fn;
    /// This function is called when the indexing operator [] is used to
    /// get a value. The function **must** return the value into the
    /// `value` pointer, and increment its reference count, and it **must**
    /// return 1 on success or 0 if it's out of bound.
    au_struct_idx_get_fn_t idx_get_fn;
    /// This function is called when the indexing operator [] is used to
    /// set a value. The function **must** return the value into the
    /// `value` pointer, and increment its reference count, and it **must**
    /// return 1 on success or 0 if it's out of bound. If the value the
    /// user wants to set replaces the object's internal value, the
    /// function **must** dereference it.
    au_struct_idx_set_fn_t idx_set_fn;
    /// This function is called when the `len` function is used on the
    /// object.
    au_struct_len_fn_t len_fn;
};
