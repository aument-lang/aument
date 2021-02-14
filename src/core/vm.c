// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#else
#include <libgen.h>
#endif

#include "parser/parser.h"
#include "platform/mmap.h"
#include "stdlib/au_stdlib.h"
#include "vm.h"

#include "rt/au_array.h"
#include "rt/au_string.h"
#include "rt/au_struct.h"
#include "rt/exception.h"

static void au_vm_frame_del(struct au_vm_frame *frame,
                            const struct au_bc_storage *bcs) {
    for (int i = 0; i < bcs->num_registers; i++) {
        au_value_deref(frame->regs[i]);
    }
    for (int i = 0; i < bcs->locals_len; i++) {
        au_value_deref(frame->locals[i]);
    }
    free(frame->locals);
    for (int i = 0; i < frame->arg_stack.len; i++) {
        au_value_deref(frame->arg_stack.data[i]);
    }
    free(frame->arg_stack.data);
    memset(frame, 0, sizeof(struct au_vm_frame));
}

void au_vm_thread_local_init(struct au_vm_thread_local *tl,
                             const struct au_program_data *p_data) {
    memset(tl, 0, sizeof(struct au_vm_thread_local));
    tl->const_cache = au_value_calloc(p_data->data_val.len);
    tl->const_len = p_data->data_val.len;
    tl->print_fn = au_value_print;
}

void au_vm_thread_local_del(struct au_vm_thread_local *tl) {
    for (size_t i = 0; i < tl->const_len; i++) {
        au_value_deref(tl->const_cache[i]);
    }
    free(tl->const_cache);
    memset(tl, 0, sizeof(struct au_vm_thread_local));
}

#ifdef DEBUG_VM
static void debug_value(au_value_t v) {
    switch (au_value_get_type(v)) {
    case VALUE_NONE: {
        printf("(none)");
        break;
    }
    case VALUE_INT: {
        printf("%d", au_value_get_int(v));
        break;
    }
    case VALUE_BOOL: {
        printf("%s\n", au_value_get_bool(v) ? "(true)" : "(false)");
        break;
    }
    case VALUE_STR: {
        printf("(string %p)\n", au_value_get_string(v));
        break;
    }
    default:
        break;
    }
}

static void debug_frame(struct au_vm_frame *frame) {
    printf("registers:\n");
    for (int i = 0; i < AU_REGS; i++) {
        if (au_value_get_type(frame->regs[i]) == VALUE_NONE)
            continue;
        printf("  %d: ", i);
        debug_value(frame->regs[i]);
        printf("\n");
    }
    char c;
    scanf("%c", &c);
}
#endif

