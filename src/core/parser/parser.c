// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/mmap.h"

#include "core/array.h"
#include "core/program.h"
#include "core/rt/au_class.h"
#include "core/rt/exception.h"

#include "lexer.h"
#include "parser.h"

#define CLASS_ID_NONE ((size_t)-1)

ARRAY_TYPE_COPY(size_t, size_t_array, 1)

static char *copy_string(const char *str, size_t len) {
    char *output = malloc(len + 1);
    memcpy(output, str, len);
    output[len] = 0;
    return output;
}

struct parser {
    /// Bytecode buffer that the parser is outputting to
    struct au_bc_buf bc;

    /// Stack of used register
    uint8_t rstack[AU_REGS];
    /// Length of rstack
    size_t rstack_len;

    /// Stack of unused "free" registers
    uint8_t free_regs[AU_REGS];
    // Length of free_regs
    size_t free_regs_len;

    /// Hash table of local variables
    struct au_hm_vars vars;

    /// Global program data. This struct does not own this pointer.
    struct au_program_data *p_data;

    /// Number of local registers
    int locals_len;
    /// Maximum register index used
    int max_register;
    /// Current scope level
    int block_level;

    /// Name of current function or NULL. This struct does not own this
    /// pointer.
    const char *self_name;
    /// Byte size of self_name
    size_t self_len;
    /// Array of offsets representing OP_CALL function index.
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

    /// Result of the parser
    struct au_parser_result res;
};

static inline int is_return_op(uint8_t op) {
    return op == OP_RET_LOCAL || op == OP_RET || op == OP_RET_NULL;
}

static void parser_flush_free_regs(struct parser *p) {
    p->rstack_len = 0;
    for (int i = 0; i < AU_REGS; i++) {
        p->free_regs[i] = AU_REGS - i - 1;
    }
    p->free_regs_len = AU_REGS;
}

static void parser_init(struct parser *p, struct au_program_data *p_data) {
    p->bc = (struct au_bc_buf){0};
    parser_flush_free_regs(p);
    au_hm_vars_init(&p->vars);
    p->p_data = p_data;

    p->locals_len = 0;
    p->max_register = -1;
    p->block_level = 0;

    p->self_name = 0;
    p->self_len = 0;
    p->self_fill_call = (struct size_t_array){0};
    p->self_num_args = 0;

    p->class_interface = 0;
    p->self_keyword = 0;
    p->self_keyword_len = 0;
}

static void parser_del(struct parser *p) {
    free(p->bc.data);
    au_hm_vars_del(&p->vars);
    free(p->self_fill_call.data);
    memset(p, 0, sizeof(struct parser));
}

static uint8_t parser_new_reg(struct parser *p) {
    assert(p->rstack_len + 1 <= AU_REGS);
    assert(p->free_regs_len > 0);
    uint8_t reg = p->free_regs[--p->free_regs_len];
    p->rstack[p->rstack_len++] = reg;
    if (reg > p->max_register)
        p->max_register = reg;
    return reg;
}

static uint8_t parser_last_reg(struct parser *p) {
    assert(p->rstack_len != 0);
    return p->rstack[p->rstack_len - 1];
}

static void parser_swap_top_regs(struct parser *p) {
    assert(p->rstack_len >= 2);
    const uint8_t top2 = p->rstack[p->rstack_len - 2];
    p->rstack[p->rstack_len - 2] = p->rstack[p->rstack_len - 1];
    p->rstack[p->rstack_len - 1] = top2;
}

static uint8_t parser_pop_reg(struct parser *p) {
    assert(p->rstack_len != 0);
    uint8_t reg = p->rstack[--p->rstack_len];
    assert(p->free_regs_len + 1 <= AU_REGS);
    p->free_regs[p->free_regs_len++] = reg;
    return reg;
}

static void parser_emit_bc_u8(struct parser *p, uint8_t val) {
    au_bc_buf_add(&p->bc, val);
}

static void replace_bc_u16(struct au_bc_buf *bc, size_t idx,
                           uint16_t val) {
    assert(idx + 1 < bc->len);
    uint16_t *ptr = (uint16_t *)(&bc->data[idx]);
    ptr[0] = val;
}

static void parser_replace_bc_u16(struct parser *p, size_t idx,
                                  uint16_t val) {
    replace_bc_u16(&p->bc, idx, val);
}

static void parser_emit_bc_u16(struct parser *p, uint16_t val) {
    const size_t offset = p->bc.len;
    parser_emit_bc_u8(p, 0);
    parser_emit_bc_u8(p, 0);
    uint16_t *ptr = (uint16_t *)(&p->bc.data[offset]);
    ptr[0] = val;
}

static void parser_emit_pad8(struct parser *p) { parser_emit_bc_u8(p, 0); }

static int parser_exec_statement(struct parser *p, struct lexer *l);

static int parser_exec_expr(struct parser *p, struct lexer *l);
static int parser_exec_assign(struct parser *p, struct lexer *l);
static int parser_exec_logical(struct parser *p, struct lexer *l);
static int parser_exec_eq(struct parser *p, struct lexer *l);
static int parser_exec_cmp(struct parser *p, struct lexer *l);
static int parser_exec_addsub(struct parser *p, struct lexer *l);
static int parser_exec_muldiv(struct parser *p, struct lexer *l);
static int parser_exec_unary_expr(struct parser *p, struct lexer *l);
static int parser_exec_index_expr(struct parser *p, struct lexer *l);
static int parser_exec_val(struct parser *p, struct lexer *l);
static int parser_exec_array_or_tuple(struct parser *p, struct lexer *l,
                                      int is_tuple);
static int parser_exec_new_expr(struct parser *p, struct lexer *l);

static int parser_exec(struct parser *p, struct lexer *l) {
    while (1) {
        int retval = parser_exec_statement(p, l);
        if (retval == 0)
            return 0;
        else if (retval == -1)
            break;
        parser_flush_free_regs(p);
    }
    parser_emit_bc_u8(p, OP_RET_NULL);
    return 1;
}

static int parser_exec_export_statement(struct parser *p, struct lexer *l);
static int parser_exec_import_statement(struct parser *p, struct lexer *l);
static int parser_exec_class_statement(struct parser *p, struct lexer *l,
                                       int exported);
static int parser_exec_def_statement(struct parser *p, struct lexer *l,
                                     int exported);
static int parser_exec_while_statement(struct parser *p, struct lexer *l);
static int parser_exec_if_statement(struct parser *p, struct lexer *l);
static int parser_exec_print_statement(struct parser *p, struct lexer *l);
static int parser_exec_return_statement(struct parser *p, struct lexer *l);

