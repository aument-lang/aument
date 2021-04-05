// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AU_USE_ALLOCA
#ifdef _WIN32
#include <malloc.h>
#ifndef alloca
#define alloca _alloca
#endif
#else
#include <alloca.h>
#endif
#define ALLOCA_MAX_VALUES 256
#endif

#include "platform/arithmetic.h"
#include "platform/platform.h"

#include "os/mmap.h"
#include "os/path.h"

#include "core/fn.h"
#include "core/parser/parser.h"
#include "exception.h"
#include "module.h"
#include "stdlib/au_stdlib.h"
#include "vm.h"

#include "core/int_error/error_printer.h"
#include "core/rt/au_array.h"
#include "core/rt/au_class.h"
#include "core/rt/au_dict.h"
#include "core/rt/au_fn_value.h"
#include "core/rt/au_string.h"
#include "core/rt/au_struct.h"
#include "core/rt/au_tuple.h"
#include "core/rt/exception.h"

#ifdef DEBUG_VM
static void debug_value(au_value_t v) {
    switch (au_value_get_type(v)) {
    case AU_VALUE_NONE: {
        printf("(none)");
        break;
    }
    case AU_VALUE_INT: {
        printf("%d", au_value_get_int(v));
        break;
    }
    case AU_VALUE_BOOL: {
        printf("%s\n", au_value_get_bool(v) ? "(true)" : "(false)");
        break;
    }
    case AU_VALUE_STR: {
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
        if (au_value_get_type(frame->regs[i]) == AU_VALUE_NONE)
            continue;
        printf("  %d: ", i);
        debug_value(frame->regs[i]);
        printf("\n");
    }
    char c;
    scanf("%c", &c);
}
#endif

static struct au_interpreter_result
link_to_imported(struct au_vm_thread_local *tl,
                 const struct au_program_data *p_data,
                 const uint32_t relative_module_idx,
                 const struct au_program_data *loaded_module) {
    struct au_imported_module *relative_module =
        &p_data->imported_modules.data[relative_module_idx];
    struct au_interpreter_result retval =
        (struct au_interpreter_result){.type = AU_INT_ERR_OK};
    AU_HM_VARS_FOREACH_PAIR(&relative_module->fn_map, key, entry, {
        assert(p_data->fns.data[entry].type == AU_FN_IMPORTER);
        const struct au_imported_func *imported_func =
            &p_data->fns.data[entry].as.imported_func;
        const au_hm_var_value_t *fn_idx =
            au_hm_vars_get(&loaded_module->fn_map, key, key_len);
        if (fn_idx == 0) {
            retval.type = AU_INT_ERR_UNKNOWN_FUNCTION;
            retval.data.unknown_id.key = au_data_strndup(key, key_len);
            goto end;
        }
        struct au_fn *fn = &loaded_module->fns.data[*fn_idx];
        if ((fn->flags & AU_FN_FLAG_EXPORTED) == 0) {
            retval.type = AU_INT_ERR_UNKNOWN_FUNCTION;
            retval.data.unknown_id.key = au_data_strndup(key, key_len);
            goto end;
        }
        if ((fn->flags & AU_FN_FLAG_MAY_FAIL) !=
            (p_data->fns.data[entry].flags & AU_FN_FLAG_MAY_FAIL)) {
            au_fatal("invalid may fail %.*s\n", (int)key_len, key); // TODO
        }
        if (au_fn_num_args(fn) != imported_func->num_args) {
            retval.type = AU_INT_ERR_WRONG_ARGS;
            retval.data.wrong_args.key = au_data_strndup(key, key_len);
            retval.data.wrong_args.got_args = imported_func->num_args;
            retval.data.wrong_args.expected_args = au_fn_num_args(fn);
            goto end;
        }
        au_fn_fill_import_cache(&p_data->fns.data[entry], *fn_idx,
                                loaded_module);
    })
    AU_HM_VARS_FOREACH_PAIR(&relative_module->class_map, key, entry, {
        assert(p_data->classes.data[entry] == 0);
        const au_hm_var_value_t *class_idx =
            au_hm_vars_get(&loaded_module->class_map, key, key_len);
        if (class_idx == 0) {
            retval.type = AU_INT_ERR_UNKNOWN_CLASS;
            retval.data.unknown_id.key = au_data_strndup(key, key_len);
            goto end;
        }
        struct au_class_interface *class_interface =
            loaded_module->classes.data[*class_idx];
        if ((class_interface->flags & AU_CLASS_FLAG_EXPORTED) == 0) {
            retval.type = AU_INT_ERR_UNKNOWN_CLASS;
            retval.data.unknown_id.key = au_data_strndup(key, key_len);
            goto end;
        }
        p_data->classes.data[entry] = class_interface;
        au_class_interface_ref(class_interface);
    })
    AU_HM_VARS_FOREACH_PAIR(&relative_module->const_map, key, entry, {
        const au_hm_var_value_t *const_idx =
            au_hm_vars_get(&loaded_module->exported_consts, key, key_len);
        if (const_idx == 0) {
            retval.type = AU_INT_ERR_UNKNOWN_CONST;
            retval.data.unknown_id.key = au_data_strndup(key, key_len);
            goto end;
        }
        tl->const_cache[p_data->tl_constant_start + entry] =
            tl->const_cache[loaded_module->tl_constant_start + *const_idx];
    })
    if (relative_module->class_map.nitems > 0) {
        for (size_t i = 0; i < p_data->fns.len; i++) {
            au_fn_fill_class_cache(&p_data->fns.data[i], p_data);
        }
    }
end:
    return retval;
}

// * Error functions *

static struct au_interpreter_result bin_op_error(au_value_t left,
                                                 au_value_t right) {
    au_value_ref(left);
    au_value_ref(right);
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_INCOMPAT_BIN_OP,
        .data.incompat_bin_op.left = left,
        .data.incompat_bin_op.right = right,
        .pos = 0,
    };
}

static struct au_interpreter_result call_error() {
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_INCOMPAT_CALL,
        .pos = 0,
    };
}

static struct au_interpreter_result
indexing_non_collection_error(au_value_t value) {
    au_value_ref(value);
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_INDEXING_NON_COLLECTION,
        .data.invalid_collection.value = value,
        .pos = 0,
    };
}

