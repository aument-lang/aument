// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#include "lyra/context.h"
#include "lyra/insn.h"
#include "lyra/function.h"
#include "lyra/block.h"
#include "lyra/comp.h"
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

void au_c_comp_state_del(struct au_c_comp_state *state) {
    au_data_free(state->str.data);
}

struct function_ctx {
    struct lyra_function *lyra_fn;
    struct lyra_block current_block;
};

static void function_ctx_init(struct function_ctx *fctx, struct lyra_function *lyra_fn) {
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

static struct au_interpreter_result
function_to_lyra(struct function_ctx *fctx,
                 const struct au_bc_storage *bcs,
                 const struct au_program_data *p_data) {
#define bc(x) au_bc_buf_at(&bcs->bc, x)

#define DEF_BC16(VAR, OFFSET)                                             \
    uint16_t VAR;                                                         \
    do {                                                                  \
        assert(pos + OFFSET + 2 <= bcs->bc.len);                          \
        VAR = *((uint16_t *)(&bcs->bc.data[pos + OFFSET]));               \
    } while (0)

    size_t pos = 0;
    while (pos < bcs->bc.len) {
        uint8_t opcode = bc(pos);
        pos++;

        switch (opcode) {
        // Load/store constants
        case AU_OP_LOAD_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1);

            const struct au_program_data_val *data_val =
                &p_data->data_val.data[c];
            au_value_t value = data_val->real_value;
            switch (au_value_get_type(value)) {
                case AU_VALUE_DOUBLE: {
                    struct lyra_insn *insn = lyra_insn_imm(
                        LYRA_OP_MOV_F64, LYRA_INSN_F64(au_value_get_double(value)), reg, fctx->lyra_fn->ctx);
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
        case AU_OP_ADD: {
            uint8_t lhs = bc(pos);
            uint8_t rhs = bc(pos + 1);
            uint8_t res = bc(pos + 2);

            struct lyra_insn *insn = lyra_insn_new(
                LYRA_OP_ADD_VAR, lhs, LYRA_INSN_REG(rhs), res, fctx->lyra_fn->ctx);
            lyra_block_add_insn(&fctx->current_block, insn);

            pos += 3;
            break;
        }
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
            
            (void)reg;
            abort(); // TODO

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
    (void)state;
    (void)program;
    (void)options;
    (void)cc;

    struct lyra_ctx lctx;
    lyra_ctx_init(&lctx);

    struct lyra_function *main_fn = lyra_function_new(0, 0, &lctx);

    {
        struct function_ctx fctx;
        function_ctx_init(&fctx, main_fn);
        function_to_lyra(&fctx, &program->main, &program->data);
        function_ctx_finalize(&fctx);

        struct lyra_comp lcomp = {0};
        lyra_comp_init(&lcomp, &lctx);
        lyra_function_comp(fctx.lyra_fn, &lcomp);
        for(size_t i = 0; i < lcomp.source.len; i++)
            au_char_array_add(&state->str, lcomp.source.data[i]);
        lyra_comp_del(&lcomp);
    }

    struct au_interpreter_result retval =
        (struct au_interpreter_result){.type = AU_INT_ERR_OK};
    return retval;
}
