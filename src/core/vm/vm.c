// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_ALLOCA
#include <alloca.h>
#define ALLOCA_MAX_VALUES 256
#endif

#include "platform/mmap.h"
#include "platform/path.h"
#include "platform/platform.h"

#include "core/fn.h"
#include "core/parser/parser.h"
#include "exception.h"
#include "stdlib/au_stdlib.h"
#include "vm.h"

#include "core/rt/au_array.h"
#include "core/rt/au_class.h"
#include "core/rt/au_string.h"
#include "core/rt/au_struct.h"
#include "core/rt/au_tuple.h"
#include "core/rt/exception.h"

static void au_vm_frame_del(struct au_vm_frame *frame,
                            _Unused const struct au_bc_storage *bcs) {
#ifndef USE_ALLOCA
    for (int i = 0; i < bcs->num_registers; i++) {
        au_value_deref(frame->regs[i]);
    }
    for (int i = 0; i < bcs->locals_len; i++) {
        au_value_deref(frame->locals[i]);
    }
    free(frame->locals);
#endif
    for (size_t i = 0; i < frame->arg_stack.len; i++) {
        au_value_deref(frame->arg_stack.data[i]);
    }
    free(frame->arg_stack.data);
    memset(frame, 0, sizeof(struct au_vm_frame));
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

static void link_to_imported(const struct au_program_data *p_data,
                             const uint32_t relative_module_idx,
                             const struct au_program_data *loaded_module) {
    struct au_imported_module *relative_module =
        &p_data->imported_modules.data[relative_module_idx];
    AU_HM_VARS_FOREACH_PAIR(&relative_module->fn_map, key, entry, {
        assert(p_data->fns.data[entry->idx].type == AU_FN_IMPORTER);
        const struct au_imported_func *import_func =
            &p_data->fns.data[entry->idx].as.import_func;
        const struct au_hm_var_value *fn_idx =
            au_hm_vars_get(&loaded_module->fn_map, key, key_len);
        if (fn_idx == 0)
            au_fatal("unknown function %.*s", key_len, key);
        struct au_fn *fn = &loaded_module->fns.data[fn_idx->idx];
        if ((fn->flags & AU_FN_FLAG_EXPORTED) == 0)
            au_fatal("this function is not exported");
        if (au_fn_num_args(fn) != import_func->num_args)
            au_fatal("unexpected number of arguments");
        au_fn_fill_import_cache_unsafe(&p_data->fns.data[entry->idx], fn,
                                       loaded_module);
    })
    AU_HM_VARS_FOREACH_PAIR(&relative_module->class_map, key, entry, {
        assert(p_data->classes.data[entry->idx] == 0);
        const struct au_hm_var_value *class_idx =
            au_hm_vars_get(&loaded_module->class_map, key, key_len);
        if (class_idx == 0)
            au_fatal("unknown class %.*s", key_len, key);
        struct au_class_interface *class_interface =
            loaded_module->classes.data[class_idx->idx];
        if ((class_interface->flags & AU_CLASS_FLAG_EXPORTED) == 0)
            au_fatal("this class is not exported");
        p_data->classes.data[entry->idx] = class_interface;
        au_class_interface_ref(class_interface);
    })
    if (relative_module->class_map.entries_occ > 0) {
        for (size_t i = 0; i < p_data->fns.len; i++) {
            au_fn_fill_class_cache_unsafe(&p_data->fns.data[i], p_data);
        }
    }
}

static void bin_op_error(au_value_t left, au_value_t right,
                         const struct au_program_data *p_data,
                         struct au_vm_frame *frame) {
    au_vm_error(
        (struct au_interpreter_result){
            .type = AU_INT_ERR_INCOMPAT_BIN_OP,
            .data.incompat_bin_op.left = left,
            .data.incompat_bin_op.right = right,
            .pos = 0,
        },
        p_data, frame);
}

static void call_error(const struct au_program_data *p_data,
                       struct au_vm_frame *frame) {
    au_vm_error(
        (struct au_interpreter_result){
            .type = AU_INT_ERR_INCOMPAT_CALL,
            .pos = 0,
        },
        p_data, frame);
}

au_value_t au_vm_exec_unverified(struct au_vm_thread_local *tl,
                                 const struct au_bc_storage *bcs,
                                 const struct au_program_data *p_data,
                                 const au_value_t *args) {
#ifdef USE_ALLOCA
    au_value_t *alloca_values = 0;
    if (_Likely((bcs->num_registers + bcs->locals_len) <
                ALLOCA_MAX_VALUES)) {
        const size_t n_values = bcs->num_registers + bcs->locals_len;
        alloca_values = alloca(n_values * sizeof(au_value_t));
        au_value_clear(alloca_values, n_values);
    }
#endif
    struct au_vm_frame frame;

    // We add the frame to the linked list first,
    // because tl and frame are fresh in the stack/registers
    frame.link = tl->current_frame;
    tl->current_frame.data = p_data;
    tl->current_frame.frame = &frame;

#ifdef USE_ALLOCA
    if (_Likely(alloca_values)) {
        frame.regs = alloca_values;
        frame.locals = &alloca_values[bcs->num_registers];
    } else {
        frame.regs = au_value_calloc(bcs->num_registers);
        frame.locals = au_value_calloc(bcs->locals_len);
    }
#else
    au_value_clear(frame.regs, bcs->num_registers);
    frame.locals = au_value_calloc(bcs->locals_len);
#endif
    frame.retval = au_value_none();
    if (args != 0) {
#ifdef DEBUG_VM
        assert(bcs->locals_len >= bcs->num_args);
#endif
        memcpy(frame.locals, args, bcs->num_args * sizeof(au_value_t));
    }
    frame.bc = bcs->bc.data;
    frame.bc_start = bcs->bc.data;
    frame.arg_stack = (struct au_value_array){0};

    au_value_t retval;

    while (1) {
#ifdef DEBUG_VM
#define DISPATCH_DEBUG debug_frame(&frame);
#else
#define DISPATCH_DEBUG
#endif

#ifndef USE_DISPATCH_JMP
#define CASE(x) case x
#define DISPATCH                                                          \
    DISPATCH_DEBUG;                                                       \
    frame.bc += 4;                                                        \
    continue
#define DISPATCH_JMP                                                      \
    DISPATCH_DEBUG;                                                       \
    continue
        switch (frame.bc[0]) {
#else
#define CASE(x) CB_##x
#define DISPATCH                                                          \
    do {                                                                  \
        DISPATCH_DEBUG;                                                   \
        frame.bc += 4;                                                    \
        uint8_t op = frame.bc[0];                                         \
        goto *cb[op];                                                     \
    } while (0)
#define DISPATCH_JMP                                                      \
    do {                                                                  \
        DISPATCH_DEBUG;                                                   \
        uint8_t op = frame.bc[0];                                         \
        goto *cb[op];                                                     \
    } while (0)
        static void *cb[] = {
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
            &&CASE(OP_CALL),
            &&CASE(OP_RET_LOCAL),
            &&CASE(OP_RET),
            &&CASE(OP_RET_NULL),
            &&CASE(OP_IMPORT),
            &&CASE(OP_ARRAY_NEW),
            &&CASE(OP_ARRAY_PUSH),
            &&CASE(OP_IDX_GET),
            &&CASE(OP_IDX_SET),
            &&CASE(OP_NOT),
            &&CASE(OP_TUPLE_NEW),
            &&CASE(OP_IDX_SET_STATIC),
            &&CASE(OP_CLASS_GET_INNER),
            &&CASE(OP_CLASS_SET_INNER),
            &&CASE(OP_CLASS_NEW),
            &&CASE(OP_CALL1),
        };
        goto *cb[frame.bc[0]];
#endif

/// Copies an au_value from src to dest. For memory safety, please use this
/// function instead of copying directly
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

            // Register/local move operations
            CASE(OP_MOV_U16) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t n = *(uint16_t *)(&frame.bc[2]);
                MOVE_VALUE(frame.regs[reg], au_value_int(n));
                DISPATCH;
            }
            CASE(OP_MOV_REG_LOCAL) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t local = *(uint16_t *)(&frame.bc[2]);
                COPY_VALUE(frame.locals[local], frame.regs[reg]);
                DISPATCH;
            }
            CASE(OP_MOV_LOCAL_REG) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t local = *(uint16_t *)(&frame.bc[2]);
                COPY_VALUE(frame.regs[reg], frame.locals[local]);
                DISPATCH;
            }
            CASE(OP_MOV_BOOL) : {
                const uint8_t n = frame.bc[1];
                const uint8_t reg = frame.bc[2];
                COPY_VALUE(frame.regs[reg], au_value_bool(n));
                DISPATCH;
            }
            CASE(OP_LOAD_CONST) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t rel_c = *(uint16_t *)(&frame.bc[2]);
                const size_t abs_c = rel_c + p_data->tl_constant_start;
                au_value_t v;
                if (au_value_get_type(tl->const_cache[abs_c]) !=
                    VALUE_NONE) {
                    v = tl->const_cache[abs_c];
                } else {
                    const struct au_program_data_val *data_val =
                        &p_data->data_val.data[rel_c];
                    v = data_val->real_value;
                    switch (au_value_get_type(v)) {
                    case VALUE_STR: {
                        v = au_value_string(au_string_from_const(
                            (const char
                                 *)(&p_data->data_buf[data_val->buf_idx]),
                            data_val->buf_len));
                        tl->const_cache[abs_c] = v;
                        break;
                    }
                    default:
                        break;
                    }
                }
                COPY_VALUE(frame.regs[reg], v);
                DISPATCH;
            }
            // Unary operations
            CASE(OP_NOT) : {
                const uint8_t reg = frame.bc[1];
                if (_Likely(au_value_get_type(frame.regs[reg]) ==
                            VALUE_BOOL)) {
                    frame.regs[reg] =
                        au_value_bool(!au_value_get_bool(frame.regs[reg]));
                } else {
                    MOVE_VALUE(frame.regs[reg],
                               au_value_bool(
                                   !au_value_is_truthy(frame.regs[reg])));
                }
                DISPATCH;
            }
            // Binary operations
