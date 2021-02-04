// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "platform/mmap.h"

#include "bc.h"
#include "exception.h"
#include "program.h"
#include "array.h"

#include "lexer.c"

ARRAY_TYPE(size_t, size_t_array, 1)

static inline int is_return_op(uint8_t op) {
    return op == OP_RET_LOCAL ||
           op == OP_RET ||
           op == OP_RET_NULL;
}

struct parser {
    struct au_bc_buf bc;

    uint8_t rstack[AU_REGS];
    size_t rstack_len;
    
    uint8_t free_regs[AU_REGS];
    size_t free_regs_len;

    struct au_bc_vars vars;

    /// This struct does not own the data
    struct au_program_data *p_data;

    int locals_len;
    int max_register;
    int block_level;

    // Not owned by this parser
    char *self_name;
    size_t self_len;
    struct size_t_array self_fill_call;
};

static void parser_flush_free_regs(struct parser *p) {
    p->rstack_len = 0;
    for(int i = 0; i < AU_REGS; i++) {
        p->free_regs[i] = AU_REGS - i - 1;
    }
    p->free_regs_len = AU_REGS;
}

static void parser_init(struct parser *p, struct au_program_data *p_data) {
    p->bc = (struct au_bc_buf){0};
    parser_flush_free_regs(p);
    au_bc_vars_init(&p->vars);
    p->p_data = p_data;

    p->locals_len = 0;
    p->max_register = -1;
    p->block_level = 0;

    p->self_name = 0;
    p->self_len = 0;
    p->self_fill_call = (struct size_t_array){0};
}

static void parser_del(struct parser *p) {
    free(p->bc.data);
    au_bc_vars_del(&p->vars);
    free(p->self_fill_call.data);
    memset(p, 0, sizeof(struct parser));
}