static int parser_exec_with_semicolon(struct parser *p, struct lexer *l,
                                      int retval) {
    if (!retval)
        return retval;
    struct token t = lexer_next(l);
    if (t.type == TOK_EOF)
        return 1;
    if (!(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ';')) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = t,
        };
        return 0;
    }
    return 1;
}

// Block statements
static int parser_exec_block(struct parser *p, struct lexer *l) {
    p->block_level++;

    {
        struct token t = lexer_next(l);
        assert(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == '{');
    }

    while (1) {
        {
            struct token t = lexer_peek(l, 0);
            if (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == '}') {
                lexer_next(l);
                break;
            }
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

static int parser_exec_statement(struct parser *p, struct lexer *l) {
#define WITH_SEMICOLON(FUNC) parser_exec_with_semicolon(p, l, FUNC(p, l))
    const struct token t = lexer_peek(l, 0);
    const size_t bc_from = p->bc.len;
    int retval = 0;
    if (t.type == TOK_EOF) {
        return -1;
    } else if (t.type == TOK_IDENTIFIER) {
        if (token_keyword_cmp(&t, "class")) {
            lexer_next(l);
            retval = parser_exec_class_statement(p, l, 0);
        } else if (token_keyword_cmp(&t, "def")) {
            lexer_next(l);
            retval = parser_exec_def_statement(p, l, 0);
        } else if (token_keyword_cmp(&t, "if")) {
            lexer_next(l);
            retval = parser_exec_if_statement(p, l);
        } else if (token_keyword_cmp(&t, "while")) {
            lexer_next(l);
            retval = parser_exec_while_statement(p, l);
        } else if (token_keyword_cmp(&t, "print")) {
            lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_print_statement);
        } else if (token_keyword_cmp(&t, "return")) {
            lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_return_statement);
        } else if (token_keyword_cmp(&t, "import")) {
            lexer_next(l);
            retval = WITH_SEMICOLON(parser_exec_import_statement);
        } else if (token_keyword_cmp(&t, "export")) {
            lexer_next(l);
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
        struct au_program_source_map map = (struct au_program_source_map){
            .bc_from = bc_from,
            .bc_to = bc_to,
            .source_start = source_start,
        };
        au_program_source_map_array_add(&p->p_data->source_map, map);
    }
    return retval;
}

static int parser_exec_import_statement(struct parser *p,
                                        struct lexer *l) {
    assert(p->block_level == 0);

    const struct token path_tok = lexer_next(l);
    if (path_tok.type != TOK_STRING) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = path_tok,
        };
        return 0;
    }

    char *path_dup = malloc(path_tok.len + 1);
    memcpy(path_dup, path_tok.src, path_tok.len);
    path_dup[path_tok.len] = 0;

    const size_t idx = p->p_data->imports.len;
    struct token tok = lexer_peek(l, 0);
    if (token_keyword_cmp(&tok, "as")) {
        lexer_next(l);
        const struct token module_tok = lexer_next(l);

        const size_t module_idx = p->p_data->imported_modules.len;
        struct au_imported_module module;
        au_imported_module_init(&module);
        au_imported_module_array_add(&p->p_data->imported_modules, module);

        const struct au_hm_var_value value = (struct au_hm_var_value){
            .idx = module_idx,
        };
        const struct au_hm_var_value *old_value =
            au_hm_vars_add(&p->p_data->imported_module_map, module_tok.src,
                           module_tok.len, &value);
        assert(old_value == 0);

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

    parser_emit_bc_u8(p, OP_IMPORT);
    parser_emit_pad8(p);
    parser_emit_bc_u16(p, (uint16_t)idx);

    return 1;
}

static int parser_exec_export_statement(struct parser *p,
                                        struct lexer *l) {
    assert(p->block_level == 0);

    struct token tok = lexer_next(l);
    if (token_keyword_cmp(&tok, "def")) {
        return parser_exec_def_statement(p, l, 1);
    } else if (token_keyword_cmp(&tok, "class")) {
        return parser_exec_class_statement(p, l, 1);
    } else {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = tok,
        };
        return 0;
    }

    return 1;
}

static int parser_exec_class_statement(struct parser *p, struct lexer *l,
                                       int exported) {
    assert(p->block_level == 0);

    uint32_t class_flags = 0;
    if (exported)
        class_flags |= AU_CLASS_FLAG_EXPORTED;

    const struct token id_tok = lexer_next(l);
    assert(id_tok.type == TOK_IDENTIFIER);

    struct au_hm_var_value class_value = (struct au_hm_var_value){
        .idx = p->p_data->classes.len,
    };
    struct au_hm_var_value *old_value = au_hm_vars_add(
        &p->p_data->class_map, id_tok.src, id_tok.len, &class_value);
    assert(old_value == 0);
    au_class_interface_ptr_array_add(&p->p_data->classes, 0);

    struct au_class_interface *interface =
        malloc(sizeof(struct au_class_interface));
    au_class_interface_init(interface,
                            copy_string(id_tok.src, id_tok.len));
    interface->flags = class_flags;

    struct token t = lexer_next(l);
    if (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ';') {
        au_class_interface_ptr_array_set(&p->p_data->classes,
                                         class_value.idx, interface);
        return 1;
    } else if (!(t.type == TOK_OPERATOR && t.len == 1 &&
                 t.src[0] == '{')) {
        au_class_interface_deref(interface);
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = t,
        };
        return 0;
    }

    while (1) {
        t = lexer_next(l);
        if (token_keyword_cmp(&t, "val")) {
            const struct token name_tok = lexer_next(l);
            assert(name_tok.type == TOK_IDENTIFIER);
            struct au_hm_var_value prop_value = (struct au_hm_var_value){
                .idx = interface->map.entries_occ,
            };
            const struct au_hm_var_value *old_prop_value = au_hm_vars_add(
                &interface->map, name_tok.src, name_tok.len, &prop_value);
            assert(old_prop_value == 0);

            const struct token semicolon = lexer_next(l);
            if (semicolon.type == TOK_OPERATOR && semicolon.len == 1) {
                if (semicolon.src[0] == ';') {
                    continue;
                } else if (semicolon.src[0] == '}') {
                    break;
                }
            }
        } else if (t.type == TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == '}') {
            break;
        }
        au_class_interface_deref(interface);
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = t,
        };
        return 0;
    }

    au_class_interface_ptr_array_set(&p->p_data->classes, class_value.idx,
                                     interface);
    return 1;
}