static struct au_interpreter_result
invalid_index_error(au_value_t collection, au_value_t idx) {
    au_value_ref(collection);
    au_value_ref(idx);
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_INVALID_INDEX,
        .data.invalid_index.collection = collection,
        .data.invalid_index.idx = idx,
        .pos = 0,
    };
}

static struct au_interpreter_result import_path_resolve_error() {
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_IMPORT_PATH,
        .pos = 0,
    };
}

static struct au_interpreter_result circular_import_error() {
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_CIRCULAR_IMPORT,
        .pos = 0,
    };
}

static struct au_interpreter_result raised_error(au_value_t value) {
    au_value_ref(value);
    return (struct au_interpreter_result){
        .type = AU_INT_ERR_RAISED_ERROR,
        .data.raised_error.value = value,
        .pos = 0,
    };
}

static au_value_t extract_error_value(struct au_vm_thread_local *tl) {
    if (AU_UNLIKELY(tl->error.result.type != AU_INT_ERR_RAISED_ERROR))
        return au_value_error();
    au_value_t retval = tl->error.result.data.raised_error.value;
    tl->error.file = 0;
    tl->error.result.type = AU_INT_ERR_OK;
    tl->error.result.pos = 0;
    au_data_free(tl->backtrace.data);
    tl->backtrace.data = 0;
    tl->backtrace.len = 0;
    tl->backtrace.cap = 0;
    return retval;
}

// * Implementation *