static uint8_t parser_new_reg(struct parser *p) {
    assert(p->rstack_len + 1 <= AU_REGS);
    assert(p->free_regs_len > 0);
    uint8_t reg = p->free_regs[--p->free_regs_len];
    p->rstack[p->rstack_len++] = reg;
    if(reg > p->max_register)
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

static void replace_bc_u16(struct au_bc_buf *bc, size_t idx, uint16_t val) {
    assert(idx + 1 < bc->len);
    uint16_t *ptr = (uint16_t*)(&bc->data[idx]);
    ptr[0] = val;
}

static void parser_replace_bc_u16(struct parser *p, size_t idx, uint16_t val) {
    replace_bc_u16(&p->bc, idx, val);
}

static void parser_emit_bc_u16(struct parser *p, uint16_t val) {
    const size_t offset = p->bc.len;
    parser_emit_bc_u8(p, 0);
    parser_emit_bc_u8(p, 0);
    uint16_t *ptr = (uint16_t*)(&p->bc.data[offset]);
    ptr[0] = val;
}

static void parser_emit_pad8(struct parser *p) {
    parser_emit_bc_u8(p, 0);
}

static int parser_exec_statement(struct parser *p, struct lexer *l);
static int parser_exec_expr(struct parser *p, struct lexer *l);
static int parser_exec_assign(struct parser *p, struct lexer *l);
static int parser_exec_logical(struct parser *p, struct lexer *l);
static int parser_exec_eq(struct parser *p, struct lexer *l);
static int parser_exec_cmp(struct parser *p, struct lexer *l);
static int parser_exec_addsub(struct parser *p, struct lexer *l);
static int parser_exec_muldiv(struct parser *p, struct lexer *l);
static int parser_exec_val(struct parser *p, struct lexer *l);

static int parser_exec(struct parser *p, struct lexer *l) {
    while(1) {
        int retval = parser_exec_statement(p, l);
        if(retval == 0) return 0;
        else if (retval == -1) break;
        parser_flush_free_regs(p);
    }
    parser_emit_bc_u8(p, OP_EXIT);
    return 1;
}

static int parser_exec_import_statement(struct parser *p, struct lexer *l);
static int parser_exec_def_statement(struct parser *p, struct lexer *l);
static int parser_exec_while_statement(struct parser *p, struct lexer *l);
static int parser_exec_if_statement(struct parser *p, struct lexer *l);
static int parser_exec_print_statement(struct parser *p, struct lexer *l);
static int parser_exec_return_statement(struct parser *p, struct lexer *l);

static int parser_exec_with_semicolon(struct lexer *l, int retval) {
    if(!retval) return retval;
    struct token t = lexer_next(l);
    if(t.type == TOK_EOF) return 1;
    if(!(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ';'))
        return 0;
    return 1;
}

// Block statements
static int parser_exec_block(struct parser *p, struct lexer *l) {
    p->block_level++;

    {
        struct token t = lexer_next(l);
        assert(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == '{');
    }

    while(1) {
        {
            struct token t = lexer_peek(l, 0);
            if(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == '}') {
                lexer_next(l);
                break;
            }
        }

        int retval = parser_exec_statement(p, l);
        if(retval == 0) return 0;
        else if (retval == -1) break;
        parser_flush_free_regs(p);
    }

    p->block_level--;
    return 1;
}

static int parser_exec_statement(struct parser *p, struct lexer *l) {
    const struct token t = lexer_peek(l, 0);
    if(t.type == TOK_EOF) {
        return -1;
    } else if(t.type == TOK_IDENTIFIER) {
        if(token_keyword_cmp(&t, "def")) {
            lexer_next(l);
            return parser_exec_def_statement(p, l);
        } else if(token_keyword_cmp(&t, "if")) {
            lexer_next(l);
            return parser_exec_if_statement(p, l);
        } else if(token_keyword_cmp(&t, "while")) {
            lexer_next(l);
            return parser_exec_while_statement(p, l);
        } else if(token_keyword_cmp(&t, "print")) {
            lexer_next(l);
            return parser_exec_with_semicolon(l, parser_exec_print_statement(p, l));
        } else if(token_keyword_cmp(&t, "return")) {
            lexer_next(l);
            return parser_exec_with_semicolon(l, parser_exec_return_statement(p, l));
        } else if(token_keyword_cmp(&t, "import")) {
            lexer_next(l);
            return parser_exec_with_semicolon(l, parser_exec_import_statement(p, l));
        }
    }
    return parser_exec_with_semicolon(l, parser_exec_expr(p, l));
}

static int parser_exec_import_statement(struct parser *p, struct lexer *l) {
    assert(p->block_level == 0);

    const struct token path_tok = lexer_next(l);
    assert(path_tok.type == TOK_STRING);

    char *path_dup = malloc(path_tok.len + 1);
    strncpy(path_dup, path_tok.src, path_tok.len);
    path_dup[path_tok.len] = 0;
    const size_t idx = p->p_data->imports.len;
    au_str_array_add(&p->p_data->imports, path_dup);

    parser_emit_bc_u8(p, OP_IMPORT);
    parser_emit_pad8(p);
    parser_emit_bc_u16(p, (uint16_t)idx);

    return 1;
}

static int parser_exec_def_statement(struct parser *p, struct lexer *l) {
    assert(p->block_level == 0);

    const struct token id_tok = lexer_next(l);
    assert(id_tok.type == TOK_IDENTIFIER);

    struct parser func_p = {0};
    parser_init(&func_p, p->p_data);
    func_p.self_name = id_tok.src;
    func_p.self_len = id_tok.len;
    struct au_bc_storage bcs = {0};

    struct token tok = lexer_next(l);
    assert(tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == '(');
    tok = lexer_peek(l, 0);
    if(tok.type == TOK_IDENTIFIER) {
        lexer_next(l);
        const struct au_bc_var_value v = (struct au_bc_var_value) {
            .idx = bcs.num_args,
        };
        const struct au_bc_var_value *old = au_bc_vars_add(&func_p.vars, tok.src, tok.len, &v);
        assert(old == NULL);
        func_p.locals_len++;
        bcs.num_args++;
        while(1){
            tok = lexer_peek(l, 0);
            if(tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == ')') {
                lexer_next(l);
                break;
            } else if(tok.type == TOK_OPERATOR && tok.len == 1 && tok.src[0] == ',') {
                lexer_next(l);
                tok = lexer_next(l);
                assert(tok.type == TOK_IDENTIFIER);
                const struct au_bc_var_value v = (struct au_bc_var_value) {
                    .idx = bcs.num_args,
                };
                const struct au_bc_var_value *old = au_bc_vars_add(&func_p.vars, tok.src, tok.len, &v);
                assert(old == NULL);
                func_p.locals_len++;
                bcs.num_args++;
            } else {
                assert(0);
            }
        }
    } else if(tok.len == 1 && tok.src[0] == ')') {
        lexer_next(l);
    } else {
        assert(0);
    }

    assert(parser_exec_block(&func_p, l) == 1);
    parser_emit_bc_u8(&func_p, OP_RET_NULL);

    bcs.bc = func_p.bc;
    bcs.locals_len = func_p.locals_len;
    bcs.num_registers = func_p.max_register + 1;
    func_p.bc = (struct au_bc_buf){0};

    struct au_bc_var_value var_value = (struct au_bc_var_value){
        .idx = p->p_data->fns.len,
    };
    struct au_bc_var_value *old_value = au_bc_vars_add(
        &p->p_data->fn_map,
        id_tok.src, id_tok.len,
        &var_value
    );
    if(old_value) {
        // Forward declaration
        assert(0);
    } else {
        for(int i = 0; i < func_p.self_fill_call.len; i++) {
            const size_t offset = func_p.self_fill_call.data[i];
            replace_bc_u16(&bcs.bc, offset, var_value.idx);
        }
        au_bc_storage_vals_add(&p->p_data->fns, bcs);
    }

    parser_del(&func_p);
    
    return 1;
}

static int parser_exec_if_statement(struct parser *p, struct lexer *l) {
    // condition
    int has_else_part = 0;
    if(!parser_exec_expr(p, l)) return 0;
    const size_t c_len = p->bc.len;
    parser_emit_bc_u8(p, OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    // body
    size_t body_len;
    size_t body_replace_idx = (size_t)-1;
    if(!parser_exec_block(p, l)) return 0;
    if(!is_return_op(p->bc.data[p->bc.len - 4])) {
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
        if(token_keyword_cmp(&t, "else")) {
            lexer_next(l);

            const size_t else_start = p->bc.len;
            {
                const struct token t = lexer_peek(l, 0);
                if(token_keyword_cmp(&t, "if")) {
                    lexer_next(l);
                    if(!parser_exec_if_statement(p, l)) return 0;
                } else {
                    if(!parser_exec_block(p, l)) return 0;
                }
            }
            has_else_part = 1;

            const size_t else_len = p->bc.len;
            size_t else_replace_idx = (size_t)-1;
            if(!is_return_op(p->bc.data[p->bc.len - 4])) {
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
                if(offset > (uint16_t)-1)
                    au_fatal("offset is more than (uint16_t)-1\n");
                parser_replace_bc_u16(p, else_replace_idx, (uint16_t)offset);
            }

            // Condition jump
            {
                const size_t offset = (else_start - c_len) / 4;
                if(offset > (uint16_t)-1)
                    au_fatal("offset is more than (uint16_t)-1\n");
                parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
            }
        }
    }

    const size_t end_len = p->bc.len;

    // Condition jump
    if (!has_else_part) {
        const size_t offset = (end_len - c_len) / 4;
        if(offset > (uint16_t)-1)
            au_fatal("offset is more than (uint16_t)-1\n");
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if (body_replace_idx != (size_t)-1) {
        const size_t offset = (end_len - body_len) / 4;
        if(offset > (uint16_t)-1)
            au_fatal("offset is more than (uint16_t)-1\n");
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
    if(!parser_exec_expr(p, l)) return 0;
    const size_t c_len = p->bc.len;
    parser_emit_bc_u8(p, OP_JNIF);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    const size_t c_replace_idx = p->bc.len;
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    // body
    if(!parser_exec_block(p, l)) return 0;
    size_t body_len;
    size_t body_replace_idx = (size_t)-1;
    if(!is_return_op(p->bc.data[p->bc.len - 4])) {
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
        if(offset > (uint16_t)-1)
            au_fatal("offset is more than (uint16_t)-1\n");
        parser_replace_bc_u16(p, c_replace_idx, (uint16_t)offset);
    }

    // Block jump
    if(body_replace_idx != (size_t)-1){
        const size_t offset = (body_len - cond_part) / 4;
        if(offset > (uint16_t)-1)
            au_fatal("offset is more than (uint16_t)-1\n");
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
    if(!parser_exec_expr(p, l)) return 0;
    parser_emit_bc_u8(p, OP_PRINT);
    parser_emit_bc_u8(p, parser_pop_reg(p));
    parser_emit_pad8(p);
    parser_emit_pad8(p);
    while (1) {
        const struct token t = lexer_peek(l, 0);
        if(t.type == TOK_EOF || (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ';')) {
            return 1;
        } else if(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ',') {
            lexer_next(l);
            if(!parser_exec_expr(p, l)) return 0;
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

static int parser_exec_return_statement(struct parser *p, struct lexer *l) {
    if(!parser_exec_expr(p, l)) return 0;
    const uint8_t reg = parser_pop_reg(p);
    if( p->bc.data[p->bc.len - 4] == OP_MOV_LOCAL_REG &&
        p->bc.data[p->bc.len - 2] == reg ) {
        const uint8_t local = p->bc.data[p->bc.len - 3];
        p->bc.data[p->bc.len - 4] = OP_RET_LOCAL;
        p->bc.data[p->bc.len - 3] = local;
    } else {
        parser_emit_bc_u8(p, OP_RET);
        parser_emit_bc_u8(p, reg);
        parser_emit_pad8(p);
        parser_emit_pad8(p);
    }
    return 1;
}

static int parser_exec_call_args(struct parser *p, struct lexer *l, int *n_args) {
    {
        const struct token t = lexer_peek(l, 0);
        if(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ')') {
            *n_args = 0;
            lexer_next(l);
            return 1;
        }
        if(!parser_exec_expr(p, l)) return 0;
        parser_emit_bc_u8(p, OP_PUSH_ARG);
        parser_emit_bc_u8(p, parser_pop_reg(p));
        parser_emit_pad8(p);
        parser_emit_pad8(p);
        *n_args = 1;
    }
    while (1) {
        const struct token t = lexer_next(l);
        if(t.type == TOK_EOF || (t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ')')) {
            return 1;
        } else if(t.type == TOK_OPERATOR && t.len == 1 && t.src[0] == ',') {
            if(!parser_exec_expr(p, l)) return 0;
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
    if(t.type == TOK_IDENTIFIER) {
        const struct token op = lexer_peek(l, 1);
        if(op.type == TOK_OPERATOR && 
            ((op.len == 1 && op.src[0] == '=') ||
            (op.len == 2 && (
                op.src[0] == '+' ||
                op.src[0] == '-' ||
                op.src[0] == '*' ||
                op.src[0] == '/' ||
                op.src[0] == '%' ||
                op.src[0] == '!'
            ) && op.src[1] == '='))) {
            lexer_next(l);
            lexer_next(l);

            if(!parser_exec_expr(p, l)) return 0;
            struct au_bc_var_value var_value = (struct au_bc_var_value){
                .idx = p->locals_len,
            };

            if(op.len == 2) {
                switch(op.src[0]) {
                    #define BIN_OP_ASG(OP, OPCODE) \
                    case OP: { \
                        parser_emit_bc_u8(p, OPCODE); \
                        break; \
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
            struct au_bc_var_value *old_value = au_bc_vars_add(
                &p->vars,
                t.src, t.len,
                &var_value
            );
            if(old_value) {
                parser_emit_bc_u8(p, old_value->idx);
            } else {
                p->locals_len++;
                parser_emit_bc_u8(p, var_value.idx);
            }
            parser_emit_pad8(p);
            return 1;
        }
    }
    return parser_exec_logical(p, l);
}

static int parser_exec_logical(struct parser *p, struct lexer *l) {
    if(!parser_exec_eq(p, l)) return 0;

    const size_t len = l->pos;
    const struct token t = lexer_next(l);
    if(t.type == TOK_OPERATOR && t.len == 2) {
        if(t.src[0] == '&' && t.src[1] == '&') {
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

            if(!parser_exec_expr(p, l)) return 0;
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
            parser_replace_bc_u16(p, left_replace_idx, (end_label - left_len) / 4);
            parser_replace_bc_u16(p, right_replace_idx, (end_label - right_len) / 4);

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
        } else if(t.src[0] == '|' && t.src[1] == '|') {
            const uint8_t reg = parser_new_reg(p);
            parser_swap_top_regs(p);

            const size_t left_len = p->bc.len;
            parser_emit_bc_u8(p, OP_JIF);
            parser_emit_bc_u8(p, parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            parser_emit_pad8(p);
            parser_emit_pad8(p);

            if(!parser_exec_expr(p, l)) return 0;
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
            parser_replace_bc_u16(p, false_replace_idx, (end_label - false_len) / 4);
            parser_replace_bc_u16(p, left_replace_idx, (truth_len - left_len) / 4);
            parser_replace_bc_u16(p, right_replace_idx, (truth_len - right_len) / 4);

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

#define BIN_EXPR(FN_NAME, BIN_COND, BIN_EXEC, FN_LOWER) \
static int FN_NAME(struct parser *p, struct lexer *l) { \
    if(!FN_LOWER(p, l)) return 0; \
    while (1) { \
        const size_t len = l->pos; \
        const struct token t = lexer_next(l); \
        if(t.type == TOK_EOF) { \
            l->pos = len; \
            return 1; \
        } else if(t.type == TOK_OPERATOR && (BIN_COND)) { \
            if(!FN_LOWER(p, l)) return 0; \
            do { BIN_EXEC } while(0); \
            continue; \
        } else { \
            l->pos = len; \
            return 1; \
        } \
        l->pos = len; \
    } \
}

static void parser_emit_bc_binary_expr(struct parser *p) {
    uint8_t rhs = parser_pop_reg(p);
    uint8_t lhs = parser_pop_reg(p);
    uint8_t res = parser_new_reg(p);

    parser_emit_bc_u8(p, lhs);
    parser_emit_bc_u8(p, rhs);
    parser_emit_bc_u8(p, res);
}

BIN_EXPR(parser_exec_eq, t.len == 2 && t.src[1] == '=' && (t.src[0] == '=' || t.src[0] == '!'), {
    if(t.src[0] == '=')
        parser_emit_bc_u8(p, OP_EQ);
    else if(t.src[0] == '!')
        parser_emit_bc_u8(p, OP_NEQ);
    parser_emit_bc_binary_expr(p);
}, parser_exec_cmp)

BIN_EXPR(parser_exec_cmp, t.len >= 1 && (t.src[0] == '<' || t.src[0] == '>'), {
    if(t.len == 1)
        if(t.src[0] == '<') parser_emit_bc_u8(p, OP_LT);
        else parser_emit_bc_u8(p, OP_GT);
    else
        if(t.src[0] == '<') parser_emit_bc_u8(p, OP_LEQ);
        else parser_emit_bc_u8(p, OP_GEQ);
    parser_emit_bc_binary_expr(p);
}, parser_exec_addsub)

BIN_EXPR(parser_exec_addsub, t.len == 1 && (t.src[0] == '+' || t.src[0] == '-'), {
    if(t.src[0] == '+')
        parser_emit_bc_u8(p, OP_ADD);
    else if (t.src[0] == '-')
        parser_emit_bc_u8(p, OP_SUB);
    parser_emit_bc_binary_expr(p);
}, parser_exec_muldiv)

BIN_EXPR(parser_exec_muldiv, t.len == 1 && (t.src[0] == '*' || t.src[0] == '/' || t.src[0] == '%'), {
    if(t.src[0] == '*')
        parser_emit_bc_u8(p, OP_MUL);
    else if (t.src[0] == '/')
        parser_emit_bc_u8(p, OP_DIV);
    else if (t.src[0] == '%')
        parser_emit_bc_u8(p, OP_MOD);
    parser_emit_bc_binary_expr(p);
}, parser_exec_val)

static int parser_exec_val(struct parser *p, struct lexer *l) {
    struct token t;
    if((t = lexer_next(l)).type == TOK_EOF) {
        return 0;
    }

    switch(t.type) {
        case TOK_INT: {
            int num = 0;
            for(size_t i = 0; i < t.len; i++) {
                num = num * 10 + (t.src[i] - '0');
            }
            assert(num <= 0xffff);
            parser_emit_bc_u8(p, OP_MOV_U16);
            parser_emit_bc_u8(p, parser_new_reg(p));
            parser_emit_bc_u16(p, num);
            break;
        }
        case TOK_OPERATOR: {
            if(t.len == 1 && t.src[0] == '(') {
                parser_exec_expr(p, l);
                t = lexer_next(l);
                if(!(t.len == 1 && t.src[0] == ')'))
                    au_fatal("expected )\n");
            } else {
                au_fatal("unexpected operator %.*s\n", t.len, t.src);
            }
            break;
        }
        case TOK_IDENTIFIER: {
            struct token peek = lexer_peek(l, 0);
            if(peek.type == TOK_OPERATOR && peek.len == 1 && peek.src[0] == '(') {
                lexer_next(l);
                int n_args = 0;
                if(!parser_exec_call_args(p, l, &n_args)) return 0;

                const struct au_bc_var_value *val = 0;
                int execute_self = 0;
                if(p->self_name && t.len == p->self_len && memcmp(p->self_name, t.src, p->self_len) == 0) {
                    execute_self = 1;
                } else {
                    val = au_bc_vars_get(&p->p_data->fn_map, t.src, t.len);
                }
                if(!execute_self && val == NULL) {
                    au_fatal("unknown function '%.*s'\n", t.len, t.src);
                }
                assert(n_args <= AU_MAX_ARGS);
                parser_emit_bc_u8(p, OP_CALL0 + n_args);
                parser_emit_bc_u8(p, parser_new_reg(p));
                const size_t offset = p->bc.len;
                parser_emit_pad8(p);
                parser_emit_pad8(p);
                if(execute_self) {
                    size_t_array_add(&p->self_fill_call, offset);
                } else {
                    parser_replace_bc_u16(p, offset, val->idx);
                }
            } else {
                const struct au_bc_var_value *val = au_bc_vars_get(&p->vars, t.src, t.len);
                if(val == NULL) {
                    au_fatal("unknown variable '%.*s'\n", t.len, t.src);
                }
                parser_emit_bc_u8(p, OP_MOV_LOCAL_REG);
                parser_emit_bc_u8(p, val->idx);
                parser_emit_bc_u8(p, parser_new_reg(p));
                parser_emit_pad8(p);
            }
            break;
        }
        case TOK_STRING: {
            // Perform string escaping
            char *formatted_string = 0;
            size_t formatted_string_len = 0;
            int in_escape = 0;
            for(size_t i = 0; i < t.len; i++) {
                if(t.src[i] == '\\' && !in_escape) {
                    in_escape = 1;
                    continue;
                }
                if(in_escape) {
                    if(formatted_string == 0) {
                        formatted_string = malloc(t.len);
                        formatted_string_len = i - 1;
                        memcpy(formatted_string, t.src, i - 1);
                    }
                    switch(t.src[i]) {
                        case 'n': {
                            formatted_string[formatted_string_len++] = '\n';
                            break;
                        }
                    }
                    in_escape = 0;
                } else if(formatted_string != 0) {
                    formatted_string[formatted_string_len++] = t.src[i];
                }
            }

            int idx = -1;
            if(formatted_string) {
                idx = au_program_data_add_data(
                    p->p_data,
                    au_value_string(0),
                    (uint8_t*)formatted_string,
                    formatted_string_len
                );
                free(formatted_string);
            } else {
                idx = au_program_data_add_data(
                    p->p_data,
                    au_value_string(0),
                    (uint8_t*)t.src,
                    t.len
                );
            }
            parser_emit_bc_u8(p, OP_LOAD_CONST);
            parser_emit_bc_u8(p, idx);
            parser_emit_bc_u8(p, parser_new_reg(p));
            parser_emit_pad8(p);
            break;
        }
        default: {
            au_fatal("unexpected token %.*s\n", t.len, t.src);
            break;
        }
    }

    return 1;
}

int au_parse(char *src, size_t len, struct au_program *program) {
    struct au_program_data p_data;
    au_program_data_init(&p_data);

    struct lexer l;
    lexer_init(&l, src, len);

    struct parser p;
    parser_init(&p, &p_data);
    if(!parser_exec(&p, &l)) {
        lexer_del(&l);
        parser_del(&p);
        au_program_data_del(&p_data);
        return 0;
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
    return 1;
}
