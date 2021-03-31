// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "expr.h"
#include "bc.h"
#include "def.h"
#include "regs.h"
#include "resolve.h"
#include "strtod.h"

// ** Utility functions **

static int au_parser_exec_call_args(struct au_parser *p,
                                    struct au_lexer *l,
                                    struct owned_reg_state_array *params) {
    {
        const struct au_token t = au_lexer_peek(l, 0);
        if (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ')') {
            au_lexer_next(l);
            return 1;
        }
        if (!au_parser_exec_expr(p, l))
            return 0;
        const struct owned_reg_state state =
            au_parser_pop_reg_take_ownership(p);
        owned_reg_state_array_add(params, state);
    }
    while (1) {
        const struct au_token t = au_lexer_next(l);
        if (t.type == AU_TOK_EOF ||
            (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ')')) {
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                   t.src[0] == ',') {
            if (!au_parser_exec_expr(p, l))
                return 0;
            const struct owned_reg_state state =
                au_parser_pop_reg_take_ownership(p);
            owned_reg_state_array_add(params, state);
            continue;
        } else {
            EXPECT_TOKEN(0, t, "',' or ')'");
        }
    }
    return 1;
}

// ** Main function **

int au_parser_exec_expr(struct au_parser *p, struct au_lexer *l) {
    return au_parser_exec_assign(p, l);
}

// ** Binary operations

int au_parser_exec_assign(struct au_parser *p, struct au_lexer *l) {
    const struct au_token t = au_lexer_peek(l, 0);
    if (t.type == AU_TOK_IDENTIFIER || t.type == AU_TOK_AT_IDENTIFIER) {
        const struct au_token op = au_lexer_peek(l, 1);
        if (is_assign_tok(op)) {
            au_lexer_next(l);
            au_lexer_next(l);

            if (!au_parser_exec_expr(p, l))
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
                    EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
                    au_parser_emit_bc_u8(p, AU_OP_CLASS_GET_INNER);
                    au_parser_emit_bc_u8(p, reg);
                    au_parser_emit_bc_u16(p, *value);
                    switch (op.src[0]) {
#define BIN_OP_ASG(OP, OPCODE)                                            \
    case OP: {                                                            \
        au_parser_emit_bc_u8(p, OPCODE);                                  \
        break;                                                            \
    }
                        BIN_OP_ASG('*', AU_OP_MUL)
                        BIN_OP_ASG('/', AU_OP_DIV)
                        BIN_OP_ASG('+', AU_OP_ADD)
                        BIN_OP_ASG('-', AU_OP_SUB)
                        BIN_OP_ASG('%', AU_OP_MOD)
#undef BIN_OP_ASG
                    }
                    if (!au_parser_emit_bc_binary_expr(p))
                        return 0;
                }

                au_parser_emit_bc_u8(p, AU_OP_CLASS_SET_INNER);
                au_parser_emit_bc_u8(p, au_parser_last_reg(p));
                au_parser_emit_bc_u16(p, *value);

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
                const uint8_t modifier_reg = au_parser_last_reg(p);
                uint8_t result_reg;

                if (local < p->local_to_reg.len &&
                    p->local_to_reg.data[local] != CACHED_REG_NONE) {
                    result_reg = p->local_to_reg.data[local];
                } else {
                    EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
                    for (size_t i = p->local_to_reg.len;
                         i < (size_t)local + 1; i++) {
                        reg_array_add(&p->local_to_reg, CACHED_REG_NONE);
                    }
                    p->local_to_reg.data[local] = result_reg;
                }

                au_parser_emit_bc_u8(p, AU_OP_MOV_LOCAL_REG);
                au_parser_emit_bc_u8(p, result_reg);
                au_parser_emit_bc_u16(p, local);

                if (op.src[0] == '*')
                    au_parser_emit_bc_u8(p, AU_OP_MUL);
                else if (op.src[0] == '/')
                    au_parser_emit_bc_u8(p, AU_OP_DIV);
                else if (op.src[0] == '+')
                    au_parser_emit_bc_u8(p, AU_OP_ADD);
                else if (op.src[0] == '-')
                    au_parser_emit_bc_u8(p, AU_OP_SUB);
                else if (op.src[0] == '%')
                    au_parser_emit_bc_u8(p, AU_OP_MOD);
                else
                    au_fatal("unimplemented op '%.*s'\n", (int)op.len,
                             op.src);
                au_parser_emit_bc_u8(p, result_reg);
                au_parser_emit_bc_u8(p, modifier_reg);
                au_parser_emit_bc_u8(p, result_reg);

                au_parser_emit_bc_u8(p, AU_OP_MOV_REG_LOCAL);
                au_parser_emit_bc_u8(p, result_reg);
                au_parser_emit_bc_u16(p, local);

                return 1;
            }

            au_parser_emit_bc_u8(p, AU_OP_MOV_REG_LOCAL);

            const uint8_t new_reg = au_parser_last_reg(p);
            au_parser_emit_bc_u8(p, new_reg);

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
                au_parser_emit_bc_u16(p, *old_value);
            } else {
                p->num_locals++;
                EXPECT_BYTECODE(p->num_locals <= AU_MAX_LOCALS);
                au_parser_emit_bc_u16(p, new_value);
            }
            return 1;
        }
    }
    return au_parser_exec_logical(p, l);
}