au_value_t au_vm_exec_unverified(struct au_vm_thread_local *tl,
                                 const struct au_bc_storage *bcs,
                                 const struct au_program_data *p_data,
                                 const au_value_t *args) {
    struct au_vm_frame frame;
    au_value_clear(frame.regs, bcs->num_registers);
    au_value_clear(&frame.retval, 1);
    frame.locals = au_value_calloc(bcs->locals_len);
    if (args != 0) {
#ifdef DEBUG_VM
        assert(bcs->locals_len >= bcs->num_args);
#endif
        memcpy(frame.locals, args, bcs->num_args * sizeof(au_value_t));
    }
    frame.bc = bcs->bc.data;
    frame.pc = 0;
    frame.arg_stack = (struct au_value_array){0};
    au_value_t retval;

    while (1) {
#ifdef DEBUG_VM
#define MAYBE_DISPATCH_DEBUG debug_frame(&frame);
#else
#define MAYBE_DISPATCH_DEBUG
#endif

#ifndef USE_DISPATCH_JMP
#define CASE(x) case x
#define DISPATCH                                                          \
    MAYBE_DISPATCH_DEBUG;                                                 \
    frame.pc += 4;                                                        \
    continue
#define DISPATCH_JMP                                                      \
    MAYBE_DISPATCH_DEBUG;                                                 \
    continue
        switch (frame.bc[frame.pc]) {
#else
#define CASE(x) CB_##x
#define DISPATCH                                                          \
    do {                                                                  \
        MAYBE_DISPATCH_DEBUG;                                             \
        frame.pc += 4;                                                    \
        uint8_t op = frame.bc[frame.pc];                                  \
        goto *cb[op];                                                     \
    } while (0)
#define DISPATCH_JMP                                                      \
    do {                                                                  \
        MAYBE_DISPATCH_DEBUG;                                             \
        uint8_t op = frame.bc[frame.pc];                                  \
        goto *cb[op];                                                     \
    } while (0)
        static void *cb[] = {
            &&CASE(OP_EXIT),
            &&CASE(OP_MOV_U16),
            &&CASE(OP_MUL),
            &&CASE(OP_DIV),
            &&CASE(OP_ADD),
            &&CASE(OP_SUB),
            &&CASE(OP_MOD),
            &&CASE(OP_MOV_REG_LOCAL),
            &&CASE(OP_MOV_LOCAL_REG),
            &&CASE(OP_PRINT),
            &&CASE(OP_EQ),
            &&CASE(OP_NEQ),
            &&CASE(OP_LT),
            &&CASE(OP_GT),
            &&CASE(OP_LEQ),
            &&CASE(OP_GEQ),
            &&CASE(OP_JIF),
            &&CASE(OP_JNIF),
            &&CASE(OP_JREL),
            &&CASE(OP_JRELB),
            &&CASE(OP_LOAD_CONST),
            &&CASE(OP_MOV_BOOL),
            &&CASE(OP_NOP),
            &&CASE(OP_MUL_ASG),
            &&CASE(OP_DIV_ASG),
            &&CASE(OP_ADD_ASG),
            &&CASE(OP_SUB_ASG),
            &&CASE(OP_MOD_ASG),
            &&CASE(OP_PUSH_ARG),
            &&CASE(OP_CALL0),
            &&CASE(OP_CALL1),
            &&CASE(OP_CALL2),
            &&CASE(OP_CALL3),
            &&CASE(OP_CALL4),
            &&CASE(OP_CALL5),
            &&CASE(OP_CALL6),
            &&CASE(OP_CALL7),
            &&CASE(OP_CALL8),
            &&CASE(OP_CALL9),
            &&CASE(OP_CALL10),
            &&CASE(OP_CALL11),
            &&CASE(OP_CALL12),
            &&CASE(OP_CALL13),
            &&CASE(OP_CALL14),
            &&CASE(OP_CALL15),
            &&CASE(OP_RET_LOCAL),
            &&CASE(OP_RET),
            &&CASE(OP_RET_NULL),
            &&CASE(OP_IMPORT),
            &&CASE(OP_ARRAY_NEW),
            &&CASE(OP_ARRAY_PUSH),
            &&CASE(OP_IDX_GET),
            &&CASE(OP_IDX_SET),
        };
        goto *cb[frame.bc[0]];
#endif
            CASE(OP_EXIT) : { goto end; }
            CASE(OP_NOP) : { DISPATCH; }
            CASE(OP_MOV_U16) : {
                const uint8_t reg = frame.bc[frame.pc + 1];
                const uint16_t n = *(uint16_t *)(&frame.bc[frame.pc + 2]);
                au_value_deref(frame.regs[reg]);
                frame.regs[reg] = au_value_int(n);
                DISPATCH;
            }
#define BIN_OP(NAME, FUN)                                                 \
    CASE(NAME) : {                                                        \
        const au_value_t old = frame.regs[frame.bc[frame.pc + 3]];        \
        const au_value_t lhs = frame.regs[frame.bc[frame.pc + 1]];        \
        const au_value_t rhs = frame.regs[frame.bc[frame.pc + 2]];        \
        frame.regs[frame.bc[frame.pc + 3]] = au_value_##FUN(lhs, rhs);    \
        au_value_deref(old);                                              \
        DISPATCH;                                                         \
    }
            BIN_OP(OP_MUL, mul)
            BIN_OP(OP_DIV, div)
            BIN_OP(OP_ADD, add)
            BIN_OP(OP_SUB, sub)
            BIN_OP(OP_MOD, mod)
            BIN_OP(OP_EQ, eq)
            BIN_OP(OP_NEQ, neq)
            BIN_OP(OP_LT, lt)
            BIN_OP(OP_GT, gt)
            BIN_OP(OP_LEQ, leq)
            BIN_OP(OP_GEQ, geq)
#undef BIN_OP

/// Copies an au_value from src to dest.
/// NOTE: for memory safety, please use this function instead of
///     copying directly
#define COPY_VALUE(dest, src)                                             \
    do {                                                                  \
        const au_value_t old = dest;                                      \
        dest = src;                                                       \
        au_value_ref(dest);                                               \
        au_value_deref(old);                                              \
    } while (0)

#define MOVE_VALUE(dest, src)                                             \
    do {                                                                  \
        const au_value_t old = dest;                                      \
        dest = src;                                                       \
        au_value_deref(old);                                              \
    } while (0)

            CASE(OP_MOV_REG_LOCAL) : {
                const uint8_t reg = frame.bc[frame.pc + 1];
                const uint8_t local = frame.bc[frame.pc + 2];
                COPY_VALUE(frame.locals[local], frame.regs[reg]);
                DISPATCH;
            }
            CASE(OP_MOV_LOCAL_REG) : {
                const uint8_t local = frame.bc[frame.pc + 1];
                const uint8_t reg = frame.bc[frame.pc + 2];
                COPY_VALUE(frame.regs[reg], frame.locals[local]);
                DISPATCH;
            }
            CASE(OP_PRINT) : {
                const au_value_t lhs = frame.regs[frame.bc[frame.pc + 1]];
                tl->print_fn(lhs);
                DISPATCH;
            }
            CASE(OP_JIF) : {
                const au_value_t cmp = frame.regs[frame.bc[frame.pc + 1]];
                const uint16_t n = *(uint16_t *)(&frame.bc[frame.pc + 2]);
                const size_t offset = ((size_t)n) * 4;
                if (au_value_is_truthy(cmp)) {
                    frame.pc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(OP_JNIF) : {
                const au_value_t cmp = frame.regs[frame.bc[frame.pc + 1]];
                const uint16_t n = *(uint16_t *)(&frame.bc[frame.pc + 2]);
                const size_t offset = ((size_t)n) * 4;
                if (!au_value_is_truthy(cmp)) {
                    frame.pc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(OP_JREL) : {
                const uint16_t *ptr =
                    (uint16_t *)(&frame.bc[frame.pc + 2]);
                const size_t offset = ((size_t)ptr[0]) * 4;
                frame.pc += offset;
                DISPATCH_JMP;
            }
            CASE(OP_JRELB) : {
                const uint16_t n = *(uint16_t *)(&frame.bc[frame.pc + 2]);
                const size_t offset = ((size_t)n) * 4;
                frame.pc -= offset;
                DISPATCH_JMP;
            }
            CASE(OP_LOAD_CONST) : {
                const uint8_t reg = frame.bc[frame.pc + 1];
                const uint16_t c = *(uint16_t *)(&frame.bc[frame.pc + 2]);
                au_value_t v;
                if (au_value_get_type(tl->const_cache[c]) != VALUE_NONE) {
                    v = tl->const_cache[c];
                } else {
                    const struct au_program_data_val *data_val =
                        &p_data->data_val.data[c];
                    v = data_val->real_value;
                    switch (au_value_get_type(v)) {
                    case VALUE_STR: {
                        v = au_value_string(au_string_from_const(
                            (const char
                                 *)(&p_data->data_buf[data_val->buf_idx]),
                            data_val->buf_len));
                        // Transfer ownership of v to the cache
                        tl->const_cache[c] = v;
                        break;
                    }
                    default:
                        break;
                    }
                }
                COPY_VALUE(frame.regs[reg], v);
                DISPATCH;
            }
            CASE(OP_MOV_BOOL) : {
                const uint8_t n = frame.bc[frame.pc + 1];
                const uint8_t reg = frame.bc[frame.pc + 2];
                COPY_VALUE(frame.regs[reg], au_value_bool(n));
                DISPATCH;
            }
            CASE(OP_CALL0)
                : CASE(OP_CALL1)
                : CASE(OP_CALL2)
                : CASE(OP_CALL3)
                : CASE(OP_CALL4)
                : CASE(OP_CALL5)
                : CASE(OP_CALL6)
                : CASE(OP_CALL7)
                : CASE(OP_CALL8)
                : CASE(OP_CALL9)
                : CASE(OP_CALL10)
                : CASE(OP_CALL11)
                : CASE(OP_CALL12)
                : CASE(OP_CALL13) : CASE(OP_CALL14) : CASE(OP_CALL15) : {
                const int n_regs = frame.bc[frame.pc] - OP_CALL0;
                const uint8_t ret_reg = frame.bc[frame.pc + 1];
                const uint16_t func_id =
                    *((uint16_t *)(&frame.bc[frame.pc + 2]));
                const struct au_fn *call_fn = &p_data->fns.data[func_id];
                const au_value_t *args =
                    &frame.arg_stack.data[frame.arg_stack.len - n_regs];
                switch (call_fn->type) {
                case AU_FN_BC: {
                    const struct au_bc_storage *call_bcs =
                        &call_fn->as.bc_func;
                    const au_value_t callee_retval =
                        au_vm_exec_unverified(tl, call_bcs, p_data, args);
                    MOVE_VALUE(frame.regs[ret_reg], callee_retval);
                    break;
                }
                case AU_FN_NATIVE: {
                    const struct au_lib_func *lib_func =
                        &call_fn->as.native_func;
                    const au_value_t callee_retval =
                        lib_func->func(tl, p_data, args);
                    MOVE_VALUE(frame.regs[ret_reg], callee_retval);
                }
                }
                for (int i = frame.arg_stack.len - n_regs;
                     i < frame.arg_stack.len; i++)
                    au_value_deref(frame.arg_stack.data[i]);
                frame.arg_stack.len -= n_regs;
                DISPATCH;
            }
#define BIN_OP_ASG(NAME, FUN)                                             \
    CASE(NAME) : {                                                        \
        const uint8_t reg = frame.bc[frame.pc + 1];                       \
        const uint8_t local = frame.bc[frame.pc + 2];                     \
        au_value_deref(frame.locals[local]);                              \
        frame.locals[local] =                                             \
            au_value_##FUN(frame.locals[local], frame.regs[reg]);         \
        DISPATCH;                                                         \
    }
            BIN_OP_ASG(OP_MUL_ASG, mul)
            BIN_OP_ASG(OP_DIV_ASG, div)
            BIN_OP_ASG(OP_ADD_ASG, add)
            BIN_OP_ASG(OP_SUB_ASG, sub)
            BIN_OP_ASG(OP_MOD_ASG, mod)
#undef BIN_OP_ASG

            CASE(OP_PUSH_ARG) : {
                const uint8_t reg = frame.bc[frame.pc + 1];
                au_value_array_add(&frame.arg_stack, frame.regs[reg]);
                au_value_ref(frame.regs[reg]);
                DISPATCH;
            }
            CASE(OP_RET_LOCAL) : {
                const uint8_t ret_local = frame.bc[frame.pc + 1];
                // Move ownership of value in ret_local -> return reg in
                // prev. frame
                frame.retval = frame.locals[ret_local];
                frame.locals[ret_local] = au_value_none();
                goto end;
            }
            CASE(OP_RET) : {
                const uint8_t ret_reg = frame.bc[frame.pc + 1];
                // Move ownership of value in ret_reg -> return reg in
                // prev. frame
                frame.retval = frame.regs[ret_reg];
                frame.regs[ret_reg] = au_value_none();
                goto end;
            }
            CASE(OP_RET_NULL) : { goto end; }
            CASE(OP_IMPORT) : {
                const uint16_t idx =
                    *((uint16_t *)(&frame.bc[frame.pc + 2]));
                const char *relpath = p_data->imports.data[idx];

                struct au_mmap_info mmap;
                char *abspath;

                if (relpath[0] == '.' && relpath[1] == '/') {
                    const char *relpath_canon = &relpath[2];
                    const size_t abspath_len =
                        strlen(p_data->cwd) + strlen(relpath_canon) + 2;
                    abspath = malloc(abspath_len);
                    snprintf(abspath, abspath_len, "%s/%s", p_data->cwd,
                             relpath_canon);
                    if (!au_mmap_read(abspath, &mmap))
                        au_perror("mmap");
                } else {
                    assert(0);
                }

                struct au_program program;
                assert(au_parse(mmap.bytes, mmap.size, &program).type ==
                       AU_PARSER_RES_OK);
                au_mmap_del(&mmap);
#ifdef _WIN32
                {
                    char program_path[_MAX_DIR];
                    _splitpath_s(abspath, 0, 0, program_path, _MAX_DIR, 0,
                                 0, 0, 0);
                    program.data.cwd = strdup(program_path);
                    free(abspath);
                }
#else
            program.data.cwd = dirname(abspath);
            // abspath is transferred to program.data
#endif

                au_vm_exec_unverified_main(tl, &program);
                au_program_del(&program);
                DISPATCH;
            }
            CASE(OP_ARRAY_NEW) : {
                const uint8_t reg = frame.bc[frame.pc + 1];
                const uint16_t capacity =
                    *((uint16_t *)(&frame.bc[frame.pc + 2]));
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_array_new(capacity)));
                DISPATCH;
            }
            CASE(OP_ARRAY_PUSH) : {
                const au_value_t array_val =
                    frame.regs[frame.bc[frame.pc + 1]];
                const au_value_t value_val =
                    frame.regs[frame.bc[frame.pc + 2]];
                struct au_obj_array *obj_array =
                    au_obj_array_coerce(array_val);
                au_obj_array_push(obj_array, value_val);
                DISPATCH;
            }
            CASE(OP_IDX_GET) : {
                const au_value_t col_val =
                    frame.regs[frame.bc[frame.pc + 1]];
                const au_value_t idx_val =
                    frame.regs[frame.bc[frame.pc + 2]];
                const uint8_t ret_reg = frame.bc[frame.pc + 3];
                struct au_struct *collection = au_struct_coerce(col_val);
                COPY_VALUE(
                    frame.regs[ret_reg],
                    collection->vdata->idx_get_fn(collection, idx_val));
                DISPATCH;
            }
            CASE(OP_IDX_SET) : {
                const au_value_t col_val =
                    frame.regs[frame.bc[frame.pc + 1]];
                const au_value_t idx_val =
                    frame.regs[frame.bc[frame.pc + 2]];
                const au_value_t value_val =
                    frame.regs[frame.bc[frame.pc + 3]];
                struct au_struct *collection = au_struct_coerce(col_val);
                collection->vdata->idx_set_fn(collection, idx_val,
                                              value_val);
                DISPATCH;
            }
#undef COPY_VALUE
#ifndef USE_DISPATCH_JMP
        }
#endif
    }
end:
    retval = frame.retval;
    frame.retval = au_value_none();
    au_vm_frame_del(&frame, bcs);
    return retval;
}
