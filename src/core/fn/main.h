// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/array.h"
#include "core/bc.h"
#include "core/hm_vars.h"
#include "core/parser/lexer.h"
#include "core/str_array.h"

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"

enum au_fn_type {
    AU_FN_NONE = 0,
    AU_FN_NATIVE,
    AU_FN_BC,
    AU_FN_IMPORTER,
    AU_FN_DISPATCH,
};

struct au_imported_func {
    int num_args;
    int module_idx;
    char *name;
    size_t name_len;
    const struct au_fn *fn_cached;
    const struct au_program_data *p_data_cached;
};

/// [func] Deinitializes an au_imported_func instance
/// @param fn instance to be initialized
void au_imported_func_del(struct au_imported_func *fn);

/// A forward declared function. This object must only appear while
/// parsing, and must not appear anywhere else.
struct au_none_func {
    /// Expected number of arguments this function has
    int num_args;
    /// Expected name of the function
    struct au_token name_token;
};

struct au_dispatch_func_instance {
    size_t function_idx;
    struct au_class_interface *class_interface_cache;
    size_t class_idx;
};

ARRAY_TYPE_COPY(struct au_dispatch_func_instance,
                au_dispatch_func_instance_array, 1)

#define AU_DISPATCH_FUNC_NO_FALLBACK ((size_t)-1)

struct au_dispatch_func {
    int num_args;
    struct au_dispatch_func_instance_array data;
    size_t fallback_fn;
};

/// [func] Deinitializes an au_dispatch_func instance
/// @param fn instance to be initialized
void au_dispatch_func_del(struct au_dispatch_func *fn);

#define AU_FN_FLAG_EXPORTED (1 << 0)
#define AU_FN_FLAG_HAS_CLASS (1 << 1)

struct au_fn {
    enum au_fn_type type;
    uint32_t flags;
    union {
        struct au_lib_func native_func;
        struct au_bc_storage bc_func;
        struct au_imported_func import_func;
        struct au_none_func none_func;
        struct au_dispatch_func dispatch_func;
    } as;
};

static inline int au_fn_num_args(const struct au_fn *fn) {
    switch (fn->type) {
    case AU_FN_NATIVE: {
        return fn->as.native_func.num_args;
    }
    case AU_FN_BC: {
        return fn->as.bc_func.num_args;
    }
    case AU_FN_IMPORTER: {
        return fn->as.import_func.num_args;
    }
    case AU_FN_DISPATCH: {
        return fn->as.dispatch_func.num_args;
    }
    default: {
        return 0;
    }
    }
}

/// [func] Deinitializes an au_fn instance
/// @param fn instance to be initialized
void au_fn_del(struct au_fn *fn);

/// [func] Fills a cached reference to a function in an external
///     module, and a reference to module itself, into the
///     au_fn instance. This function is unsafe so DO NOT
///     use it if the au_fn instance is shared across threads.
void au_fn_fill_import_cache_unsafe(
    const struct au_fn *fn, const struct au_fn *fn_cached,
    const struct au_program_data *p_data_cached);

void au_fn_fill_class_cache_unsafe(
    const struct au_fn *fn, const struct au_program_data *current_p_data);

ARRAY_TYPE_STRUCT(struct au_fn, au_fn_array, 1)