static int parser_exec_def_statement(struct parser *p, struct lexer *l,
                                     int exported) {
    assert(p->block_level == 0);

    uint32_t fn_flags = 0;
    if (exported)
        fn_flags |= AU_FN_FLAG_EXPORTED;

    struct token tok = lexer_peek(l, 0);
    struct token self_tok = (struct token){.type = TOK_EOF};
    size_t class_idx = CLASS_ID_NONE;
    struct au_class_interface *class_interface = 0;
    if (tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == '(') {
        lexer_next(l);
        fn_flags |= AU_FN_FLAG_HAS_CLASS;

        self_tok = lexer_next(l);
        assert(self_tok.type == TOK_IDENTIFIER);

        tok = lexer_next(l);
        assert(tok.type == TOK_OPERATOR && tok.len == 1 &&
               tok.src[0] == ':');

        struct token name_tok = lexer_next(l);
        assert(name_tok.type == TOK_IDENTIFIER);

        struct token module_tok = (struct token){.type = TOK_EOF};
        tok = lexer_peek(l, 0);
        if (tok.type == TOK_OPERATOR && tok.len == 2 &&
            tok.src[0] == ':' && tok.src[1] == ':') {
            module_tok = name_tok;
            lexer_next(l);
            name_tok = lexer_next(l);
            const struct au_hm_var_value *module_val =
                au_hm_vars_get(&p->p_data->imported_module_map,
                               module_tok.src, module_tok.len);
            assert(module_val != 0);
            const uint32_t module_idx = module_val->idx;
            struct au_imported_module *module =
                &p->p_data->imported_modules.data[module_idx];
            struct au_hm_var_value class_val = (struct au_hm_var_value){
                .idx = p->p_data->classes.len,
            };
            const struct au_hm_var_value *old_class_val =
                au_hm_vars_add(&module->class_map, name_tok.src,
                               name_tok.len, &class_val);
            if (old_class_val != 0) {
                class_val = *old_class_val;
            } else {
                au_class_interface_ptr_array_add(&p->p_data->classes, 0);
            }
            class_idx = class_val.idx;
        } else {
            const struct au_hm_var_value *class_val = au_hm_vars_get(
                &p->p_data->class_map, name_tok.src, name_tok.len);
            assert(class_val != 0);
            class_idx = class_val->idx;
            class_interface = p->p_data->classes.data[class_idx];
        }

        tok = lexer_next(l);
        assert(tok.type == TOK_OPERATOR && tok.len == 1 &&
               tok.src[0] == ')');
    }

    const struct token id_tok = lexer_next(l);
    if (id_tok.type != TOK_IDENTIFIER) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = id_tok,
        };
        return 0;
    }

    int expected_num_args = -1;

    struct au_hm_var_value func_value = (struct au_hm_var_value){
        .idx = p->p_data->fns.len,
    };
    struct au_hm_var_value *old_value = au_hm_vars_add(
        &p->p_data->fn_map, id_tok.src, id_tok.len, &func_value);
    if (old_value) {
        struct au_fn *old = &p->p_data->fns.data[old_value->idx];
        expected_num_args = au_fn_num_args(old);

        // Record the new function into a multi-dispatch function, and
        // turn the regular old function into multi-dispatch function if
        // necessary
        if ((old->flags & AU_FN_FLAG_HAS_CLASS) != 0 ||
            (fn_flags & AU_FN_FLAG_HAS_CLASS) != 0) {
            if (old->type == AU_FN_DISPATCH) {
                struct au_dispatch_func_instance el =
                    (struct au_dispatch_func_instance){
                        .function_idx = func_value.idx,
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
                if ((fallback_fn.type & AU_FN_FLAG_HAS_CLASS) != 0 &&
                    fallback_fn.type == AU_FN_BC) {
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

                func_value.idx = new_fn_idx;
                struct au_fn none_func = (struct au_fn){
                    .type = AU_FN_NONE,
                    .as.none_func.num_args = 0,
                    .as.none_func.name_token.type = TOK_EOF,
                };
                au_fn_array_add(&p->p_data->fns, none_func);
                au_str_array_add(&p->p_data->fn_names,
                                 copy_string(id_tok.src, id_tok.len));
            }
        }
        // If the old function is already a multi-dispatch function,
        // add it to the dispatch list
        else if (old->type == AU_FN_DISPATCH) {
            struct au_dispatch_func_instance el =
                (struct au_dispatch_func_instance){
                    .function_idx = func_value.idx,
                    .class_idx = class_idx,
                };
            au_dispatch_func_instance_array_add(
                &old->as.dispatch_func.data, el);

            old->as.dispatch_func.fallback_fn = func_value.idx;
            old->as.dispatch_func.num_args = expected_num_args;
            old = 0;

            struct au_fn none_func = (struct au_fn){
                .type = AU_FN_NONE,
                .as.none_func.num_args = 0,
                .as.none_func.name_token.type = TOK_EOF,
            };
            au_fn_array_add(&p->p_data->fns, none_func);
            au_str_array_add(&p->p_data->fn_names,
                             copy_string(id_tok.src, id_tok.len));
        } else {
            func_value.idx = old_value->idx;
            au_fn_del(old);
            *old = (struct au_fn){
                .type = AU_FN_NONE,
                .as.none_func.num_args = 0,
                .as.none_func.name_token.type = TOK_EOF,
            };
        }
    } else {
        struct au_fn none_func = (struct au_fn){
            .type = AU_FN_NONE,
            .as.none_func.num_args = 0,
            .as.none_func.name_token.type = TOK_EOF,
        };
        au_fn_array_add(&p->p_data->fns, none_func);
        au_str_array_add(&p->p_data->fn_names,
                         copy_string(id_tok.src, id_tok.len));
    }

    struct parser func_p = {0};
    parser_init(&func_p, p->p_data);
    func_p.self_name = id_tok.src;
    func_p.self_len = id_tok.len;
    func_p.class_interface = class_interface;
    struct au_bc_storage bcs = {0};

    if (self_tok.type != TOK_EOF) {
        if (!token_keyword_cmp(&self_tok, "_")) {
            func_p.self_keyword = self_tok.src;
            func_p.self_keyword_len = self_tok.len;
            const struct au_hm_var_value v = (struct au_hm_var_value){
                .idx = 0,
            };
            au_hm_vars_add(&func_p.vars, self_tok.src, self_tok.len, &v);
        }
        bcs.num_args++;
        func_p.locals_len++;
    }

    tok = lexer_next(l);
    assert(tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == '(');

    tok = lexer_peek(l, 0);
    if (tok.type == TOK_IDENTIFIER) {
        lexer_next(l);
        const struct au_hm_var_value value = (struct au_hm_var_value){
            .idx = bcs.num_args,
        };
        const struct au_hm_var_value *old =
            au_hm_vars_add(&func_p.vars, tok.src, tok.len, &value);
        assert(old == NULL);
        func_p.locals_len++;
        assert(func_p.locals_len < AU_MAX_LOCALS);
        bcs.num_args++;
        while (1) {
            tok = lexer_peek(l, 0);
            if (tok.type == TOK_OPERATOR && tok.len == 1 &&
                tok.src[0] == ')') {
                lexer_next(l);
                break;
            } else if (tok.type == TOK_OPERATOR && tok.len == 1 &&
                       tok.src[0] == ',') {
                lexer_next(l);
                tok = lexer_next(l);
                assert(tok.type == TOK_IDENTIFIER);
                const struct au_hm_var_value v = (struct au_hm_var_value){
                    .idx = bcs.num_args,
                };
                const struct au_hm_var_value *old =
                    au_hm_vars_add(&func_p.vars, tok.src, tok.len, &v);
                assert(old == NULL);
                func_p.locals_len++;
                assert(func_p.locals_len < AU_MAX_LOCALS);
                bcs.num_args++;
            } else {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
                    .data.unexpected_token.got_token = tok,
                };
                return 0;
            }
        }
    } else if (tok.len == 1 && tok.src[0] == ')') {
        lexer_next(l);
    } else {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = tok,
        };
        return 0;
    }

    if (expected_num_args != -1)
        assert(bcs.num_args == expected_num_args);
    func_p.self_num_args = bcs.num_args;
    if (!parser_exec_block(&func_p, l)) {
        p->res = func_p.res;
        func_p.res = (struct au_parser_result){0};
        parser_del(&func_p);
        return 0;
    }
    parser_emit_bc_u8(&func_p, OP_RET_NULL);

    bcs.bc = func_p.bc;
    bcs.locals_len = func_p.locals_len;
    bcs.num_registers = func_p.max_register + 1;
    bcs.class_idx = class_idx;
    bcs.class_interface_cache = class_interface;
    func_p.bc = (struct au_bc_buf){0};

    for (size_t i = 0; i < func_p.self_fill_call.len; i++) {
        const size_t offset = func_p.self_fill_call.data[i];
        replace_bc_u16(&bcs.bc, offset, func_value.idx);
    }
    *au_fn_array_at_mut(&p->p_data->fns, func_value.idx) = (struct au_fn){
        .type = AU_FN_BC,
        .flags = fn_flags,
        .as.bc_func = bcs,
    };

    parser_del(&func_p);

    return 1;
}

