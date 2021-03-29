// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#ifdef _WIN32
#include <locale.h>
#else
#define _GNU_SOURCE
#include <locale.h>
#include <stdlib.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define LC_NUMERIC_MASK LC_NUMERIC
typedef _locale_t locale_t;
static inline locale_t newlocale(int category_mask, const char *locale,
                                 locale_t _locale) {
    (void)_locale;
    return _create_locale(category_mask, locale);
}
#define strtod_l _strtod_l
#endif

#include "core/utf8.h"
#include "core/array.h"
#include "core/bit_array.h"
#include "core/program.h"
#include "core/rt/au_class.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "platform/mmap.h"

#include "stdlib/au_stdlib.h"

#include "lexer.h"
#include "parser.h"

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

static inline int is_return_op(uint8_t op) {
    return op == AU_OP_RET_LOCAL || op == AU_OP_RET ||
           op == AU_OP_RET_NULL;
}

static int is_assign_tok(struct au_token op) {
    return op.type == AU_TOK_OPERATOR &&
           ((op.len == 1 && op.src[0] == '=') ||
            (op.len == 2 &&
             (op.src[0] == '+' || op.src[0] == '-' || op.src[0] == '*' ||
              op.src[0] == '/' || op.src[0] == '%') &&
             op.src[1] == '='));
}

static void parser_flush_free_regs(struct au_parser *p) {
    p->rstack_len = 0;
    for (int i = 0; i < AU_BA_LEN(AU_REGS); i++) {
        p->used_regs[i] = p->pinned_regs[i];
    }
}

static void parser_init(struct au_parser *p,
                        struct au_program_data *p_data) {
    p->bc = (struct au_bc_buf){0};
    au_hm_vars_init(&p->vars);
    au_hm_vars_init(&p->consts);
    p->p_data = p_data;

    p->num_locals = 0;
    p->max_register = -1;
    p->block_level = 0;

    p->local_to_reg = (struct reg_array){0};

    p->self_name = 0;
    p->self_len = 0;
    p->self_fill_call = (struct size_t_array){0};
    p->self_num_args = 0;
    p->func_idx = AU_SM_FUNC_ID_MAIN;

    p->class_interface = 0;
    p->self_keyword = 0;
    p->self_keyword_len = 0;

    parser_flush_free_regs(p);

    p->top_level = 0;
}

static void parser_del(struct au_parser *p) {
    au_data_free(p->bc.data);
    au_hm_vars_del(&p->vars);
    au_data_free(p->local_to_reg.data);
    au_hm_vars_del(&p->consts);
    au_data_free(p->self_fill_call.data);
    memset(p, 0, sizeof(struct au_parser));
}

static int parser_new_reg(struct au_parser *p, uint8_t *out) {
    if (p->rstack_len + 1 > AU_REGS) {
        return 0;
    }

    uint8_t reg = 0;
    int found = 0;
    for (int i = 0; i < AU_REGS; i++) {
        if (!AU_BA_GET_BIT(p->used_regs, i)) {
            found = 1;
            reg = i;
            AU_BA_SET_BIT(p->used_regs, i);
            break;
        }
    }
    if (!found) {
        return 0;
    }

    p->rstack[p->rstack_len++] = reg;
    if (reg > p->max_register)
        p->max_register = reg;
    *out = reg;
    return 1;
}

/// The caller must NOT call any opcode that changes the result in the last
/// register
static uint8_t parser_last_reg(struct au_parser *p) {
    if (p->rstack_len == 0)
        abort();
    return p->rstack[p->rstack_len - 1];
}

static void parser_swap_top_regs(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len < 2))
        abort(); // TODO
    const uint8_t top2 = p->rstack[p->rstack_len - 2];
    p->rstack[p->rstack_len - 2] = p->rstack[p->rstack_len - 1];
    p->rstack[p->rstack_len - 1] = top2;
}

static void parser_push_reg(struct au_parser *p, uint8_t reg) {
    AU_BA_SET_BIT(p->used_regs, reg);
    if (AU_UNLIKELY(p->rstack_len + 1 > AU_REGS))
        abort(); // TODO
    p->rstack[p->rstack_len++] = reg;
    if (reg > p->max_register)
        p->max_register = reg;
}

static uint8_t parser_pop_reg(struct au_parser *p) {
    if (AU_UNLIKELY(p->rstack_len == 0))
        abort(); // TODO
    uint8_t reg = p->rstack[--p->rstack_len];
    if (!AU_BA_GET_BIT(p->pinned_regs, reg)) {
        AU_BA_RESET_BIT(p->used_regs, reg);
    }
    return reg;
}

static void parser_emit_bc_u8(struct au_parser *p, uint8_t val) {
    au_bc_buf_add(&p->bc, val);
}

static void replace_bc_u16(struct au_bc_buf *bc, size_t idx,
                           uint16_t val) {
    assert(idx + 1 < bc->len);
    uint16_t *ptr = (uint16_t *)(&bc->data[idx]);
    ptr[0] = val;
}

static void parser_replace_bc_u16(struct au_parser *p, size_t idx,
                                  uint16_t val) {
    replace_bc_u16(&p->bc, idx, val);
}

static void parser_emit_bc_u16(struct au_parser *p, uint16_t val) {
    const size_t offset = p->bc.len;
    parser_emit_bc_u8(p, 0);
    parser_emit_bc_u8(p, 0);
    uint16_t *ptr = (uint16_t *)(&p->bc.data[offset]);
    ptr[0] = val;
}

static void parser_emit_pad8(struct au_parser *p) {
    parser_emit_bc_u8(p, 0);
}

static AU_UNUSED void parser_flush_cached_regs(struct au_parser *p) {
    for (size_t i = 0; i < p->local_to_reg.len; i++) {
        p->local_to_reg.data[i] = CACHED_REG_NONE;
    }
    for (int i = 0; i < AU_BA_LEN(AU_REGS); i++) {
        p->pinned_regs[i] = 0;
    }
}

static void parser_invalidate_reg(struct au_parser *p, uint8_t reg) {
    if (AU_BA_GET_BIT(p->pinned_regs, reg)) {
        AU_BA_RESET_BIT(p->pinned_regs, reg);
        for (size_t i = 0; i < p->local_to_reg.len; i++) {
            if (p->local_to_reg.data[i] == reg) {
                p->local_to_reg.data[i] = CACHED_REG_NONE;
                break;
            }
        }
    }
}

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

static int parser_exec_statement(struct au_parser *p, struct au_lexer *l);

static int parser_exec_expr(struct au_parser *p, struct au_lexer *l);
static int parser_exec_assign(struct au_parser *p, struct au_lexer *l);
static int parser_exec_logical(struct au_parser *p, struct au_lexer *l);
static int parser_emit_bc_binary_expr(struct au_parser *p);
static int parser_exec_eq(struct au_parser *p, struct au_lexer *l);
static int parser_exec_cmp(struct au_parser *p, struct au_lexer *l);
static int parser_exec_addsub(struct au_parser *p, struct au_lexer *l);
static int parser_exec_muldiv(struct au_parser *p, struct au_lexer *l);
static int parser_exec_bitwise_logic(struct au_parser *p,
                                     struct au_lexer *l);
static int parser_exec_bitwise_shift(struct au_parser *p,
                                     struct au_lexer *l);
static int parser_exec_unary_expr(struct au_parser *p, struct au_lexer *l);
static int parser_exec_index_expr(struct au_parser *p, struct au_lexer *l);
static int parser_exec_value(struct au_parser *p, struct au_lexer *l);
static int parser_resolve_fn(struct au_parser *p,
                             struct au_token module_tok,
                             struct au_token id_tok, int num_args,
                             size_t *func_idx_out, int *execute_self_out);
static int parser_exec_call(struct au_parser *p, struct au_lexer *l,
                            struct au_token module_tok,
                            struct au_token id_tok, int has_self_argument);
static int parser_exec_array_or_tuple(struct au_parser *p,
                                      struct au_lexer *l, int is_tuple);
static int parser_exec_new_expr(struct au_parser *p, struct au_lexer *l);

static int parser_exec(struct au_parser *p, struct au_lexer *l) {
    while (1) {
        int retval = parser_exec_statement(p, l);
        if (retval == 0)
            return 0;
        else if (retval == -1)
            break;
        parser_flush_free_regs(p);
    }
    parser_emit_bc_u8(p, AU_OP_RET_NULL);
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    return 1;
}

static int parser_exec_export_statement(struct au_parser *p,
                                        struct au_lexer *l);
static int parser_exec_import_statement(struct au_parser *p,
                                        struct au_lexer *l);
static int parser_exec_class_statement(struct au_parser *p,
                                       struct au_lexer *l, int exported);
static int parser_exec_def_statement(struct au_parser *p,
                                     struct au_lexer *l, int exported);
static int parser_exec_const_statement(struct au_parser *p,
                                       struct au_lexer *l, int exported);
static int parser_exec_while_statement(struct au_parser *p,
                                       struct au_lexer *l);
static int parser_exec_if_statement(struct au_parser *p,
                                    struct au_lexer *l);
static int parser_exec_print_statement(struct au_parser *p,
                                       struct au_lexer *l);
static int parser_exec_return_statement(struct au_parser *p,
                                        struct au_lexer *l);

static int parser_exec_with_semicolon(struct au_parser *p,
                                      struct au_lexer *l, int retval) {
    if (!retval)
        return retval;
    struct au_token t = au_lexer_next(l);
    if (t.type == AU_TOK_EOF)
        return 1;
    EXPECT_TOKEN(t.type == AU_TOK_OPERATOR && t.len == 1 &&
                     t.src[0] == ';',
                 t, "';'");
    return 1;
}

// Block statements
static int parser_exec_block(struct au_parser *p, struct au_lexer *l) {
    p->block_level++;

    struct au_token t = au_lexer_next(l);
    EXPECT_TOKEN(t.type == AU_TOK_OPERATOR && t.len == 1 &&
                     t.src[0] == '{',
                 t, "'{'");

    while (1) {
        t = au_lexer_peek(l, 0);
        if (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == '}') {
            au_lexer_next(l);
            break;
        }

        int retval = parser_exec_statement(p, l);
        if (retval == 0)
            return 0;
        else if (retval == -1)
            break;
        parser_flush_free_regs(p);
    }

    p->block_level--;
    return 1;
}