int au_parser_exec_logical(struct au_parser *p, struct au_lexer *l) {
    if (!au_parser_exec_eq(p, l))
        return 0;

    const size_t len = l->pos;
    const struct au_token t = au_lexer_next(l);
    if (t.type == AU_TOK_OPERATOR && t.len == 2) {
        if (t.src[0] == '&' && t.src[1] == '&') {
            au_parser_flush_cached_regs(p);

            uint8_t reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_swap_top_regs(p);

            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 0);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);

            const size_t left_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_JNIF);
            au_parser_emit_bc_u8(p, au_parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);

            if (!au_parser_exec_expr(p, l))
                return 0;

            au_parser_flush_cached_regs(p);

            const size_t right_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_JNIF);
            au_parser_emit_bc_u8(p, au_parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);

            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 1);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);

            const size_t end_label = p->bc.len;
            au_parser_replace_bc_u16(p, left_replace_idx,
                                     (end_label - left_len) / 4);
            au_parser_replace_bc_u16(p, right_replace_idx,
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
            au_parser_flush_cached_regs(p);

            uint8_t reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_swap_top_regs(p);

            const size_t left_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_JIF);
            au_parser_emit_bc_u8(p, au_parser_pop_reg(p));
            const size_t left_replace_idx = p->bc.len;
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);

            if (!au_parser_exec_expr(p, l))
                return 0;
            au_parser_flush_cached_regs(p);

            const size_t right_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_JIF);
            au_parser_emit_bc_u8(p, au_parser_pop_reg(p));
            const size_t right_replace_idx = p->bc.len;
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);

            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 0);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);
            const size_t false_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_JREL);
            au_parser_emit_pad8(p);
            const size_t false_replace_idx = p->bc.len;
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);

            const size_t truth_len = p->bc.len;
            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 1);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);

            const size_t end_label = p->bc.len;
            au_parser_replace_bc_u16(p, false_replace_idx,
                                     (end_label - false_len) / 4);
            au_parser_replace_bc_u16(p, left_replace_idx,
                                     (truth_len - left_len) / 4);
            au_parser_replace_bc_u16(p, right_replace_idx,
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
    int FN_NAME(struct au_parser *p, struct au_lexer *l) {                \
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

int au_parser_emit_bc_binary_expr(struct au_parser *p) {
    uint8_t rhs = au_parser_pop_reg(p);
    uint8_t lhs = au_parser_pop_reg(p);
    uint8_t res;
    EXPECT_BYTECODE(au_parser_new_reg(p, &res));

    au_parser_emit_bc_u8(p, lhs);
    au_parser_emit_bc_u8(p, rhs);
    au_parser_emit_bc_u8(p, res);
    return 1;
}

BIN_EXPR(
    au_parser_exec_eq,
    t.len == 2 && t.src[1] == '=' && (t.src[0] == '=' || t.src[0] == '!'),
    {
        if (t.src[0] == '=')
            au_parser_emit_bc_u8(p, AU_OP_EQ);
        else if (t.src[0] == '!')
            au_parser_emit_bc_u8(p, AU_OP_NEQ);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_cmp)

BIN_EXPR(
    au_parser_exec_cmp,
    ((t.src[0] == '<' || t.src[0] == '>') &&
     (t.len == 1 || (t.len == 2 && t.src[1] == '='))),
    {
        if (t.len == 1)
            if (t.src[0] == '<')
                au_parser_emit_bc_u8(p, AU_OP_LT);
            else
                au_parser_emit_bc_u8(p, AU_OP_GT);
        else if (t.src[0] == '<')
            au_parser_emit_bc_u8(p, AU_OP_LEQ);
        else
            au_parser_emit_bc_u8(p, AU_OP_GEQ);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_addsub)

BIN_EXPR(
    au_parser_exec_addsub,
    t.len == 1 && (t.src[0] == '+' || t.src[0] == '-'),
    {
        if (t.src[0] == '+')
            au_parser_emit_bc_u8(p, AU_OP_ADD);
        else if (t.src[0] == '-')
            au_parser_emit_bc_u8(p, AU_OP_SUB);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_muldiv)

BIN_EXPR(
    au_parser_exec_muldiv,
    t.len == 1 && (t.src[0] == '*' || t.src[0] == '/' || t.src[0] == '%'),
    {
        if (t.src[0] == '*')
            au_parser_emit_bc_u8(p, AU_OP_MUL);
        else if (t.src[0] == '/')
            au_parser_emit_bc_u8(p, AU_OP_DIV);
        else if (t.src[0] == '%')
            au_parser_emit_bc_u8(p, AU_OP_MOD);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_bitwise_logic)

BIN_EXPR(
    au_parser_exec_bitwise_logic,
    t.len == 1 && (t.src[0] == '&' || t.src[0] == '|' || t.src[0] == '^'),
    {
        if (t.src[0] == '&')
            au_parser_emit_bc_u8(p, AU_OP_BAND);
        else if (t.src[0] == '|')
            au_parser_emit_bc_u8(p, AU_OP_BOR);
        else if (t.src[0] == '^')
            au_parser_emit_bc_u8(p, AU_OP_BXOR);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_bitwise_shift)

BIN_EXPR(
    au_parser_exec_bitwise_shift,
    t.len == 2 && (t.src[0] == '<' || t.src[0] == '>') &&
        t.src[1] == t.src[0],
    {
        if (t.src[0] == '<')
            au_parser_emit_bc_u8(p, AU_OP_BSHL);
        else if (t.src[0] == '>')
            au_parser_emit_bc_u8(p, AU_OP_BSHR);
        if (!au_parser_emit_bc_binary_expr(p))
            return 0;
    },
    au_parser_exec_unary_expr)

#undef BIN_EXPR

// ** Unary operations **

int au_parser_exec_unary_expr(struct au_parser *p, struct au_lexer *l) {
    struct au_token tok = au_lexer_peek(l, 0);
#define UNARY_EXPR_BODY(OPCODE)                                           \
    do {                                                                  \
        au_lexer_next(l);                                                 \
        if (!au_parser_exec_expr(p, l))                                   \
            return 0;                                                     \
                                                                          \
        const uint8_t reg = au_parser_pop_reg(p);                         \
        au_parser_emit_bc_u8(p, OPCODE);                                  \
        au_parser_emit_bc_u8(p, reg);                                     \
        uint8_t result_reg;                                               \
        EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));               \
        au_parser_emit_bc_u8(p, result_reg);                              \
        au_parser_emit_pad8(p);                                           \
                                                                          \
        return 1;                                                         \
    } while (0)

    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == '!')
        UNARY_EXPR_BODY(AU_OP_NOT);
    else if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
             tok.src[0] == '~')
        UNARY_EXPR_BODY(AU_OP_BNOT);
    else
        return au_parser_exec_index_expr(p, l);

#undef UNARY_EXPR_BODY
}

int au_parser_exec_index_expr(struct au_parser *p, struct au_lexer *l) {
    if (!au_parser_exec_value(p, l))
        return 0;
    uint8_t left_reg = 0;
    while (1) {
        left_reg = au_parser_last_reg(p);
        struct au_token tok = au_lexer_peek(l, 0);
        if (tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
            tok.src[0] == '[') {
            au_lexer_next(l);
            if (!au_parser_exec_expr(p, l))
                return 0;
            const uint8_t idx_reg = au_parser_last_reg(p);

            tok = au_lexer_next(l);
            EXPECT_TOKEN(tok.type == AU_TOK_OPERATOR && tok.len == 1 &&
                             tok.src[0] == ']',
                         tok, "']'");

            tok = au_lexer_peek(l, 0);
            if (is_assign_tok(tok)) {
                au_lexer_next(l);
                if (!au_parser_exec_expr(p, l))
                    return 0;
                const uint8_t right_reg = au_parser_last_reg(p);

                if (!(tok.len == 1 && tok.src[0] == '=')) {
                    uint8_t result_reg;
                    EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));

                    au_parser_emit_bc_u8(p, AU_OP_IDX_GET);
                    au_parser_emit_bc_u8(p, left_reg);
                    au_parser_emit_bc_u8(p, idx_reg);
                    au_parser_emit_bc_u8(p, result_reg);

                    const struct au_token op = tok;
                    if (op.src[0] == '*')
                        au_parser_emit_bc_u8(p, AU_OP_MUL);
                    else if (op.src[0] == '/')
                        au_parser_emit_bc_u8(p, AU_OP_DIV);
                    else if (op.src[0] == '+')
                        au_parser_emit_bc_u8(p, AU_OP_ADD);
                    else if (op.src[0] == '-')
                        au_parser_emit_bc_u8(p, AU_OP_SUB);
                    else if (op.src[0] == '%')
                        au_parser_emit_bc_u8(p, AU_OP_MOD);
                    else
                        au_fatal("unimplemented op '%.*s'\n", (int)op.len,
                                 op.src);
                    au_parser_emit_bc_u8(p, result_reg);
                    au_parser_emit_bc_u8(p, right_reg);
                    au_parser_emit_bc_u8(p, result_reg);

                    au_parser_emit_bc_u8(p, AU_OP_IDX_SET);
                    au_parser_emit_bc_u8(p, left_reg);
                    au_parser_emit_bc_u8(p, idx_reg);
                    au_parser_emit_bc_u8(p, result_reg);

                    // The register stack is:
                    // ... [right reg (-2)] [result reg (-1)]
                    // This operation transforms the stack to this:
                    // ... [result reg (-1)]
                    au_parser_swap_top_regs(p);
                    au_parser_pop_reg(p);
                } else {
                    au_parser_emit_bc_u8(p, AU_OP_IDX_SET);
                    au_parser_emit_bc_u8(p, left_reg);
                    au_parser_emit_bc_u8(p, idx_reg);
                    au_parser_emit_bc_u8(p, right_reg);
                }

                // Right now, the used register stack is:
                // ... [array reg (-3)] [idx reg (-2)] [result reg (-1)]
                // Remove array and idx regs because they aren't used,
                // leaving us with:
                // ... [result reg(-1)]
                au_parser_set_reg_unused(p, p->rstack[p->rstack_len - 3]);
                au_parser_set_reg_unused(p, p->rstack[p->rstack_len - 2]);
                p->rstack[p->rstack_len - 3] =
                    p->rstack[p->rstack_len - 1];
                p->rstack_len -= 2;
                break;
            } else {
                uint8_t result_reg;
                EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
                au_parser_emit_bc_u8(p, AU_OP_IDX_GET);
                au_parser_emit_bc_u8(p, left_reg);
                au_parser_emit_bc_u8(p, idx_reg);
                au_parser_emit_bc_u8(p, result_reg);
                // ... [array reg (-3)] [idx reg (-2)] [value reg (-1)]
                // We also want to remove array/idx regs here
                au_parser_set_reg_unused(p, p->rstack[p->rstack_len - 3]);
                au_parser_set_reg_unused(p, p->rstack[p->rstack_len - 2]);
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
                struct owned_reg_state_array params = {0};
                if (!au_parser_exec_call_args(p, l, &params)) {
                    owned_reg_state_array_del(p, &params);
                    return 0;
                }
                EXPECT_BYTECODE(params.len < 256);

                au_parser_emit_bc_u8(p, AU_OP_CALL_FUNC_VALUE);
                au_parser_emit_bc_u8(p, au_parser_pop_reg(p));
                au_parser_emit_bc_u8(p, params.len);
                uint8_t return_reg;
                EXPECT_BYTECODE(au_parser_new_reg(p, &return_reg));
                au_parser_emit_bc_u8(p, return_reg);

                for (size_t i = 0; i < params.len;) {
#define EMIT_ARG()                                                        \
    do {                                                                  \
        uint8_t reg = i < params.len ? params.data[i].reg : 0;            \
        i++;                                                              \
        au_parser_emit_bc_u8(p, reg);                                     \
    } while (0)
                    au_parser_emit_bc_u8(p, AU_OP_PUSH_ARG);
                    EMIT_ARG();
                    EMIT_ARG();
                    EMIT_ARG();
#undef EMIT_ARG
                }

                owned_reg_state_array_del(p, &params);
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
                    if (!au_parser_exec_call(p, l, module_tok, id_tok, 1))
                        return 0;
                } else {
                    size_t func_idx = 0;
                    int execute_self = 0;

                    if (!au_parser_resolve_fn(p, module_tok, id_tok, 1,
                                              &func_idx, &execute_self))
                        return 0;

                    EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

                    au_parser_emit_bc_u8(p, AU_OP_LOAD_FUNC);
                    uint8_t func_reg;
                    EXPECT_BYTECODE(au_parser_new_reg(p, &func_reg));
                    au_parser_emit_bc_u8(p, func_reg);
                    au_parser_emit_bc_u16(p, func_idx);

                    au_parser_emit_bc_u8(p, AU_OP_BIND_ARG_TO_FUNC);
                    au_parser_emit_bc_u8(p, func_reg);
                    au_parser_emit_bc_u8(p, left_reg);
                    au_parser_emit_pad8(p);
                }
            }
        } else {
            break;
        }
    }

    return 1;
}