static int parser_exec_if_statement(struct parser *p, struct lexer *l) {
    // condition
    int has_else_part = 0;
    if (!parser_exec_expr(p, l))
        return 0;
    const size_t c_len = p->bc.len;
    parser_emit_bc_u8(p, OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    // body
    size_t body_len;
    size_t body_replace_idx = (size_t)-1;
    if (!parser_exec_block(p, l))
        return 0;
    if (!is_return_op(p->bc.data[p->bc.len - 4])) {
        body_len = p->bc.len;
        parser_emit_bc_u8(p, OP_JREL);
        parser_emit_pad8(p);
        body_replace_idx = p->bc.len;
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }
    // else
    {
        const struct token t = lexer_peek(l, 0);
        if (token_keyword_cmp(&t, "else")) {
            lexer_next(l);

            const size_t else_start = p->bc.len;
            {
                const struct token t = lexer_peek(l, 0);
                if (token_keyword_cmp(&t, "if")) {
                    lexer_next(l);
                    if (!parser_exec_if_statement(p, l))
                        return 0;
                } else {
                    if (!parser_exec_block(p, l))
                        return 0;
                }
            }
            has_else_part = 1;

            const size_t else_len = p->bc.len;
            size_t else_replace_idx = (size_t)-1;
            if (!is_return_op(p->bc.data[p->bc.len - 4])) {
                parser_emit_bc_u8(p, OP_JREL);
                parser_emit_pad8(p);
                else_replace_idx = p->bc.len;
                parser_emit_pad8(p);
                parser_emit_pad8(p);
            }

            const size_t end_len = p->bc.len;

            // Else jump
            if (else_replace_idx != (size_t)-1) {
                const size_t offset = (end_len - else_len) / 4;
                if (offset > (uint16_t)-1) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_BYTECODE_GEN,
                    };
                    return 0;
                }
                parser_replace_bc_u16(p, else_replace_idx,
                                      (uint16_t)offset);
            }

            // Condition jump
            {
                const size_t offset = (else_start - c_len) / 4;
                if (offset > (uint16_t)-1) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_BYTECODE_GEN,
                    };
                    return 0;
                }
                parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
            }
        }
    }

    const size_t end_len = p->bc.len;

    // Condition jump
    if (!has_else_part) {
        const size_t offset = (end_len - c_len) / 4;
        if (offset > (uint16_t)-1) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_BYTECODE_GEN,
            };
            return 0;
        }
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if (body_replace_idx != (size_t)-1) {
        const size_t offset = (end_len - body_len) / 4;
        if (offset > (uint16_t)-1) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_BYTECODE_GEN,
            };
            return 0;
        }
        parser_replace_bc_u16(p, body_replace_idx, (uint16_t)offset);
    }

    /*
    condition:
        ...
        jnif [cond], else
    body:
        ...
        jmp if_end
    else:
        ...
        jmp if_end
    if_end:
        ...
    */
    return 1;
}

static int parser_exec_while_statement(struct parser *p, struct lexer *l) {
    // condition
    const size_t cond_part = p->bc.len;
    if (!parser_exec_expr(p, l))
        return 0;
    const size_t c_len = p->bc.len;
    parser_emit_bc_u8(p, OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    // body
    if (!parser_exec_block(p, l))
        return 0;
    size_t body_len;
    size_t body_replace_idx = (size_t)-1;
    if (!is_return_op(p->bc.data[p->bc.len - 4])) {
        body_len = p->bc.len;
        parser_emit_bc_u8(p, OP_JRELB);
        parser_emit_pad8(p);
        body_replace_idx = p->bc.len;
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }

    const size_t end_len = p->bc.len;

    // Condition jump
    {
        const size_t offset = (end_len - c_len) / 4;
        if (offset > (uint16_t)-1) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_BYTECODE_GEN,
            };
            return 0;
        }
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if (body_replace_idx != (size_t)-1) {
        const size_t offset = (body_len - cond_part) / 4;
        if (offset > (uint16_t)-1) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_BYTECODE_GEN,
            };
            return 0;
        }
        parser_replace_bc_u16(p, body_replace_idx, (uint16_t)offset);
    }

    /*
    condition:
        ...
        jnif [cond], end
    block:
        jrelb condition
    end
    */
    return 1;
}