static int parser_exec_statement(struct au_parser *p, struct au_lexer *l) {
#define WITH_SEMICOLON(FUNC) parser_exec_with_semicolon(p, l, FUNC(p, l))
    const struct au_token t = au_lexer_peek(l, 0);
    const size_t bc_from = p->bc.len;
    int retval = 0;
    if (t.type == AU_TOK_EOF) {
        return -1;
    } else if (t.type == AU_TOK_IDENTIFIER) {
        if (token_keyword_cmp(&t, "class")) {
            EXPECT_GLOBAL_SCOPE(t);
            au_lexer_next(l);
            retval = parser_exec_class_statement(p, l, 0);
        } else if (token_keyword_cmp(&t, "def")) {
            EXPECT_GLOBAL_SCOPE(t);
            au_lexer_next(l);
            retval = parser_exec_def_statement(p, l, 0);
        } else if (token_keyword_cmp(&t, "const")) {
            au_lexer_next(l);
            retval = parser_exec_with_semicolon(
                p, l, parser_exec_const_statement(p, l, 0));
        } else if (token_keyword_cmp(&t, "if")) {
            au_lexer_next(l);
            retval = parser_exec_if_statement(p, l);
        } else if (token_keyword_cmp(&t, "while")) {
            au_lexer_next(l);
            retval = parser_exec_while_statement(p, l);
        } else if (token_keyword_cmp(&t, "print")) {
            au_lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_print_statement);
        } else if (token_keyword_cmp(&t, "return")) {
            au_lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_return_statement);
        } else if (token_keyword_cmp(&t, "import")) {
            EXPECT_GLOBAL_SCOPE(t);
            au_lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_import_statement);
        } else if (token_keyword_cmp(&t, "export")) {
            EXPECT_GLOBAL_SCOPE(t);
            au_lexer_next(l);
            retval = parser_exec_export_statement(p, l);
        } else {
            retval = WITH_SEMICOLON(parser_exec_expr);
        }
    } else {
        retval = WITH_SEMICOLON(parser_exec_expr);
    }
    if (retval) {
        const size_t bc_to = p->bc.len;
        const size_t source_start = t.src - l->src;
        if (bc_from != bc_to) {
            struct au_program_source_map map =
                (struct au_program_source_map){
                    .bc_from = bc_from,
                    .bc_to = bc_to,
                    .source_start = source_start,
                    .func_idx = p->func_idx,
                };
            au_program_source_map_array_add(&p->p_data->source_map, map);
        }
    }
    return retval;
}

static int parser_exec_import_statement(struct au_parser *p,
                                        struct au_lexer *l) {
    const struct au_token path_tok = au_lexer_next(l);
    EXPECT_TOKEN(path_tok.type == AU_TOK_STRING, path_tok, "string");

    char *path_dup = au_data_malloc(path_tok.len + 1);
    memcpy(path_dup, path_tok.src, path_tok.len);
    path_dup[path_tok.len] = 0;

    const size_t idx = p->p_data->imports.len;
    struct au_token tok = au_lexer_peek(l, 0);
    if (token_keyword_cmp(&tok, "as")) {
        au_lexer_next(l);
        const struct au_token module_tok = au_lexer_next(l);

        const size_t module_idx = p->p_data->imported_modules.len;
        struct au_imported_module module;
        au_imported_module_init(&module);
        au_imported_module_array_add(&p->p_data->imported_modules, module);

        const au_hm_var_value_t *old_value =
            au_hm_vars_add(&p->p_data->imported_module_map, module_tok.src,
                           module_tok.len, module_idx);
        if (old_value != 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_DUPLICATE_MODULE,
                .data.duplicate_id.name_token = module_tok,
            };
            return 0;
        }

        const struct au_program_import import = (struct au_program_import){
            .path = path_dup,
            .module_idx = module_idx,
        };
        au_program_import_array_add(&p->p_data->imports, import);
    } else {
        const struct au_program_import import = (struct au_program_import){
            .path = path_dup,
            .module_idx = AU_PROGRAM_IMPORT_NO_MODULE,
        };
        au_program_import_array_add(&p->p_data->imports, import);
    }

    parser_emit_bc_u8(p, AU_OP_IMPORT);
    parser_emit_pad8(p);
    parser_emit_bc_u16(p, (uint16_t)idx);

    return 1;
}

static int parser_exec_export_statement(struct au_parser *p,
                                        struct au_lexer *l) {
    struct au_token tok = au_lexer_next(l);
    if (token_keyword_cmp(&tok, "def")) {
        return parser_exec_def_statement(p, l, 1);
    } else if (token_keyword_cmp(&tok, "class")) {
        return parser_exec_class_statement(p, l, 1);
    } else {
        EXPECT_TOKEN(0, tok, "'class', 'def'");
    }

    return 1;
}

static int parser_exec_class_statement(struct au_parser *p,
                                       struct au_lexer *l, int exported) {
    uint32_t class_flags = 0;
    if (exported)
        class_flags |= AU_CLASS_FLAG_EXPORTED;

    const struct au_token id_tok = au_lexer_next(l);
    EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok, "identifier");

    au_hm_var_value_t class_value = p->p_data->classes.len;
    au_hm_var_value_t *old_value = au_hm_vars_add(
        &p->p_data->class_map, id_tok.src, id_tok.len, class_value);
    if (old_value != 0) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_DUPLICATE_CLASS,
            .data.duplicate_id.name_token = id_tok,
        };
        return 0;
    }
    au_class_interface_ptr_array_add(&p->p_data->classes, 0);

    struct au_class_interface *interface =
        au_data_malloc(sizeof(struct au_class_interface));
    au_class_interface_init(interface,
                            au_data_strndup(id_tok.src, id_tok.len));
    interface->flags = class_flags;

    struct au_token t = au_lexer_next(l);
    if (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ';') {
        au_class_interface_ptr_array_set(&p->p_data->classes, class_value,
                                         interface);
        return 1;
    } else if (!(t.type == AU_TOK_OPERATOR && t.len == 1 &&
                 t.src[0] == '{')) {
        au_class_interface_deref(interface);
        EXPECT_TOKEN(0, t, "'}'");
    }

    while (1) {
        t = au_lexer_next(l);
        if (token_keyword_cmp(&t, "var")) {
            const struct au_token name_tok = au_lexer_next(l);
            EXPECT_TOKEN(name_tok.type == AU_TOK_IDENTIFIER, name_tok,
                         "identifier");
            au_hm_var_value_t prop_value = interface->map.entries_occ;

            const au_hm_var_value_t *old_prop_value = au_hm_vars_add(
                &interface->map, name_tok.src, name_tok.len, prop_value);
            if (old_prop_value != 0) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_DUPLICATE_PROP,
                    .data.duplicate_id.name_token = name_tok,
                };
                return 0;
            }

            const struct au_token semicolon = au_lexer_next(l);
            if (semicolon.type == AU_TOK_OPERATOR && semicolon.len == 1) {
                if (semicolon.src[0] == ';') {
                    continue;
                } else if (semicolon.src[0] == '}') {
                    break;
                }
            }
        } else if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == '}') {
            break;
        }
        au_class_interface_deref(interface);
        EXPECT_TOKEN(0, t, "'}'");
    }

    au_class_interface_ptr_array_set(&p->p_data->classes, class_value,
                                     interface);
    return 1;
}