au_value_t au_vm_exec_unverified(struct au_vm_thread_local *tl,
                                 const struct au_bc_storage *bcs,
                                 const struct au_program_data *p_data,
                                 const au_value_t *args) {
    struct au_vm_frame frame;

    // We add the frame to the linked list first,
    // because tl and frame are fresh in the stack/registers
    frame.link = tl->current_frame;
    tl->current_frame.bcs = bcs;
    tl->current_frame.data = p_data;
    tl->current_frame.frame = &frame;

    // Since frame.retval is right next to frame.link, we should update it
    // here too
    frame.retval = au_value_none();

#ifdef AU_STACK_GROWS_UP
    if (AU_UNLIKELY(((uintptr_t)&frame - tl->stack_start) >
                    tl->stack_max)) {
        au_fatal("stack overflow");
    }
#else
    if (AU_UNLIKELY((tl->stack_start - (uintptr_t)&frame) >
                    tl->stack_max)) {
        au_fatal("stack overflow");
    }
#endif

#ifdef AU_USE_ALLOCA
    au_value_t *alloca_values = 0;
    if (AU_LIKELY(bcs->num_values < ALLOCA_MAX_VALUES)) {
        alloca_values = alloca(bcs->num_values * sizeof(au_value_t));
        au_value_clear(alloca_values, bcs->num_values);
        frame.regs = alloca_values;
        frame.locals = &alloca_values[bcs->num_registers];
    } else {
        frame.regs = au_value_calloc(bcs->num_registers);
        frame.locals = au_value_calloc(bcs->num_locals);
    }
#else
    au_value_clear(frame.regs, bcs->num_registers);
    frame.locals = au_value_calloc(bcs->num_locals);
#endif

    for (int i = 0; i < bcs->num_args; i++) {
        frame.locals[i] = args[i];
    }

    frame.bc = 0;
    frame.bc_start = bcs->bc.data;
    frame.self = 0;

    /// This pointer is mutable because we want to do bytecode optimization
    /// on the fly. It should be thread safe if au_bc_storage isn't shared
    /// across threads
    register uint8_t *bc = (uint8_t *)bcs->bc.data;

#define FLUSH_BC()                                                        \
    do {                                                                  \
        frame.bc = bc;                                                    \
    } while (0)

#define DEF_BC16(VAR, OFFSET)                                             \
    const uint16_t VAR = *((uint16_t *)(&bc[OFFSET]))

#define RAISE(ERROR)                                                      \
    do {                                                                  \
        FLUSH_BC();                                                       \
        tl->error.file = p_data->file;                                    \
        tl->error.result = (ERROR);                                       \
        const size_t pc = frame.bc - frame.bc_start;                      \
        tl->error.result.pos = au_vm_locate_error(pc, bcs, p_data);       \
        frame.retval = au_value_error();                                  \
        goto end;                                                         \
    } while (0)

#define RAISE_BT()                                                        \
    do {                                                                  \
        FLUSH_BC();                                                       \
        const size_t pc = frame.bc - frame.bc_start;                      \
        if (tl->error.result.type == 0) {                                 \
            tl->error.file = p_data->file;                                \
            tl->error.result.type = AU_INT_ERR_INCOMPAT_CALL;             \
            tl->error.result.pos = au_vm_locate_error(pc, bcs, p_data);   \
        } else {                                                          \
            struct au_vm_trace_item item;                                 \
            item.file = p_data->file;                                     \
            item.pos = au_vm_locate_error(pc, bcs, p_data);               \
            au_vm_trace_item_array_add(&tl->backtrace, item);             \
        }                                                                 \
        frame.retval = au_value_error();                                  \
        goto end;                                                         \
    } while (0)

    while (1) {
#ifdef DEBUG_VM
#define DISPATCH_DEBUG debug_frame(&frame);
#else
#define DISPATCH_DEBUG
#endif

#ifndef AU_USE_DISPATCH_JMP
#define PREFETCH_INSN
#define CASE(x) case x
#define DISPATCH                                                          \
    DISPATCH_DEBUG;                                                       \
    bc += 4;                                                              \
    continue
#define DISPATCH_JMP                                                      \
    DISPATCH_DEBUG;                                                       \
    continue
        switch (bc[0]) {
#else

#ifdef AU_FEAT_PREFETCH_INSN
#define PREFETCH_INSN register const void *_next_insn = cb[bc[4]];
#define DISPATCH                                                          \
    do {                                                                  \
        DISPATCH_DEBUG;                                                   \
        bc += 4;                                                          \
        goto *_next_insn;                                                 \
    } while (0)
#else
#define DISPATCH                                                          \
    do {                                                                  \
        DISPATCH_DEBUG;                                                   \
        bc += 4;                                                          \
        uint8_t op = bc[0];                                               \
        goto *cb[op];                                                     \
    } while (0)
#endif

#define DISPATCH_JMP                                                      \
    do {                                                                  \
        DISPATCH_DEBUG;                                                   \
        uint8_t op = bc[0];                                               \
        goto *cb[op];                                                     \
    } while (0)

#define CASE(x) CB_##x
#include "core/bc_data/callbacks.txt"
        goto *cb[bc[0]];
#endif

#ifdef AU_FEAT_DELAYED_RC
#define COPY_VALUE(dest, src)                                             \
    do {                                                                  \
        dest = src;                                                       \
    } while (0)
#else
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
#endif

            CASE(AU_OP_LOAD_SELF) : {
                PREFETCH_INSN;

                frame.self = (struct au_obj_class *)au_value_get_struct(
                    frame.locals[0]);
#ifdef AU_FEAT_DELAYED_RC // clang-format off
                // INVARIANT(GC): no need to increment the rc
#else
                au_obj_ref(frame.self);
#endif // clang-format on

                DISPATCH;
            }
            // Register/local move operations
            CASE(AU_OP_MOV_U16) : {
                const uint8_t reg = bc[1];
                DEF_BC16(n, 2);
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[reg], au_value_int(n));

                DISPATCH;
            }
            CASE(AU_OP_MOV_REG_LOCAL) : {
                const uint8_t reg = bc[1];
                DEF_BC16(local, 2);
                PREFETCH_INSN;

                COPY_VALUE(frame.locals[local], frame.regs[reg]);

                DISPATCH;
            }
            CASE(AU_OP_MOV_LOCAL_REG) : {
                const uint8_t reg = bc[1];
                DEF_BC16(local, 2);
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[reg], frame.locals[local]);

                DISPATCH;
            }
            CASE(AU_OP_MOV_BOOL) : {
                const uint8_t n = bc[1];
                const uint8_t reg = bc[2];
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[reg], au_value_bool(n));

                DISPATCH;
            }
            CASE(AU_OP_LOAD_NIL) : {
                const uint8_t reg = bc[1];
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[reg], au_value_none());

                DISPATCH;
            }
            CASE(AU_OP_LOAD_CONST) : {
                const uint8_t reg = bc[1];
                DEF_BC16(rel_c, 2);
                PREFETCH_INSN;

                const size_t abs_c = rel_c + p_data->tl_constant_start;
                au_value_t v;
                if (au_value_get_type(tl->const_cache[abs_c]) !=
                    AU_VALUE_NONE) {
                    v = tl->const_cache[abs_c];
                } else {
                    const struct au_program_data_val *data_val =
                        &p_data->data_val.data[rel_c];
                    v = data_val->real_value;
                    switch (au_value_get_type(v)) {
                    case AU_VALUE_STR: {
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
            CASE(AU_OP_SET_CONST) : {
                const uint8_t reg = bc[1];
                DEF_BC16(rel_c, 2);
                PREFETCH_INSN;

                const size_t abs_c = rel_c + p_data->tl_constant_start;
                if (au_value_get_type(tl->const_cache[abs_c]) ==
                    AU_VALUE_NONE)
                    tl->const_cache[abs_c] = frame.regs[reg];

                DISPATCH;
            }
            // Unary operations
            CASE(AU_OP_NOT) : {
                const uint8_t reg = bc[1];
                const uint8_t ret = bc[2];
                PREFETCH_INSN;

                if (AU_LIKELY(au_value_get_type(frame.regs[reg]) ==
                              AU_VALUE_BOOL)) {
                    COPY_VALUE(frame.regs[ret],
                               au_value_bool(
                                   !au_value_get_bool(frame.regs[reg])));
                } else {
                    COPY_VALUE(frame.regs[ret],
                               au_value_bool(
                                   !au_value_is_truthy(frame.regs[reg])));
                }

                DISPATCH;
            }
            CASE(AU_OP_BNOT) : {
                const uint8_t reg = bc[1];
                const uint8_t ret = bc[2];
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[ret],
                           au_value_bnot(frame.regs[reg]));

                // FIXME: unary operator error
                if (AU_UNLIKELY(au_value_is_error(frame.regs[ret])))
                    RAISE(call_error());

                DISPATCH;
            }
            CASE(AU_OP_NEG) : {
                const uint8_t reg = bc[1];
                const uint8_t ret = bc[2];
                PREFETCH_INSN;

                // FIXME: unary operator error
                if (AU_UNLIKELY(au_value_get_type(frame.regs[reg]) !=
                                AU_VALUE_INT))
                    RAISE(call_error());

                frame.regs[ret] =
                    au_value_int(-au_value_get_int(frame.regs[reg]));

                DISPATCH;
            }
            // Binary operations
#define SPECIALIZED_INT_ONLY(NAME)                                        \
    if ((au_value_get_type(lhs) == AU_VALUE_INT) &&                       \
        (au_value_get_type(rhs) == AU_VALUE_INT)) {                       \
        bc[0] = NAME##_INT;                                               \
        goto _##NAME##_INT;                                               \
    }

#define SPECIALIZED_INT_AND_DOUBLE(NAME)                                  \
    SPECIALIZED_INT_ONLY(NAME)                                            \
    else if ((au_value_get_type(lhs) == AU_VALUE_DOUBLE) &&               \
             (au_value_get_type(rhs) == AU_VALUE_DOUBLE)) {               \
        bc[0] = NAME##_DOUBLE;                                            \
        goto _##NAME##_DOUBLE;                                            \
    }

// This macro stops the compiler from warning us about unused labels
#define NO_SPECIALIZER(NAME)                                              \
    while (0) {                                                           \
        goto _##NAME;                                                     \
    }

#ifdef AU_FEAT_DELAYED_RC
#define BIN_OP(NAME, FUN, SPECIALIZER)                                    \
    CASE(NAME) : {                                                        \
        _##NAME:;                                                         \
        const au_value_t lhs = frame.regs[bc[1]];                         \
        const au_value_t rhs = frame.regs[bc[2]];                         \
        const uint8_t res = bc[3];                                        \
        PREFETCH_INSN;                                                    \
                                                                          \
        SPECIALIZER                                                       \
        const au_value_t result = au_value_##FUN(lhs, rhs);               \
        if (au_value_is_error(result)) {                                  \
            RAISE(bin_op_error(lhs, rhs));                                \
        }                                                                 \
        frame.regs[res] = result;                                         \
        /* INVARIANT(GC): value operations always return an RC'd result   \
         */                                                               \
        au_value_deref(result);                                           \
                                                                          \
        DISPATCH;                                                         \
    }
#else
#define BIN_OP(NAME, FUN, SPECIALIZER)                                    \
    CASE(NAME) : {                                                        \
        _##NAME:;                                                         \
        const au_value_t lhs = frame.regs[bc[1]];                         \
        const au_value_t rhs = frame.regs[bc[2]];                         \
        const uint8_t res = bc[3];                                        \
        PREFETCH_INSN;                                                    \
                                                                          \
        SPECIALIZER                                                       \
        const au_value_t result = au_value_##FUN(lhs, rhs);               \
        if (au_value_is_error(result)) {                                  \
            RAISE(bin_op_error(lhs, rhs));                                \
        }                                                                 \
        MOVE_VALUE(frame.regs[res], result);                              \
                                                                          \
        DISPATCH;                                                         \
    }
#endif
            BIN_OP(AU_OP_MUL, mul, SPECIALIZED_INT_AND_DOUBLE(AU_OP_MUL))
            BIN_OP(AU_OP_DIV, div, SPECIALIZED_INT_AND_DOUBLE(AU_OP_DIV))
            BIN_OP(AU_OP_ADD, add, SPECIALIZED_INT_AND_DOUBLE(AU_OP_ADD))
            BIN_OP(AU_OP_SUB, sub, SPECIALIZED_INT_AND_DOUBLE(AU_OP_SUB))
            BIN_OP(AU_OP_MOD, mod, SPECIALIZED_INT_ONLY(AU_OP_MOD))
            BIN_OP(AU_OP_EQ, eq, SPECIALIZED_INT_AND_DOUBLE(AU_OP_EQ))
            BIN_OP(AU_OP_NEQ, neq, SPECIALIZED_INT_AND_DOUBLE(AU_OP_NEQ))
            BIN_OP(AU_OP_LT, lt, SPECIALIZED_INT_AND_DOUBLE(AU_OP_LT))
            BIN_OP(AU_OP_GT, gt, SPECIALIZED_INT_AND_DOUBLE(AU_OP_GT))
            BIN_OP(AU_OP_LEQ, leq, SPECIALIZED_INT_AND_DOUBLE(AU_OP_LEQ))
            BIN_OP(AU_OP_GEQ, geq, SPECIALIZED_INT_AND_DOUBLE(AU_OP_GEQ))
            BIN_OP(AU_OP_BOR, bor, NO_SPECIALIZER(AU_OP_BOR))
            BIN_OP(AU_OP_BXOR, bxor, NO_SPECIALIZER(AU_OP_BXOR))
            BIN_OP(AU_OP_BAND, band, NO_SPECIALIZER(AU_OP_BAND))
            BIN_OP(AU_OP_BSHL, bshl, NO_SPECIALIZER(AU_OP_BSHL))
            BIN_OP(AU_OP_BSHR, bshr, NO_SPECIALIZER(AU_OP_BSHR))
#undef SPECIALIZED_INT_ONLY
#undef SPECIALIZED_INT_AND_DOUBLE
#undef BIN_OP
            // Binary operations (specialized on int)
#ifdef AU_FEAT_DELAYED_RC
#define FAST_MOVE_VALUE(dest, src) dest = src;
#else
#define FAST_MOVE_VALUE(dest, src) MOVE_VALUE(dest, src)
#endif
#define BIN_OP(NAME, EXPR)                                                \
    CASE(NAME##_INT) : {                                                  \
        _##NAME##_INT:;                                                   \
        const au_value_t lhs = frame.regs[bc[1]];                         \
        const au_value_t rhs = frame.regs[bc[2]];                         \
        const uint8_t res = bc[3];                                        \
        PREFETCH_INSN;                                                    \
                                                                          \
        if (AU_UNLIKELY((au_value_get_type(lhs) != AU_VALUE_INT) ||       \
                        (au_value_get_type(rhs) != AU_VALUE_INT))) {      \
            bc[0] = NAME;                                                 \
            goto _##NAME;                                                 \
        }                                                                 \
        const int32_t li = au_value_get_int(lhs),                         \
                      ri = au_value_get_int(rhs);                         \
        const au_value_t result = EXPR;                                   \
        FAST_MOVE_VALUE(frame.regs[res], result);                         \
                                                                          \
        DISPATCH;                                                         \
    }
            BIN_OP(AU_OP_ADD, au_value_int(au_platform_iadd_wrap(li, ri)))
            BIN_OP(AU_OP_SUB, au_value_int(au_platform_isub_wrap(li, ri)))
            BIN_OP(AU_OP_MUL, au_value_int(au_platform_imul_wrap(li, ri)))
            BIN_OP(AU_OP_DIV, au_value_double((double)li / (double)ri))
            BIN_OP(AU_OP_MOD, au_value_int(li % ri))
            BIN_OP(AU_OP_EQ, au_value_bool(li == ri))
            BIN_OP(AU_OP_NEQ, au_value_bool(li != ri))
            BIN_OP(AU_OP_LT, au_value_bool(li < ri))
            BIN_OP(AU_OP_GT, au_value_bool(li > ri))
            BIN_OP(AU_OP_LEQ, au_value_bool(li <= ri))
            BIN_OP(AU_OP_GEQ, au_value_bool(li >= ri))
#undef BIN_OP
#undef FAST_MOVE_VALUE
            // Binary operations (specialized on float)
#ifdef AU_FEAT_DELAYED_RC
#define FAST_MOVE_VALUE(dest, src) dest = src;
#else
#define FAST_MOVE_VALUE(dest, src) MOVE_VALUE(dest, src)
#endif
#define BIN_OP(NAME, OP, TYPE)                                            \
    CASE(NAME##_DOUBLE) : {                                               \
        _##NAME##_DOUBLE:;                                                \
        const au_value_t lhs = frame.regs[bc[1]];                         \
        const au_value_t rhs = frame.regs[bc[2]];                         \
        const uint8_t res = bc[3];                                        \
        PREFETCH_INSN;                                                    \
                                                                          \
        if (AU_UNLIKELY((au_value_get_type(lhs) != AU_VALUE_DOUBLE) ||    \
                        (au_value_get_type(rhs) != AU_VALUE_DOUBLE))) {   \
            bc[0] = NAME;                                                 \
            goto _##NAME;                                                 \
        }                                                                 \
        const au_value_t result =                                         \
            TYPE(au_value_get_double(lhs) OP au_value_get_double(rhs));   \
        FAST_MOVE_VALUE(frame.regs[res], result);                         \
                                                                          \
        DISPATCH;                                                         \
    }
            BIN_OP(AU_OP_MUL, *, au_value_double)
            BIN_OP(AU_OP_DIV, /, au_value_double)
            BIN_OP(AU_OP_ADD, +, au_value_double)
            BIN_OP(AU_OP_SUB, -, au_value_double)
            BIN_OP(AU_OP_EQ, ==, au_value_bool)
            BIN_OP(AU_OP_NEQ, !=, au_value_bool)
            BIN_OP(AU_OP_LT, <, au_value_bool)
            BIN_OP(AU_OP_GT, >, au_value_bool)
            BIN_OP(AU_OP_LEQ, <=, au_value_bool)
            BIN_OP(AU_OP_GEQ, >=, au_value_bool)
#undef BIN_OP
#undef FAST_MOVE_VALUE
            // Jump instructions
            CASE(AU_OP_JIF) : {
_AU_OP_JIF:;
                const au_value_t cmp = frame.regs[bc[1]];
                DEF_BC16(n, 2);
                PREFETCH_INSN;

                const size_t offset = ((size_t)n) * 4;
                if (au_value_get_type(cmp) == AU_VALUE_BOOL) {
                    bc[0] = AU_OP_JIF_BOOL;
                }
                if (au_value_is_truthy(cmp)) {
                    bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(AU_OP_JNIF) : {
_AU_OP_JNIF:;
                const au_value_t cmp = frame.regs[bc[1]];
                DEF_BC16(n, 2);
                PREFETCH_INSN;

                const size_t offset = ((size_t)n) * 4;
                if (au_value_get_type(cmp) == AU_VALUE_BOOL) {
                    bc[0] = AU_OP_JNIF_BOOL;
                }
                if (!au_value_is_truthy(cmp)) {
                    bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(AU_OP_JREL) : {
                DEF_BC16(n, 2);
                const size_t offset = ((size_t)n) * 4;
                bc += offset;
                DISPATCH_JMP;
            }
            CASE(AU_OP_JRELB) : {
                DEF_BC16(n, 2);
                const size_t offset = ((size_t)n) * 4;
                bc -= offset;
                DISPATCH_JMP;
            }
            // Jump instructions optimized on bools
            CASE(AU_OP_JIF_BOOL) : {
                const au_value_t cmp = frame.regs[bc[1]];
                DEF_BC16(n, 2);
                PREFETCH_INSN;

                const size_t offset = ((size_t)n) * 4;
                if (AU_UNLIKELY(au_value_get_type(cmp) != AU_VALUE_BOOL)) {
                    bc[0] = AU_OP_JIF;
                    goto _AU_OP_JIF;
                }
                if (au_value_get_bool(cmp)) {
                    bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            CASE(AU_OP_JNIF_BOOL) : {
                const au_value_t cmp = frame.regs[bc[1]];
                DEF_BC16(n, 2);
                PREFETCH_INSN;

                const size_t offset = ((size_t)n) * 4;
                if (AU_UNLIKELY(au_value_get_type(cmp) != AU_VALUE_BOOL)) {
                    bc[0] = AU_OP_JNIF;
                    goto _AU_OP_JNIF;
                }
                if (!au_value_get_bool(cmp)) {
                    bc += offset;
                    DISPATCH_JMP;
                } else {
                    DISPATCH;
                }
            }
            // Call instructions
            // clang-format off
            CASE(AU_OP_CALL): 
            CASE(AU_OP_CALL_CATCH): // clang-format on
            {
                const uint8_t opcode = bc[0];
                const uint8_t ret_reg = bc[1];
                DEF_BC16(func_id, 2);
                bc += 4;

                const struct au_fn *call_fn = &p_data->fns.data[func_id];
                int num_args = au_fn_num_args(call_fn);
                au_value_t *args = au_value_calloc(num_args);
                for (int i = 0; i < num_args;) {
                    bc++; // OP_PUSH_ARG
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 3;
                        break;
                    }
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 2;
                        break;
                    }
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 1;
                        break;
                    }
                }
                for (int i = 0; i < num_args; i++)
                    au_value_ref(args[i]);

                FLUSH_BC();

                int is_native = 0;
                au_value_t callee_retval = au_fn_call_internal(
                    call_fn, tl, p_data, args, &is_native);
                if (au_value_is_error(callee_retval)) {
                    if (opcode == AU_OP_CALL_CATCH) {
                        callee_retval = extract_error_value(tl);
                    }
                    if (au_value_is_error(callee_retval)) {
                        au_data_free(args);
                        RAISE_BT();
                    }
                }

#ifdef AU_FEAT_DELAYED_RC // clang-format off
                frame.regs[ret_reg] = callee_retval;
                // INVARIANT(GC): native functions always return
                // a RC'd value
                if (is_native)
                    au_value_deref(callee_retval);
#else
                MOVE_VALUE(frame.regs[ret_reg], callee_retval);
#endif // clang-format on
                if (is_native) {
                    // Native functions do not clean up the argument stack
                    // for us, we have to do it manually.
                    for (int i = 0; i < num_args; i++) {
                        au_value_deref(args[i]);
                    }
                }
                au_data_free(args);

                DISPATCH_JMP;
            }
            // Function values
            CASE(AU_OP_LOAD_FUNC) : {
                const uint8_t reg = bc[1];
                DEF_BC16(func_id, 2);
                PREFETCH_INSN;

                const struct au_fn *fn = &p_data->fns.data[func_id];
                struct au_fn_value *fn_value =
                    au_fn_value_from_vm(fn, p_data);
#ifdef AU_FEAT_DELAYED_RC // clang-format off
                frame.regs[reg] = au_value_fn(fn_value);
                // INVARIANT(GC): from au_fn_value_from_vm
                au_value_deref(frame.regs[reg]);
#else
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_fn(fn_value)
                );
#endif // clang-format on

                DISPATCH;
            }
            CASE(AU_OP_BIND_ARG_TO_FUNC) : {
                const uint8_t func_reg = bc[1];
                const uint8_t arg_reg = bc[2];
                PREFETCH_INSN;

                struct au_fn_value *fn_value =
                    au_fn_value_coerce(frame.regs[func_reg]);
                if (AU_LIKELY(fn_value != 0)) {
                    au_fn_value_add_arg(fn_value, frame.regs[arg_reg]);
                } else {
                    assert(0);
                }

                DISPATCH;
            }
            // clang-format off
            CASE(AU_OP_CALL_FUNC_VALUE):
            CASE(AU_OP_CALL_FUNC_VALUE_CATCH): // clang-format on
            {
                const uint8_t opcode = bc[0];
                const uint8_t func_reg = bc[1];
                const uint8_t num_args = bc[2];
                const uint8_t ret_reg = bc[3];
                bc += 4;

                au_value_t *args = au_value_calloc(num_args);
                for (int i = 0; i < num_args;) {
                    bc++; // OP_PUSH_ARG
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 3;
                        break;
                    }
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 2;
                        break;
                    }
                    if (i < num_args) {
                        args[i] = frame.regs[*bc];
                        bc++;
                        i++;
                    } else {
                        bc += 1;
                        break;
                    }
                }
                for (int i = 0; i < num_args; i++)
                    au_value_ref(args[i]);

                struct au_fn_value *fn_value =
                    au_fn_value_coerce(frame.regs[func_reg]);
                if (AU_LIKELY(fn_value != 0)) {
                    int is_native = 0;
                    au_value_t callee_retval = au_fn_value_call_vm(
                        fn_value, tl, args, num_args, &is_native);
                    if (au_value_is_error(callee_retval)) {
                        if (opcode == AU_OP_CALL_FUNC_VALUE_CATCH) {
                            callee_retval = extract_error_value(tl);
                        }
                        if (au_value_is_error(callee_retval)) {
                            au_data_free(args);
                            RAISE(call_error(p_data, &frame));
                        }
                    }

#ifdef AU_FEAT_DELAYED_RC // clang-format off
                    frame.regs[ret_reg] = callee_retval;
                    // INVARIANT(GC): native functions always return
                    // a RC'd value
                    if (is_native)
                        au_value_deref(callee_retval);
#else
                    MOVE_VALUE(frame.regs[ret_reg], callee_retval);
#endif // clang-format on
                    if (is_native) {
                        // Native functions do not clean up the argument
                        // stack for us, we have to do it manually.
                        for (int i = 0; i < num_args; i++) {
                            au_value_deref(args[i]);
                        }
                    }
                    au_data_free(args);
                } else {
                    abort(); // TODO
                }

                DISPATCH_JMP;
            }
            // Return instructions
            CASE(AU_OP_RET_LOCAL) : {
                DEF_BC16(local, 2);
                // Move ownership of value in ret_local -> return reg in
                // prev. frame
                frame.retval = frame.locals[local];
                frame.locals[local] = au_value_none();
                goto end;
            }
            CASE(AU_OP_RET) : {
                const uint8_t ret_reg = bc[1];
                // Move ownership of value in ret_reg -> return reg in
                // prev. frame
                frame.retval = frame.regs[ret_reg];
                frame.regs[ret_reg] = au_value_none();
                goto end;
            }
            CASE(AU_OP_RET_NULL) : { goto end; }
            // Array instructions
            CASE(AU_OP_ARRAY_NEW) : {
                const uint8_t reg = bc[1];
                DEF_BC16(capacity, 2);
                PREFETCH_INSN;

#ifdef AU_FEAT_DELAYED_RC // clang-format off
                struct au_struct *s =
                    (struct au_struct *)au_obj_array_new(capacity);
                frame.regs[reg] = au_value_struct(s);
                // INVARIANT(GC): from au_obj_array_new
                au_obj_deref(s);
#else
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_array_new(capacity)
                    )
                );
#endif // clang-format on

                DISPATCH;
            }
            CASE(AU_OP_ARRAY_PUSH) : {
                const au_value_t array_val = frame.regs[bc[1]];
                const au_value_t value_val = frame.regs[bc[2]];
                PREFETCH_INSN;

                struct au_obj_array *obj_array =
                    au_obj_array_coerce(array_val);
                if (AU_LIKELY(obj_array != 0)) {
                    au_obj_array_push(obj_array, value_val);
                }

                DISPATCH;
            }
            CASE(AU_OP_IDX_GET) : {
                const au_value_t col_val = frame.regs[bc[1]];
                const au_value_t idx_val = frame.regs[bc[2]];
                PREFETCH_INSN;

                const uint8_t ret_reg = bc[3];
                struct au_struct *collection = au_struct_coerce(col_val);
                if (AU_LIKELY(collection != 0)) {
                    au_value_t value;
                    if (!collection->vdata->idx_get_fn(collection, idx_val,
                                                       &value)) {
                        RAISE(invalid_index_error(col_val, idx_val));
                    }
                    COPY_VALUE(frame.regs[ret_reg], value);
                } else {
                    RAISE(indexing_non_collection_error(col_val));
                }

                DISPATCH;
            }
            CASE(AU_OP_IDX_SET) : {
                const au_value_t col_val = frame.regs[bc[1]];
                const au_value_t idx_val = frame.regs[bc[2]];
                const au_value_t value_val = frame.regs[bc[3]];
                PREFETCH_INSN;

                struct au_struct *collection = au_struct_coerce(col_val);
                if (AU_LIKELY(collection != 0)) {
                    if (AU_UNLIKELY(collection->vdata->idx_set_fn(
                                        collection, idx_val, value_val) ==
                                    0)) {
                        RAISE(invalid_index_error(col_val, idx_val));
                    }
                } else {
                    RAISE(indexing_non_collection_error(col_val));
                }

                DISPATCH;
            }
            // Tuple instructions
            CASE(AU_OP_TUPLE_NEW) : {
                const uint8_t reg = bc[1];
                DEF_BC16(length, 2);
                PREFETCH_INSN;

#ifdef AU_FEAT_DELAYED_RC // clang-format off
                struct au_struct *s =
                    (struct au_struct *)au_obj_tuple_new(length);
                frame.regs[reg] = au_value_struct(s);
                // INVARIANT(GC): from au_obj_tuple_new
                au_obj_deref(s);
#else
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_tuple_new(length)
                    )
                );