// ** Function calls **

int au_parser_exec_call(struct au_parser *p, struct au_lexer *l,
                        struct au_token module_tok, struct au_token id_tok,
                        int has_self_argument) {
    struct owned_reg_state_array params = {0};

    if (has_self_argument) {
        const struct owned_reg_state state =
            au_parser_pop_reg_take_ownership(p);
        owned_reg_state_array_add(&params, state);
    }

    if (!au_parser_exec_call_args(p, l, &params))
        goto fail;

    size_t func_idx = 0;
    int execute_self = 0;

    if (!au_parser_resolve_fn(p, module_tok, id_tok, params.len, &func_idx,
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

    uint8_t result_reg;
    EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
    au_parser_emit_bc_u8(p, AU_OP_CALL);
    au_parser_emit_bc_u8(p, result_reg);
    call_fn_offset = p->bc.len;
    au_parser_emit_pad8(p);
    au_parser_emit_pad8(p);

    for (size_t i = 0; i < params.len;) {
#define EMIT_ARG()                                                        \
    do {                                                                  \
        uint8_t reg = i < params.len ? params.data[i].reg : 0;            \
        i++;                                                              \
        au_parser_emit_bc_u8(p, reg);                                     \
    } while (0)
        au_parser_emit_bc_u8(p, AU_OP_PUSH_ARG);
        EMIT_ARG();
        EMIT_ARG();
        EMIT_ARG();
#undef EMIT_ARG
    }

    if (execute_self) {
        size_t_array_add(&p->self_fill_call, call_fn_offset);
    } else {
        au_parser_replace_bc_u16(p, call_fn_offset, func_idx);
    }

    owned_reg_state_array_del(p, &params);
    return 1;

fail:
    owned_reg_state_array_del(p, &params);
    return 0;
}

static inline int hex_value(char ch) {
    if ('0' <= ch && ch <= '9')
        return ch - '0';
    if ('a' <= ch && ch <= 'f')
        return ch - 'a' + 10;
    if ('A' <= ch && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

// ** Values **

int au_parser_exec_value(struct au_parser *p, struct au_lexer *l) {
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
        EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));

        if (num <= 0x8000) {
            au_parser_emit_bc_u8(p, AU_OP_MOV_U16);
            au_parser_emit_bc_u8(p, result_reg);
            au_parser_emit_bc_u16(p, num);
        } else {
            int idx = au_program_data_add_data(p->p_data,
                                               au_value_int(num), 0, 0);
            au_parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
            au_parser_emit_bc_u8(p, result_reg);
            au_parser_emit_bc_u16(p, idx);
        }
        break;
    }
    case AU_TOK_DOUBLE: {
        double value = au_parser_strtod(t.src, t.len);

        uint8_t result_reg;
        EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));

        int idx = au_program_data_add_data(p->p_data,
                                           au_value_double(value), 0, 0);
        au_parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
        au_parser_emit_bc_u8(p, result_reg);
        au_parser_emit_bc_u16(p, idx);
        break;
    }
    case AU_TOK_OPERATOR: {
        if (t.len == 1 && t.src[0] == '(') {
            t = au_lexer_peek(l, 0);
            if (t.type == AU_TOK_OPERATOR && t.len == 1 &&
                t.src[0] == '-') {
                au_lexer_next(l);
                if (!au_parser_exec_expr(p, l))
                    return 0;

                const uint8_t reg = au_parser_pop_reg(p);
                au_parser_emit_bc_u8(p, AU_OP_NEG);
                au_parser_emit_bc_u8(p, reg);
                uint8_t result_reg;
                EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
                au_parser_emit_bc_u8(p, result_reg);
                au_parser_emit_pad8(p);
            } else {
                if (!au_parser_exec_expr(p, l))
                    return 0;
            }
            t = au_lexer_next(l);
            EXPECT_TOKEN(t.len == 1 && t.src[0] == ')', t, "')'");
        } else if (t.len == 1 && t.src[0] == '[') {
            return au_parser_exec_array_or_tuple(p, l, 0);
        } else if (t.len == 2 && t.src[0] == '#' && t.src[1] == '[') {
            return au_parser_exec_array_or_tuple(p, l, 1);
        } else if (t.len == 1 && t.src[0] == '{') {
            return au_parser_exec_dict(p, l);
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

            if (!au_parser_resolve_fn(p, module_tok, t, 1, &func_idx,
                                      &execute_self))
                return 0;

            EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

            au_parser_emit_bc_u8(p, AU_OP_LOAD_FUNC);
            uint8_t func_reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &func_reg));
            au_parser_emit_bc_u8(p, func_reg);
            au_parser_emit_bc_u16(p, func_idx);
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
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 1);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "false")) {
            uint8_t reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_emit_bc_u8(p, AU_OP_MOV_BOOL);
            au_parser_emit_bc_u8(p, 0);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);
            return 1;
        } else if (token_keyword_cmp(&t, "nil")) {
            uint8_t reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_emit_bc_u8(p, AU_OP_LOAD_NIL);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);
            return 1;
        }

        struct au_token peek = au_lexer_peek(l, 0);

        if (token_keyword_cmp(&t, "new")) {
            if (peek.type == AU_TOK_OPERATOR && peek.len == 1 &&
                peek.src[0] == '(') {
                // This is treated as a call to the "new" function
            } else {
                return au_parser_exec_new_expr(p, l);
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
            if (!au_parser_exec_call(p, l, module_tok, t, 0))
                return 0;
        } else if (module_tok.type != AU_TOK_EOF) {
            struct au_imported_module *module =
                au_parser_resolve_module(p, module_tok, 0);
            if (module == 0) {
                p->res = (struct au_parser_result){
                    .type = AU_PARSER_RES_UNKNOWN_MODULE,
                    .data.unknown_id.name_token = module_tok,
                };
                return 0;
            }

            au_hm_var_value_t *const_val = au_hm_vars_add(
                &module->const_map, t.src, t.len, p->p_data->data_val.len);
            size_t idx;
            if (const_val == 0) {
                idx = p->p_data->data_val.len;
                au_program_data_add_data(p->p_data, au_value_none(), 0, 0);
            } else {
                idx = *const_val;
            }

            uint8_t reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
            au_parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
            au_parser_emit_bc_u8(p, reg);
            au_parser_emit_bc_u16(p, idx);
        } else {
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
                    EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
                    au_parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
                    au_parser_emit_bc_u8(p, reg);
                    au_parser_emit_bc_u16(p, *const_val);
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
                    EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
                    au_parser_emit_bc_u8(p, AU_OP_MOV_LOCAL_REG);
                    au_parser_emit_bc_u8(p, reg);
                    au_parser_emit_bc_u16(p, local);
                    reg_array_set(&p->local_to_reg, local, reg);
                    AU_BA_SET_BIT(p->pinned_regs, reg);
                } else {
                    au_parser_push_reg(p, cached_reg);
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
                default: {
                    formatted_string[formatted_string_len++] = t.src[i];
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
            if (utf8_codepoint(t.src, &t.src[t.len], &codepoint) == 0)
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
        EXPECT_BYTECODE(au_parser_new_reg(p, &reg));

        au_parser_emit_bc_u8(p, AU_OP_LOAD_CONST);
        au_parser_emit_bc_u8(p, reg);
        au_parser_emit_bc_u16(p, idx);

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
        EXPECT_BYTECODE(au_parser_new_reg(p, &reg));
        au_parser_emit_bc_u8(p, AU_OP_CLASS_GET_INNER);
        au_parser_emit_bc_u8(p, reg);
        const au_hm_var_value_t *value =
            au_hm_vars_get(&interface->map, &t.src[1], t.len - 1);
        if (value == 0) {
            p->res = (struct au_parser_result){
                .type = AU_PARSER_RES_UNKNOWN_VAR,
                .data.unknown_id.name_token = t,
            };
            return 0;
        }
        au_parser_emit_bc_u16(p, *value);
        break;
    }
    default: {
        EXPECT_TOKEN(0, t, "value");
    }
    }

    return 1;
}