static int parser_exec_def_statement(struct au_parser *p,
                                     struct au_lexer *l, int exported) {
    uint32_t fn_flags = 0;
    if (exported)
        fn_flags |= AU_FN_FLAG_EXPORTED;

    struct au_token tok = au_lexer_peek(l, 0);

    struct au_token self_tok = (struct au_token){.type = AU_TOK_EOF};
    size_t class_idx = CLASS_ID_NONE;
    struct au_class_interface *class_interface = 0;

    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == '(') {
        au_lexer_next(l);
        fn_flags |= AU_FN_FLAG_HAS_CLASS;

        self_tok = au_lexer_next(l);
        EXPECT_TOKEN(self_tok.type == AU_TOK_IDENTIFIER, self_tok,
                     "identifier");

        tok = au_lexer_next(l);
        EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                         tok.src[0] == ':',
                     tok, "':'");

        struct au_token name_tok = au_lexer_next(l);
        EXPECT_TOKEN(name_tok.type == AU_TOK_IDENTIFIER, name_tok,
                     "identifier");

        struct au_token module_tok = (struct au_token){.type = AU_TOK_EOF};
        tok = au_lexer_peek(l, 0);
        if (tok.type == AU_TOK_OPERATOR && tok.len == 2 &&
            tok.src[0] == ':' && tok.src[1] == ':') {
            module_tok = name_tok;
            au_lexer_next(l);
            name_tok = au_lexer_next(l);
            const au_hm_var_value_t *module_val =
                au_hm_vars_get(&p->p_data->imported_module_map,
                               module_tok.src, module_tok.len);
            if (module_val == 0) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNKNOWN_MODULE,
                    .data.unknown_id.name_token = module_tok,
                };
                return 0;
            }
            const uint32_t module_idx = *module_val;
            struct au_imported_module *module =
                &p->p_data->imported_modules.data[module_idx];
            au_hm_var_value_t class_val = p->p_data->classes.len;
            const au_hm_var_value_t *old_class_val = au_hm_vars_add(
                &module->class_map, name_tok.src, name_tok.len, class_val);
            if (old_class_val != 0) {
                class_val = *old_class_val;
            } else {
                au_class_interface_ptr_array_add(&p->p_data->classes, 0);
            }
            class_idx = class_val;
        } else {
            const au_hm_var_value_t *class_val = au_hm_vars_get(
                &p->p_data->class_map, name_tok.src, name_tok.len);
            if (class_val == 0) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNKNOWN_CLASS,
                    .data.unknown_id.name_token = name_tok,
                };
                return 0;
            }
            class_idx = *class_val;
            class_interface = p->p_data->classes.data[class_idx];
        }

        tok = au_lexer_next(l);
        EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                         tok.src[0] == ')',
                     tok, "')'");
    }

    const struct au_token id_tok = au_lexer_next(l);
    EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok, "identifier");

    struct au_token old_id_tok = (struct au_token){0};
    old_id_tok.type = AU_TOK_EOF;

    int expected_num_args = -1;

    au_hm_var_value_t func_value = p->p_data->fns.len;
    au_hm_var_value_t *old_value = au_hm_vars_add(
        &p->p_data->fn_map, id_tok.src, id_tok.len, func_value);
    if (old_value) {
        struct au_fn *old = &p->p_data->fns.data[*old_value];
        expected_num_args = au_fn_num_args(old);

        const int has_same_visibility =
            (old->flags & AU_FN_FLAG_EXPORTED) ==
            (fn_flags & AU_FN_FLAG_EXPORTED);

        if (old->type == AU_FN_NONE) {
            func_value = *old_value;
            old_id_tok = old->as.none_func.name_token;
        }
        // Record the new function into a multi-dispatch function, and
        // turn the regular old function into multi-dispatch function if
        // necessary
        else if (has_same_visibility &&
                 ((old->flags & AU_FN_FLAG_HAS_CLASS) != 0 ||
                  (fn_flags & AU_FN_FLAG_HAS_CLASS) != 0)) {
            if (old->type == AU_FN_DISPATCH) {
                struct au_dispatch_func_instance el =
                    (struct au_dispatch_func_instance){
                        .function_idx = func_value,
                        .class_idx = class_idx,
                        .class_interface_cache = class_interface,
                    };
                au_dispatch_func_instance_array_add(
                    &old->as.dispatch_func.data, el);
            } else {
                const size_t fallback_fn_idx = p->p_data->fns.len;
                const size_t new_fn_idx = p->p_data->fns.len + 1;

                const struct au_fn fallback_fn = *old;

                struct au_dispatch_func dispatch_func = {0};
                dispatch_func.num_args = expected_num_args;

                if ((fallback_fn.flags & AU_FN_FLAG_HAS_CLASS) != 0 &&
                    (fallback_fn.type == AU_FN_BC)) {
                    dispatch_func.fallback_fn =
                        AU_DISPATCH_FUNC_NO_FALLBACK;
                    struct au_dispatch_func_instance el =
                        (struct au_dispatch_func_instance){
                            .function_idx = fallback_fn_idx,
                            .class_idx = fallback_fn.as.bc_func.class_idx,
                            .class_interface_cache =
                                fallback_fn.as.bc_func
                                    .class_interface_cache,
                        };
                    au_dispatch_func_instance_array_add(
                        &dispatch_func.data, el);
                } else {
                    dispatch_func.fallback_fn = fallback_fn_idx;
                }
                struct au_dispatch_func_instance el =
                    (struct au_dispatch_func_instance){
                        .function_idx = new_fn_idx,
                        .class_idx = class_idx,
                        .class_interface_cache = class_interface,
                    };
                au_dispatch_func_instance_array_add(&dispatch_func.data,
                                                    el);

                old->type = AU_FN_DISPATCH;
                old->flags |= AU_FN_FLAG_HAS_CLASS;
                old->as.dispatch_func = dispatch_func;
                old = 0;

                au_fn_array_add(&p->p_data->fns, fallback_fn);
                func_value = new_fn_idx;
            }

            struct au_fn none_func = (struct au_fn){
                .type = AU_FN_NONE,
                .as.none_func.num_args = 0,
                .as.none_func.name_token.type = AU_TOK_EOF,
            };
            au_fn_array_add(&p->p_data->fns, none_func);
            au_str_array_add(&p->p_data->fn_names,
                             au_data_strndup(id_tok.src, id_tok.len));
        } else {
            func_value = *old_value;
            au_fn_del(old);
            *old = (struct au_fn){
                .type = AU_FN_NONE,
                .as.none_func.num_args = 0,
                .as.none_func.name_token.type = AU_TOK_EOF,
            };
        }
    } else {
        struct au_fn none_func = (struct au_fn){
            .type = AU_FN_NONE,
            .as.none_func.num_args = 0,
            .as.none_func.name_token.type = AU_TOK_EOF,
        };
        au_fn_array_add(&p->p_data->fns, none_func);
        au_str_array_add(&p->p_data->fn_names,
                         au_data_strndup(id_tok.src, id_tok.len));
    }

    struct au_parser func_p = {0};
    parser_init(&func_p, p->p_data);
    func_p.top_level = p;
    func_p.self_name = id_tok.src;
    func_p.self_len = id_tok.len;
    func_p.func_idx = func_value;
    func_p.class_interface = class_interface;
    struct au_bc_storage bcs = {0};

    if (self_tok.type != AU_TOK_EOF) {
        if (!token_keyword_cmp(&self_tok, "_")) {
            func_p.self_keyword = self_tok.src;
            func_p.self_keyword_len = self_tok.len;
            au_hm_vars_add(&func_p.vars, self_tok.src, self_tok.len, 0);
        }
        bcs.num_args++;
        func_p.num_locals++;
    }

    tok = au_lexer_next(l);
    EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                     tok.src[0] == '(',
                 tok, "'('");

    tok = au_lexer_peek(l, 0);
    if (tok.type == AU_TOK_IDENTIFIER) {
        au_lexer_next(l);

        const au_hm_var_value_t *old =
            au_hm_vars_add(&func_p.vars, tok.src, tok.len, bcs.num_args);
        if (old != NULL) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_DUPLICATE_ARG,
                .data.duplicate_id.name_token = tok,
            };
            return 0;
        }

        func_p.num_locals++;
        EXPECT_BYTECODE(func_p.num_locals <= AU_MAX_LOCALS);
        bcs.num_args++;
        while (1) {
            tok = au_lexer_peek(l, 0);
            if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                tok.src[0] == ')') {
                au_lexer_next(l);
                break;
            } else if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                       tok.src[0] == ',') {
                au_lexer_next(l);
                tok = au_lexer_next(l);
                EXPECT_TOKEN(tok.type == AU_TOK_IDENTIFIER, tok,
                             "identifier");
                const au_hm_var_value_t *old = au_hm_vars_add(
                    &func_p.vars, tok.src, tok.len, bcs.num_args);
                if (old != NULL) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_DUPLICATE_ARG,
                        .data.duplicate_id.name_token = tok,
                    };
                    return 0;
                }
                func_p.num_locals++;
                EXPECT_BYTECODE(func_p.num_locals < AU_MAX_LOCALS);
                bcs.num_args++;
            } else {
                EXPECT_TOKEN(0, tok, "arguments");
            }
        }
    } else if (tok.len == 1 && tok.src[0] == ')') {
        au_lexer_next(l);
    } else {
        EXPECT_TOKEN(0, tok, "arguments");
    }

    if (expected_num_args != -1 && bcs.num_args != expected_num_args) {
        if (old_id_tok.type != AU_TOK_EOF) {
            // The old_id_tok token is the identifier token belonging to
            // the call expression that forward-declared this function. As
            // such, our parser error should be reported differently.
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_WRONG_ARGS,
                .data.wrong_args.got_args = expected_num_args,
                .data.wrong_args.expected_args = bcs.num_args,
                .data.wrong_args.at_token = old_id_tok,
            };
            return 0;
        } else {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_WRONG_ARGS,
                .data.wrong_args.got_args = bcs.num_args,
                .data.wrong_args.expected_args = expected_num_args,
                .data.wrong_args.at_token = id_tok,
            };
            return 0;
        }
    }
    func_p.self_num_args = bcs.num_args;

    if ((fn_flags & AU_FN_FLAG_HAS_CLASS) != 0) {
        parser_emit_bc_u8(&func_p, AU_OP_LOAD_SELF);
        parser_emit_pad8(&func_p);
        parser_emit_pad8(&func_p);
        parser_emit_pad8(&func_p);
    }

    const size_t source_map_start = p->p_data->source_map.len;
    if (!parser_exec_block(&func_p, l)) {
        p->res = func_p.res;
        func_p.res = (struct au_parser_result){0};
        parser_del(&func_p);
        return 0;
    }
    parser_emit_bc_u8(&func_p, AU_OP_RET_NULL);

    bcs.num_locals = func_p.num_locals;
    bcs.num_registers = func_p.max_register + 1;
    bcs.num_values = bcs.num_locals + bcs.num_registers;
    bcs.bc = func_p.bc;
    bcs.class_idx = class_idx;
    bcs.class_interface_cache = class_interface;
    bcs.source_map_start = source_map_start;
    bcs.func_idx = func_p.func_idx;
    func_p.bc = (struct au_bc_buf){0};

    for (size_t i = 0; i < func_p.self_fill_call.len; i++) {
        const size_t offset = func_p.self_fill_call.data[i];
        replace_bc_u16(&bcs.bc, offset, func_value);
    }
    *au_fn_array_at_mut(&p->p_data->fns, func_value) = (struct au_fn){
        .type = AU_FN_BC,
        .flags = fn_flags,
        .as.bc_func = bcs,
    };

    parser_del(&func_p);

    return 1;
}

static int parser_exec_const_statement(struct au_parser *p,
                                       struct au_lexer *l, int exported) {
    (void)exported;

    const struct au_token id_tok = au_lexer_next(l);
    EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok, "identifier");

    const struct au_token eq_tok = au_lexer_next(l);
    EXPECT_TOKEN(eq_tok.type == AU_TOK_OPERATOR &&
                     (eq_tok.len == 1 && eq_tok.src[0] == '='),
                 eq_tok, "'='");

    if (!parser_exec_expr(p, l))
        return 0;
    const uint8_t right_reg = parser_pop_reg(p);

    if (p->func_idx == AU_SM_FUNC_ID_MAIN) {
        // Main function
        int data_len = p->p_data->data_val.len;
        au_hm_var_value_t *old =
            au_hm_vars_add(&p->consts, id_tok.src, id_tok.len, data_len);

        if (old == 0) {
            au_program_data_add_data(p->p_data, au_value_none(), 0, 0);
            parser_emit_bc_u8(p, AU_OP_SET_CONST);
            parser_emit_bc_u8(p, right_reg);
            parser_emit_bc_u16(p, data_len);
        } else {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_DUPLICATE_CONST,
                .data.duplicate_id.name_token = id_tok,
            };
            return 0;
        }
    } else {
        abort(); // TODO
    }

    return 1;
}

