// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#include "lyra/block.h"
#include "lyra/comp.h"
#include "lyra/context.h"
#include "lyra/function.h"
#include "lyra/insn.h"
#include "lyra/passes.h"

#include "c_comp.h"
#include "core/fn.h"
#include "core/rt/malloc.h"

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

#ifdef AU_TEST_RT_CODE
char *TEST_RT_CODE;
size_t TEST_RT_CODE_LEN;
#endif

static inline void au_c_comp_state_append_nt(struct au_c_comp_state *state,
                                             const char *s) {
    for (size_t i = 0; s[i]; i++) {
        au_char_array_add(&state->str, s[i]);
    }
}

static void au_c_comp_state_append(struct au_c_comp_state *state,
                                   const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        au_char_array_add(&state->str, s[i]);
    }
}

void au_c_comp_state_del(struct au_c_comp_state *state) {
    au_data_free(state->str.data);
}

struct function_ctx {
    struct lyra_function *lyra_fn;
    struct lyra_block current_block;
};

static void function_ctx_init(struct function_ctx *fctx,
                              struct lyra_function *lyra_fn) {
    fctx->lyra_fn = lyra_fn;
    lyra_block_init(&fctx->current_block);
}

static void function_ctx_finish_block(struct function_ctx *fctx) {
    lyra_function_add_block(fctx->lyra_fn, fctx->current_block);
    lyra_block_init(&fctx->current_block);
}

static void function_ctx_finalize(struct function_ctx *fctx) {
    lyra_function_finalize(fctx->lyra_fn);
    lyra_function_all_blocks(fctx->lyra_fn, lyra_pass_check_multiple_use);
    lyra_function_all_blocks(fctx->lyra_fn, lyra_pass_into_semi_ssa);
    lyra_function_all_blocks(fctx->lyra_fn, lyra_pass_type_inference);
    lyra_function_all_blocks(fctx->lyra_fn, lyra_pass_const_prop);
    lyra_function_all_blocks(fctx->lyra_fn, lyra_pass_purge_dead_code);
    lyra_ctx_gc_run(fctx->lyra_fn->ctx);
}

struct global_ctx {
    size_t main_idx;
};