#endif // clang-format on

                DISPATCH;
            }
            CASE(AU_OP_IDX_SET_STATIC) : {
                const au_value_t col_val = frame.regs[bc[1]];
                const au_value_t idx_val = au_value_int(bc[2]);
                const au_value_t value_val = frame.regs[bc[3]];
                PREFETCH_INSN;

                struct au_struct *collection = au_struct_coerce(col_val);
                if (AU_LIKELY(collection != 0)) {
                    if (AU_UNLIKELY(collection->vdata->idx_set_fn(
                                        collection, idx_val, value_val) ==
                                    0)) {
                        RAISE(invalid_index_error(col_val, idx_val));
                    }
                } else {
                    RAISE(indexing_non_collection_error(col_val));
                }

                DISPATCH;
            }
            // Dictionary instructions
            CASE(AU_OP_DICT_NEW) : {
                const uint8_t reg = bc[1];
                PREFETCH_INSN;

#ifdef AU_FEAT_DELAYED_RC // clang-format off
                struct au_struct *s =
                    (struct au_struct *)au_obj_dict_new();
                frame.regs[reg] = au_value_struct(s);
                // INVARIANT(GC): from au_obj_dict_new
                au_obj_deref(s);
#else
                MOVE_VALUE(
                    frame.regs[reg],
                    au_value_struct(
                        (struct au_struct *)au_obj_dict_new()
                    )
                );
#endif // clang-format on

                DISPATCH;
            }
            // Class instructions
            CASE(AU_OP_CLASS_NEW) : {
                const uint8_t reg = bc[1];
                DEF_BC16(class_id, 2);
                PREFETCH_INSN;

                struct au_struct *obj_class =
                    (struct au_struct *)au_obj_class_new(
                        p_data->classes.data[class_id]);
                const au_value_t new_value = au_value_struct(obj_class);
#ifdef AU_FEAT_DELAYED_RC // clang-format off
                frame.regs[reg] = new_value;
                // INVARIANT(GC): from au_obj_class_new
                au_obj_deref(obj_class);
#else
                MOVE_VALUE(frame.regs[reg], new_value);
#endif // clang-format on

                DISPATCH;
            }
            CASE(AU_OP_CLASS_NEW_INITIALZIED) : {
                const uint8_t reg = bc[1];
                DEF_BC16(class_id, 2);

                struct au_obj_class *obj_class =
                    au_obj_class_new(p_data->classes.data[class_id]);
                const au_value_t new_value =
                    au_value_struct((struct au_struct *)obj_class);
#ifdef AU_FEAT_DELAYED_RC // clang-format off
                frame.regs[reg] = new_value;
                // INVARIANT(GC): from au_obj_class_new
                au_obj_deref(obj_class);
#else
                MOVE_VALUE(frame.regs[reg], new_value);
#endif // clang-format on

                while (*bc != AU_OP_NOP) {
                    const uint8_t reg = bc[1];
                    DEF_BC16(inner, 2);
                    bc += 4;
                    au_value_ref(frame.regs[reg]);
                    obj_class->data[inner] = frame.regs[reg];
                }

                bc += 4; // skip AU_OP_NOP
                DISPATCH_JMP;
            }
            CASE(AU_OP_CLASS_GET_INNER) : {
                const uint8_t reg = bc[1];
                DEF_BC16(inner, 2);
                PREFETCH_INSN;

                COPY_VALUE(frame.regs[reg], frame.self->data[inner]);

                DISPATCH;
            }
            CASE(AU_OP_CLASS_SET_INNER) : {
                const uint8_t reg = bc[1];
                DEF_BC16(inner, 2);
                PREFETCH_INSN;

                COPY_VALUE(frame.self->data[inner], frame.regs[reg]);

                DISPATCH;
            }
            // Module instructions
            CASE(AU_OP_IMPORT) : {
                DEF_BC16(idx, 2);
                PREFETCH_INSN;

                const size_t relative_module_idx =
                    p_data->imports.data[idx].module_idx;
                const char *relpath = p_data->imports.data[idx].path;

                struct au_module_resolve_result resolve_res;
                if (!au_module_resolve(&resolve_res, relpath, p_data->cwd))
                    RAISE(import_path_resolve_error());

                const char *module_path = resolve_res.abspath;

                char *module_path_with_subpath = 0;
                if (resolve_res.subpath != 0) {
                    const size_t len = strlen(resolve_res.abspath) +
                                       strlen(resolve_res.subpath) + 2;
                    module_path_with_subpath = au_data_malloc(len + 1);
                    snprintf(module_path_with_subpath, len, "%s:%s",
                             resolve_res.abspath, resolve_res.subpath);
                    module_path_with_subpath[len] = 0;
                    module_path = module_path_with_subpath;
                }

                struct au_program_data *loaded_module =
                    au_vm_thread_local_get_module(tl, module_path);
                if (loaded_module != 0) {
                    if (module_path_with_subpath != 0)
                        au_data_free(module_path_with_subpath);
                    au_module_resolve_result_del(&resolve_res);

                    if (relative_module_idx !=
                        AU_PROGRAM_IMPORT_NO_MODULE) {
                        struct au_interpreter_result res =
                            link_to_imported(tl, p_data,
                                             relative_module_idx,
                                             loaded_module);
                        if (res.type != AU_INT_ERR_OK)
                            RAISE(res);
                    }

                    DISPATCH;
                }

                uint32_t tl_module_idx = ((uint32_t)-1);
                // TODO: deallocate
                if (!au_vm_thread_local_reserve_module(tl, module_path,
                                                       &tl_module_idx))
                    RAISE(circular_import_error());

                if (module_path_with_subpath != 0)
                    au_data_free(module_path_with_subpath);
                module_path = 0;

                struct au_module module;
                switch (au_module_import(&module, &resolve_res)) {
                case AU_MODULE_IMPORT_SUCCESS: {
                    break;
                }
                case AU_MODULE_IMPORT_SUCCESS_NO_MODULE: {
                    goto _import_dispatch;
                }
                case AU_MODULE_IMPORT_FAIL:
                case AU_MODULE_IMPORT_FAIL_DL: {
                    RAISE(import_path_resolve_error());
                    break;
                }
                }

                switch (module.type) {
                case AU_MODULE_SOURCE: {
                    struct au_mmap_info mmap = module.data.source;

                    struct au_program program;
                    struct au_parser_result parse_res =
                        au_parse(mmap.bytes, mmap.size, &program);
                    if (parse_res.type != AU_PARSER_RES_OK) {
                        au_print_parser_error(
                            parse_res, (struct au_error_location){
                                           .src = mmap.bytes,
                                           .len = mmap.size,
                                           .path = resolve_res.abspath,
                                       });
                        RAISE_BT();
                    }

                    program.data.tl_constant_start = tl->const_len;
                    au_vm_thread_local_add_const_cache(
                        tl, program.data.data_val.len);

                    if (!au_split_path(resolve_res.abspath,
                                       &program.data.file,
                                       &program.data.cwd))
                        RAISE(import_path_resolve_error());

                    au_module_resolve_result_del(&resolve_res);

                    // FIXME: deallocate
                    if (au_value_is_error(
                            au_vm_exec_unverified_main(tl, &program)))
                        RAISE_BT();

                    au_bc_storage_del(&program.main);

                    struct au_program_data *loaded_module =
                        au_data_malloc(sizeof(struct au_program_data));
                    memcpy(loaded_module, &program.data,
                           sizeof(struct au_program_data));
                    au_vm_thread_local_add_module(tl, tl_module_idx,
                                                  loaded_module);

                    if (relative_module_idx !=
                        AU_PROGRAM_IMPORT_NO_MODULE) {
                        struct au_interpreter_result res =
                            link_to_imported(tl, p_data,
                                             relative_module_idx,
                                             loaded_module);
                        if (res.type != AU_INT_ERR_OK)
                            RAISE(res);
                    }
                    break;
                }
                case AU_MODULE_LIB: {
                    struct au_program_data *loaded_module =
                        module.data.lib.lib;
                    module.data.lib.lib = 0;

                    au_vm_thread_local_add_module(tl, tl_module_idx,
                                                  loaded_module);

                    if (relative_module_idx !=
                        AU_PROGRAM_IMPORT_NO_MODULE) {
                        struct au_interpreter_result res =
                            link_to_imported(tl, p_data,
                                             relative_module_idx,
                                             loaded_module);
                        if (res.type != AU_INT_ERR_OK)
                            RAISE(res);
                    }
                    break;
                }
                }
_import_dispatch:;
                DISPATCH;
            }
            // Exceptions
            CASE(AU_OP_RAISE) : {
                const au_value_t reg = frame.regs[bc[1]];
                RAISE(raised_error(reg));
            }
            // Other
            CASE(AU_OP_PRINT) : {
                const au_value_t reg = frame.regs[bc[1]];
                PREFETCH_INSN;

                tl->print_fn(reg);

                DISPATCH;
            }
            // clang-format off
            CASE(AU_OP_PUSH_ARG) :
            CASE(AU_OP_NOP) : {
                PREFETCH_INSN;
                DISPATCH;
            }
            // clang-format on