static int parser_exec_if_statement(struct au_parser *p,
                                    struct au_lexer *l) {
    parser_flush_cached_regs(p);

    // condition
    int has_else_part = 0;
    if (!parser_exec_expr(p, l))
        return 0;

    const size_t c_len = p->bc.len;
    parser_flush_cached_regs(p);

    parser_emit_bc_u8(p, AU_OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    // body
    size_t body_len;
    size_t body_replace_idx = (size_t)-1;

    if (!parser_exec_block(p, l))
        return 0;
    parser_flush_cached_regs(p);

    if (!is_return_op(p->bc.data[p->bc.len - 4])) {
        body_len = p->bc.len;

        parser_emit_bc_u8(p, AU_OP_JREL);
        parser_emit_pad8(p);
        body_replace_idx = p->bc.len;
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }
    // else
    {
        const struct au_token t = au_lexer_peek(l, 0);
        if (token_keyword_cmp(&t, "else")) {
            au_lexer_next(l);

            const size_t else_start = p->bc.len;
            {
                const struct au_token t = au_lexer_peek(l, 0);
                if (token_keyword_cmp(&t, "if")) {
                    au_lexer_next(l);
                    if (!parser_exec_if_statement(p, l))
                        return 0;
                } else {
                    if (!parser_exec_block(p, l))
                        return 0;
                }
            }
            parser_flush_cached_regs(p);
            has_else_part = 1;

            const size_t else_len = p->bc.len;
            parser_flush_cached_regs(p);

            size_t else_replace_idx = (size_t)-1;
            if (!is_return_op(p->bc.data[p->bc.len - 4])) {
                parser_emit_bc_u8(p, AU_OP_JREL);
                parser_emit_pad8(p);
                else_replace_idx = p->bc.len;
                parser_emit_pad8(p);
                parser_emit_pad8(p);
            }

            const size_t end_len = p->bc.len;

            // Else jump
            if (else_replace_idx != (size_t)-1) {
                const size_t offset = (end_len - else_len) / 4;
                EXPECT_BYTECODE(offset <= (uint16_t)-1);
                parser_replace_bc_u16(p, else_replace_idx,
                                      (uint16_t)offset);
            }

            // Condition jump
            {
                const size_t offset = (else_start - c_len) / 4;
                EXPECT_BYTECODE(offset <= (uint16_t)-1);
                parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
            }
        }
    }

    const size_t end_len = p->bc.len;

    // Condition jump
    if (!has_else_part) {
        const size_t offset = (end_len - c_len) / 4;
        EXPECT_BYTECODE(offset <= (uint16_t)-1);
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if (body_replace_idx != (size_t)-1) {
        const size_t offset = (end_len - body_len) / 4;
        EXPECT_BYTECODE(offset <= (uint16_t)-1);
        parser_replace_bc_u16(p, body_replace_idx, (uint16_t)offset);
    }

    // The resulting bytecode should look like this:
    //       (flush locals)
    //   condition:
    //       ...
    //       (flush locals)
    //       jnif [cond], else
    //   body:
    //       ...
    //       (flush locals)
    //       jmp if_end
    //   else:
    //       ...
    //       (flush locals)
    //       jmp if_end
    //   if_end:
    //       ...
    return 1;
}

static int parser_exec_while_statement(struct au_parser *p,
                                       struct au_lexer *l) {
    parser_flush_cached_regs(p);

    // condition
    const size_t cond_part = p->bc.len;
    if (!parser_exec_expr(p, l))
        return 0;

    parser_flush_cached_regs(p);

    const size_t c_len = p->bc.len;
    parser_emit_bc_u8(p, AU_OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);

    // body
    if (!parser_exec_block(p, l))
        return 0;
    parser_flush_cached_regs(p);

    size_t body_len = p->bc.len;
    size_t body_replace_idx = (size_t)-1;
    parser_emit_bc_u8(p, AU_OP_JRELB);
    parser_emit_pad8(p);
    body_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);

    const size_t end_len = p->bc.len;

    // Condition jump
    {
        const size_t offset = (end_len - c_len) / 4;
        EXPECT_BYTECODE(offset <= (uint16_t)-1);
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if (body_replace_idx != (size_t)-1) {
        const size_t offset = (body_len - cond_part) / 4;
        EXPECT_BYTECODE(offset <= (uint16_t)-1);
        parser_replace_bc_u16(p, body_replace_idx, (uint16_t)offset);
    }

    // The resulting bytecode should look like this:
    //       (flush locals)
    //   condition:
    //       [cond] = ...
    //       (flush locals)
    //       jnif [cond], end
    //   block:
    //       ...
    //       (flush locals)
    //       jrelb condition
    //   end:
    return 1;
}

static int parser_exec_print_statement(struct au_parser *p,
                                       struct au_lexer *l) {
    if (!parser_exec_expr(p, l))
        return 0;
    parser_emit_bc_u8(p, AU_OP_PRINT);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    while (1) {
        const struct au_token t = au_lexer_peek(l, 0);
        if (t.type == AU_TOK_EOF ||
            (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ';')) {
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == ',') {
            au_lexer_next(l);
            if (!parser_exec_expr(p, l))
                return 0;
            parser_emit_bc_u8(p, AU_OP_PRINT);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            continue;
        } else {
            return 1;
        }
    }
}

static int parser_exec_return_statement(struct au_parser *p,
                                        struct au_lexer *l) {
    if (!parser_exec_expr(p, l))
        return 0;
    const uint8_t reg = parser_pop_reg(p);
    if (p->bc.len > 4 &&
        (p->bc.data[p->bc.len - 4] == AU_OP_MOV_LOCAL_REG &&
         p->bc.data[p->bc.len - 3] == reg)) {
        // OPTIMIZE: peephole optimization for local returns
        p->bc.data[p->bc.len - 4] = AU_OP_RET_LOCAL;
    } else {
        parser_emit_bc_u8(p, AU_OP_RET);
        parser_emit_bc_u8(p, reg);
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }
    return 1;
}

static int parser_exec_call_args(struct au_parser *p, struct au_lexer *l,
                                 struct reg_array *regs) {
    {
        const struct au_token t = au_lexer_peek(l, 0);
        if (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ')') {
            au_lexer_next(l);
            return 1;
        }
        if (!parser_exec_expr(p, l))
            return 0;
        const uint8_t reg = parser_pop_reg(p);
        reg_array_add(regs, reg);
        AU_BA_SET_BIT(p->used_regs, reg);
    }
    while (1) {
        const struct au_token t = au_lexer_next(l);
        if (t.type == AU_TOK_EOF ||
            (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ')')) {
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == ',') {
            if (!parser_exec_expr(p, l))
                return 0;
            const uint8_t reg = parser_pop_reg(p);
            reg_array_add(regs, reg);
            AU_BA_SET_BIT(p->used_regs, reg);
            continue;
        } else {
            EXPECT_TOKEN(0, t, "',' or ')'");
        }
    }
    return 1;
}

static int parser_exec_expr(struct au_parser *p, struct au_lexer *l) {
    return parser_exec_assign(p, l);
}

static int parser_exec_assign(struct au_parser *p, struct au_lexer *l) {
    const struct au_token t = au_lexer_peek(l, 0);
    if (t.type == AU_TOK_IDENTIFIER || t.type == AU_TOK_AT_IDENTIFIER) {
        const struct au_token op = au_lexer_peek(l, 1);
        if (is_assign_tok(op)) {
            au_lexer_next(l);
            au_lexer_next(l);

            if (!parser_exec_expr(p, l))
                return 0;

            if (t.type == AU_TOK_AT_IDENTIFIER) {
                const struct au_class_interface *interface =
                    p->class_interface;
                if (interface == 0) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_CLASS_SCOPE_ONLY,
                        .data.class_scope.at_token = t,
                    };
                    return 0;
                }

                const au_hm_var_value_t *value =
                    au_hm_vars_get(&interface->map, &t.src[1], t.len - 1);
                if (value == 0) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_UNKNOWN_VAR,
                        .data.unknown_id.name_token = t,
                    };
                    return 0;
                }

                if (!(op.len == 1 && op.src[0] == '=')) {
                    uint8_t reg;
                    EXPECT_BYTECODE(parser_new_reg(p, &reg));
                    parser_emit_bc_u8(p, AU_OP_CLASS_GET_INNER);
                    parser_emit_bc_u8(p, reg);
                    parser_emit_bc_u16(p, *value);
                    switch (op.src[0]) {
#define BIN_OP_ASG(OP, OPCODE)                                            \
    case OP: {                                                            \
        parser_emit_bc_u8(p, OPCODE);                                     \
        break;                                                            \
    }
                        BIN_OP_ASG('*', AU_OP_MUL)
                        BIN_OP_ASG('/', AU_OP_DIV)
                        BIN_OP_ASG('+', AU_OP_ADD)
                        BIN_OP_ASG('-', AU_OP_SUB)
                        BIN_OP_ASG('%', AU_OP_MOD)
#undef BIN_OP_ASG
                    }
                    if (!parser_emit_bc_binary_expr(p))
                        return 0;
                }

                parser_emit_bc_u8(p, AU_OP_CLASS_SET_INNER);
                parser_emit_bc_u8(p, parser_last_reg(p));
                parser_emit_bc_u16(p, *value);

                return 1;
            }

            if (!(op.len == 1 && op.src[0] == '=')) {
                const au_hm_var_value_t *local_value =
                    au_hm_vars_get(&p->vars, t.src, t.len);
                if (local_value == 0) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_UNKNOWN_VAR,
                        .data.unknown_id.name_token = t,
                    };
                    return 0;
                }

                const au_hm_var_value_t local = *local_value;
                const uint8_t modifier_reg = parser_last_reg(p);
                uint8_t result_reg;

                if (local < p->local_to_reg.len &&
                    p->local_to_reg.data[local] != CACHED_REG_NONE) {
                    result_reg = p->local_to_reg.data[local];
                } else {
                    EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
                    for (size_t i = p->local_to_reg.len;
                         i < (size_t)local + 1; i++) {
                        reg_array_add(&p->local_to_reg, CACHED_REG_NONE);
                    }
                    p->local_to_reg.data[local] = result_reg;
                }

                parser_emit_bc_u8(p, AU_OP_MOV_LOCAL_REG);
                parser_emit_bc_u8(p, result_reg);
                parser_emit_bc_u16(p, local);

                if (op.src[0] == '*')
                    parser_emit_bc_u8(p, AU_OP_MUL);
                else if (op.src[0] == '/')
                    parser_emit_bc_u8(p, AU_OP_DIV);
                else if (op.src[0] == '+')
                    parser_emit_bc_u8(p, AU_OP_ADD);
                else if (op.src[0] == '-')
                    parser_emit_bc_u8(p, AU_OP_SUB);
                else if (op.src[0] == '%')
                    parser_emit_bc_u8(p, AU_OP_MOD);
                else
                    au_fatal("unimplemented op '%.*s'\n", (int)op.len,
                             op.src);
                parser_emit_bc_u8(p, result_reg);
                parser_emit_bc_u8(p, modifier_reg);
                parser_emit_bc_u8(p, result_reg);

                parser_emit_bc_u8(p, AU_OP_MOV_REG_LOCAL);
                parser_emit_bc_u8(p, result_reg);
                parser_emit_bc_u16(p, local);

                return 1;
            }

            parser_emit_bc_u8(p, AU_OP_MOV_REG_LOCAL);

            const uint8_t new_reg = parser_last_reg(p);
            parser_emit_bc_u8(p, new_reg);

            const au_hm_var_value_t new_value = p->num_locals;
            const au_hm_var_value_t *old_value =
                au_hm_vars_add(&p->vars, t.src, t.len, new_value);
            if (old_value) {
                if (*old_value < p->local_to_reg.len) {
                    const uint8_t old_reg =
                        p->local_to_reg.data[*old_value];
                    AU_BA_RESET_BIT(p->pinned_regs, old_reg);
                    p->local_to_reg.data[*old_value] = new_reg;
                    AU_BA_SET_BIT(p->pinned_regs, new_reg);
                }
                parser_emit_bc_u16(p, *old_value);
            } else {
                p->num_locals++;
                EXPECT_BYTECODE(p->num_locals <= AU_MAX_LOCALS);
                parser_emit_bc_u16(p, new_value);
            }
            return 1;
        }
    }
    return parser_exec_logical(p, l);
}