static struct au_interpreter_result function_to_lyra(
    struct function_ctx *fctx, const struct au_bc_storage *bcs,
    const struct au_program_data *p_data, const struct global_ctx *gctx) {
    (void)gctx;

    for (int i = 0; i < bcs->num_values; i++) {
        lyra_function_add_variable(fctx->lyra_fn, LYRA_VALUE_UNTYPED);
    }

#define bc(x) au_bc_buf_at(&bcs->bc, x)

#define DEF_BC16(VAR, OFFSET)                                             \
    uint16_t VAR;                                                         \
    do {                                                                  \
        assert(pos + OFFSET + 2 <= bcs->bc.len);                          \
        VAR = *((uint16_t *)(&bcs->bc.data[pos + OFFSET]));               \
    } while (0)

#define LOCAL_TO_LYRA_VAR(X) ((X) + bcs->num_registers)

    size_t pos = 0;
    while (pos < bcs->bc.len) {
        uint8_t opcode = bc(pos);
        pos++;

        switch (opcode) {
        // Move instructions
        case AU_OP_MOV_U16: {
            uint8_t reg = bc(pos);
            DEF_BC16(n, 1);

            struct lyra_insn *insn =
                lyra_insn_imm(LYRA_OP_MOV_I32, LYRA_INSN_I32(((int32_t)n)),
                              reg, fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
        case AU_OP_MOV_REG_LOCAL: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);

            struct lyra_insn *insn = lyra_insn_new(
                LYRA_OP_MOV_VAR, reg, LYRA_INSN_REG(0),
                LOCAL_TO_LYRA_VAR(local), fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
        case AU_OP_MOV_LOCAL_REG: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);

            struct lyra_insn *insn =
                lyra_insn_new(LYRA_OP_MOV_VAR, LOCAL_TO_LYRA_VAR(local),
                              LYRA_INSN_REG(0), reg, fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
        case AU_OP_MOV_BOOL: {
            uint8_t n = bc(pos), reg = bc(pos + 1);

            struct lyra_insn *insn = lyra_insn_imm(
                LYRA_OP_MOV_BOOL, LYRA_INSN_I32(((int32_t)n)), reg,
                fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
        case AU_OP_LOAD_NIL: {
            uint8_t reg = bc(pos);

            abort(); // TODO

            pos += 3;
            break;
        }
        // Load/store constants
        case AU_OP_LOAD_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1);

            const struct au_program_data_val *data_val =
                &p_data->data_val.data[c];
            au_value_t value = data_val->real_value;
            switch (au_value_get_type(value)) {
            case AU_VALUE_INT: {
                struct lyra_insn *insn =
                    lyra_insn_imm(LYRA_OP_MOV_I32,
                                  LYRA_INSN_I32(au_value_get_int(value)),
                                  reg, fctx->lyra_fn->ctx);
                lyra_block_add_insn(&fctx->current_block, insn);
                break;
            }
            case AU_VALUE_DOUBLE: {
                struct lyra_insn *insn = lyra_insn_imm(
                    LYRA_OP_MOV_F64,
                    LYRA_INSN_F64(au_value_get_double(value)), reg,
                    fctx->lyra_fn->ctx);
                lyra_block_add_insn(&fctx->current_block, insn);
                break;
            }
            default: {
                abort(); // TODO
                break;
            }
            }

            pos += 3;
            break;
        }
        case AU_OP_SET_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1);

            (void)reg;
            (void)c;
            abort(); // TODO

            pos += 3;
            break;
        }
        // Binary operations
#define BIN_OP(LYRA_OP)                                                   \
    {                                                                     \
        uint8_t lhs = bc(pos);                                            \
        uint8_t rhs = bc(pos + 1);                                        \
        uint8_t res = bc(pos + 2);                                        \
                                                                          \
        struct lyra_insn *insn = lyra_insn_new(                           \
            LYRA_OP, lhs, LYRA_INSN_REG(rhs), res, fctx->lyra_fn->ctx);   \
        lyra_block_add_insn(&fctx->current_block, insn);                  \
                                                                          \
        pos += 3;                                                         \
        break;                                                            \
    }
        case AU_OP_MUL:
            BIN_OP(LYRA_OP_MUL_VAR)
        case AU_OP_DIV:
            BIN_OP(LYRA_OP_DIV_VAR)
        case AU_OP_MOD:
            BIN_OP(LYRA_OP_MOD_I32)
        case AU_OP_ADD:
            BIN_OP(LYRA_OP_ADD_VAR)
        case AU_OP_SUB:
            BIN_OP(LYRA_OP_SUB_VAR)
        case AU_OP_EQ:
            BIN_OP(LYRA_OP_EQ_VAR)
        case AU_OP_NEQ:
            BIN_OP(LYRA_OP_NEQ_VAR)
        case AU_OP_LT:
            BIN_OP(LYRA_OP_LT_VAR)
        case AU_OP_GT:
            BIN_OP(LYRA_OP_GT_VAR)
        case AU_OP_LEQ:
            BIN_OP(LYRA_OP_LEQ_VAR)
        case AU_OP_GEQ:
            BIN_OP(LYRA_OP_GEQ_VAR)
        case AU_OP_BAND:
            BIN_OP(LYRA_OP_BAND_I32)
        case AU_OP_BOR:
            BIN_OP(LYRA_OP_BOR_I32)
        case AU_OP_BSHL:
            BIN_OP(LYRA_OP_BSHL_I32)
        case AU_OP_BSHR:
            BIN_OP(LYRA_OP_BSHR_I32)
#undef BIN_OP
        // Unary instructions
        // Jump instructions
        // Call instructions
        // Return instructions
        case AU_OP_RET_NULL: {
            fctx->current_block.connector.type = LYRA_BLOCK_RET_NULL;
            fctx->current_block.connector.var = 0;
            fctx->current_block.connector.label = 0;
            function_ctx_finish_block(fctx);
            pos += 3;
            break;
        }
        // Other
        case AU_OP_PRINT: {
            uint8_t reg = bc(pos);

            struct lyra_insn_call_args *args =
                lyra_insn_call_args_new_name("au_value_print_wrapper", 1,
                                             fctx->lyra_fn->ctx);
            args->data[0] = (size_t)reg;

            size_t empty_reg = lyra_function_add_variable(
                fctx->lyra_fn, LYRA_VALUE_UNTYPED);
            struct lyra_insn *insn =
                lyra_insn_imm(LYRA_OP_CALL_FLAT, LYRA_INSN_CALL_ARGS(args),
                              empty_reg, fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
        default:
            abort(); // TODO
        }
    }

    return (struct au_interpreter_result){.type = AU_INT_ERR_OK};
}

struct au_interpreter_result
au_c_comp(struct au_c_comp_state *state, const struct au_program *program,
          const struct au_c_comp_options *options,
          struct au_cc_options *cc) {
    (void)options;
    (void)cc;

    au_c_comp_state_append_nt(
        state,
        "/* Code for Aument's core library. Do not edit this. */\n");
    au_c_comp_state_append(state, AU_RT_HDR, AU_RT_HDR_LEN);
    au_c_comp_state_append_nt(
        state, "\n\n/* Code generated from source file */\n");

    struct lyra_ctx lctx;
    lyra_ctx_init(&lctx);

    struct global_ctx gctx = (struct global_ctx){0};
    gctx.main_idx = 0;

    struct lyra_function *main_fn =
        lyra_function_new(gctx.main_idx, 0, &lctx);
    {
        struct function_ctx fctx;
        function_ctx_init(&fctx, main_fn);
        function_to_lyra(&fctx, &program->main, &program->data, &gctx);
        function_ctx_finalize(&fctx);

        struct lyra_comp lcomp = {0};
        lyra_comp_init(&lcomp, &lctx);
        lyra_function_comp(fctx.lyra_fn, &lcomp);
        au_c_comp_state_append(state, lcomp.source.data, lcomp.source.len);
        au_c_comp_state_append_nt(state, "\n");
        lyra_comp_del(&lcomp);
    }

    au_c_comp_state_append_nt(state, "int main() { f0(0); }\n");

    struct au_interpreter_result retval =
        (struct au_interpreter_result){.type = AU_INT_ERR_OK};
    return retval;
}
