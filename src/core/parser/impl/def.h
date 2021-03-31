// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/array.h"
#include "core/bit_array.h"
#include "core/program.h"
#include "core/rt/au_class.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "core/utf8.h"
#include "os/mmap.h"
#include "platform/platform.h"
#include "stdlib/au_stdlib.h"

#include "core/parser/exception.h"
#include "core/parser/lexer.h"
#include "core/parser/parser.h"

#define CLASS_ID_NONE ((size_t)-1)
#define CACHED_REG_NONE ((uint8_t)-1)

AU_ARRAY_COPY(size_t, size_t_array, 1)
AU_ARRAY_COPY(uint8_t, reg_array, 1)

struct au_parser {
    /// Bytecode buffer that the parser is outputting to
    struct au_bc_buf bc;

    /// Stack of used register
    uint8_t rstack[AU_REGS];
    /// Length of rstack
    int rstack_len;

    /// Bitmap of used registers
    char used_regs[AU_BA_LEN(AU_REGS)];
    char pinned_regs[AU_BA_LEN(AU_REGS)];

    /// Hash table of local variables
    struct au_hm_vars vars;
    /// Local => reg mapping
    struct reg_array local_to_reg;

    /// Hash table of constant variables
    struct au_hm_vars consts;

    /// Global program data. This struct does not own this pointer.
    struct au_program_data *p_data;

    /// Number of local registers
    int num_locals;
    /// Maximum register index used
    int max_register;
    /// Current scope level
    int block_level;

    /// Name of current function or NULL. This struct does not own this
    /// pointer.
    const char *self_name;
    /// Byte size of self_name
    size_t self_len;
    /// Array of offsets representing AU_OP_CALL function index.
    ///     After parsing, the bytecode at offsets in this array
    ///     opcode will be filled with the current function index.
    struct size_t_array self_fill_call;
    /// Number of arguments in the current function
    int self_num_args;

    /// The index of this function's class
    struct au_class_interface *class_interface;
    /// Name of the "self" keyword in this function. This struct does not
    /// own this pointer.
    const char *self_keyword;
    /// Bytesize of self_keyword
    size_t self_keyword_len;

    /// Function ID
    size_t func_idx;

    /// Top level parser
    struct au_parser *top_level;

    /// Result of the parser
    struct au_parser_result res;
};

AU_PRIVATE void au_parser_init(struct au_parser *p,
                               struct au_program_data *p_data);
AU_PRIVATE void au_parser_del(struct au_parser *p);

#define EXPECT_TOKEN(CONDITION, TOKEN, EXPECTED)                          \
    do {                                                                  \
        if (AU_UNLIKELY(!(CONDITION))) {                                  \
            p->res = (struct au_parser_result){                           \
                .type = AU_PARSER_RES_UNEXPECTED_TOKEN,                   \
                .data.unexpected_token.got_token = TOKEN,                 \
                .data.unexpected_token.expected = EXPECTED,               \
            };                                                            \
            return 0;                                                     \
        }                                                                 \
    } while (0)

#define EXPECT_GLOBAL_SCOPE(TOKEN)                                        \
    do {                                                                  \
        if (AU_UNLIKELY(p->block_level != 0)) {                           \
            p->res = (struct au_parser_result){                           \
                .type = AU_PARSER_RES_EXPECT_GLOBAL_SCOPE,                \
                .data.expect_global.at_token = TOKEN,                     \
            };                                                            \
            return 0;                                                     \
        }                                                                 \
    } while (0)

#define EXPECT_BYTECODE(CONDITION)                                        \
    do {                                                                  \
        if (AU_UNLIKELY(!(CONDITION))) {                                  \
            p->res = (struct au_parser_result){                           \
                .type = AU_PARSER_RES_BYTECODE_GEN,                       \
            };                                                            \
            return 0;                                                     \
        }                                                                 \
    } while (0)

static inline int is_return_op(uint8_t op) {
    return op == AU_OP_RET_LOCAL || op == AU_OP_RET ||
           op == AU_OP_RET_NULL;
}

static inline int is_assign_tok(struct au_token op) {
    return op.type == AU_TOK_OPERATOR &&
           ((op.len == 1 && op.src[0] == '=') ||
            (op.len == 2 &&
             (op.src[0] == '+' || op.src[0] == '-' || op.src[0] == '*' ||
              op.src[0] == '/' || op.src[0] == '%') &&
             op.src[1] == '='));
}