static int parser_exec_logical(struct au_parser *p, struct au_lexer *l) {
    if (!parser_exec_eq(p, l))
        return 0;

    const size_t len = l->pos;
    const struct au_token t = au_lexer_next(l);
    if (t.type == AU_TOK_OPERATOR && t.len == 2) {
        if (t.src[0] == '&' && t.src[1] == '&') {
            parser_flush_cached_regs(p);

            uint8_t reg;
            EXPECT_BYTECODE(parser_new_reg(p, &reg));
            parser_swap_top_regs(p);

            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);

            const size_t left_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_JNIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            if (!parser_exec_expr(p, l))
                return 0;

            parser_flush_cached_regs(p);

            const size_t right_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_JNIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 1);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);

            const size_t end_label = p->bc.len;
            parser_replace_bc_u16(p, left_replace_idx,
                                  (end_label - left_len) / 4);
            parser_replace_bc_u16(p, right_replace_idx,
                                  (end_label - right_len) / 4);

            // The resulting bytecode should look like this:
            //      register = 0
            //   left:
            //      (eval left)
            //      (flush locals)
            //      jnif end
            //   right:
            //      (eval right)
            //      (flush locals)
            //      jnif end
            //   body:
            //       register = 1
            //   end:
            //       ...
        } else if (t.src[0] == '|' && t.src[1] == '|') {
            parser_flush_cached_regs(p);

            uint8_t reg;
            EXPECT_BYTECODE(parser_new_reg(p, &reg));
            parser_swap_top_regs(p);

            const size_t left_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_JIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            if (!parser_exec_expr(p, l))
                return 0;
            parser_flush_cached_regs(p);

            const size_t right_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_JIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            const size_t false_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_JREL);
            parser_emit_pad8(p);
            const size_t false_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            const size_t truth_len = p->bc.len;
            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 1);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);

            const size_t end_label = p->bc.len;
            parser_replace_bc_u16(p, false_replace_idx,
                                  (end_label - false_len) / 4);
            parser_replace_bc_u16(p, left_replace_idx,
                                  (truth_len - left_len) / 4);
            parser_replace_bc_u16(p, right_replace_idx,
                                  (truth_len - right_len) / 4);

            // The resulting bytecode should look like this:
            //       (flush locals)
            //   left:
            //       (eval left)
            //       (flush locals)
            //       jif end
            //   right:
            //       (eval right)
            //       (flush locals)
            //       jif end
            //   body:
            //       register = 0
            //       jmp end1
            //   end:
            //       register = 1
            //   end1:
            //       ...
        }
    } else {
        l->pos = len;
    }
    return 1;
}

#define BIN_EXPR(FN_NAME, BIN_COND, BIN_EXEC, FN_LOWER)                   \
    static int FN_NAME(struct au_parser *p, struct au_lexer *l) {         \
        if (!FN_LOWER(p, l))                                              \
            return 0;                                                     \
        while (1) {                                                       \
            const size_t len = l->pos;                                    \
            const struct au_token t = au_lexer_next(l);                   \
            if (t.type == AU_TOK_EOF) {                                   \
                l->pos = len;                                             \
                return 1;                                                 \
            } else if (t.type == AU_TOK_OPERATOR && (BIN_COND)) {         \
                if (!FN_LOWER(p, l))                                      \
                    return 0;                                             \
                do {                                                      \
                    BIN_EXEC                                              \
                } while (0);                                              \
                continue;                                                 \
            } else {                                                      \
                l->pos = len;                                             \
                return 1;                                                 \
            }                                                             \
            l->pos = len;                                                 \
        }                                                                 \
    }

static int parser_emit_bc_binary_expr(struct au_parser *p) {
    uint8_t rhs = parser_pop_reg(p);
    uint8_t lhs = parser_pop_reg(p);
    uint8_t res;
    EXPECT_BYTECODE(parser_new_reg(p, &res));

    parser_emit_bc_u8(p, lhs);
    parser_emit_bc_u8(p, rhs);
    parser_emit_bc_u8(p, res);
    return 1;
}