static int parser_exec_print_statement(struct parser *p, struct lexer *l) {
    if (!parser_exec_expr(p, l))
        return 0;
    parser_emit_bc_u8(p, OP_PRINT);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    while (1) {
        const struct token t = lexer_peek(l, 0);
        if (t.type == TOK_EOF ||
            (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ';')) {
            return 1;
        } else if (t.type == TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == ',') {
            lexer_next(l);
            if (!parser_exec_expr(p, l))
                return 0;
            parser_emit_bc_u8(p, OP_PRINT);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            continue;
        } else {
            return 1;
        }
    }
}

static int parser_exec_return_statement(struct parser *p,
                                        struct lexer *l) {
    if (!parser_exec_expr(p, l))
        return 0;
    const uint8_t reg = parser_pop_reg(p);
    if (p->bc.data[p->bc.len - 4] == OP_MOV_LOCAL_REG &&
        p->bc.data[p->bc.len - 3] == reg) {
        p->bc.data[p->bc.len - 4] = OP_RET_LOCAL;
    } else {
        parser_emit_bc_u8(p, OP_RET);
        parser_emit_bc_u8(p, reg);
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }
    return 1;
}

static int parser_exec_call_args(struct parser *p, struct lexer *l,
                                 int *n_args) {
    {
        const struct token t = lexer_peek(l, 0);
        if (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ')') {
            *n_args = 0;
            lexer_next(l);
            return 1;
        }
        if (!parser_exec_expr(p, l))
            return 0;
        parser_emit_bc_u8(p, OP_PUSH_ARG);
        parser_emit_bc_u8(p, parser_pop_reg(p));
        parser_emit_pad8(p);
        parser_emit_pad8(p);
        *n_args = 1;
    }
    while (1) {
        const struct token t = lexer_next(l);
        if (t.type == TOK_EOF ||
            (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ')')) {
            return 1;
        } else if (t.type == TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == ',') {
            if (!parser_exec_expr(p, l))
                return 0;
            parser_emit_bc_u8(p, OP_PUSH_ARG);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            (*n_args)++;
            continue;
        } else {
            return 0;
        }
    }
    return 1;
}

static int parser_exec_expr(struct parser *p, struct lexer *l) {
    return parser_exec_assign(p, l);
}

static int parser_exec_assign(struct parser *p, struct lexer *l) {
    const struct token t = lexer_peek(l, 0);
    if (t.type == TOK_IDENTIFIER || t.type == TOK_AT_IDENTIFIER) {
        const struct token op = lexer_peek(l, 1);
        if (op.type == TOK_OPERATOR &&
            ((op.len == 1 && op.src[0] == '=') ||
             (op.len == 2 &&
              (op.src[0] == '+' || op.src[0] == '-' || op.src[0] == '*' ||
               op.src[0] == '/' || op.src[0] == '%' || op.src[0] == '!') &&
              op.src[1] == '='))) {
            lexer_next(l);
            lexer_next(l);

            if (!parser_exec_expr(p, l))
                return 0;

            if (t.type == TOK_AT_IDENTIFIER) {
                const struct au_class_interface *interface =
                    p->class_interface;
                assert(interface != 0);
                parser_emit_bc_u8(p, OP_CLASS_SET_INNER);
                parser_emit_bc_u8(p, parser_last_reg(p));
                const struct au_hm_var_value *value =
                    au_hm_vars_get(&interface->map, &t.src[1], t.len - 1);
                assert(value != 0);
                parser_emit_bc_u16(p, value->idx);
                return 1;
            }

            struct au_hm_var_value var_value = (struct au_hm_var_value){
                .idx = p->locals_len,
            };

            if (!(op.len == 1 && op.src[0] == '=')) {
                switch (op.src[0]) {
#define BIN_OP_ASG(OP, OPCODE)                                            \
    case OP: {                                                            \
        parser_emit_bc_u8(p, OPCODE);                                     \
        break;                                                            \
    }
                    BIN_OP_ASG('*', OP_MUL_ASG)
                    BIN_OP_ASG('/', OP_DIV_ASG)
                    BIN_OP_ASG('+', OP_ADD_ASG)
                    BIN_OP_ASG('-', OP_SUB_ASG)
                    BIN_OP_ASG('%', OP_MOD_ASG)
                }
            } else {
                parser_emit_bc_u8(p, OP_MOV_REG_LOCAL);
            }

            parser_emit_bc_u8(p, parser_last_reg(p));
            struct au_hm_var_value *old_value =
                au_hm_vars_add(&p->vars, t.src, t.len, &var_value);
            if (old_value) {
                parser_emit_bc_u16(p, old_value->idx);
            } else {
                p->locals_len++;
                assert(p->locals_len < AU_MAX_LOCALS);
                parser_emit_bc_u16(p, var_value.idx);
            }
            return 1;
        }
    }
    return parser_exec_logical(p, l);
}

static int parser_exec_logical(struct parser *p, struct lexer *l) {
    if (!parser_exec_eq(p, l))
        return 0;

    const size_t len = l->pos;
    const struct token t = lexer_next(l);
    if (t.type == TOK_OPERATOR && t.len == 2) {
        if (t.src[0] == '&' && t.src[1] == '&') {
            const uint8_t reg = parser_new_reg(p);
            parser_swap_top_regs(p);
            parser_emit_bc_u8(p, OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);

            const size_t left_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JNIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            if (!parser_exec_expr(p, l))
                return 0;
            const size_t right_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JNIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            parser_emit_bc_u8(p, OP_MOV_BOOL);
            parser_emit_bc_u8(p, 1);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);

            const size_t end_label = p->bc.len;
            parser_replace_bc_u16(p, left_replace_idx,
                                  (end_label - left_len) / 4);
            parser_replace_bc_u16(p, right_replace_idx,
                                  (end_label - right_len) / 4);

            /*
            register = 0
            eval left
            jnif end
            eval right
            jnif end
            body:
                register = 1
            end:
                ...
            */
        } else if (t.src[0] == '|' && t.src[1] == '|') {
            const uint8_t reg = parser_new_reg(p);
            parser_swap_top_regs(p);

            const size_t left_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            if (!parser_exec_expr(p, l))
                return 0;
            const size_t right_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            parser_emit_bc_u8(p, OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            const size_t false_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JREL);
            parser_emit_pad8(p);
            const size_t false_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            const size_t truth_len = p->bc.len;
            parser_emit_bc_u8(p, OP_MOV_BOOL);
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

            /*
            eval left
            jif end
            eval right
            jif end
            body:
                register = 0
                jmp end1
            end:
                register = 1
            end1:
                ...
            */
        }
    } else {
        l->pos = len;
    }
    return 1;
}

