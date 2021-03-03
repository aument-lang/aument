// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#include "platform/mmap.h"
#include "platform/path.h"

#include "core/bc.h"
#include "core/bit_array.h"
#include "core/hm_vars.h"
#include "core/parser/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"

#include "c_comp.h"

struct au_c_comp_module {
    struct au_hm_vars fn_map;
    struct au_fn_array fns;
    char *source;
};

ARRAY_TYPE_STRUCT(struct au_c_comp_module, au_c_comp_module_array, 1)

struct au_c_comp_global_state {
    struct au_hm_vars modules_map;
    struct au_c_comp_module_array modules;
};

static void au_c_comp_module(struct au_c_comp_state *state,
                             const struct au_program *program,
                             const size_t module_idx,
                             struct au_c_comp_global_state *g_state);

void au_c_comp_state_del(struct au_c_comp_state *state) {
    switch (state->type) {
    case AU_C_COMP_FILE: {
        fclose(state->as.f);
        break;
    }
    case AU_C_COMP_STR: {
        free(state->as.str.data);
        break;
    }
    }
}

static void comp_write(struct au_c_comp_state *state, const char *bytes,
                       size_t len) {
    switch (state->type) {
    case AU_C_COMP_FILE: {
        fwrite(bytes, 1, len, state->as.f);
        break;
    }
    case AU_C_COMP_STR: {
        for (size_t i = 0; i < len; i++)
            au_char_array_add(&state->as.str, bytes[i]);
        break;
    }
    }
}

static void comp_putc(struct au_c_comp_state *state, int byte) {
    switch (state->type) {
    case AU_C_COMP_FILE: {
        fputc(byte, state->as.f);
        break;
    }
    case AU_C_COMP_STR: {
        au_char_array_add(&state->as.str, byte);
        break;
    }
    }
}

static void comp_printf(struct au_c_comp_state *state, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int long_flag = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            comp_putc(state, *fmt);
            continue;
        }

        long_flag = 0;

try_continue:;
        char ch = *(++fmt);
        switch (ch) {
        case '%': {
            comp_putc(state, '%');
            continue;
        }
        case 'l': {
            long_flag = 1;
            break;
        }
        case 's': {
            char *source = va_arg(args, char *);
            for (; *source; source++)
                comp_putc(state, *source);
            continue;
        }
        case 'c': {
            comp_putc(state, va_arg(args, int));
            continue;
        }
        case 'd': {
            if (long_flag) {
                char buf[32];
                int len =
                    snprintf(buf, sizeof(buf), "%ld", va_arg(args, long));
                comp_write(state, buf, len);
            } else {
                char buf[32];
                int len =
                    snprintf(buf, sizeof(buf), "%d", va_arg(args, int));
                comp_write(state, buf, len);
            }
            continue;
        }
        case 'f': {
            char buf[32];
            int len =
                snprintf(buf, sizeof(buf), "%f", va_arg(args, double));
            comp_write(state, buf, len);
            continue;
        }
        default: {
            au_fatal("unknown format character %c\n", ch);
        }
        }
        goto try_continue;
    }
    va_end(args);
}

static void comp_cleanup(struct au_c_comp_state *state,
                         const struct au_bc_storage *bcs,
                         int except_register, int except_local) {
    for (int i = 0; i < bcs->num_registers; i++)
        if (i != except_register)
            comp_printf(state, "au_value_deref(r%d);", i);
    for (int i = 0; i < bcs->locals_len; i++)
        if (i != except_local)
            comp_printf(state, "au_value_deref(l%d);", i);
}

#define INDENT "    "

