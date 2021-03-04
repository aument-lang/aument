// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdint.h>

#include "array.h"
#include "hm_vars.h"

#define AU_REGS 256
#define AU_MAX_LOCALS 65536
#define AU_MAX_ARRAY 65536
#define AU_MAX_STATIC_IDX 256

enum au_opcode {
    OP_MOV_U16 = 0,
    OP_MUL = 1,
    OP_DIV = 2,
    OP_ADD = 3,
    OP_SUB = 4,
    OP_MOD = 5,
    OP_MOV_REG_LOCAL = 6,
    OP_MOV_LOCAL_REG = 7,
    OP_PRINT = 8,
    OP_EQ = 9,
    OP_NEQ = 10,
    OP_LT = 11,
    OP_GT = 12,
    OP_LEQ = 13,
    OP_GEQ = 14,
    OP_JIF = 15,
    OP_JNIF = 16,
    OP_JREL = 17,
    OP_JRELB = 18,
    OP_LOAD_CONST = 19,
    OP_MOV_BOOL = 20,
    OP_NOP = 21,
    OP_MUL_ASG = 22,
    OP_DIV_ASG = 23,
    OP_ADD_ASG = 24,
    OP_SUB_ASG = 25,
    OP_MOD_ASG = 26,
    OP_PUSH_ARG = 27,
    OP_CALL = 28,
    OP_RET_LOCAL = 29,
    OP_RET = 30,
    OP_RET_NULL = 31,
    OP_IMPORT = 32,
    OP_ARRAY_NEW = 33,
    OP_ARRAY_PUSH = 34,
    OP_IDX_GET = 35,
    OP_IDX_SET = 36,
    OP_NOT = 37,
    OP_TUPLE_NEW = 38,
    OP_IDX_SET_STATIC = 39,
    OP_CLASS_GET_INNER = 40,
    OP_CLASS_SET_INNER = 41,
    OP_CLASS_NEW = 42,
    OP_CALL1 = 43,
    PRINTABLE_OP_LEN,
};

extern const char *au_opcode_dbg[AU_REGS];

ARRAY_TYPE_COPY(uint8_t, au_bc_buf, 4)

struct au_bc_storage {
    int num_args;
    int locals_len;
    size_t class_idx;
    struct au_class_interface *class_interface_cache;
    struct au_bc_buf bc;
    size_t source_map_start;
    int num_registers;
};

/// [func] Initializes an au_bc_storage instance
/// @param bc_storage instance to be initialized
void au_bc_storage_init(struct au_bc_storage *bc_storage);

/// [func] Deinitializes an au_bc_storage instance
/// @param bc_storage instance to be deinitialized
void au_bc_storage_del(struct au_bc_storage *bc_storage);

struct au_program_data;

/// [func] Debugs an bytecode storage container
/// @param bcs the bytecode storage
/// @param data program data
void au_bc_dbg(const struct au_bc_storage *bcs,
               const struct au_program_data *data);