#define BIN_EXPR(FN_NAME, BIN_COND, BIN_EXEC, FN_LOWER)                   \
    static int FN_NAME(struct parser *p, struct lexer *l) {               \
        if (!FN_LOWER(p, l))                                              \
            return 0;                                                     \
        while (1) {                                                       \
            const size_t len = l->pos;                                    \
            const struct token t = lexer_next(l);                         \
            if (t.type == TOK_EOF) {                                      \
                l->pos = len;                                             \
                return 1;                                                 \
            } else if (t.type == TOK_OPERATOR && (BIN_COND)) {            \
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

static void parser_emit_bc_binary_expr(struct parser *p) {
    uint8_t rhs = parser_pop_reg(p);
    uint8_t lhs = parser_pop_reg(p);
    uint8_t res = parser_new_reg(p);

    parser_emit_bc_u8(p, lhs);
    parser_emit_bc_u8(p, rhs);
    parser_emit_bc_u8(p, res);
}

BIN_EXPR(
    parser_exec_eq,
    t.len == 2 && t.src[1] == '=' && (t.src[0] == '=' || t.src[0] == '!'),
    {
        if (t.src[0] == '=')
            parser_emit_bc_u8(p, OP_EQ);
        else if (t.src[0] == '!')
            parser_emit_bc_u8(p, OP_NEQ);
        parser_emit_bc_binary_expr(p);
    },
    parser_exec_cmp)

BIN_EXPR(
    parser_exec_cmp, t.len >= 1 && (t.src[0] == '<' || t.src[0] == '>'),
    {
        if (t.len == 1)
            if (t.src[0] == '<')
                parser_emit_bc_u8(p, OP_LT);
            else
                parser_emit_bc_u8(p, OP_GT);
        else if (t.src[0] == '<')
            parser_emit_bc_u8(p, OP_LEQ);
        else
            parser_emit_bc_u8(p, OP_GEQ);
        parser_emit_bc_binary_expr(p);
    },
    parser_exec_addsub)

BIN_EXPR(
    parser_exec_addsub, t.len == 1 && (t.src[0] == '+' || t.src[0] == '-'),
    {
        if (t.src[0] == '+')
            parser_emit_bc_u8(p, OP_ADD);
        else if (t.src[0] == '-')
            parser_emit_bc_u8(p, OP_SUB);
        parser_emit_bc_binary_expr(p);
    },
    parser_exec_muldiv)

BIN_EXPR(
    parser_exec_muldiv,
    t.len == 1 && (t.src[0] == '*' || t.src[0] == '/' || t.src[0] == '%'),
    {
        if (t.src[0] == '*')
            parser_emit_bc_u8(p, OP_MUL);
        else if (t.src[0] == '/')
            parser_emit_bc_u8(p, OP_DIV);
        else if (t.src[0] == '%')
            parser_emit_bc_u8(p, OP_MOD);
        parser_emit_bc_binary_expr(p);
    },
    parser_exec_unary_expr)

static int parser_exec_unary_expr(struct parser *p, struct lexer *l) {
    struct token tok = lexer_peek(l, 0);
    if (tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == '!') {
        lexer_next(l);
        if (!parser_exec_expr(p, l))
            return 0;

        const uint8_t reg = parser_last_reg(p);
        parser_emit_bc_u8(p, OP_NOT);
        parser_emit_bc_u8(p, reg);
        parser_emit_pad8(p);
        parser_emit_pad8(p);

        return 1;
    } else {
        return parser_exec_index_expr(p, l);
    }
}

static int parser_exec_index_expr(struct parser *p, struct lexer *l) {
    if (!parser_exec_val(p, l))
        return 0;
    const uint8_t left_reg = parser_last_reg(p);

    struct token tok = lexer_peek(l, 0);
    if (tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == '[') {
        lexer_next(l);
        if (!parser_exec_expr(p, l))
            return 0;
        const uint8_t idx_reg = parser_last_reg(p);
        tok = lexer_next(l);
        if (!(tok.type == TOK_OPERATOR && tok.len == 1 &&
              tok.src[0] == ']'))
            assert(0);
        tok = lexer_peek(l, 0);
        if (tok.type == TOK_OPERATOR && tok.len == 1 &&
            tok.src[0] == '=') {
            lexer_next(l);
            if (!parser_exec_expr(p, l))
                return 0;
            const uint8_t right_reg = parser_last_reg(p);
            parser_emit_bc_u8(p, OP_IDX_SET);
            parser_emit_bc_u8(p, left_reg);
            parser_emit_bc_u8(p, idx_reg);
            parser_emit_bc_u8(p, right_reg);
            // Right now, the free register stack is:
            // ... [array reg (-3)] [idx reg (-2)] [right reg (-1)]
            // We want to remove array and idx regs because
            // they aren't used
            p->rstack[p->rstack_len - 3] = p->rstack[p->rstack_len - 1];
            p->rstack_len -= 2;
        } else {
            const uint8_t result_reg = parser_new_reg(p);
            parser_emit_bc_u8(p, OP_IDX_GET);
            parser_emit_bc_u8(p, left_reg);
            parser_emit_bc_u8(p, idx_reg);
            parser_emit_bc_u8(p, result_reg);
            // ... [array reg (-3)] [idx reg (-2)] [array value reg (-1)]
            // We also want to remove array/idx regs here, too
            p->rstack[p->rstack_len - 3] = p->rstack[p->rstack_len - 1];
            p->rstack_len -= 2;
        }
    }

    return 1;
}

static int parser_exec_val(struct parser *p, struct lexer *l) {
    struct token t;
    if ((t = lexer_next(l)).type == TOK_EOF) {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = t,
        };
        return 0;
    }

    switch (t.type) {
    case TOK_INT: {
        int num = 0;
        for (size_t i = 0; i < t.len; i++) {
            num = num * 10 + (t.src[i] - '0');
        }

        if (-0x7fff <= num && num <= 0x8000) {
            parser_emit_bc_u8(p, OP_MOV_U16);
            parser_emit_bc_u8(p, parser_new_reg(p));
            parser_emit_bc_u16(p, num);
        } else {
            int idx = au_program_data_add_data(p->p_data,
                                               au_value_int(num), 0, 0);
            parser_emit_bc_u8(p, OP_LOAD_CONST);
            parser_emit_bc_u8(p, parser_new_reg(p));
            parser_emit_bc_u16(p, idx);
        }
        break;
    }
    case TOK_DOUBLE: {
        double value = 0.0;
        for (size_t i = 0; i < t.len; i++) {
            if (t.src[i] == '.') {
                i++;
                unsigned int fractional = 0, power = 1;
                for (; i < t.len; i++) {
                    fractional = (fractional * 10) + (t.src[i] - '0');
                    power *= 10;
                }
                value += ((double)fractional / (double)power);
                break;
            }
            value = (value * 10.0) + (t.src[i] - '0');
        }

        int idx = au_program_data_add_data(p->p_data,
                                           au_value_double(value), 0, 0);
        parser_emit_bc_u8(p, OP_LOAD_CONST);
        parser_emit_bc_u8(p, parser_new_reg(p));
        parser_emit_bc_u16(p, idx);
        break;
    }
    case TOK_OPERATOR: {
        if (t.len == 1 && t.src[0] == '(') {
            if (!parser_exec_expr(p, l))
                return 0;
            t = lexer_next(l);
            if (!(t.len == 1 && t.src[0] == ')')) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
                    .data.unexpected_token.got_token = t,
                };
                return 0;
            }
        } else if (t.len == 1 && t.src[0] == '[') {
            return parser_exec_array_or_tuple(p, l, 0);
        } else if (t.len == 2 && t.src[0] == '#' && t.src[1] == '[') {
            return parser_exec_array_or_tuple(p, l, 1);
        } else {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
                .data.unexpected_token.got_token = t,
            };
            return 0;
        }
        break;
    }
    case TOK_IDENTIFIER: {
        if (token_keyword_cmp(&t, "true")) {
            const uint8_t reg = parser_new_reg(p);
            parser_emit_bc_u8(p, OP_MOV_BOOL);
            parser_emit_bc_u8(p, 1);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "false")) {
            const uint8_t reg = parser_new_reg(p);
            parser_emit_bc_u8(p, OP_MOV_BOOL);
            parser_emit_bc_u8(p, 0);
            parser_emit_bc_u8(p, reg);
            parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "new")) {
            return parser_exec_new_expr(p, l);
        }

        struct token peek = lexer_peek(l, 0);

        struct token module_tok = (struct token){.type = TOK_EOF};
        if (peek.type == TOK_OPERATOR && peek.len == 2 &&
            peek.src[0] == ':' && peek.src[1] == ':') {
            module_tok = t;
            lexer_next(l);
            t = lexer_next(l);
            assert(t.type == TOK_IDENTIFIER);
            peek = lexer_peek(l, 0);
        }

        if (peek.type == TOK_OPERATOR && peek.len == 1 &&
            peek.src[0] == '(') {
            lexer_next(l);
            int n_args = 0;
            if (!parser_exec_call_args(p, l, &n_args))
                return 0;

            size_t func_idx = 0;
            int func_idx_found = 0;
            int execute_self = 0;
            if (module_tok.type != TOK_EOF) {
                const struct au_hm_var_value *module_val =
                    au_hm_vars_get(&p->p_data->imported_module_map,
                                   module_tok.src, module_tok.len);
                assert(module_val != 0);
                const uint32_t module_idx = module_val->idx;
                struct au_imported_module *module =
                    &p->p_data->imported_modules.data[module_idx];
                const struct au_hm_var_value *val =
                    au_hm_vars_get(&module->fn_map, t.src, t.len);
                if (val == 0) {
                    struct au_hm_var_value value =
                        (struct au_hm_var_value){
                            .idx = p->p_data->fns.len,
                        };
                    char *import_name = malloc(t.len);
                    memcpy(import_name, t.src, t.len);
                    struct au_fn fn = (struct au_fn){
                        .type = AU_FN_IMPORTER,
                        .flags = 0,
                        .as.import_func.num_args = n_args,
                        .as.import_func.module_idx = module_idx,
                        .as.import_func.name = import_name,
                        .as.import_func.name_len = t.len,
                        .as.import_func.fn_cached = 0,
                        .as.import_func.p_data_cached = 0,
                    };
                    au_fn_array_add(&p->p_data->fns, fn);
                    func_idx = value.idx;
                    struct au_hm_var_value *old = au_hm_vars_add(
                        &module->fn_map, t.src, t.len, &value);
                    assert(old == 0);
                    val = au_hm_vars_get(&module->fn_map, t.src, t.len);
                } else {
                    func_idx = val->idx;
                }
                func_idx_found = 1;
            } else if (p->self_name && t.len == p->self_len &&
                       memcmp(p->self_name, t.src, p->self_len) == 0) {
                execute_self = 1;
                func_idx_found = 1;
            } else {
                const struct au_hm_var_value *val =
                    au_hm_vars_get(&p->p_data->fn_map, t.src, t.len);
                if (val) {
                    func_idx = val->idx;
                    func_idx_found = 1;
                }
            }

            if (!func_idx_found) {
                struct au_hm_var_value func_value =
                    (struct au_hm_var_value){
                        .idx = p->p_data->fns.len,
                    };
                au_hm_vars_add(&p->p_data->fn_map, t.src, t.len,
                               &func_value);
                struct au_fn none_func = (struct au_fn){
                    .type = AU_FN_NONE,
                    .as.none_func.num_args = n_args,
                    .as.none_func.name_token = t,
                };
                au_fn_array_add(&p->p_data->fns, none_func);
                au_str_array_add(&p->p_data->fn_names,
                                 copy_string(t.src, t.len));
                func_idx = func_value.idx;
            }

            if (execute_self) {
                if (p->self_num_args != n_args) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_WRONG_ARGS,
                        .data.wrong_args.got_args = n_args,
                        .data.wrong_args.expected_args = p->self_num_args,
                        .data.wrong_args.call_token = t,
                    };
                    return 0;
                }
            } else {
                const struct au_fn *fn = &p->p_data->fns.data[func_idx];
                int expected_n_args = au_fn_num_args(fn);
                if (expected_n_args != n_args) {
                    p->res = (struct au_parser_result){
                        .type = AU_PARSER_RES_WRONG_ARGS,
                        .data.wrong_args.got_args = n_args,
                        .data.wrong_args.expected_args = expected_n_args,
                        .data.wrong_args.call_token = t,
                    };
                    return 0;
                }
            }

            parser_emit_bc_u8(p, OP_CALL);
            parser_emit_bc_u8(p, parser_new_reg(p));
            const size_t offset = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);
            if (execute_self) {
                size_t_array_add(&p->self_fill_call, offset);
            } else {
                parser_replace_bc_u16(p, offset, func_idx);
            }
        } else {
            const struct au_hm_var_value *val =
                au_hm_vars_get(&p->vars, t.src, t.len);
            if (val == NULL) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNKNOWN_VAR,
                    .data.unknown_var.name_token = t,
                };
                return 0;
            }
            parser_emit_bc_u8(p, OP_MOV_LOCAL_REG);
            parser_emit_bc_u8(p, parser_new_reg(p));
            parser_emit_bc_u16(p, val->idx);
        }
        break;
    }
    case TOK_STRING: {
        // Perform string escaping
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
                    formatted_string = malloc(t.len);
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
        if (formatted_string) {
            idx = au_program_data_add_data(p->p_data, au_value_string(0),
                                           (uint8_t *)formatted_string,
                                           formatted_string_len);
            free(formatted_string);
        } else {
            idx = au_program_data_add_data(p->p_data, au_value_string(0),
                                           (uint8_t *)t.src, t.len);
        }
        parser_emit_bc_u8(p, OP_LOAD_CONST);
        parser_emit_bc_u8(p, parser_new_reg(p));
        parser_emit_bc_u16(p, idx);
        break;
    }
    case TOK_AT_IDENTIFIER: {
        const struct au_class_interface *interface = p->class_interface;
        assert(interface != 0);
        parser_emit_bc_u8(p, OP_CLASS_GET_INNER);
        parser_emit_bc_u8(p, parser_new_reg(p));
        const struct au_hm_var_value *value =
            au_hm_vars_get(&interface->map, &t.src[1], t.len - 1);
        assert(value != 0);
        parser_emit_bc_u16(p, value->idx);
        break;
    }
    default: {
        p->res = (struct au_parser_result){
            .type = AU_PARSER_RES_UNEXPECTED_TOKEN,
            .data.unexpected_token.got_token = t,
        };
        return 0;
    }
    }

    return 1;
}

