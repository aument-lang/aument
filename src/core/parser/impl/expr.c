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

#include "platform/dconv.h"

#include <stdio.h>

// ** Utility functions **

enum calling_type {
    CA_NORMAL = 0,
    CA_CATCH_ERROR = 1,
    CA_IGNORE_ERROR = 2,
};

static int au_parser_exec_call_args(struct au_parser *p,
                                    struct au_lexer *l,
                                    struct owned_reg_state_array *params,
                                    enum calling_type *calling_type_out) {
    *calling_type_out = CA_NORMAL;
    {
        const struct au_token t = au_lexer_peek(l, 0);
        if (t.type == AU_TOK_OPERATOR && t.len == 1 && t.src[0] == ')') {
            au_lexer_next(l);
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
                   t.src[0] == ')' && t.src[1] == '?') {
            au_lexer_next(l);
            *calling_type_out = CA_CATCH_ERROR;
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
                   t.src[0] == ')' && t.src[1] == '!') {
            au_lexer_next(l);
            *calling_type_out = CA_IGNORE_ERROR;
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
        } else if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
                   t.src[0] == ')' && t.src[1] == '?') {
            *calling_type_out = CA_CATCH_ERROR;
            return 1;
        } else if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
                   t.src[0] == ')' && t.src[1] == '!') {
            *calling_type_out = CA_IGNORE_ERROR;
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
            EXPECT_TOKEN(0, t, "call arguments");
        }
    }
    return 1;
}

// ** Main function **

int au_parser_exec_expr(struct au_parser *p, struct au_lexer *l) {
    return au_parser_exec_logical_expr(p, l);
}

// ** Binary operations **

int au_parser_exec_logical_expr(struct au_parser *p, struct au_lexer *l) {
    if (!au_parser_exec_binary_expr(p, l))
        return 0;

    const struct au_token t = au_lexer_peek(l, 0);
    if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
        (t.src[0] == '&' && t.src[1] == '&')) {
        au_lexer_next(l);

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
    } else if (t.type == AU_TOK_OPERATOR && t.len == 2 &&
               (t.src[0] == '|' && t.src[1] == '|')) {
        au_lexer_next(l);

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
    return 1;
}

enum operator_assoc {
    ASSOC_LEFT,
    ASSOC_RIGHT,
};

struct operator_info {
    int precedence;
    enum operator_assoc assoc;
    char src[4];
    enum au_opcode op;
};

AU_ARRAY_COPY(const struct operator_info *, operator_info_ptr_array, 1)

static const struct operator_info *
get_operator_info(const struct operator_info *infos, size_t infos_len,
                  struct au_token tok) {
    if (tok.type != AU_TOK_OPERATOR)
        return 0;
    for (size_t i = 0; i < infos_len; i++) {
        const size_t len = strlen(infos[i].src);
        if (tok.len == len && memcmp(tok.src, infos[i].src, len) == 0) {
            return &infos[i];
        }
    }
    return 0;
}

static int operator_info_generate(const struct operator_info *info,
                                  struct au_parser *p) {
    au_parser_emit_bc_u8(p, info->op);

    const uint8_t right = au_parser_pop_reg(p);
    const uint8_t left = au_parser_pop_reg(p);

    au_parser_emit_bc_u8(p, left);
    au_parser_emit_bc_u8(p, right);

    uint8_t result_reg;
    EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
    au_parser_emit_bc_u8(p, result_reg);
    return 1;
}