#define BIN_OP(NAME, FUN)                                                 \
    CASE(NAME) : {                                                        \
        const au_value_t lhs = frame.regs[frame.bc[1]];                   \
        const au_value_t rhs = frame.regs[frame.bc[2]];                   \
        const uint8_t res = frame.bc[3];                                  \
        const au_value_t result = au_value_##FUN(lhs, rhs);               \
        if (_Unlikely(au_value_is_op_error(frame.regs[res]))) {           \
            bin_op_error(lhs, rhs, p_data, &frame);                       \
        }                                                                 \
        MOVE_VALUE(frame.regs[res], result);                              \
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
            // Jump instructions
            CASE(OP_JIF) : {
                const au_value_t cmp = frame.regs[frame.bc[1]];
                const uint16_t n = *(uint16_t *)(&frame.bc[2]);
                const size_t offset = ((size_t)n) * 4;
                if (au_value_is_truthy(cmp)) {
                    frame.bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(OP_JNIF) : {
                const au_value_t cmp = frame.regs[frame.bc[1]];
                const uint16_t n = *(uint16_t *)(&frame.bc[2]);
                const size_t offset = ((size_t)n) * 4;
                if (!au_value_is_truthy(cmp)) {
                    frame.bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(OP_JREL) : {
                const uint16_t *ptr = (uint16_t *)(&frame.bc[2]);
                const size_t offset = ((size_t)ptr[0]) * 4;
                frame.bc += offset;
                DISPATCH_JMP;
            }
            CASE(OP_JRELB) : {
                const uint16_t n = *(uint16_t *)(&frame.bc[2]);
                const size_t offset = ((size_t)n) * 4;
                frame.bc -= offset;
                DISPATCH_JMP;
            }
            // Binary operation into local instructions
#define BIN_OP_ASG(NAME, FUN)                                             \
    CASE(NAME) : {                                                        \
        const uint8_t reg = frame.bc[1];                                  \
        const uint8_t local = frame.bc[2];                                \
        const au_value_t lhs = frame.locals[local];                       \
        const au_value_t rhs = frame.regs[reg];                           \
        const au_value_t result = au_value_##FUN(lhs, rhs);               \
        if (_Unlikely(au_value_is_op_error(result))) {                    \
            bin_op_error(lhs, rhs, p_data, &frame);                       \
        }                                                                 \
        MOVE_VALUE(frame.locals[local], result);                          \
        DISPATCH;                                                         \
    }
            BIN_OP_ASG(OP_MUL_ASG, mul)
            BIN_OP_ASG(OP_DIV_ASG, div)
            BIN_OP_ASG(OP_ADD_ASG, add)
            BIN_OP_ASG(OP_SUB_ASG, sub)
            BIN_OP_ASG(OP_MOD_ASG, mod)
#undef BIN_OP_ASG
            // Call instructions
            CASE(OP_PUSH_ARG) : {
                const uint8_t reg = frame.bc[1];
                au_value_array_add(&frame.arg_stack, frame.regs[reg]);
                au_value_ref(frame.regs[reg]);
                DISPATCH;
            }
            CASE(OP_CALL) : {
                const uint8_t ret_reg = frame.bc[1];
                const uint16_t func_id = *((uint16_t *)(&frame.bc[2]));
                const struct au_fn *call_fn = &p_data->fns.data[func_id];
                int n_regs = au_fn_num_args(call_fn);
                const au_value_t *args =
                    &frame.arg_stack.data[frame.arg_stack.len - n_regs];
                const au_value_t callee_retval =
                    au_fn_call(call_fn, tl, p_data, args);
                if (_Unlikely(au_value_is_op_error(callee_retval)))
                    call_error(p_data, &frame);
                MOVE_VALUE(frame.regs[ret_reg], callee_retval);
                frame.arg_stack.len -= n_regs;
                DISPATCH;
            }
            CASE(OP_CALL1) : {
                const uint8_t ret_reg = frame.bc[1];
                const uint16_t func_id = *((uint16_t *)(&frame.bc[2]));
                const struct au_fn *call_fn = &p_data->fns.data[func_id];
                au_value_t arg_reg = frame.regs[ret_reg];
                // arg_reg is moved to locals in au_fn_call
                const au_value_t callee_retval =
                    au_fn_call(call_fn, tl, p_data, &arg_reg);
                if (_Unlikely(au_value_is_op_error(callee_retval)))
                    call_error(p_data, &frame);
                frame.regs[ret_reg] = callee_retval;
                DISPATCH;
            }
            // Return instructions
            CASE(OP_RET_LOCAL) : {
                const uint8_t ret_local = frame.bc[2];
                // Move ownership of value in ret_local -> return reg in
                // prev. frame
                frame.retval = frame.locals[ret_local];
                frame.locals[ret_local] = au_value_none();
                goto end;
            }
            CASE(OP_RET) : {
                const uint8_t ret_reg = frame.bc[1];
                // Move ownership of value in ret_reg -> return reg in
                // prev. frame
                frame.retval = frame.regs[ret_reg];
                frame.regs[ret_reg] = au_value_none();
                goto end;
            }
            CASE(OP_RET_NULL) : { goto end; }
            // Array instructions
            CASE(OP_ARRAY_NEW) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t capacity = *((uint16_t *)(&frame.bc[2]));
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_array_new(capacity)));
                DISPATCH;
            }
            CASE(OP_ARRAY_PUSH) : {
                const au_value_t array_val = frame.regs[frame.bc[1]];
                const au_value_t value_val = frame.regs[frame.bc[2]];
                struct au_obj_array *obj_array =
                    au_obj_array_coerce(array_val);
                if (_Likely(obj_array != 0)) {
                    au_obj_array_push(obj_array, value_val);
                }
                DISPATCH;
            }
            CASE(OP_IDX_GET) : {
                const au_value_t col_val = frame.regs[frame.bc[1]];
                const au_value_t idx_val = frame.regs[frame.bc[2]];
                const uint8_t ret_reg = frame.bc[3];
                struct au_struct *collection = au_struct_coerce(col_val);
                if (_Likely(collection != 0)) {
                    au_value_t value;
                    if (!collection->vdata->idx_get_fn(collection, idx_val,
                                                       &value)) {
                        au_fatal("invalid index");
                    }
                    COPY_VALUE(frame.regs[ret_reg], value);
                }
                DISPATCH;
            }
            CASE(OP_IDX_SET) : {
                const au_value_t col_val = frame.regs[frame.bc[1]];
                const au_value_t idx_val = frame.regs[frame.bc[2]];
                const au_value_t value_val = frame.regs[frame.bc[3]];
                struct au_struct *collection = au_struct_coerce(col_val);
                if (_Likely(collection != 0)) {
                    if (!collection->vdata->idx_set_fn(collection, idx_val,
                                                       value_val)) {
                        au_fatal("invalid index");
                    }
                }
                DISPATCH;
            }
            // Tuple instructions
            CASE(OP_TUPLE_NEW) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t length = *((uint16_t *)(&frame.bc[2]));
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_tuple_new(length)));
                DISPATCH;
            }
            CASE(OP_IDX_SET_STATIC) : {
                const au_value_t col_val = frame.regs[frame.bc[1]];
                const au_value_t idx_val = au_value_int(frame.bc[2]);
                const au_value_t value_val = frame.regs[frame.bc[3]];
                struct au_struct *collection = au_struct_coerce(col_val);
                if (_Likely(collection != 0)) {
                    if (!collection->vdata->idx_set_fn(collection, idx_val,
                                                       value_val)) {
                        au_fatal("invalid index");
                    }
                }
                DISPATCH;
            }
            // Class instructions
            CASE(OP_CLASS_NEW) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t class_id = *(uint16_t *)(&frame.bc[2]);
                struct au_struct *obj_class =
                    (struct au_struct *)au_obj_class_new(
                        p_data->classes.data[class_id]);
                const au_value_t new_value = au_value_struct(obj_class);
                MOVE_VALUE(frame.regs[reg], new_value);
                DISPATCH;
            }
            CASE(OP_CLASS_GET_INNER) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t inner = *(uint16_t *)(&frame.bc[2]);
                struct au_obj_class *self =
                    au_obj_class_coerce(frame.locals[0]);
                COPY_VALUE(frame.regs[reg], self->data[inner]);
                DISPATCH;
            }
            CASE(OP_CLASS_SET_INNER) : {
                const uint8_t reg = frame.bc[1];
                const uint16_t inner = *(uint16_t *)(&frame.bc[2]);
                struct au_obj_class *self =
                    au_obj_class_coerce(frame.locals[0]);
                COPY_VALUE(self->data[inner], frame.regs[reg]);
                DISPATCH;
            }
            // Module instructions
            CASE(OP_IMPORT) : {
                const uint16_t idx = *((uint16_t *)(&frame.bc[2]));
                const size_t relative_module_idx =
                    p_data->imports.data[idx].module_idx;
                const char *relpath = p_data->imports.data[idx].path;

                struct au_mmap_info mmap;
                char *abspath = 0;

                if (relpath[0] == '.' && relpath[1] == '/') {
                    const char *relpath_canon = &relpath[2];
                    const size_t abspath_len =
                        strlen(p_data->cwd) + strlen(relpath_canon) + 2;
                    abspath = malloc(abspath_len);
                    snprintf(abspath, abspath_len, "%s/%s", p_data->cwd,
                             relpath_canon);
                } else {
                    assert(0);
                }

                struct au_program_data *loaded_module =
                    au_vm_thread_local_get_module(tl, abspath);
                if (loaded_module != 0) {
                    free(abspath);
                    link_to_imported(p_data, relative_module_idx,
                                     loaded_module);
                    DISPATCH;
                }

                uint32_t tl_module_idx = ((uint32_t)-1);
                enum au_tl_reserve_mod_retval rmod_retval =
                    AU_TL_RESMOD_RETVAL_FAIL;
                if (relative_module_idx == AU_PROGRAM_IMPORT_NO_MODULE) {
                    rmod_retval = au_vm_thread_local_reserve_import_only(
                        tl, abspath);
                    if (rmod_retval ==
                        AU_TL_RESMOD_RETVAL_OK_MAIN_CALLED) {
                        free(abspath);
                        DISPATCH;
                    }
                } else {
                    rmod_retval = au_vm_thread_local_reserve_module(
                        tl, abspath, &tl_module_idx);
                }

                if (rmod_retval == AU_TL_RESMOD_RETVAL_FAIL) {
                    au_fatal("circular import detected");
                }

                if (!au_mmap_read(abspath, &mmap)) {
                    au_perror("mmap");
                }

                struct au_program program;
                assert(au_parse(mmap.bytes, mmap.size, &program).type ==
                       AU_PARSER_RES_OK);
                au_mmap_del(&mmap);

                if (!au_split_path(abspath, &program.data.file,
                                   &program.data.cwd))
                    au_perror("au_split_path");
                free(abspath);

                if (rmod_retval != AU_TL_RESMOD_RETVAL_OK_MAIN_CALLED) {
                    au_vm_exec_unverified_main(tl, &program);
                }

                if (relative_module_idx == AU_PROGRAM_IMPORT_NO_MODULE) {
                    au_program_del(&program);
                } else {
                    au_bc_storage_del(&program.main);

                    struct au_program_data *loaded_module =
                        malloc(sizeof(struct au_program_data));
                    memcpy(loaded_module, &program.data,
                           sizeof(struct au_program_data));
                    au_vm_thread_local_add_module(tl, tl_module_idx,
                                                  loaded_module);

                    link_to_imported(p_data, relative_module_idx,
                                     loaded_module);
                }
                DISPATCH;
            }
            // Other
            CASE(OP_PRINT) : {
                const au_value_t reg = frame.regs[frame.bc[1]];
                tl->print_fn(reg);
                DISPATCH;
            }
            CASE(OP_NOP) : { DISPATCH; }
#undef COPY_VALUE
#ifndef USE_DISPATCH_JMP
        }
#endif
    }
end:
#ifdef USE_ALLOCA
    if (_Likely(alloca_values)) {
        int n_values = bcs->num_registers + bcs->locals_len;
        for (int i = 0; i < n_values; i++) {
            au_value_deref(alloca_values[i]);
        }
    } else {
        for (int i = 0; i < bcs->num_registers; i++) {
            au_value_deref(frame.regs[i]);
        }
        for (int i = 0; i < bcs->locals_len; i++) {
            au_value_deref(frame.locals[i]);
        }
        free(frame.regs);
        free(frame.locals);
        frame.regs = 0;
        frame.locals = 0;
    }
#endif
    retval = frame.retval;
    frame.retval = au_value_none();
    au_vm_frame_del(&frame, bcs);
    return retval;
}