static void au_c_comp_func(struct au_c_comp_state *state,
                           const struct au_bc_storage *bcs,
                           const struct au_program_data *p_data,
                           const size_t module_idx,
                           struct au_c_comp_global_state *g_state) {
    comp_printf(state, INDENT "au_value_t t;\n");
    if (bcs->num_registers > 0) {
        comp_printf(state, INDENT "au_value_t r0 = au_value_none()");
        for (int i = 1; i < bcs->num_registers; i++) {
            comp_printf(state, ", r%d = au_value_none()", i);
        }
        comp_printf(state, ";\n");
    }

    if (bcs->locals_len > 0) {
        comp_printf(state, INDENT "au_value_t l0 = au_value_none()");
        for (int i = 1; i < bcs->locals_len; i++) {
            comp_printf(state, ", l%d = au_value_none()", i);
        }
        comp_printf(state, ";\n");
        if (bcs->num_args > 0) {
            for (int i = 0; i < bcs->num_args; i++) {
                comp_printf(state, INDENT "l%d=args[%d];\n", i, i);
            }
        }
    }

#define bc(x) au_bc_buf_at(&bcs->bc, x)
#define DEF_BC16(VAR, OFFSET)                                             \
    assert(pos + OFFSET + 2 <= bcs->bc.len);                              \
    uint16_t VAR = *((uint16_t *)(&bcs->bc.data[pos + OFFSET]));

    au_bit_array labelled_lines = calloc(1, AU_BA_LEN(bcs->bc.len / 4));

    int arg_stack_max = 0;
    int arg_stack_len = 0;

    for (size_t pos = 0; pos < bcs->bc.len;) {
        uint8_t opcode = bc(pos);
        pos++;

        switch (opcode) {
        case OP_JIF:
        case OP_JNIF:
        case OP_JREL: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            AU_BA_SET_BIT(labelled_lines, (abs_offset / 4));
            break;
        }
        case OP_JRELB: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            AU_BA_SET_BIT(labelled_lines, (abs_offset / 4));
            break;
        }
        case OP_PUSH_ARG: {
            arg_stack_len++;
            arg_stack_max = arg_stack_max > arg_stack_len ? arg_stack_max
                                                          : arg_stack_len;
            break;
        }
        case OP_CALL: {
            DEF_BC16(func_id, 1)
            const struct au_fn *fn =
                au_fn_array_at_ptr(&p_data->fns, func_id);
            arg_stack_len -= au_fn_num_args(fn);
            break;
        }
        default:
            break;
        }
        pos += 3;
    }

    if (arg_stack_max > 0) {
        comp_printf(state,
                    INDENT "au_value_t s_data[%d]; size_t s_len=0;\n",
                    arg_stack_max);
    }

    for (size_t pos = 0; pos < bcs->bc.len;) {
        if (AU_BA_GET_BIT(labelled_lines, pos / 4)) {
            comp_printf(state, INDENT "L%ld: ", pos);
        } else {
            comp_printf(state, INDENT);
        }

        uint8_t opcode = bc(pos);
        if (opcode >= PRINTABLE_OP_LEN) {
            au_fatal("unknown opcode %d", opcode);
        }
        pos++;

        switch (opcode) {
        // Move instructions
        case OP_MOV_U16: {
            uint8_t reg = bc(pos);
            DEF_BC16(n, 1)
            comp_printf(state, "MOVE_VALUE(r%d, au_value_int(%d));\n", reg,
                        n);
            break;
        }
        case OP_MOV_REG_LOCAL: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1)
            comp_printf(state, "COPY_VALUE(l%d,r%d);\n", local, reg);
            break;
        }
        case OP_MOV_LOCAL_REG: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1)
            comp_printf(state, "COPY_VALUE(r%d,l%d);\n", reg, local);
            break;
        }
        case OP_MOV_BOOL: {
            uint8_t n = bc(pos), reg = bc(pos + 1);
            comp_printf(state, "MOVE_VALUE(r%d,au_value_bool(%d));\n", reg,
                        n);
            break;
        }
        case OP_LOAD_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1)
            comp_printf(state, "MOVE_VALUE(r%d,_M%ld_c%d());\n", reg,
                        module_idx, c);
            break;
        }
        // Binary operations