static int parser_exec_array_or_tuple(struct parser *p, struct lexer *l,
                                      int is_tuple) {
    const uint8_t array_reg = parser_new_reg(p);
    if (is_tuple) {
        parser_emit_bc_u8(p, OP_TUPLE_NEW);
        parser_emit_bc_u8(p, array_reg);
    } else {
        parser_emit_bc_u8(p, OP_ARRAY_NEW);
        parser_emit_bc_u8(p, array_reg);
    }
    const size_t cap_offset = p->bc.len;
    parser_emit_bc_u16(p, 0);

    struct token tok = lexer_peek(l, 0);
    if (tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == ']') {
        lexer_next(l);
        return 1;
    }

    uint16_t capacity = 1;
    if (!parser_exec_expr(p, l))
        return 0;
    const uint8_t value_reg = parser_pop_reg(p);
    if (is_tuple) {
        parser_emit_bc_u8(p, OP_IDX_SET_STATIC);
        parser_emit_bc_u8(p, array_reg);
        parser_emit_bc_u8(p, 0);
        parser_emit_bc_u8(p, value_reg);
    } else {
        parser_emit_bc_u8(p, OP_ARRAY_PUSH);
        parser_emit_bc_u8(p, array_reg);
        parser_emit_bc_u8(p, value_reg);
        parser_emit_pad8(p);
    }

    while (1) {
        tok = lexer_peek(l, 0);
        if (tok.type == TOK_OPERATOR && tok.len == 1 &&
            tok.src[0] == ']') {
            lexer_next(l);
            break;
        } else if (tok.type == TOK_OPERATOR && tok.len == 1 &&
                   tok.src[0] == ',') {
            lexer_next(l);

            tok = lexer_peek(l, 0);
            if (tok.type == TOK_OPERATOR && tok.len == 1 &&
                tok.src[0] == ']')
                break;

            if (!parser_exec_expr(p, l))
                return 0;
            const uint8_t value_reg = parser_pop_reg(p);

            if (is_tuple) {
                parser_emit_bc_u8(p, OP_IDX_SET_STATIC);
                parser_emit_bc_u8(p, array_reg);
                parser_emit_bc_u8(p, capacity);
                parser_emit_bc_u8(p, value_reg);
                capacity++;
                assert(capacity < AU_MAX_STATIC_IDX);
            } else {
                parser_emit_bc_u8(p, OP_ARRAY_PUSH);
                parser_emit_bc_u8(p, array_reg);
                parser_emit_bc_u8(p, value_reg);
                parser_emit_pad8(p);
                if (capacity < (AU_MAX_ARRAY - 1)) {
                    capacity++;
                }
            }
        } else {
            assert(0);
        }
    }

    parser_replace_bc_u16(p, cap_offset, capacity);
    return 1;
}

