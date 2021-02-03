// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdint.h>

#include "bc_vars.h"
#include "array.h"

#define AU_REGS 256
#define AU_MAX_ARGS 15

enum au_opcode {
    OP_EXIT = 0,
    OP_MOV_U16 = 1,
    OP_MUL = 2,
    OP_DIV = 3,
    OP_ADD = 4,
    OP_SUB = 5,
    OP_MOD = 6,
    OP_MOV_REG_LOCAL = 7,
    OP_MOV_LOCAL_REG = 8,
    OP_PRINT = 9,
    OP_EQ = 10,
    OP_NEQ = 11,
    OP_LT = 12,
    OP_GT = 13,
    OP_LEQ = 14,
    OP_GEQ = 15,
    /// In bytecode, operation follows the format:
    /// [  op (8)  ] [  reg (8)  ] [  addr' (16)  ]
    ///   where addr = addr' * 4
    ///   (note that endianness of addr' depends on the platform)
    OP_JIF = 16,
    /// Bytecode is same as OP_JIF
    OP_JNIF = 17,
    OP_JREL = 18,
    OP_JRELB = 19,
    OP_LOAD_CONST = 20,
    OP_MOV_BOOL = 21,
    OP_NOP = 22,
    OP_MUL_ASG = 23,
    OP_DIV_ASG = 24,
    OP_ADD_ASG = 25,
    OP_SUB_ASG = 26,
    OP_MOD_ASG = 27,
    OP_PUSH_ARG = 28,
    OP_CALL0 = 29,
    OP_CALL1 = 30,
    OP_CALL2 = 31,
    OP_CALL3 = 32,
    OP_CALL4 = 33,
    OP_CALL5 = 34,
    OP_CALL6 = 35,
    OP_CALL7 = 36,
    OP_CALL8 = 37,
    OP_CALL9 = 38,
    OP_CALL10 = 39,
    OP_CALL11 = 40,
    OP_CALL12 = 41,
    OP_CALL13 = 42,
    OP_CALL14 = 43,
    OP_CALL15 = 44,
    OP_RET_LOCAL = 45,
    OP_RET = 46,
    OP_RET_NULL = 47,
    OP_IMPORT = 48,
    PRINTABLE_OP_LEN,
};

extern const char *au_opcode_dbg[256];

ARRAY_TYPE(uint8_t, au_bc_buf, 4)

struct au_bc_storage {
    struct au_bc_buf bc;
    int locals_len;
    int num_registers;
    int num_args;
};

void au_bc_storage_init(struct au_bc_storage *bc_storage);
void au_bc_storage_del(struct au_bc_storage *bc_storage);

struct au_program_data;
void au_bc_dbg(const struct au_bc_storage *bcs, const struct au_program_data *data);

struct au_program;
int au_parse(char *src, size_t len, struct au_program *program);