#define BIN_OP(NAME)                                                      \
    {                                                                     \
        uint8_t lhs = bc(pos);                                            \
        uint8_t rhs = bc(pos + 1);                                        \
        uint8_t res = bc(pos + 2);                                        \
        comp_printf(state,                                                \
                    "MOVE_VALUE(r%d,au_value_" NAME "(r%d,r%d));\n", res, \
                    lhs, rhs);                                            \
        break;                                                            \
    }
        case OP_MUL:
            BIN_OP("mul")
        case OP_DIV:
            BIN_OP("div")
        case OP_ADD:
            BIN_OP("add")
        case OP_SUB:
            BIN_OP("sub")
        case OP_MOD:
            BIN_OP("mod")
        case OP_EQ:
            BIN_OP("eq")
        case OP_NEQ:
            BIN_OP("neq")
        case OP_LT:
            BIN_OP("lt")
        case OP_GT:
            BIN_OP("gt")
        case OP_LEQ:
            BIN_OP("leq")
        case OP_GEQ:
            BIN_OP("geq")
#undef BIN_OP
        // Binary operations on local variables
#define BIN_OP_ASG(NAME)                                                  \
    {                                                                     \
        uint8_t reg = bc(pos);                                            \
        DEF_BC16(local, 1)                                                \
        comp_printf(state,                                                \
                    "MOVE_VALUE(l%d,au_value_" NAME "(l%d,r%d));\n",      \
                    local, local, reg);                                   \
        break;                                                            \
    }
        case OP_MUL_ASG:
            BIN_OP_ASG("mul")
        case OP_DIV_ASG:
            BIN_OP_ASG("div")
        case OP_ADD_ASG:
            BIN_OP_ASG("add")
        case OP_SUB_ASG:
            BIN_OP_ASG("sub")
        case OP_MOD_ASG:
            BIN_OP_ASG("mod")