int au_parser_exec_array_or_tuple(struct au_parser *p, struct au_lexer *l,
                                  int is_tuple) {
    uint8_t array_reg;
    EXPECT_BYTECODE(au_parser_new_reg(p, &array_reg));
    if (is_tuple) {
        au_parser_emit_bc_u8(p, AU_OP_TUPLE_NEW);
        au_parser_emit_bc_u8(p, array_reg);
    } else {
        au_parser_emit_bc_u8(p, AU_OP_ARRAY_NEW);
        au_parser_emit_bc_u8(p, array_reg);
    }
    const size_t cap_offset = p->bc.len;
    au_parser_emit_bc_u16(p, 0);

    struct au_token tok = au_lexer_peek(l, 0);
    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == ']') {
        au_lexer_next(l);
        return 1;
    }

    uint16_t capacity = 1;
    if (!au_parser_exec_expr(p, l))
        return 0;
    const uint8_t value_reg = au_parser_pop_reg(p);
    if (is_tuple) {
        au_parser_emit_bc_u8(p, AU_OP_IDX_SET_STATIC);
        au_parser_emit_bc_u8(p, array_reg);
        au_parser_emit_bc_u8(p, 0);
        au_parser_emit_bc_u8(p, value_reg);
    } else {
        au_parser_emit_bc_u8(p, AU_OP_ARRAY_PUSH);
        au_parser_emit_bc_u8(p, array_reg);
        au_parser_emit_bc_u8(p, value_reg);
        au_parser_emit_pad8(p);
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

            if (!au_parser_exec_expr(p, l))
                return 0;
            const uint8_t value_reg = au_parser_pop_reg(p);

            if (is_tuple) {
                au_parser_emit_bc_u8(p, AU_OP_IDX_SET_STATIC);
                au_parser_emit_bc_u8(p, array_reg);
                au_parser_emit_bc_u8(p, capacity);
                au_parser_emit_bc_u8(p, value_reg);
                capacity++;
                EXPECT_BYTECODE(capacity < AU_MAX_STATIC_IDX);
            } else {
                au_parser_emit_bc_u8(p, AU_OP_ARRAY_PUSH);
                au_parser_emit_bc_u8(p, array_reg);
                au_parser_emit_bc_u8(p, value_reg);
                au_parser_emit_pad8(p);
                if (capacity < (AU_MAX_ARRAY - 1)) {
                    capacity++;
                }
            }
        } else {
            EXPECT_TOKEN(0, tok, "',' or ']'");
        }
    }

    au_parser_replace_bc_u16(p, cap_offset, capacity);
    return 1;
}