BIN_EXPR(
    parser_exec_eq,
    t.len == 2 && t.src[1] == '=' && (t.src[0] == '=' || t.src[0] == '!'),
    {
        if (t.src[0] == '=')
            parser_emit_bc_u8(p, AU_OP_EQ);
        else if (t.src[0] == '!')
            parser_emit_bc_u8(p, AU_OP_NEQ);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_cmp)

BIN_EXPR(
    parser_exec_cmp,
    ((t.src[0] == '<' || t.src[0] == '>') &&
     (t.len == 1 || (t.len == 2 && t.src[1] == '='))),
    {
        if (t.len == 1)
            if (t.src[0] == '<')
                parser_emit_bc_u8(p, AU_OP_LT);
            else
                parser_emit_bc_u8(p, AU_OP_GT);
        else if (t.src[0] == '<')
            parser_emit_bc_u8(p, AU_OP_LEQ);
        else
            parser_emit_bc_u8(p, AU_OP_GEQ);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_addsub)

BIN_EXPR(
    parser_exec_addsub, t.len == 1 && (t.src[0] == '+' || t.src[0] == '-'),
    {
        if (t.src[0] == '+')
            parser_emit_bc_u8(p, AU_OP_ADD);
        else if (t.src[0] == '-')
            parser_emit_bc_u8(p, AU_OP_SUB);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_muldiv)

BIN_EXPR(
    parser_exec_muldiv,
    t.len == 1 && (t.src[0] == '*' || t.src[0] == '/' || t.src[0] == '%'),
    {
        if (t.src[0] == '*')
            parser_emit_bc_u8(p, AU_OP_MUL);
        else if (t.src[0] == '/')
            parser_emit_bc_u8(p, AU_OP_DIV);
        else if (t.src[0] == '%')
            parser_emit_bc_u8(p, AU_OP_MOD);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_bitwise_logic)

BIN_EXPR(
    parser_exec_bitwise_logic,
    t.len == 1 && (t.src[0] == '&' || t.src[0] == '|' || t.src[0] == '^'),
    {
        if (t.src[0] == '&')
            parser_emit_bc_u8(p, AU_OP_BAND);
        else if (t.src[0] == '|')
            parser_emit_bc_u8(p, AU_OP_BOR);
        else if (t.src[0] == '^')
            parser_emit_bc_u8(p, AU_OP_BXOR);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_bitwise_shift)

BIN_EXPR(
    parser_exec_bitwise_shift,
    t.len == 2 && (t.src[0] == '<' || t.src[0] == '>') &&
        t.src[1] == t.src[0],
    {
        if (t.src[0] == '<')
            parser_emit_bc_u8(p, AU_OP_BSHL);
        else if (t.src[0] == '>')
            parser_emit_bc_u8(p, AU_OP_BSHR);
        if (!parser_emit_bc_binary_expr(p))
            return 0;
    },
    parser_exec_unary_expr)

#undef BIN_EXPR

static int parser_exec_unary_expr(struct au_parser *p,
                                  struct au_lexer *l) {
    struct au_token tok = au_lexer_peek(l, 0);
#define UNARY_EXPR_BODY(OPCODE)                                           \
    do {                                                                  \
        au_lexer_next(l);                                                 \
        if (!parser_exec_expr(p, l))                                      \
            return 0;                                                     \
                                                                          \
        const uint8_t reg = parser_pop_reg(p);                            \
        parser_emit_bc_u8(p, OPCODE);                                     \
        parser_emit_bc_u8(p, reg);                                        \
        uint8_t result_reg;                                               \
        EXPECT_BYTECODE(parser_new_reg(p, &result_reg));                  \
        parser_emit_bc_u8(p, result_reg);                                 \
        parser_emit_pad8(p);                                              \
                                                                          \
        return 1;                                                         \
    } while (0)

    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == '!')
        UNARY_EXPR_BODY(AU_OP_NOT);
    else if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
             tok.src[0] == '~')
        UNARY_EXPR_BODY(AU_OP_BNOT);
    else
        return parser_exec_index_expr(p, l);

#undef UNARY_EXPR_BODY
}

static int parser_exec_index_expr(struct au_parser *p,
                                  struct au_lexer *l) {
    if (!parser_exec_value(p, l))
        return 0;
    uint8_t left_reg = 0;
    while (1) {
        left_reg = parser_last_reg(p);
        struct au_token tok = au_lexer_peek(l, 0);
        if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
            tok.src[0] == '[') {
            au_lexer_next(l);
            if (!parser_exec_expr(p, l))
                return 0;
            const uint8_t idx_reg = parser_last_reg(p);
            tok = au_lexer_next(l);
            EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                             tok.src[0] == ']',
                         tok, "']'");
            tok = au_lexer_peek(l, 0);
            if (is_assign_tok(tok)) {
                au_lexer_next(l);
                if (!parser_exec_expr(p, l))
                    return 0;
                const uint8_t right_reg = parser_last_reg(p);

                if (!(tok.len == 1 && tok.src[0] == '=')) {
                    uint8_t result_reg;
                    EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
                    parser_emit_bc_u8(p, AU_OP_IDX_GET);
                    parser_emit_bc_u8(p, left_reg);
                    parser_emit_bc_u8(p, idx_reg);
                    parser_emit_bc_u8(p, result_reg);

                    const struct au_token op = tok;
                    if (op.src[0] == '*')
                        parser_emit_bc_u8(p, AU_OP_MUL);
                    else if (op.src[0] == '/')
                        parser_emit_bc_u8(p, AU_OP_DIV);
                    else if (op.src[0] == '+')
                        parser_emit_bc_u8(p, AU_OP_ADD);
                    else if (op.src[0] == '-')
                        parser_emit_bc_u8(p, AU_OP_SUB);
                    else if (op.src[0] == '%')
                        parser_emit_bc_u8(p, AU_OP_MOD);
                    else
                        au_fatal("unimplemented op '%.*s'\n", (int)op.len,
                                 op.src);
                    parser_emit_bc_u8(p, result_reg);
                    parser_emit_bc_u8(p, right_reg);
                    parser_emit_bc_u8(p, result_reg);

                    parser_emit_bc_u8(p, AU_OP_IDX_SET);
                    parser_emit_bc_u8(p, left_reg);
                    parser_emit_bc_u8(p, idx_reg);
                    parser_emit_bc_u8(p, result_reg);

                    // The register stack is:
                    // ... [right reg (-2)] [result reg (-1)]
                    // This operation transforms the stack to this:
                    // ... [result reg (-1)]
                    parser_swap_top_regs(p);
                    parser_pop_reg(p);
                } else {
                    parser_emit_bc_u8(p, AU_OP_IDX_SET);
                    parser_emit_bc_u8(p, left_reg);
                    parser_emit_bc_u8(p, idx_reg);
                    parser_emit_bc_u8(p, right_reg);
                }
                // Right now, the used register stack is:
                // ... [array reg (-3)] [idx reg (-2)] [right reg (-1)]
                // Remove array and idx regs because they aren't used,
                // leaving us with:
                // ... [right reg(-1)]
                AU_BA_RESET_BIT(p->used_regs,
                                p->rstack[p->rstack_len - 3]);
                AU_BA_RESET_BIT(p->used_regs,
                                p->rstack[p->rstack_len - 2]);
                p->rstack[p->rstack_len - 3] =
                    p->rstack[p->rstack_len - 1];
                p->rstack_len -= 2;
                break;
            } else {
                uint8_t result_reg;
                EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
                parser_emit_bc_u8(p, AU_OP_IDX_GET);
                parser_emit_bc_u8(p, left_reg);
                parser_emit_bc_u8(p, idx_reg);
                parser_emit_bc_u8(p, result_reg);
                // ... [array reg (-3)] [idx reg (-2)] [value reg (-1)]
                // We also want to remove array/idx regs here
                AU_BA_RESET_BIT(p->used_regs,
                                p->rstack[p->rstack_len - 3]);
                AU_BA_RESET_BIT(p->used_regs,
                                p->rstack[p->rstack_len - 2]);
                p->rstack[p->rstack_len - 3] =
                    p->rstack[p->rstack_len - 1];
                p->rstack_len -= 2;
            }
        } else if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                   tok.src[0] == '.') {
            au_lexer_next(l);

            struct au_token id_tok = au_lexer_next(l);
            if (id_tok.type == AU_TOK_OPERATOR && id_tok.len == 1 &&
                id_tok.src[0] == '(') {
                struct reg_array params = {0};
                if (!parser_exec_call_args(p, l, &params)) {
                    au_data_free(params.data);
                    return 0;
                }
                EXPECT_BYTECODE(params.len < 256);

                parser_emit_bc_u8(p, AU_OP_CALL_FUNC_VALUE);
                parser_emit_bc_u8(p, parser_pop_reg(p));
                parser_emit_bc_u8(p, params.len);
                uint8_t return_reg;
                EXPECT_BYTECODE(parser_new_reg(p, &return_reg));
                parser_emit_bc_u8(p, return_reg);

                for (size_t i = 0; i < params.len;) {
                    uint8_t reg = 0;

                    parser_emit_bc_u8(p, AU_OP_PUSH_ARG);

                    reg = i < params.len ? params.data[i] : 0;
                    i++;
                    parser_emit_bc_u8(p, reg);

                    reg = i < params.len ? params.data[i] : 0;
                    i++;
                    parser_emit_bc_u8(p, reg);

                    reg = i < params.len ? params.data[i] : 0;
                    i++;
                    parser_emit_bc_u8(p, reg);
                }

                for (size_t i = 0; i < params.len; i++) {
                    AU_BA_RESET_BIT(p->used_regs, i);
                }

                au_data_free(params.data);
            } else {
                EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok,
                             "identifier or arguments");

                struct au_token peek = au_lexer_peek(l, 0);
                struct au_token module_tok =
                    (struct au_token){.type = AU_TOK_EOF};

                if (peek.type == AU_TOK_OPERATOR && peek.len == 2 &&
                    peek.src[0] == ':' && peek.src[1] == ':') {
                    module_tok = id_tok;
                    au_lexer_next(l);
                    id_tok = au_lexer_next(l);
                    EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok,
                                 "identifier");
                    peek = au_lexer_peek(l, 0);
                }

                if (peek.type == AU_TOK_OPERATOR && peek.len == 1 &&
                    peek.src[0] == '(') {
                    au_lexer_next(l);
                    if (!parser_exec_call(p, l, module_tok, id_tok, 1))
                        return 0;
                } else {
                    size_t func_idx = 0;
                    int execute_self = 0;

                    if (!parser_resolve_fn(p, module_tok, id_tok, 1,
                                           &func_idx, &execute_self))
                        return 0;

                    EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

                    parser_emit_bc_u8(p, AU_OP_LOAD_FUNC);
                    uint8_t func_reg;
                    EXPECT_BYTECODE(parser_new_reg(p, &func_reg));
                    parser_emit_bc_u8(p, func_reg);
                    parser_emit_bc_u16(p, func_idx);

                    parser_emit_bc_u8(p, AU_OP_BIND_ARG_TO_FUNC);
                    parser_emit_bc_u8(p, func_reg);
                    parser_emit_bc_u8(p, left_reg);
                    parser_emit_pad8(p);
                }
            }
        } else {
            break;
        }
    }

    return 1;
}

static int parser_resolve_fn(struct au_parser *p,
                             struct au_token module_tok,
                             struct au_token id_tok, int num_args_in,
                             size_t *func_idx_out, int *execute_self_out) {
    size_t func_idx = 0;
    int func_idx_found = 0;
    int execute_self = 0;
    if (module_tok.type != AU_TOK_EOF) {
        const au_hm_var_value_t *module_val =
            au_hm_vars_get(&p->p_data->imported_module_map, module_tok.src,
                           module_tok.len);
        if (module_val == 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_MODULE,
                .data.unknown_id.name_token = module_tok,
            };
            return 0;
        }
        const uint32_t module_idx = *module_val;
        struct au_imported_module *module =
            &p->p_data->imported_modules.data[module_idx];
        const au_hm_var_value_t *val =
            au_hm_vars_get(&module->fn_map, id_tok.src, id_tok.len);
        if (val == 0) {
            au_hm_var_value_t value = p->p_data->fns.len;
            char *import_name = au_data_malloc(id_tok.len);
            memcpy(import_name, id_tok.src, id_tok.len);
            struct au_fn fn = (struct au_fn){
                .type = AU_FN_IMPORTER,
                .flags = 0,
                .as.imported_func.num_args = num_args_in,
                .as.imported_func.module_idx = module_idx,
                .as.imported_func.name = import_name,
                .as.imported_func.name_len = id_tok.len,
                .as.imported_func.fn_idx_cached = 0,
                .as.imported_func.p_data_cached = 0,
            };
            au_fn_array_add(&p->p_data->fns, fn);
            func_idx = value;
            au_hm_var_value_t *old = au_hm_vars_add(
                &module->fn_map, id_tok.src, id_tok.len, value);
            EXPECT_BYTECODE(old == 0);
            val = au_hm_vars_get(&module->fn_map, id_tok.src, id_tok.len);
        } else {
            func_idx = *val;
        }
        func_idx_found = 1;
    } else if (p->self_name && id_tok.len == p->self_len &&
               memcmp(p->self_name, id_tok.src, p->self_len) == 0) {
        execute_self = 1;
        func_idx_found = 1;
    } else {
        const au_hm_var_value_t *val =
            au_hm_vars_get(&p->p_data->fn_map, id_tok.src, id_tok.len);
        if (val) {
            func_idx = *val;
            func_idx_found = 1;
        }
    }

    if (!func_idx_found) {
        au_hm_var_value_t func_value = p->p_data->fns.len;
        au_hm_vars_add(&p->p_data->fn_map, id_tok.src, id_tok.len,
                       func_value);
        struct au_fn none_func = (struct au_fn){
            .type = AU_FN_NONE,
            .as.none_func.num_args = num_args_in,
            .as.none_func.name_token = id_tok,
        };
        au_fn_array_add(&p->p_data->fns, none_func);
        au_str_array_add(&p->p_data->fn_names,
                         au_data_strndup(id_tok.src, id_tok.len));
        func_idx = func_value;
    }

    *func_idx_out = func_idx;
    *execute_self_out = execute_self;
    return 1;
}