#undef BIN_OP_ASG
        // Unary instructions
        case OP_NOT: {
            uint8_t reg = bc(pos);
            comp_printf(
                state,
                "MOVE_VALUE(r%d,au_value_bool(!au_value_is_truthy(r%d)));",
                reg, reg);
            break;
        }
        // Jump instructions
        case OP_JIF:
        case OP_JNIF: {
            uint8_t reg = bc(pos);
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            if (opcode == OP_JIF)
                comp_printf(state,
                            "if(au_value_is_truthy(r%d)) goto L%ld;\n",
                            reg, abs_offset);
            else
                comp_printf(state,
                            "if(!au_value_is_truthy(r%d)) goto L%ld;\n",
                            reg, abs_offset);
            break;
        }
        case OP_JREL: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            comp_printf(state, "goto L%ld;\n", abs_offset);
            break;
        }
        case OP_JRELB: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            comp_printf(state, "goto L%ld;\n", abs_offset);
            break;
        }
        // Call instructions
        case OP_PUSH_ARG: {
            uint8_t reg = bc(pos);
            comp_printf(state,
                        "au_value_ref(r%d);"
                        "s_data[s_len++]=r%d;\n",
                        reg, reg);
            break;
        }
        case OP_CALL: {
            uint8_t reg = bc(pos);
            DEF_BC16(func_id, 1)
            const struct au_fn *fn =
                au_fn_array_at_ptr(&p_data->fns, func_id);
            int n_args;
            switch (fn->type) {
            case AU_FN_DISPATCH:
            case AU_FN_BC: {
                const struct au_bc_storage *bcs = &fn->as.bc_func;
                n_args = bcs->num_args;
                comp_printf(state, "MOVE_VALUE(r%d,", reg);
                if (n_args > 0) {
                    comp_printf(state, "_M%ld_f%d(&s_data[s_len-%d])",
                                module_idx, func_id, n_args);
                } else {
                    comp_printf(state, "_M%ld_f%d()", module_idx, func_id);
                }
                comp_printf(state, ");");
                break;
            }
            case AU_FN_NATIVE: {
                const struct au_lib_func *lib_func = &fn->as.native_func;
                n_args = lib_func->num_args;
                comp_printf(state, "MOVE_VALUE(r%d,", reg);
                if (n_args > 0) {
                    comp_printf(state, "%s(0,&s_data[s_len-%d])",
                                lib_func->symbol, n_args);
                } else {
                    comp_printf(state, "%s(0,0,0)", lib_func->symbol);
                }
                comp_printf(state, ");");
                break;
            }
            case AU_FN_IMPORTER: {
                const struct au_imported_func *import_func =
                    &fn->as.import_func;
                n_args = import_func->num_args;
                comp_printf(state, "MOVE_VALUE(r%d,", reg);
                if (n_args > 0) {
                    comp_printf(state, "(*_M%ld_f%d)(&s_data[s_len-%d])",
                                module_idx, func_id, n_args);
                } else {
                    comp_printf(state, "(*_M%ld_f%d)()", module_idx,
                                func_id);
                }
                comp_printf(state, ");");
                break;
            }
            case AU_FN_NONE: {
                au_fatal("generating none function");
            }
            }
            if (n_args > 0) {
                comp_printf(state, "s_len-=%d;", n_args);
            }
            comp_printf(state, "\n");
            break;
        }
        case OP_CALL1: {
            uint8_t reg = bc(pos);
            DEF_BC16(func_id, 1)
            const struct au_fn *fn =
                au_fn_array_at_ptr(&p_data->fns, func_id);
            switch (fn->type) {
            case AU_FN_DISPATCH:
            case AU_FN_BC: {
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "_M%ld_f%d(&r%d)",
                            module_idx, func_id, reg);
                comp_printf(state, ";");
                break;
            }
            case AU_FN_NATIVE: {
                const struct au_lib_func *lib_func = &fn->as.native_func;
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "%s(0,&r%d)",
                            lib_func->symbol, reg);
                comp_printf(state, ");");
                break;
            }
            case AU_FN_IMPORTER: {
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "(*_M%ld_f%d)(&r%d)",
                            module_idx, func_id, reg);
                comp_printf(state, ";");
                break;
            }
            case AU_FN_NONE: {
                au_fatal("generating none function");
            }
            }
            comp_printf(state, "\n");
            break;
        }
        // Return instructions
        case OP_RET: {
            uint8_t reg = bc(pos);
            comp_cleanup(state, bcs, reg, -1);
            comp_printf(state, "return r%d;\n", reg);
            break;
        }
        case OP_RET_LOCAL: {
            DEF_BC16(local, 1)
            comp_cleanup(state, bcs, -1, local);
            comp_printf(state, "return l%d;\n", local);
            break;
        }
        case OP_RET_NULL: {
            comp_cleanup(state, bcs, -1, -1);
            comp_printf(state, "return au_value_none();\n");
            break;
        }
        // Modules
        case OP_IMPORT: {
            DEF_BC16(idx, 1)
            const struct au_program_import *import =
                au_program_import_array_at_ptr(&p_data->imports, idx);
            const size_t relative_module_idx = import->module_idx;
            const char *relpath = import->path;

            struct au_mmap_info mmap;
            char *abspath;

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

            size_t imported_module_idx = g_state->modules.len;
            int has_old_value = 0;

            if (!au_mmap_read(abspath, &mmap))
                au_perror("mmap");
            {
                struct au_hm_var_value value = (struct au_hm_var_value){
                    .idx = imported_module_idx,
                };
                struct au_hm_var_value *old_value =
                    au_hm_vars_add(&g_state->modules_map, abspath,
                                   strlen(abspath), &value);
                if (old_value != 0) {
                    imported_module_idx = old_value->idx;
                    has_old_value = 1;
                }
            }

            // 0 is reserved for main module
            const size_t imported_module_idx_in_source =
                imported_module_idx + 1;

            if (!has_old_value) {
                struct au_program program;
                assert(au_parse(mmap.bytes, mmap.size, &program).type ==
                       AU_PARSER_RES_OK);
                au_mmap_del(&mmap);

                program.data.file = 0;
                program.data.cwd = 0;
                au_split_path(abspath, &program.data.file,
                              &program.data.cwd);
                free(abspath);

                struct au_c_comp_state mod_state = {
                    .as.str = (struct au_char_array){0},
                    .type = AU_C_COMP_STR,
                };
                au_c_comp_module(&mod_state, &program,
                                 imported_module_idx_in_source, g_state);

                struct au_c_comp_module comp_module =
                    (struct au_c_comp_module){0};
                comp_module.fns = program.data.fns;
                comp_module.fn_map = program.data.fn_map;
                program.data.fns = (struct au_fn_array){0};
                program.data.fn_map = (struct au_hm_vars){0};
                au_program_del(&program);

                au_char_array_add(&mod_state.as.str, 0);
                comp_module.source = mod_state.as.str.data;
                mod_state.as.str.data = 0;
                mod_state.as.str.len = 0;

                au_c_comp_module_array_add(&g_state->modules, comp_module);
            }

            comp_printf(state,
                        "extern au_value_t _M%ld_main();_M%ld_main();\n",
                        imported_module_idx_in_source,
                        imported_module_idx_in_source);

            if (relative_module_idx != AU_PROGRAM_IMPORT_NO_MODULE) {
                const struct au_imported_module *relative_module =
                    au_imported_module_array_at_ptr(
                        &p_data->imported_modules, relative_module_idx);
                const struct au_c_comp_module *loaded_module =
                    au_c_comp_module_array_at_ptr(&g_state->modules,
                                                  imported_module_idx);

                AU_HM_VARS_FOREACH_PAIR(
                    &relative_module->fn_map, key, entry, {
                        // TODO: this is ripped straight outta
                        // link_to_imported from core/vm/vm.c. It might be
                        // better to unify these two sections
                        const struct au_fn *relative_fn =
                            au_fn_array_at_ptr(&p_data->fns, entry->idx);
                        assert(relative_fn->type == AU_FN_IMPORTER);
                        const struct au_imported_func *import_func =
                            &relative_fn->as.import_func;
                        const struct au_hm_var_value *fn_idx =
                            au_hm_vars_get(&loaded_module->fn_map, key,
                                           key_len);
                        if (fn_idx == 0)
                            au_fatal("unknown function %.*s", key_len,
                                     key);
                        struct au_fn *fn =
                            &loaded_module->fns.data[fn_idx->idx];
                        if ((fn->flags & AU_FN_FLAG_EXPORTED) != 0)
                            au_fatal("this function is not exported");
                        if (au_fn_num_args(fn) != import_func->num_args)
                            au_fatal("unexpected number of arguments");
                        if (import_func->num_args == 0) {
                            comp_printf(state,
                                        "extern au_value_t _M%ld_f%ld();"
                                        "_M%ld_f%ld=&_M%ld_f%ld;\n",
                                        imported_module_idx_in_source,
                                        fn_idx->idx, module_idx,
                                        entry->idx,
                                        imported_module_idx_in_source,
                                        fn_idx->idx);
                        } else {
                            comp_printf(state,
                                        "extern au_value_t "
                                        "_M%ld_f%ld(au_value_t*);"
                                        "_M%ld_f%ld=&_M%ld_f%ld;\n",
                                        imported_module_idx_in_source,
                                        fn_idx->idx, module_idx,
                                        entry->idx,
                                        imported_module_idx_in_source,
                                        fn_idx->idx);
                        }
                    })
            }

            break;
        }
        // Array instructions
        case OP_ARRAY_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(capacity, 1)
            comp_printf(state,
                        "MOVE_VALUE(r%d,"
                        "au_value_struct("
                        "(struct au_struct*)au_obj_array_new(%d)));\n",
                        reg, capacity);
            break;
        }
        case OP_ARRAY_PUSH: {
            uint8_t reg = bc(pos);
            uint8_t value = bc(pos + 1);
            comp_printf(
                state,
                "au_value_ref(r%d);"
                "au_obj_array_push(au_obj_array_coerce(r%d),r%d);\n",
                value, reg, value);
            break;
        }
        case OP_IDX_GET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            comp_printf(state,
                        "COPY_VALUE(r%d,au_struct_idx_get(r%d,r%d));\n",
                        ret, reg, idx);
            break;
        }
        case OP_IDX_SET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            comp_printf(state,
                        "au_value_ref(r%d);"
                        "au_struct_idx_set(r%d,r%d,r%d);\n",
                        ret, reg, idx, ret);
            break;
        }
        // Tuple instructions
        case OP_TUPLE_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(len, 1)
            comp_printf(state,
                        "MOVE_VALUE(r%d,"
                        "au_value_struct("
                        "(struct au_struct*)au_obj_tuple_new(%d)));\n",
                        reg, len);
            break;
        }
        case OP_IDX_SET_STATIC: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            comp_printf(state,
                        "au_value_ref(r%d);"
                        "au_struct_idx_set(r%d,au_value_int(%d),r%d);\n",
                        ret, reg, idx, ret);
            break;
        }
        // Other
        case OP_NOP: break;
        case OP_PRINT: {
            uint8_t lhs = bc(pos);
            comp_printf(state, "au_value_print(r%d);\n", lhs);
            break;
        }
        default: {
            au_fatal("unimplemented: %s\n", au_opcode_dbg[opcode]);
            break;
        }
        }
        pos += 3;
    }

    free(labelled_lines);
}