int au_parser_exec_dict(struct au_parser *p, struct au_lexer *l) {
    uint8_t dict_reg;
    EXPECT_BYTECODE(au_parser_new_reg(p, &dict_reg));

    au_parser_emit_bc_u8(p, AU_OP_DICT_NEW);
    au_parser_emit_bc_u8(p, dict_reg);
    au_parser_emit_bc_u16(p, 0);

    struct au_token tok = au_lexer_next(l);
    if (tok.type == AU_TOK_OPERATOR && tok.len == 1 && tok.src[0] == '}') {
        return 1;
    } else {
        abort(); // TODO
    }
}

struct new_initializer {
    struct owned_reg_state reg_state;
    uint16_t local;
};

AU_ARRAY_COPY(struct new_initializer, new_initializer_array, 1)

int au_parser_exec_new_expr(struct au_parser *p, struct au_lexer *l) {
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

                if (!au_parser_exec_expr(p, l))
                    return 0;

                const struct owned_reg_state right_reg =
                    au_parser_pop_reg_take_ownership(p);
                new_initializer_array_add(&array,
                                          (struct new_initializer){
                                              .reg_state = right_reg,
                                              .local = *key_value,
                                          });

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
            EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
            au_parser_emit_bc_u8(p, AU_OP_CLASS_NEW);
            au_parser_emit_bc_u8(p, result_reg);
            au_parser_emit_bc_u16(p, class_idx);
        } else {
            uint8_t result_reg;
            EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
            au_parser_emit_bc_u8(p, AU_OP_CLASS_NEW_INITIALZIED);
            au_parser_emit_bc_u8(p, result_reg);
            au_parser_emit_bc_u16(p, class_idx);

            for (size_t i = 0; i < array.len; i++) {
                struct new_initializer init = array.data[i];
                au_parser_emit_bc_u8(p, AU_OP_CLASS_SET_INNER);
                au_parser_emit_bc_u8(p, init.reg_state.reg);
                au_parser_emit_bc_u16(p, init.local);
                au_parser_set_reg_unused(p, init.reg_state.reg);
            }

            for (size_t i = 0; i < array.len; i++) {
                au_parser_del_reg_from_ownership(p,
                                                 array.data[i].reg_state);
            }
            au_data_free(array.data);

            au_parser_emit_bc_u8(p, AU_OP_NOP);
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);
            au_parser_emit_pad8(p);
        }
    } else {
        uint8_t result_reg;
        EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
        au_parser_emit_bc_u8(p, AU_OP_CLASS_NEW);
        au_parser_emit_bc_u8(p, result_reg);
        au_parser_emit_bc_u16(p, class_idx);
    }

    return 1;
}