#undef COPY_VALUE
#ifndef AU_USE_DISPATCH_JMP
        }
#endif
    }
end:
#ifdef AU_USE_ALLOCA
    if (AU_LIKELY(alloca_values != 0)) {
#ifndef AU_FEAT_DELAYED_RC
        for (int i = 0; i < bcs->num_values; i++) {
            au_value_deref(alloca_values[i]);
        }
#endif
    } else {
#ifndef AU_FEAT_DELAYED_RC
        for (int i = 0; i < bcs->num_registers; i++) {
            au_value_deref(frame.regs[i]);
        }
        for (int i = 0; i < bcs->num_locals; i++) {
            au_value_deref(frame.locals[i]);
        }
#endif
        au_data_free(frame.regs);
        au_data_free(frame.locals);
        frame.regs = 0;
        frame.locals = 0;
    }
#else
#ifndef AU_FEAT_DELAYED_RC
    for (int i = 0; i < bcs->num_registers; i++) {
        au_value_deref(frame.regs[i]);
    }
    for (int i = 0; i < bcs->num_locals; i++) {
        au_value_deref(frame.locals[i]);
    }
#endif
    au_data_free(frame.locals);
#endif

#ifdef AU_FEAT_DELAYED_RC
    // INVARIANT(GC): we don't hold a ref to self
#else
    if (frame.self != 0)
        au_obj_deref(frame.self);
#endif

    tl->current_frame = frame.link;
    return frame.retval;
}

au_value_t au_vm_exec_unverified_main(struct au_vm_thread_local *tl,
                                      struct au_program *program) {
    for (size_t relative_idx = 0;
         relative_idx < program->data.imported_modules.len;
         relative_idx++) {
        const struct au_imported_module *module =
            &program->data.imported_modules.data[relative_idx];
        if (module->stdlib_module_idx != AU_IMPORTED_MODULE_NOT_STDLIB) {
            const struct au_program_data *stdlib_module =
                au_program_data_array_at(&tl->stdlib_modules,
                                         module->stdlib_module_idx);
            struct au_interpreter_result res = link_to_imported(
                tl, &program->data, (uint32_t)relative_idx, stdlib_module);
            if (AU_UNLIKELY(res.type != AU_INT_ERR_OK)) {
                // TODO: error location
                tl->error.result = res;
                tl->error.file = program->data.file;
                tl->error.result.pos = 0;
                return au_value_error();
            }
        }
    }
    return au_vm_exec_unverified(tl, &program->main, &program->data, 0);
}