void au_c_comp_module(struct au_c_comp_state *state,
                      const struct au_program *program,
                      const size_t module_idx,
                      struct au_c_comp_global_state *g_state) {
    for (size_t i = 0; i < program->data.data_val.len; i++) {
        const struct au_program_data_val *val =
            &program->data.data_val.data[i];
        comp_printf(state, "static inline au_value_t _M%ld_c%ld() {\n",
                    module_idx, i);
        switch (au_value_get_type(val->real_value)) {
        case VALUE_STR: {
            comp_printf(
                state, INDENT
                "return au_value_string(au_string_from_const((const "
                "char[]){");
            for (size_t i = 0; i < val->buf_len; i++) {
                comp_printf(state, "%d,",
                            program->data.data_buf[val->buf_idx + i]);
            }
            comp_printf(state, "}, %d));\n", val->buf_len);
            break;
        }
        case VALUE_INT: {
            comp_printf(state, "return au_value_int(%d);\n",
                        au_value_get_int(val->real_value));
            break;
        }
        case VALUE_DOUBLE: {
            comp_printf(state, "return au_value_double(%f);\n",
                        au_value_get_double(val->real_value));
            break;
        }
        default:
            break;
        }
        comp_printf(state, "}\n");
    }
    for (size_t i = 0; i < program->data.fns.len; i++) {
        const struct au_fn *fn = &program->data.fns.data[i];
        switch (fn->type) {
        case AU_FN_BC: {
            if ((fn->flags & AU_FN_FLAG_EXPORTED) != 0) {
                comp_printf(state, "static ");
            }
            if (fn->as.bc_func.num_args > 0) {
                comp_printf(state,
                            "au_value_t _M%ld_f%ld"
                            "(const au_value_t*args);\n",
                            module_idx, i);
            } else {
                comp_printf(state, "au_value_t _M%ld_f%ld();\n",
                            module_idx, i);
            }
            break;
        }
        case AU_FN_NATIVE: {
            comp_printf(state, "extern AU_EXTERN_FUNC_DECL(%s);\n",
                        fn->as.native_func.symbol);
            break;
        }
        case AU_FN_IMPORTER: {
            if (fn->as.import_func.num_args > 0) {
                comp_printf(state,
                            "static au_value_t (*_M%ld_f%ld)"
                            "(const au_value_t*)=0;\n",
                            module_idx, i);
            } else {
                comp_printf(state,
                            "static au_value_t (*_M%ld_f%ld)()=0;\n",
                            module_idx, i);
            }
            break;
        }
        case AU_FN_DISPATCH: {
            au_fatal("generating none function");
        }
        case AU_FN_NONE: {
            au_fatal("generating none function");
        }
        }
    }
    for (size_t i = 0; i < program->data.fns.len; i++) {
        const struct au_fn *fn = &program->data.fns.data[i];
        if (fn->type == AU_FN_BC) {
            const struct au_bc_storage *bcs = &fn->as.bc_func;
            if (bcs->num_args > 0) {
                comp_printf(
                    state,
                    "au_value_t _M%ld_f%ld(const au_value_t *args) {\n",
                    module_idx, i);
            } else {
                comp_printf(state, "au_value_t _M%ld_f%ld() {\n",
                            module_idx, i);
            }
            au_c_comp_func(state, bcs, &program->data, module_idx,
                           g_state);
            comp_printf(state, "}\n");
        }
    }
    comp_printf(state, "static int _M%ld_main_init=0;\n", module_idx);
    comp_printf(state, "au_value_t _M%ld_main() {\n", module_idx);
    comp_printf(state,
                INDENT "if(_M%ld_main_init){return au_value_none();}\n",
                module_idx);
    comp_printf(state, INDENT "_M%ld_main_init=1;\n", module_idx);
    au_c_comp_func(state, &program->main, &program->data, module_idx,
                   g_state);
    comp_printf(state, "}\n");
}

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