int au_parser_exec_binary_expr(struct au_parser *p, struct au_lexer *l) {
    static const struct operator_info infos[] = {
        // clang-format off
        { .src = ">>", .precedence = 0,  .assoc = ASSOC_LEFT,  .op = AU_OP_BSHR },
        { .src = "<<", .precedence = 0,  .assoc = ASSOC_LEFT,  .op = AU_OP_BSHL },
        // Bitwise ops
        { .src = "^",  .precedence = 10, .assoc = ASSOC_LEFT,  .op = AU_OP_BXOR },
        { .src = "&",  .precedence = 10, .assoc = ASSOC_LEFT,  .op = AU_OP_BAND },
        { .src = "|",  .precedence = 10, .assoc = ASSOC_LEFT,  .op = AU_OP_BOR  },
        // Add/sub
        { .src = "-",  .precedence = 20, .assoc = ASSOC_RIGHT, .op = AU_OP_SUB  },
        { .src = "+",  .precedence = 20, .assoc = ASSOC_LEFT,  .op = AU_OP_ADD  },
        // Multiply/div
        { .src = "/",  .precedence = 30, .assoc = ASSOC_LEFT,  .op = AU_OP_DIV  },
        { .src = "*",  .precedence = 30, .assoc = ASSOC_LEFT,  .op = AU_OP_MUL  },
        // Comparison ops
        { .src = ">",  .precedence = 40, .assoc = ASSOC_LEFT,  .op = AU_OP_GT   },
        { .src = ">=", .precedence = 40, .assoc = ASSOC_LEFT,  .op = AU_OP_GEQ  },
        { .src = "<",  .precedence = 40, .assoc = ASSOC_LEFT,  .op = AU_OP_LT   },
        { .src = "<=", .precedence = 40, .assoc = ASSOC_LEFT,  .op = AU_OP_LEQ  },
        { .src = "==", .precedence = 40, .assoc = ASSOC_LEFT,  .op = AU_OP_EQ   },
        // clang-format on
    };
    const size_t infos_len = sizeof(infos) / sizeof(infos[0]);

    struct operator_info_ptr_array operator_stack = {0};
    struct au_token tok = {.type = AU_TOK_EOF};

    int balance = 0;

    while (1) {
        tok = au_lexer_peek(l, 0);
        const struct operator_info *cur_op;
        if ((cur_op = get_operator_info(infos, infos_len, tok)) != 0) {
            au_lexer_next(l);
            while (operator_stack.len > 0) {
                const struct operator_info *top_op =
                    operator_stack.data[operator_stack.len - 1];
                if ((cur_op->assoc == ASSOC_LEFT &&
                     cur_op->precedence <= top_op->precedence) ||
                    (cur_op->assoc == ASSOC_RIGHT &&
                     cur_op->precedence < top_op->precedence)) {
                    if (!operator_info_generate(top_op, p))
                        return 0;
                    operator_stack.len--;
                    continue;
                }
                break;
            }
            operator_info_ptr_array_add(&operator_stack, cur_op);
            balance--;
        } else {
            if (balance == 1)
                break;
            if (!au_parser_exec_unary_expr(p, l))
                return 0;
            balance++;
        }
    }

    while (operator_stack.len > 0) {
        operator_info_generate(operator_stack.data[--operator_stack.len],
                               p);
    }

    au_data_free(operator_stack.data);
    return 1;
}

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
                enum calling_type calling_type;
                if (!au_parser_exec_call_args(p, l, &params,
                                              &calling_type)) {
                    owned_reg_state_array_del(p, &params);
                    return 0;
                }
                EXPECT_BYTECODE(params.len < 256);

                switch (calling_type) {
                case CA_NORMAL: {
                    au_parser_emit_bc_u8(p, AU_OP_CALL_FUNC_VALUE);
                    break;
                }
                case CA_CATCH_ERROR: {
                    p->self_flags |= AU_FN_FLAG_MAY_FAIL;
                    au_parser_emit_bc_u8(p, AU_OP_CALL_FUNC_VALUE_CATCH);
                    break;
                }
                case CA_IGNORE_ERROR: {
                    p->self_flags |= AU_FN_FLAG_MAY_FAIL;
                    au_parser_emit_bc_u8(p, AU_OP_CALL_FUNC_VALUE);
                    break;
                }
                }
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

                    int create_new_function = 0;
                    if (!au_parser_resolve_fn(p, module_tok, id_tok, 1,
                                              &func_idx, &execute_self,
                                              &create_new_function))
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
    // Parse parameters
    struct owned_reg_state_array params = {0};
    if (has_self_argument) {
        const struct owned_reg_state state =
            au_parser_pop_reg_take_ownership(p);
        owned_reg_state_array_add(&params, state);
    }
    enum calling_type calling_type;
    if (!au_parser_exec_call_args(p, l, &params, &calling_type))
        goto fail;

    // Resolve function being called
    size_t func_idx = 0;
    int execute_self = 0;
    int create_new_function = 0;
    if (!au_parser_resolve_fn(p, module_tok, id_tok, params.len, &func_idx,
                              &execute_self, &create_new_function))
        goto fail;
    EXPECT_BYTECODE(func_idx < AU_MAX_FUNC_ID);

    // Check arguments and calling type
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
        if ((p->self_flags & AU_FN_FLAG_MAY_FAIL) != 0 &&
            calling_type == CA_NORMAL) {
            printf("CA_NORMAL while calling may fail function (self)\n");
            abort(); // TODO
        }
    } else {
        struct au_fn *fn = &p->p_data->fns.data[func_idx];
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
        if (create_new_function) {
            if (calling_type != CA_NORMAL &&
                (fn->flags & AU_FN_FLAG_MAY_FAIL) == 0) {
                fn->flags |= AU_FN_FLAG_MAY_FAIL;
            }
        }
        if ((fn->flags & AU_FN_FLAG_MAY_FAIL) != 0 &&
            calling_type == CA_NORMAL) {
            printf("CA_NORMAL while calling may fail function\n");
            abort(); // TODO
        }
    }

    // Generate code
    switch (calling_type) {
    case CA_NORMAL: {
        au_parser_emit_bc_u8(p, AU_OP_CALL);
        break;
    }
    case CA_CATCH_ERROR: {
        p->self_flags |= AU_FN_FLAG_MAY_FAIL;
        au_parser_emit_bc_u8(p, AU_OP_CALL_CATCH);
        break;
    }
    case CA_IGNORE_ERROR: {
        p->self_flags |= AU_FN_FLAG_MAY_FAIL;
        au_parser_emit_bc_u8(p, AU_OP_CALL);
        break;
    }
    }

    uint8_t result_reg;
    EXPECT_BYTECODE(au_parser_new_reg(p, &result_reg));
    au_parser_emit_bc_u8(p, result_reg);

    size_t call_fn_offset = p->bc.len;
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
        double value;
        au_dconv_strtod_s(t.src, t.len, &value);

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

            int create_new_function = 0;
            if (!au_parser_resolve_fn(p, module_tok, t, 1, &func_idx,
                                      &execute_self, &create_new_function))
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

        // Identifier:

        struct au_class_interface *interface =
            au_class_interface_ptr_array_at(&p->p_data->classes,
                                            class_idx);
        if (interface == 0) {
            abort(); // TODO
        }

        // NewInits:

