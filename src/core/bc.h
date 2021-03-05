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
#define AU_OP_MAX_PRINTABLE 44

enum au_opcode {
    AU_OP_MOV_U16 = 0,
    AU_OP_MUL = 1,
    AU_OP_DIV = 2,
    AU_OP_ADD = 3,
    AU_OP_SUB = 4,
    AU_OP_MOD = 5,
    AU_OP_MOV_REG_LOCAL = 6,
    AU_OP_MOV_LOCAL_REG = 7,
    AU_OP_PRINT = 8,
    AU_OP_EQ = 9,
    AU_OP_NEQ = 10,
    AU_OP_LT = 11,
    AU_OP_GT = 12,
    AU_OP_LEQ = 13,
    AU_OP_GEQ = 14,
    AU_OP_JIF = 15,
    AU_OP_JNIF = 16,
    AU_OP_JREL = 17,
    AU_OP_JRELB = 18,
    AU_OP_LOAD_CONST = 19,
    AU_OP_MOV_BOOL = 20,
    AU_OP_NOP = 21,
    AU_OP_MUL_ASG = 22,
    AU_OP_DIV_ASG = 23,
    AU_OP_ADD_ASG = 24,
    AU_OP_SUB_ASG = 25,
    AU_OP_MOD_ASG = 26,
    AU_OP_PUSH_ARG = 27,
    AU_OP_CALL = 28,
    AU_OP_RET_LOCAL = 29,
    AU_OP_RET = 30,
    AU_OP_RET_NULL = 31,
    AU_OP_IMPORT = 32,
    AU_OP_ARRAY_NEW = 33,
    AU_OP_ARRAY_PUSH = 34,
    AU_OP_IDX_GET = 35,
    AU_OP_IDX_SET = 36,
    AU_OP_NOT = 37,
    AU_OP_TUPLE_NEW = 38,
    AU_OP_IDX_SET_STATIC = 39,
    AU_OP_CLASS_GET_INNER = 40,
    AU_OP_CLASS_SET_INNER = 41,
    AU_OP_CLASS_NEW = 42,
    AU_OP_CALL1 = 43,
    AU_OP_LOAD_SELF = 44,
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