static int parser_exec_call(struct au_parser *p, struct au_lexer *l,
                            struct au_token module_tok,
                            struct au_token id_tok,
                            int has_self_argument) {
    struct reg_array params = {0};

    if (has_self_argument)
        reg_array_add(&params, parser_pop_reg(p));

    if (!parser_exec_call_args(p, l, &params))
        goto fail;

    size_t func_idx = 0;
    int execute_self = 0;

    if (!parser_resolve_fn(p, module_tok, id_tok, params.len, &func_idx,
                           &execute_self))
        goto fail;

    EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

    if (execute_self) {
        if (p->self_num_args != (int)params.len) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_WRONG_ARGS,
                .data.wrong_args.got_args = params.len,
                .data.wrong_args.expected_args = p->self_num_args,
                .data.wrong_args.at_token = id_tok,
            };
            goto fail;
        }
    } else {
        const struct au_fn *fn = &p->p_data->fns.data[func_idx];
        int expected_num_args = au_fn_num_args(fn);
        if (expected_num_args != (int)params.len) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_WRONG_ARGS,
                .data.wrong_args.got_args = params.len,
                .data.wrong_args.expected_args = expected_num_args,
                .data.wrong_args.at_token = id_tok,
            };
            goto fail;
        }
    }

    size_t call_fn_offset = 0;

    if (params.len == 1) {
        // OPTIMIZE: optimization for function calls with 1 argument
        parser_emit_bc_u8(p, AU_OP_CALL1);
        parser_emit_bc_u8(p, params.data[0]);
        call_fn_offset = p->bc.len;
        parser_emit_pad8(p);
        parser_emit_pad8(p);

        parser_push_reg(p, params.data[0]);
        parser_invalidate_reg(p, params.data[0]);
    } else {
        uint8_t result_reg;
        EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
        parser_emit_bc_u8(p, AU_OP_CALL);
        parser_emit_bc_u8(p, result_reg);
        call_fn_offset = p->bc.len;
        parser_emit_pad8(p);
        parser_emit_pad8(p);

        for (size_t i = 0; i < params.len;) {
            uint8_t reg = 0;

            parser_emit_bc_u8(p, AU_OP_PUSH_ARG);

            reg = i < params.len ? params.data[i] : 0;
            i++;
            parser_emit_bc_u8(p, reg);

            reg = i < params.len ? params.data[i] : 0;
            i++;
            parser_emit_bc_u8(p, reg);

            reg = i < params.len ? params.data[i] : 0;
            i++;
            parser_emit_bc_u8(p, reg);
        }

        for (size_t i = 0; i < params.len; i++) {
            AU_BA_RESET_BIT(p->used_regs, i);
        }
    }

    if (execute_self) {
        size_t_array_add(&p->self_fill_call, call_fn_offset);
    } else {
        parser_replace_bc_u16(p, call_fn_offset, func_idx);
    }

    au_data_free(params.data);
    return 1;

fail:
    au_data_free(params.data);
    return 0;
}

static int hex_value(char ch) {
    if ('0' <= ch && ch <= '9')
        return ch - '0';
    if ('a' <= ch && ch <= 'f')
        return ch - 'a' + 10;
    if ('A' <= ch && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static int parser_exec_value(struct au_parser *p, struct au_lexer *l) {
    struct au_token t = au_lexer_next(l);

    switch (t.type) {
    case AU_TOK_INT: {
        int32_t num = 0;
        if (t.len >= 2 && t.src[0] == '0' && t.src[1] == 'x') {
            for (size_t i = 2; i < t.len; i++) {
                num = num * 16 + hex_value(t.src[i]);
            }
        } else {
            for (size_t i = 0; i < t.len; i++) {
                num = num * 10 + (t.src[i] - '0');
            }
        }

        uint8_t result_reg;
        EXPECT_BYTECODE(parser_new_reg(p, &result_reg));

        if (num <= 0x8000) {
            parser_emit_bc_u8(p, AU_OP_MOV_U16);
            parser_emit_bc_u8(p, result_reg);
            parser_emit_bc_u16(p, num);
        } else {
            int idx = au_program_data_add_data(p->p_data,
                                               au_value_int(num), 0, 0);
            parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
            parser_emit_bc_u8(p, result_reg);
            parser_emit_bc_u16(p, idx);
        }
        break;
    }
    case AU_TOK_DOUBLE: {
        // We've already parsed the token as a floating-point literal in
        // the lexer so calling strtod_l should not cause a buffer overflow
        locale_t locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
        double value = strtod_l(t.src, 0, locale);

        uint8_t result_reg;
        EXPECT_BYTECODE(parser_new_reg(p, &result_reg));

        int idx = au_program_data_add_data(p->p_data,
                                           au_value_double(value), 0, 0);
        parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
        parser_emit_bc_u8(p, result_reg);
        parser_emit_bc_u16(p, idx);
        break;
    }
    case AU_TOK_OPERATOR: {
        if (t.len == 1 && t.src[0] == '(') {
            t = au_lexer_peek(l, 0);
            if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                t.src[0] == '-') {
                au_lexer_next(l);
                if (!parser_exec_expr(p, l))
                    return 0;

                const uint8_t reg = parser_pop_reg(p);
                parser_emit_bc_u8(p, AU_OP_NEG);
                parser_emit_bc_u8(p, reg);
                uint8_t result_reg;
                EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
                parser_emit_bc_u8(p, result_reg);
                parser_emit_pad8(p);
            } else {
                if (!parser_exec_expr(p, l))
                    return 0;
            }
            t = au_lexer_next(l);
            EXPECT_TOKEN(t.len == 1 && t.src[0] == ')', t, "')'");
        } else if (t.len == 1 && t.src[0] == '[') {
            return parser_exec_array_or_tuple(p, l, 0);
        } else if (t.len == 2 && t.src[0] == '#' && t.src[1] == '[') {
            return parser_exec_array_or_tuple(p, l, 1);
        } else if (t.len == 1 && t.src[0] == '.') {
            t = au_lexer_next(l);
            EXPECT_TOKEN(t.type == AU_TOK_IDENTIFIER, t, "identifier");

            struct au_token peek = au_lexer_peek(l, 0);
            struct au_token module_tok =
                (struct au_token){.type = AU_TOK_EOF};

            if (peek.type == AU_TOK_OPERATOR && peek.len == 2 &&
                peek.src[0] == ':' && peek.src[1] == ':') {
                module_tok = t;
                au_lexer_next(l);
                t = au_lexer_next(l);
                EXPECT_TOKEN(t.type == AU_TOK_IDENTIFIER, t, "identifier");
            }

            size_t func_idx = 0;
            int execute_self = 0;

            if (!parser_resolve_fn(p, module_tok, t, 1, &func_idx,
                                   &execute_self))
                return 0;

            EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

            parser_emit_bc_u8(p, AU_OP_LOAD_FUNC);
            uint8_t func_reg;
            EXPECT_BYTECODE(parser_new_reg(p, &func_reg));
            parser_emit_bc_u8(p, func_reg);
            parser_emit_bc_u16(p, func_idx);
        } else {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
                .data.unexpected_token.got_token = t,
                .data.unexpected_token.expected = 0,
            };
            return 0;
        }
        break;
    }
    case AU_TOK_IDENTIFIER: {
        if (token_keyword_cmp(&t, "true")) {
            uint8_t reg;
            EXPECT_BYTECODE(parser_new_reg(p, &reg));
            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 1);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "false")) {
            uint8_t reg;
            EXPECT_BYTECODE(parser_new_reg(p, &reg));
            parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "nil")) {
            uint8_t reg;
            EXPECT_BYTECODE(parser_new_reg(p, &reg));
            parser_emit_bc_u8(p, AU_OP_LOAD_NIL);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            return 1;
        }

        struct au_token peek = au_lexer_peek(l, 0);

        if (token_keyword_cmp(&t, "new")) {
            if (peek.type == AU_TOK_OPERATOR && peek.len == 1 &&
                peek.src[0] == '(') {
                // This is treated as a call to the "new" function
            } else {
                return parser_exec_new_expr(p, l);
            }
        }

        struct au_token module_tok = (struct au_token){.type = AU_TOK_EOF};
        if (peek.type == AU_TOK_OPERATOR && peek.len == 2 &&
            peek.src[0] == ':' && peek.src[1] == ':') {
            module_tok = t;
            au_lexer_next(l);
            t = au_lexer_next(l);
            EXPECT_TOKEN(t.type == AU_TOK_IDENTIFIER, t, "identifier");
            peek = au_lexer_peek(l, 0);
        }

        if (peek.type == AU_TOK_OPERATOR && peek.len == 1 &&
            peek.src[0] == '(') {
            au_lexer_next(l);
            if (!parser_exec_call(p, l, module_tok, t, 0))
                return 0;
        } else {
            assert(module_tok.type == AU_TOK_EOF);
            const au_hm_var_value_t *val =
                au_hm_vars_get(&p->vars, t.src, t.len);
            if (val == NULL) {
                const au_hm_var_value_t *const_val = 0;

                const_val = au_hm_vars_get(&p->consts, t.src, t.len);
                if (const_val == NULL && p->top_level != 0) {
                    const_val = au_hm_vars_get(&p->top_level->consts,
                                               t.src, t.len);
                }

                if (const_val == NULL) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_UNKNOWN_VAR,
                        .data.unknown_id.name_token = t,
                    };
                    return 0;
                } else {
                    // TODO: constants in local functions
                    uint8_t reg;
                    EXPECT_BYTECODE(parser_new_reg(p, &reg));
                    parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
                    parser_emit_bc_u8(p, reg);
                    parser_emit_bc_u16(p, *const_val);
                }
            } else {
                const uint16_t local = *val;
                uint8_t cached_reg;
                if (local < p->local_to_reg.len) {
                    cached_reg = p->local_to_reg.data[local];
                } else {
                    for (size_t i = p->local_to_reg.len;
                         i < (size_t)local + 1; i++) {
                        reg_array_add(&p->local_to_reg, CACHED_REG_NONE);
                    }
                    cached_reg = CACHED_REG_NONE;
                }
                if (cached_reg == CACHED_REG_NONE) {
                    uint8_t reg;
                    EXPECT_BYTECODE(parser_new_reg(p, &reg));
                    parser_emit_bc_u8(p, AU_OP_MOV_LOCAL_REG);
                    parser_emit_bc_u8(p, reg);
                    parser_emit_bc_u16(p, local);
                    reg_array_set(&p->local_to_reg, local, reg);
                    AU_BA_SET_BIT(p->pinned_regs, reg);
                } else {
                    parser_push_reg(p, cached_reg);
                    AU_BA_SET_BIT(p->pinned_regs, cached_reg);
                }
            }
        }
        break;
    }
    case AU_TOK_STRING:
    case AU_TOK_CHAR_STRING: {
        const int is_char_string = t.type == AU_TOK_CHAR_STRING;

        // Perform string escape
        char *formatted_string = 0;
        size_t formatted_string_len = 0;
        int in_escape = 0;
        for (size_t i = 0; i < t.len; i++) {
            if (t.src[i] == '\\' && !in_escape) {
                in_escape = 1;
                continue;
            }
            if (in_escape) {
                if (formatted_string == 0) {
                    formatted_string = au_data_malloc(t.len);
                    formatted_string_len = i - 1;
                    memcpy(formatted_string, t.src, i - 1);
                }
                switch (t.src[i]) {
                case 'n': {
                    formatted_string[formatted_string_len++] = '\n';
                    break;
                }
                }
                in_escape = 0;
            } else if (formatted_string != 0) {
                formatted_string[formatted_string_len++] = t.src[i];
            }
        }

        int idx = -1;
        if (is_char_string) {
            int32_t codepoint = 0;
            if (utf8_codepoint(t.src, t.len, &codepoint) == 0)
                abort(); // TODO

            idx = au_program_data_add_data(p->p_data,
                                           au_value_int(codepoint), 0, 0);

            if (formatted_string)
                au_data_free(formatted_string);
        } else {
            if (formatted_string) {
                idx = au_program_data_add_data(
                    p->p_data, au_value_string(0),
                    (uint8_t *)formatted_string, formatted_string_len);
                au_data_free(formatted_string);
            } else {
                idx =
                    au_program_data_add_data(p->p_data, au_value_string(0),
                                             (uint8_t *)t.src, t.len);
            }
        }

        uint8_t reg;
        EXPECT_BYTECODE(parser_new_reg(p, &reg));

        parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
        parser_emit_bc_u8(p, reg);
        parser_emit_bc_u16(p, idx);

        break;
    }
    case AU_TOK_AT_IDENTIFIER: {
        const struct au_class_interface *interface = p->class_interface;
        if (interface == 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_CLASS_SCOPE_ONLY,
                .data.class_scope.at_token = t,
            };
            return 0;
        }
        uint8_t reg;
        EXPECT_BYTECODE(parser_new_reg(p, &reg));
        parser_emit_bc_u8(p, AU_OP_CLASS_GET_INNER);
        parser_emit_bc_u8(p, reg);
        const au_hm_var_value_t *value =
            au_hm_vars_get(&interface->map, &t.src[1], t.len - 1);
        if (value == 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_VAR,
                .data.unknown_id.name_token = t,
            };
            return 0;
        }
        parser_emit_bc_u16(p, *value);
        break;
    }
    default: {
        EXPECT_TOKEN(0, t, "value");
    }
    }

    return 1;
}

