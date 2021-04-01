// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
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

#include "platform/platform.h"

enum au_fn_type {
    AU_FN_NONE = 0,
    AU_FN_LIB,
    AU_FN_BC,
    AU_FN_IMPORTER,
    AU_FN_DISPATCH,
};

/// [struct] An imported function. This object stores information about a
/// possibly unresolved function imported from another module.
struct au_imported_func {
    int32_t num_args;
    int32_t module_idx;
    char *name;
    size_t name_len;
    size_t fn_idx_cached;
    const struct au_program_data *p_data_cached;
};
// end-struct

/// [func] Deinitializes an au_imported_func instance
/// @param fn instance to be initialized
AU_PUBLIC void au_imported_func_del(struct au_imported_func *fn);

/// [struct] A forward declared function. This object must
/// only appear while parsing, and must not appear anywhere else.
struct au_none_func {
    /// Expected number of arguments this function has
    int32_t num_args;
    /// Expected name of the function
    struct au_token name_token;
};
// end-struct

struct au_dispatch_func_instance {
    struct au_class_interface *class_interface_cache;
    size_t function_idx;
    size_t class_idx;
};

AU_ARRAY_COPY(struct au_dispatch_func_instance,
              au_dispatch_func_instance_array, 1)

#define AU_DISPATCH_FUNC_NO_FALLBACK ((size_t)-1)

/// [struct] A dispatch method
struct au_dispatch_func {
    int32_t num_args;
    struct au_dispatch_func_instance_array data;
    size_t fallback_fn;
};
// end-struct

/// [func] Deinitializes an au_dispatch_func instance
/// @param fn instance to be initialized
AU_PUBLIC void au_dispatch_func_del(struct au_dispatch_func *fn);

#define AU_FN_FLAG_EXPORTED (1 << 0)
#define AU_FN_FLAG_HAS_CLASS (1 << 1)
#define AU_FN_FLAG_MAY_FAIL (1 << 2)

/// [struct] Representation of an Aument function, used by the parser and
/// the virtual machine.
struct au_fn {
    enum au_fn_type type;
    uint32_t flags;
    union {
        struct au_lib_func lib_func;
        struct au_bc_storage bc_func;
        struct au_imported_func imported_func;
        struct au_none_func none_func;
        struct au_dispatch_func dispatch_func;
    } as;
};

/// [func] Returns the number of arguments the function takes.
/// @param fn the function
/// @return the number of arguments
static inline int32_t au_fn_num_args(const struct au_fn *fn);

static inline int32_t au_fn_num_args(const struct au_fn *fn) {
    switch (fn->type) {
    case AU_FN_NONE: {
        return fn->as.none_func.num_args;
    }
    case AU_FN_LIB: {
        return fn->as.lib_func.num_args;
    }
    case AU_FN_BC: {
        return fn->as.bc_func.num_args;
    }
    case AU_FN_IMPORTER: {
        return fn->as.imported_func.num_args;
    }
    case AU_FN_DISPATCH: {
        return fn->as.dispatch_func.num_args;
    }
    default: {
        return -1;
    }
    }
}

/// [func] Deinitializes an au_fn instance
/// @param fn instance to be initialized
AU_PUBLIC void au_fn_del(struct au_fn *fn);

/// [func] Fills a cached reference to a function in an external
///     module, and a reference to module itself, into an
///     au_fn instance. This function is called on every import and
///     must NOT be used in functions shared across threads.
/// @param fn the imported function. This function must have the type
/// AU_FN_IMPORTER
AU_PRIVATE void
au_fn_fill_import_cache(struct au_fn *fn, size_t fn_idx_cached,
                        const struct au_program_data *p_data_cached);

/// [func] Resolves references to classes imported from external modules.
///     This function is called on every import and must NOT be used in
///     functions shared across threads.
/// @param fn an au_fn function
/// @param current_p_data the au_program_data instance that the function
///     belongs to.
AU_PRIVATE void
au_fn_fill_class_cache(struct au_fn *fn,
                       const struct au_program_data *current_p_data);

AU_ARRAY_STRUCT(struct au_fn, au_fn_array, 1)
