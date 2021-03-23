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

#define AU_OP_MAX_PRINTABLE AU_OP_MUL_INT

enum au_opcode {
    AU_OP_LOAD_SELF = 0,
    AU_OP_MOV_U16 = 1,
    AU_OP_MUL = 2,
    AU_OP_DIV = 3,
    AU_OP_ADD = 4,
    AU_OP_SUB = 5,
    AU_OP_MOD = 6,
    AU_OP_MOV_REG_LOCAL = 7,
    AU_OP_MOV_LOCAL_REG = 8,
    AU_OP_PRINT = 9,
    AU_OP_EQ = 10,
    AU_OP_NEQ = 11,
    AU_OP_LT = 12,
    AU_OP_GT = 13,
    AU_OP_LEQ = 14,
    AU_OP_GEQ = 15,
    AU_OP_JIF = 16,
    AU_OP_JNIF = 17,
    AU_OP_JREL = 18,
    AU_OP_JRELB = 19,
    AU_OP_LOAD_CONST = 20,
    AU_OP_MOV_BOOL = 21,
    AU_OP_NOP = 22,
    AU_OP_MUL_ASG = 23,
    AU_OP_DIV_ASG = 24,
    AU_OP_ADD_ASG = 25,
    AU_OP_SUB_ASG = 26,
    AU_OP_MOD_ASG = 27,
    AU_OP_PUSH_ARG = 28,
    AU_OP_CALL = 29,
    AU_OP_RET_LOCAL = 30,
    AU_OP_RET = 31,
    AU_OP_RET_NULL = 32,
    AU_OP_IMPORT = 33,
    AU_OP_ARRAY_NEW = 34,
    AU_OP_ARRAY_PUSH = 35,
    AU_OP_IDX_GET = 36,
    AU_OP_IDX_SET = 37,
    AU_OP_NOT = 38,
    AU_OP_TUPLE_NEW = 39,
    AU_OP_IDX_SET_STATIC = 40,
    AU_OP_CLASS_GET_INNER = 41,
    AU_OP_CLASS_SET_INNER = 42,
    AU_OP_CLASS_NEW = 43,
    AU_OP_CALL1 = 44,
    AU_OP_SET_CONST = 45,
    AU_OP_LOAD_FUNC = 46,
    AU_OP_BIND_ARG_TO_FUNC = 47,
    AU_OP_CALL_FUNC_VALUE = 48,
    AU_OP_LOAD_NIL = 49,
    AU_OP_MUL_INT = 50,
    AU_OP_DIV_INT = 51,
    AU_OP_ADD_INT = 52,
    AU_OP_SUB_INT = 53,
    AU_OP_MOD_INT = 54,
    AU_OP_EQ_INT = 55,
    AU_OP_NEQ_INT = 56,
    AU_OP_LT_INT = 57,
    AU_OP_GT_INT = 58,
    AU_OP_LEQ_INT = 59,
    AU_OP_GEQ_INT = 60,
    AU_OP_JIF_BOOL = 61,
    AU_OP_JNIF_BOOL = 62,
    AU_OP_MUL_DOUBLE = 63,
    AU_OP_DIV_DOUBLE = 64,
    AU_OP_ADD_DOUBLE = 65,
    AU_OP_SUB_DOUBLE = 66,
    AU_OP_EQ_DOUBLE = 67,
    AU_OP_NEQ_DOUBLE = 68,
    AU_OP_LT_DOUBLE = 69,
    AU_OP_GT_DOUBLE = 70,
    AU_OP_LEQ_DOUBLE = 71,
    AU_OP_GEQ_DOUBLE = 72,
};

extern const char *au_opcode_dbg[AU_REGS];

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