static int parser_exec_new_expr(struct parser *p, struct lexer *l) {
    const struct token id_tok = lexer_next(l);
    assert(id_tok.type == TOK_IDENTIFIER);

    const struct au_hm_var_value *class_value =
        au_hm_vars_get(&p->p_data->class_map, id_tok.src, id_tok.len);
    size_t class_idx = class_value->idx;

    parser_emit_bc_u8(p, OP_CLASS_NEW);
    parser_emit_bc_u8(p, parser_new_reg(p));
    parser_emit_bc_u16(p, class_idx);

    return 1;
}

struct au_parser_result au_parse(const char *src, size_t len,
                                 struct au_program *program) {
    struct au_program_data p_data;
    au_program_data_init(&p_data);

    struct lexer l;
    lexer_init(&l, src, len);

    struct parser p;
    parser_init(&p, &p_data);
    if (!parser_exec(&p, &l)) {
        struct au_parser_result res = p.res;
        lexer_del(&l);
        parser_del(&p);
        au_program_data_del(&p_data);
        assert(res.type != AU_PARSER_RES_OK);
        return res;
    }

    for (size_t i = 0; i < p_data.fns.len; i++) {
        if (p_data.fns.data[i].type == AU_FN_NONE) {
            struct token name_token =
                p_data.fns.data[i].as.none_func.name_token;
            lexer_del(&l);
            parser_del(&p);
            au_program_data_del(&p_data);
            return (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_FUNCTION,
                .data.unknown_function.name_token = name_token,
            };
        }
    }

    struct au_bc_storage p_main;
    au_bc_storage_init(&p_main);
    p_main.bc = p.bc;
    p_main.locals_len = p.locals_len;
    p_main.num_registers = p.max_register + 1;
    p.bc = (struct au_bc_buf){0};

    program->main = p_main;
    program->data = p_data;

    lexer_del(&l);
    parser_del(&p);
    // p_data is moved
    return (struct au_parser_result){
        .type = AU_PARSER_RES_OK,
    };
}