static int parser_exec_array_or_tuple(struct au_parser *p,
                                      struct au_lexer *l, int is_tuple) {
    uint8_t array_reg;
    EXPECT_BYTECODE(parser_new_reg(p, &array_reg));
    if (is_tuple) {
        parser_emit_bc_u8(p, AU_OP_TUPLE_NEW);
        parser_emit_bc_u8(p, array_reg);
    } else {
        parser_emit_bc_u8(p, AU_OP_ARRAY_NEW);
        parser_emit_bc_u8(p, array_reg);
    }
    const size_t cap_offset = p->bc.len;
    parser_emit_bc_u16(p, 0);

    struct au_token tok = au_lexer_peek(l, 0);
    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == ']') {
        au_lexer_next(l);
        return 1;
    }

    uint16_t capacity = 1;
    if (!parser_exec_expr(p, l))
        return 0;
    const uint8_t value_reg = parser_pop_reg(p);
    if (is_tuple) {
        parser_emit_bc_u8(p, AU_OP_IDX_SET_STATIC);
        parser_emit_bc_u8(p, array_reg);
        parser_emit_bc_u8(p, 0);
        parser_emit_bc_u8(p, value_reg);
    } else {
        parser_emit_bc_u8(p, AU_OP_ARRAY_PUSH);
        parser_emit_bc_u8(p, array_reg);
        parser_emit_bc_u8(p, value_reg);
        parser_emit_pad8(p);
    }

    while (1) {
        tok = au_lexer_peek(l, 0);
        if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
            tok.src[0] == ']') {
            au_lexer_next(l);
            break;
        } else if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                   tok.src[0] == ',') {
            au_lexer_next(l);

            tok = au_lexer_peek(l, 0);
            if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                tok.src[0] == ']')
                break;

            if (!parser_exec_expr(p, l))
                return 0;
            const uint8_t value_reg = parser_pop_reg(p);

            if (is_tuple) {
                parser_emit_bc_u8(p, AU_OP_IDX_SET_STATIC);
                parser_emit_bc_u8(p, array_reg);
                parser_emit_bc_u8(p, capacity);
                parser_emit_bc_u8(p, value_reg);
                capacity++;
                EXPECT_BYTECODE(capacity < AU_MAX_STATIC_IDX);
            } else {
                parser_emit_bc_u8(p, AU_OP_ARRAY_PUSH);
                parser_emit_bc_u8(p, array_reg);
                parser_emit_bc_u8(p, value_reg);
                parser_emit_pad8(p);
                if (capacity < (AU_MAX_ARRAY - 1)) {
                    capacity++;
                }
            }
        } else {
            EXPECT_TOKEN(0, tok, "',' or ']'");
        }
    }

    parser_replace_bc_u16(p, cap_offset, capacity);
    return 1;
}

struct new_initializer {
    uint16_t local;
    uint8_t reg;
};

AU_ARRAY_COPY(struct new_initializer, new_initializer_array, 1)

static int parser_exec_new_expr(struct au_parser *p, struct au_lexer *l) {
    const struct au_token id_tok = au_lexer_next(l);
    EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok, "identifier");

    const au_hm_var_value_t *class_value =
        au_hm_vars_get(&p->p_data->class_map, id_tok.src, id_tok.len);
    if (class_value == 0) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNKNOWN_CLASS,
            .data.unknown_id.name_token = id_tok,
        };
        return 0;
    }

    size_t class_idx = *class_value;

    struct au_token tok = au_lexer_peek(l, 0);
    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == '{') {
        au_lexer_next(l);

        struct new_initializer_array array = {0};

        struct au_class_interface *interface =
            au_class_interface_ptr_array_at(&p->p_data->classes,
                                            class_idx);
        if (interface == 0) {
            abort(); // TODO
        }

        while (1) {
            tok = au_lexer_next(l);
            if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                tok.src[0] == '}') {
                break;
            } else if (tok.type == AU_TOK_IDENTIFIER) {
                struct au_token name_tok = tok;
                const au_hm_var_value_t *key_value = au_hm_vars_get(
                    &interface->map, name_tok.src, name_tok.len);
                if (key_value == 0) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_UNKNOWN_VAR,
                        .data.unknown_id.name_token = name_tok,
                    };
                    return 0;
                }

                tok = au_lexer_next(l);
                EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                                 tok.src[0] == ':',
                             tok, "':'");

                if (!parser_exec_expr(p, l))
                    return 0;
                const uint8_t right_reg = parser_pop_reg(p);
                new_initializer_array_add(&array, (struct new_initializer){
                                                      .local = *key_value,
                                                      .reg = right_reg,
                                                  });
                AU_BA_SET_BIT(p->used_regs, right_reg);

                tok = au_lexer_next(l);
                EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                                 tok.src[0] == ',',
                             tok, "','");
            } else {
                EXPECT_TOKEN(0, tok, "'}' or identifier");
            }
        }

        if (array.len == 0) {
            uint8_t result_reg;
            EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
            parser_emit_bc_u8(p, AU_OP_CLASS_NEW);
            parser_emit_bc_u8(p, result_reg);
            parser_emit_bc_u16(p, class_idx);
        } else {
            uint8_t result_reg;
            EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
            parser_emit_bc_u8(p, AU_OP_CLASS_NEW_INITIALZIED);
            parser_emit_bc_u8(p, result_reg);
            parser_emit_bc_u16(p, class_idx);

            for (size_t i = 0; i < array.len; i++) {
                struct new_initializer init = array.data[i];
                parser_emit_bc_u8(p, AU_OP_CLASS_SET_INNER);
                parser_emit_bc_u8(p, init.reg);
                parser_emit_bc_u16(p, init.local);
                AU_BA_RESET_BIT(p->used_regs, init.reg);
            }
            au_data_free(array.data);

            parser_emit_bc_u8(p, AU_OP_NOP);
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            parser_emit_pad8(p);
        }
    } else {
        uint8_t result_reg;
        EXPECT_BYTECODE(parser_new_reg(p, &result_reg));
        parser_emit_bc_u8(p, AU_OP_CLASS_NEW);
        parser_emit_bc_u8(p, result_reg);
        parser_emit_bc_u16(p, class_idx);
    }

    return 1;
}

struct au_parser_result au_parse(const char *src, size_t len,
                                 struct au_program *program) {
    struct au_program_data p_data;
    au_program_data_init(&p_data);
    au_stdlib_export(&p_data);

    struct au_lexer l;
    au_lexer_init(&l, src, len);

    struct au_parser p;
    parser_init(&p, &p_data);
    if (!parser_exec(&p, &l)) {
        struct au_parser_result res = p.res;
        au_lexer_del(&l);
        parser_del(&p);
        au_program_data_del(&p_data);
        assert(res.type != AU_PARSER_RES_OK);
        return res;
    }

    for (size_t i = 0; i < p_data.fns.len; i++) {
        if (p_data.fns.data[i].type == AU_FN_NONE) {
            struct au_token name_token =
                p_data.fns.data[i].as.none_func.name_token;
            au_lexer_del(&l);
            parser_del(&p);
            au_program_data_del(&p_data);
            return (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_FUNCTION,
                .data.unknown_id.name_token = name_token,
            };
        }
    }

    struct au_bc_storage p_main;
    au_bc_storage_init(&p_main);
    p_main.func_idx = AU_SM_FUNC_ID_MAIN;
    p_main.bc = p.bc;
    p_main.num_locals = p.num_locals;
    p_main.num_registers = p.max_register + 1;
    p_main.num_values = p_main.num_locals + p_main.num_registers;
    p.bc = (struct au_bc_buf){0};

    program->main = p_main;
    program->data = p_data;

    au_lexer_del(&l);
    parser_del(&p);
    // p_data is moved
    return (struct au_parser_result){
        .type = AU_PARSER_RES_OK,
    };
}