extern const char AU_RT_CODE[];
extern const size_t AU_RT_CODE_LEN;

#ifdef TEST
char *TEST_RT_CODE;
size_t TEST_RT_CODE_LEN;
#endif

void au_c_comp(struct au_c_comp_state *state,
               const struct au_program *program) {
    comp_write(state, AU_RT_HDR, AU_RT_HDR_LEN);
    comp_printf(state, "\n" AU_C_COMP_EXTERN_FUNC_DECL "\n");

    struct au_c_comp_global_state g_state =
        (struct au_c_comp_global_state){0};

    struct au_c_comp_state main_mod_state = {
        .as.str = (struct au_char_array){0},
        .type = AU_C_COMP_STR,
    };
    au_c_comp_module(&main_mod_state, program, 0, &g_state);
    au_char_array_add(&main_mod_state.as.str, 0);
    comp_printf(state, "%s\n", main_mod_state.as.str.data);
    au_c_comp_state_del(&main_mod_state);
    comp_printf(state, "int main() { _M0_main(); return 0; }\n");

    for (size_t i = 0; i < g_state.modules.len; i++) {
        comp_printf(state, "%s\n", g_state.modules.data[i].source);
    }

    comp_write(state, AU_RT_CODE, AU_RT_CODE_LEN);
#ifdef TEST
    comp_printf(state, "\n");
    comp_write(state, TEST_RT_CODE, TEST_RT_CODE_LEN);
#endif

    // Cleanup
    for (size_t i = 0; i < g_state.modules.len; i++) {
        struct au_c_comp_module *module = &g_state.modules.data[i];
        au_hm_vars_del(&module->fn_map);
        for (size_t i = 0; i < module->fns.len; i++) {
            au_fn_del(&module->fns.data[i]);
        }
        free(module->fns.data);
        free(module->source);
    }
    free(g_state.modules.data);
    au_hm_vars_del(&g_state.modules_map);
}