#define RAISE_UNKNOWN_VAR(name_tok)                                       \
    do {                                                                  \
        p->res = (struct au_parser_result){                               \
            .type = AU_PARSER_RES_UNKNOWN_VAR,                            \
            .data.unknown_id.name_token = name_tok,                       \
        };                                                                \
        return 0;                                                         \
    } while (0)

        PARSE_COMMA_LIST('}', "new initializer", {
            const struct au_token name_tok = au_lexer_next(l);
            EXPECT_BYTECODE(name_tok.type == AU_TOK_IDENTIFIER);

            const au_hm_var_value_t *key_value = au_hm_vars_get(
                &interface->map, name_tok.src, name_tok.len);
            if (key_value == 0)
                RAISE_UNKNOWN_VAR(name_tok);

            const struct au_token separator_tok = au_lexer_next(l);
            EXPECT_TOKEN(separator_tok.type == AU_TOK_OPERATOR &&
                             separator_tok.len == 1 &&
                             separator_tok.src[0] == ':',
                         separator_tok, "':'");

            if (!au_parser_exec_expr(p, l))
                return 0;
            const struct owned_reg_state right_reg =
                au_parser_pop_reg_take_ownership(p);

            struct new_initializer new_init;
            new_init.reg_state = right_reg;
            new_init.local = *key_value;
            new_initializer_array_add(&array, new_init);
        });

#undef RAISE_UNKNOWN_VAR

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

int au_parser_exec_fixed_element_name(struct au_parser *p,
                                      struct au_lexer *l,
                                      struct au_token *module_tok_out,
                                      struct au_token *id_tok_out) {
    struct au_token module_tok = (struct au_token){.type = AU_TOK_EOF};
    struct au_token id_tok = au_lexer_next(l);
    if (id_tok.type == AU_TOK_OPERATOR && id_tok.len == 2 &&
        id_tok.src[0] == ':' && id_tok.src[1] == ':') {
        module_tok = id_tok;
        id_tok = au_lexer_next(l);
        EXPECT_TOKEN(id_tok.type == AU_TOK_IDENTIFIER, id_tok,
                     "identifier");
        id_tok = au_lexer_next(l);
    }
    *module_tok_out = module_tok;
    *id_tok_out = id_tok;
    return 1;
}
