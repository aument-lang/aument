// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdint.h>

#include "array.h"
#include "hm_vars.h"
#include "platform/platform.h"

#define AU_REGS 256
#define AU_MAX_LOCALS 65536
#define AU_MAX_ARRAY 65536
#define AU_MAX_STATIC_IDX 256
#define AU_MAX_FUNC_ID 65536
#define AU_MAX_OPCODE 256

#include "bc_data/opcodes.txt"

extern const char *au_opcode_dbg[AU_MAX_OPCODE];

AU_ARRAY_COPY(uint8_t, au_bc_buf, 4)

struct au_bc_storage {
    /// Number of arguments the function takes
    int num_args;
    /// Number of registers the function uses
    int num_registers;
    /// Number of locals the function uses
    int num_locals;
    /// Sum total of the number of registers and locals
    int num_values;
    /// Index of the class (if function is a method)
    size_t class_idx;
    /// Cached pointer to the class. This pointer is not reference counted.
    struct au_class_interface *class_interface_cache;
    /// Bytecode buffer
    struct au_bc_buf bc;
    /// At which index does the source map representation of the function
    /// start
    size_t source_map_start;
    size_t func_idx;
};

/// [func] Initializes an au_bc_storage instance
/// @param bc_storage instance to be initialized
AU_PUBLIC void au_bc_storage_init(struct au_bc_storage *bc_storage);

/// [func] Deinitializes an au_bc_storage instance
/// @param bc_storage instance to be deinitialized
AU_PUBLIC void au_bc_storage_del(struct au_bc_storage *bc_storage);

struct au_program_data;

/// [func] Debugs an bytecode storage container
/// @param bcs the bytecode storage
/// @param data program data
AU_PUBLIC void au_bc_dbg(const struct au_bc_storage *bcs,
                         const struct au_program_data *data);
