// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef AU_FEAT_LIBDL
#include <dlfcn.h>
#endif

#include "platform/mmap.h"
#include "platform/path.h"

#include "core/bc.h"
#include "core/bit_array.h"
#include "core/hm_vars.h"
#include "core/int_error/error_printer.h"
#include "core/parser/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "core/vm/module.h"

#include "c_comp.h"

#define INDENT "    "

struct line_info {
    size_t source_start;
    int line;
};

ARRAY_TYPE_STRUCT(struct line_info, line_info_array, 1)

static void create_line_info_array(struct line_info_array *array,
                                   const char *source, size_t source_len) {
    int line = 1;
    size_t line_start = 0;
    for (size_t source_idx = 0; source_idx < source_len; source_idx++) {
        if (source[source_idx] == '\n') {
            // printf("line %d, from %ld\n", line, line_start);
            line_info_array_add(array, (struct line_info){
                                           .line = line,
                                           .source_start = line_start,
                                       });
            line++;
            line_start = source_idx;
        }
    }
}

struct au_c_comp_module {
    struct au_hm_vars fn_map;
    struct au_fn_array fns;
    struct au_class_interface_ptr_array classes;
    struct au_hm_vars class_map;
    struct line_info_array line_info;
    struct au_char_array c_source;
};

ARRAY_TYPE_STRUCT(struct au_c_comp_module, au_c_comp_module_array, 1)

struct au_c_comp_global_state {
    struct au_hm_vars modules_map;
    struct au_c_comp_module_array modules;
    struct line_info_array main_line_info;
    struct au_c_comp_options options;
    struct au_c_comp_state header_file;
};

static void au_c_comp_module(struct au_c_comp_state *state,
                             const struct au_program *program,
                             const size_t module_idx,
                             struct au_c_comp_global_state *g_state);

void au_c_comp_state_del(struct au_c_comp_state *state) {
    au_data_free(state->str.data);
}

static void comp_write(struct au_c_comp_state *state, const char *bytes,
                       size_t len) {
    for (size_t i = 0; i < len; i++)
        au_char_array_add(&state->str, bytes[i]);
}

static void comp_putc(struct au_c_comp_state *state, int byte) {
    au_char_array_add(&state->str, byte);
}

static void comp_printf(struct au_c_comp_state *state, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int long_flag = 0;
    int dot_star_flag = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            comp_putc(state, *fmt);
            continue;
        }

        long_flag = 0;
        dot_star_flag = 0;

try_continue:;
        char ch = *(++fmt);
        switch (ch) {
        case '%': {
            comp_putc(state, '%');
            continue;
        }
        case '.': {
            if (*(fmt + 1) == '*') {
                fmt++;
                dot_star_flag = 1;
            } else {
                au_fatal("comp_printf doesn't support this flag: %%.%c",
                         *(fmt + 1));
            }
            break;
        }
        case 'l': {
            long_flag = 1;
            break;
        }
        case 's': {
            if (dot_star_flag) {
                int len = va_arg(args, int);
                char *source = va_arg(args, char *);
                for (int i = 0; i < len; i++)
                    comp_putc(state, source[i]);
            } else {
                char *source = va_arg(args, char *);
                for (; *source; source++)
                    comp_putc(state, *source);
            }
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
                         const struct au_bc_storage *bcs, int module_idx,
                         int except_register, int except_local,
                         int has_self) {
    for (int i = 0; i < bcs->num_registers; i++)
        if (i != except_register)
            comp_printf(state, "au_value_deref(r%d);", i);
    for (int i = 0; i < bcs->num_locals; i++)
        if (i != except_local)
            comp_printf(state, "au_value_deref(l%d);", i);
    if (has_self) {
        comp_printf(
            state,
            "if((--self->header.rc)==0)_struct_M%ld_%d_del_fn(self);",
            module_idx, bcs->class_idx);
    }
}

static void link_to_imported(
    const struct au_program_data *p_data, size_t module_idx,
    enum au_module_type module_type, size_t imported_module_idx_in_source,
    struct au_c_comp_global_state *g_state, struct au_c_comp_state *state,
    const struct au_imported_module *relative_module,
    const struct au_c_comp_module *loaded_module) {
    AU_HM_VARS_FOREACH_PAIR(&relative_module->fn_map, key, entry, {
        const struct au_fn *relative_fn =
            au_fn_array_at_ptr(&p_data->fns, entry->idx);
        assert(relative_fn->type == AU_FN_IMPORTER);
        const struct au_imported_func *import_func =
            &relative_fn->as.import_func;
        const struct au_hm_var_value *fn_idx =
            au_hm_vars_get(&loaded_module->fn_map, key, key_len);
        if (fn_idx == 0)
            au_fatal("unknown function %.*s", key_len, key);
        struct au_fn *fn = &loaded_module->fns.data[fn_idx->idx];
        if ((fn->flags & AU_FN_FLAG_EXPORTED) == 0)
            au_fatal("this function is not exported");
        if (au_fn_num_args(fn) != import_func->num_args)
            au_fatal("unexpected number of arguments");
        if (module_type == AU_MODULE_SOURCE) {
            if (import_func->num_args == 0) {
                comp_printf(state,
                            INDENT "extern au_value_t _M%ld_f%ld();",
                            imported_module_idx_in_source, fn_idx->idx);
            } else {
                comp_printf(state,
                            INDENT "extern au_value_t _M%ld_f%ld"
                                   "(au_value_t *args);",
                            imported_module_idx_in_source, fn_idx->idx);
            }
        } else {
            comp_printf(state, INDENT);
        }
        comp_printf(state, "_M%ld_f%ld=&_M%ld_f%ld;\n", module_idx,
                    entry->idx, imported_module_idx_in_source,
                    fn_idx->idx);
    })
#define X(FMT)                                                            \
    do {                                                                  \
        comp_printf(&g_state->header_file, FMT, module_idx, entry->idx,   \
                    imported_module_idx_in_source, class_idx->idx);       \
    } while (0)
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
        X("#define _M%ld_%ld _M%ld_%ld\n");
        X("#define _struct_M%ld_%ld_vdata"
          " _struct_M%ld_%ld_vdata\n");
        X("#define _struct_M%ld_%ld_del_fn"
          " _struct_M%ld_%ld_del_fn\n");
    })
#undef X
}

static void
write_imported_module_main_start(size_t imported_module_idx_in_source,
                                 struct au_c_comp_global_state *g_state,
                                 const char *lib_filename) {
    comp_printf(&g_state->header_file, "static int _M%ld_main_init=0;\n",
                imported_module_idx_in_source);
    comp_printf(&g_state->header_file, "static void _M%ld_main() {\n",
                imported_module_idx_in_source);
    comp_printf(&g_state->header_file,
                INDENT "if(_M%ld_main_init){return;}\n",
                imported_module_idx_in_source);
    comp_printf(&g_state->header_file, INDENT "_M%ld_main_init=1;\n",
                imported_module_idx_in_source);
    comp_printf(&g_state->header_file, INDENT "struct au_module m;\n");
    comp_printf(&g_state->header_file,
                INDENT "switch(au_module_import(&m,\"./%s\")){\n" INDENT
                       "case AU_MODULE_IMPORT_SUCCESS: break;\n" INDENT
                       "case AU_MODULE_IMPORT_SUCCESS_NO_MODULE:"
                       " return;\n" INDENT "default:{"
                       "au_module_lib_perror();"
                       "au_fatal(\"failed to import '%s'\");}}\n",
                lib_filename, lib_filename);
}

static void
write_imported_module_main(size_t imported_module_idx_in_source,
                           struct au_c_comp_global_state *g_state,
                           const char *lib_filename) {
    write_imported_module_main_start(imported_module_idx_in_source,
                                     g_state, lib_filename);
    comp_printf(&g_state->header_file, "}\n");
}

static void au_c_comp_func(struct au_c_comp_state *state,
                           const struct au_bc_storage *bcs,
                           const struct au_program_data *p_data,
                           const size_t module_idx, const size_t func_idx,
                           struct au_c_comp_global_state *g_state) {
    comp_printf(state, INDENT "au_value_t t;\n");
    if (bcs->num_registers > 0) {
        comp_printf(state, INDENT "au_value_t r0 = au_value_none()");
        for (int i = 1; i < bcs->num_registers; i++) {
            comp_printf(state, ", r%d = au_value_none()", i);
        }
        comp_printf(state, ";\n");
    }

    if (bcs->num_locals > 0) {
        comp_printf(state, INDENT "au_value_t l0 = au_value_none()");
        for (int i = 1; i < bcs->num_locals; i++) {
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

    au_bit_array labelled_lines =
        au_data_malloc(AU_BA_LEN(bcs->bc.len / 4));
    memset(labelled_lines, 0, AU_BA_LEN(bcs->bc.len / 4));

    int arg_stack_max = 0;
    int arg_stack_len = 0;

    for (size_t pos = 0; pos < bcs->bc.len;) {
        uint8_t opcode = bc(pos);
        pos++;

        switch (opcode) {
        case AU_OP_JIF:
        case AU_OP_JNIF:
        case AU_OP_JREL: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            AU_BA_SET_BIT(labelled_lines, (abs_offset / 4));
            break;
        }
        case AU_OP_JRELB: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            AU_BA_SET_BIT(labelled_lines, (abs_offset / 4));
            break;
        }
        case AU_OP_PUSH_ARG: {
            arg_stack_len++;
            arg_stack_max = arg_stack_max > arg_stack_len ? arg_stack_max
                                                          : arg_stack_len;
            break;
        }
        case AU_OP_CALL: {
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

    const struct au_program_source_map *current_source_map = 0;
    size_t current_source_map_idx = bcs->source_map_start;
    const struct line_info_array *line_info_array = 0;
    const struct line_info *current_line_info = 0;
    size_t current_line_info_idx = 0;

    if (g_state->options.with_debug) {
        if (p_data->source_map.len >= current_source_map_idx) {
            if (func_idx == AU_SM_FUNC_ID_MAIN) {
                while (current_source_map_idx < p_data->source_map.len) {
                    if (p_data->source_map.data[current_source_map_idx]
                            .func_idx == AU_SM_FUNC_ID_MAIN) {
                        current_source_map =
                            &p_data->source_map
                                 .data[current_source_map_idx];
                        break;
                    }
                    current_source_map_idx++;
                }
            } else {
                current_source_map =
                    &p_data->source_map.data[current_source_map_idx];
                assert(current_source_map->func_idx == func_idx);
                current_source_map_idx++;
            }
        }

        if (module_idx == 0) {
            line_info_array = &g_state->main_line_info;
        } else {
            line_info_array = &au_c_comp_module_array_at_ptr(
                                   &g_state->modules, module_idx - 1)
                                   ->line_info;
        }

        if (current_source_map) {
            for (size_t i = 0; i < line_info_array->len; i++) {
                if (line_info_array->data[i].source_start >=
                    current_source_map->source_start) {
                    if (line_info_array->data[i].source_start >
                        current_source_map->source_start) {
                        i--;
                    }
                    current_line_info_idx = i;
                    current_line_info =
                        line_info_array_at_ptr(line_info_array, i);
                    // printf("start from # %ld\n", i);
                    break;
                }
            }
            if (!current_line_info && line_info_array->len > 0) {
                current_line_info =
                    &line_info_array->data[line_info_array->len - 1];
            }
        }
    }

    int has_self = 0;

    for (size_t pos = 0; pos < bcs->bc.len;) {
        if (current_source_map && current_line_info) {
            if (pos == current_source_map->bc_to - 4) {
                comp_printf(state, INDENT "#line %d \"%s\"\n",
                            current_line_info->line, p_data->file);
            } else if (pos > current_source_map->bc_to) {
                if (current_source_map_idx == p_data->source_map.len) {
                    current_source_map = 0;
                } else {
                    current_source_map =
                        au_program_source_map_array_at_ptr(
                            &p_data->source_map, current_source_map_idx++);
                    if (module_idx == AU_SM_FUNC_ID_MAIN &&
                        current_source_map->func_idx !=
                            AU_SM_FUNC_ID_MAIN) {
                        while (current_source_map_idx <
                               p_data->source_map.len) {
                            if (p_data->source_map
                                    .data[current_source_map_idx]
                                    .func_idx == AU_SM_FUNC_ID_MAIN) {
                                break;
                            }
                            current_source_map_idx++;
                        }
                    }
                    if (current_line_info) {
                        while (current_source_map->source_start <
                               current_line_info->source_start) {
                            if (current_line_info->source_start >
                                current_source_map->source_start) {
                                int old_line = current_line_info->line;
                                current_line_info = line_info_array_at_ptr(
                                    line_info_array,
                                    ++current_line_info_idx);
                                assert(current_line_info->line >=
                                       old_line);
                                break;
                            }
                            if (current_line_info_idx ==
                                line_info_array->len) {
                                current_source_map = 0;
                                break;
                            }
                            current_line_info = line_info_array_at_ptr(
                                line_info_array, current_line_info_idx++);
                        }
                    }
                }
            }
        }

        if (AU_BA_GET_BIT(labelled_lines, pos / 4)) {
            comp_printf(state, INDENT "L%ld: ", pos);
        } else {
            comp_printf(state, INDENT);
        }

        const uint8_t opcode = bc(pos++);

        switch (opcode) {
        case AU_OP_LOAD_SELF: {
            comp_printf(
                state,
                INDENT
                "struct _M%ld_%d*self=(void*)au_struct_coerce(args[0]);"
                "if(self->header.vdata!=&_struct_M%ld_%d_vdata){abort();}"
                "self->header.rc++;"
                "\n",
                module_idx, bcs->class_idx, module_idx, bcs->class_idx,
                module_idx, bcs->class_idx);
            has_self = 1;
            break;
        }
        // Move instructions
        case AU_OP_MOV_U16: {
            uint8_t reg = bc(pos);
            DEF_BC16(n, 1)
            comp_printf(state, "MOVE_VALUE(r%d, au_value_int(%d));\n", reg,
                        n);
            break;
        }
        case AU_OP_MOV_REG_LOCAL: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1)
            comp_printf(state, "COPY_VALUE(l%d,r%d);\n", local, reg);
            break;
        }
        case AU_OP_MOV_LOCAL_REG: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1)
            comp_printf(state, "COPY_VALUE(r%d,l%d);\n", reg, local);
            break;
        }
        case AU_OP_MOV_BOOL: {
            uint8_t n = bc(pos), reg = bc(pos + 1);
            comp_printf(state, "MOVE_VALUE(r%d,au_value_bool(%d));\n", reg,
                        n);
            break;
        }
        case AU_OP_LOAD_CONST: {
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
        case AU_OP_MUL:
            BIN_OP("mul")
        case AU_OP_DIV:
            BIN_OP("div")
        case AU_OP_ADD:
            BIN_OP("add")
        case AU_OP_SUB:
            BIN_OP("sub")
        case AU_OP_MOD:
            BIN_OP("mod")
        case AU_OP_EQ:
            BIN_OP("eq")
        case AU_OP_NEQ:
            BIN_OP("neq")
        case AU_OP_LT:
            BIN_OP("lt")
        case AU_OP_GT:
            BIN_OP("gt")
        case AU_OP_LEQ:
            BIN_OP("leq")
        case AU_OP_GEQ:
            BIN_OP("geq")
#undef BIN_OP
            // Binary operations on local variables
#define BIN_AU_OP_ASG(NAME)                                               \
    {                                                                     \
        uint8_t reg = bc(pos);                                            \
        DEF_BC16(local, 1)                                                \
        comp_printf(state,                                                \
                    "MOVE_VALUE(l%d,au_value_" NAME "(l%d,r%d));\n",      \
                    local, local, reg);                                   \
        break;                                                            \
    }
        case AU_OP_MUL_ASG:
            BIN_AU_OP_ASG("mul")
        case AU_OP_DIV_ASG:
            BIN_AU_OP_ASG("div")
        case AU_OP_ADD_ASG:
            BIN_AU_OP_ASG("add")
        case AU_OP_SUB_ASG:
            BIN_AU_OP_ASG("sub")
        case AU_OP_MOD_ASG:
            BIN_AU_OP_ASG("mod")
#undef BIN_AU_OP_ASG
        // Unary instructions
        case AU_OP_NOT: {
            uint8_t reg = bc(pos);
            comp_printf(
                state,
                "MOVE_VALUE(r%d,au_value_bool(!au_value_is_truthy(r%d)));",
                reg, reg);
            break;
        }
        // Jump instructions
        case AU_OP_JIF:
        case AU_OP_JNIF: {
            uint8_t reg = bc(pos);
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            if (opcode == AU_OP_JIF)
                comp_printf(state,
                            "if(au_value_is_truthy(r%d)) goto L%ld;\n",
                            reg, abs_offset);
            else
                comp_printf(state,
                            "if(!au_value_is_truthy(r%d)) goto L%ld;\n",
                            reg, abs_offset);
            break;
        }
        case AU_OP_JREL: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            comp_printf(state, "goto L%ld;\n", abs_offset);
            break;
        }
        case AU_OP_JRELB: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            comp_printf(state, "goto L%ld;\n", abs_offset);
            break;
        }
        // Call instructions
        case AU_OP_PUSH_ARG: {
            uint8_t reg = bc(pos);
            comp_printf(state,
                        "au_value_ref(r%d);"
                        "s_data[s_len++]=r%d;\n",
                        reg, reg);
            break;
        }
        case AU_OP_CALL: {
            uint8_t reg = bc(pos);
            DEF_BC16(func_id, 1)
            const struct au_fn *fn =
                au_fn_array_at_ptr(&p_data->fns, func_id);
            int n_args = 0;
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
        case AU_OP_CALL1: {
            uint8_t reg = bc(pos);
            DEF_BC16(func_id, 1)
            const struct au_fn *fn =
                au_fn_array_at_ptr(&p_data->fns, func_id);
            switch (fn->type) {
            case AU_FN_DISPATCH:
            case AU_FN_BC: {
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "_M%ld_f%d(&r%d)", module_idx, func_id,
                            reg);
                comp_printf(state, ";");
                break;
            }
            case AU_FN_NATIVE: {
                const struct au_lib_func *lib_func = &fn->as.native_func;
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "%s(0,&r%d);", lib_func->symbol, reg);
                break;
            }
            case AU_FN_IMPORTER: {
                comp_printf(state, "r%d=", reg);
                comp_printf(state, "(*_M%ld_f%d)(&r%d)", module_idx,
                            func_id, reg);
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
        case AU_OP_RET: {
            uint8_t reg = bc(pos);
            comp_cleanup(state, bcs, module_idx, reg, -1, has_self);
            comp_printf(state, "return r%d;\n", reg);
            break;
        }
        case AU_OP_RET_LOCAL: {
            DEF_BC16(local, 1)
            comp_cleanup(state, bcs, module_idx, -1, local, has_self);
            comp_printf(state, "return l%d;\n", local);
            break;
        }
        case AU_OP_RET_NULL: {
            comp_cleanup(state, bcs, module_idx, -1, -1, has_self);
            comp_printf(state, "return au_value_none();\n");
            break;
        }
        // Modules
        case AU_OP_IMPORT: {
            DEF_BC16(idx, 1)
            const struct au_program_import *import =
                au_program_import_array_at_ptr(&p_data->imports, idx);
            const size_t relative_module_idx = import->module_idx;
            const char *relpath = import->path;

            char *abspath = 0;
            if ((abspath = au_module_resolve(relpath, p_data->cwd)) == 0)
                au_fatal("abspath is null\n");

            struct au_module module;
            switch (au_module_import(&module, abspath)) {
            case AU_MODULE_IMPORT_SUCCESS: {
                break;
            }
            case AU_MODULE_IMPORT_FAIL: {
                au_fatal("unable to import %s\n", abspath);
                break;
            }
            case AU_MODULE_IMPORT_FAIL_DLERROR: {
#ifdef AU_FEAT_LIBDL
                au_fatal("dlerror: %s\n", dlerror());
#endif
                break;
            }
            default: {
                break;
            }
            }

            size_t imported_module_idx = g_state->modules.len;
            int has_old_value = 0;

            {
                struct au_hm_var_value *old_value = au_hm_vars_add(
                    &g_state->modules_map, abspath, strlen(abspath),
                    AU_HM_VAR_VALUE(imported_module_idx));
                if (old_value != 0) {
                    imported_module_idx = old_value->idx;
                    has_old_value = 1;
                }
            }

            // 0 is reserved for main module
            const size_t imported_module_idx_in_source =
                imported_module_idx + 1;

            switch (module.type) {
            case AU_MODULE_SOURCE: {
                struct au_mmap_info mmap;
                if (!au_mmap_read(abspath, &mmap))
                    au_perror("mmap");

                struct line_info_array line_info_array = {0};
                create_line_info_array(&line_info_array, mmap.bytes,
                                       mmap.size);

                if (!has_old_value) {
                    struct au_program program;
                    struct au_parser_result parse_res =
                        au_parse(mmap.bytes, mmap.size, &program);
                    if (parse_res.type != AU_PARSER_RES_OK) {
                        au_print_parser_error(parse_res,
                                              (struct au_error_location){
                                                  .src = mmap.bytes,
                                                  .len = mmap.size,
                                                  .path = p_data->file,
                                              });
                        au_mmap_del(&mmap);
                        abort();
                    }
                    au_mmap_del(&mmap);

                    program.data.file = 0;
                    program.data.cwd = 0;
                    au_split_path(abspath, &program.data.file,
                                  &program.data.cwd);
                    au_data_free(abspath);

                    struct au_c_comp_state mod_state = {0};
                    au_c_comp_module(&mod_state, &program,
                                     imported_module_idx_in_source,
                                     g_state);

                    struct au_c_comp_module comp_module =
                        (struct au_c_comp_module){0};
                    comp_module.fns = program.data.fns;
                    comp_module.fn_map = program.data.fn_map;
                    comp_module.classes = program.data.classes;
                    comp_module.class_map = program.data.class_map;
                    program.data.fns = (struct au_fn_array){0};
                    program.data.fn_map = (struct au_hm_vars){0};
                    program.data.classes =
                        (struct au_class_interface_ptr_array){0};
                    program.data.class_map = (struct au_hm_vars){0};
                    au_program_del(&program);

                    comp_module.c_source = mod_state.str;
                    mod_state.str = (struct au_char_array){0};

                    comp_module.line_info = line_info_array;
                    line_info_array = (struct line_info_array){0};

                    au_c_comp_module_array_add(&g_state->modules,
                                               comp_module);
                }
                break;
            }
            case AU_MODULE_LIB: {
                struct au_program_data *loaded_module =
                    module.data.lib.lib;
                module.data.lib.lib = 0;

                char *lib_filename = basename(abspath);

                if (loaded_module == 0) {
                    write_imported_module_main(
                        imported_module_idx_in_source, g_state,
                        lib_filename);
                    break;
                }

                if (!has_old_value) {
                    struct au_c_comp_module comp_module =
                        (struct au_c_comp_module){0};
                    AU_HM_VARS_FOREACH_PAIR(
                        &loaded_module->fn_map, name, entry, {
                            const struct au_fn *loaded_fn =
                                au_fn_array_at_ptr(&loaded_module->fns,
                                                   entry->idx);
                            comp_printf(&g_state->header_file,
                                        "static au_extern_func_t "
                                        "_M%ld_f%ld_ext=0;\n",
                                        imported_module_idx_in_source,
                                        entry->idx);
                            if (au_fn_num_args(loaded_fn) == 0) {
                                comp_printf(&g_state->header_file,
                                            "static au_value_t "
                                            "_M%ld_f%ld(){"
                                            "return _M%ld_f%ld_ext(0,0);"
                                            "}\n",
                                            imported_module_idx_in_source,
                                            entry->idx,
                                            imported_module_idx_in_source,
                                            entry->idx);
                            } else {
                                comp_printf(&g_state->header_file,
                                            "static au_value_t "
                                            "_M%ld_f%ld(au_value_t*a){"
                                            "return _M%ld_f%ld_ext(0,a);"
                                            "}\n",
                                            imported_module_idx_in_source,
                                            entry->idx,
                                            imported_module_idx_in_source,
                                            entry->idx);
                            }
                            if ((loaded_fn->flags & AU_FN_FLAG_EXPORTED) !=
                                0) {
                                au_fn_array_add(
                                    &comp_module.fns,
                                    (struct au_fn){
                                        .flags = loaded_fn->flags,
                                        .type = AU_FN_NONE,
                                    });
                                au_hm_vars_add(&comp_module.fn_map, name,
                                               name_len,
                                               AU_HM_VAR_VALUE(0));
                            }
                        });
                    au_c_comp_module_array_add(&g_state->modules,
                                               comp_module);

                    write_imported_module_main_start(
                        imported_module_idx_in_source, g_state,
                        lib_filename);
                    AU_HM_VARS_FOREACH_PAIR(
                        &loaded_module->fn_map, name, entry, {
                            (void)name_len;
                            comp_printf(&g_state->header_file,
                                        INDENT "_M%ld_f%d_ext="
                                               "au_module_get_fn"
                                               "(&m,\"%s\");\n",
                                        imported_module_idx_in_source,
                                        entry->idx, name);
                            comp_printf(&g_state->header_file,
                                        INDENT
                                        "if(_M%ld_f%d_ext==0)"
                                        "au_fatal(\"failed to function "
                                        "'%s' from '%s'\");\n",
                                        imported_module_idx_in_source,
                                        entry->idx, name, lib_filename);
                        });
                    comp_printf(&g_state->header_file, "}\n");
                }

                au_program_data_del(loaded_module);
                au_data_free(loaded_module);
                au_module_lib_del(&module.data.lib);
                au_data_free(abspath);
                break;
            }
            }

            comp_printf(state, "_M%ld_main();\n",
                        imported_module_idx_in_source);

            if (relative_module_idx != AU_PROGRAM_IMPORT_NO_MODULE) {
                const struct au_imported_module *relative_module =
                    au_imported_module_array_at_ptr(
                        &p_data->imported_modules, relative_module_idx);
                const struct au_c_comp_module *loaded_module =
                    au_c_comp_module_array_at_ptr(&g_state->modules,
                                                  imported_module_idx);
                link_to_imported(p_data, module_idx, module.type,
                                 imported_module_idx_in_source, g_state,
                                 state, relative_module, loaded_module);
            }

            break;
        }
        // Array instructions
        case AU_OP_ARRAY_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(capacity, 1)
            comp_printf(state,
                        "MOVE_VALUE(r%d,"
                        "au_value_struct("
                        "(struct au_struct*)au_obj_array_new(%d)));\n",
                        reg, capacity);
            break;
        }
        case AU_OP_ARRAY_PUSH: {
            uint8_t reg = bc(pos);
            uint8_t value = bc(pos + 1);
            comp_printf(
                state,
                "au_value_ref(r%d);"
                "au_obj_array_push(au_obj_array_coerce(r%d),r%d);\n",
                value, reg, value);
            break;
        }
        case AU_OP_IDX_GET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            comp_printf(state,
                        "COPY_VALUE(r%d,au_struct_idx_get(r%d,r%d));\n",
                        ret, reg, idx);
            break;
        }
        case AU_OP_IDX_SET: {
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
        case AU_OP_TUPLE_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(len, 1)
            comp_printf(state,
                        "MOVE_VALUE(r%d,"
                        "au_value_struct("
                        "(struct au_struct*)au_obj_tuple_new(%d)));\n",
                        reg, len);
            break;
        }
        case AU_OP_IDX_SET_STATIC: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            comp_printf(state,
                        "au_value_ref(r%d);"
                        "au_struct_idx_set(r%d,au_value_int(%d),r%d);\n",
                        ret, reg, idx, ret);
            break;
        }
        // Class instructions
        case AU_OP_CLASS_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(class_idx, 1);
            comp_printf(state,
                        "MOVE_VALUE(r%d,au_value_struct("
                        "(struct au_struct*)_struct_M%ld_%ld_new()"
                        "));\n",
                        reg, module_idx, class_idx, module_idx, class_idx);
            break;
        }
        case AU_OP_CLASS_GET_INNER: {
            uint8_t reg = bc(pos);
            DEF_BC16(inner, 1);
            comp_printf(state, "COPY_VALUE(r%d,self->v[%d]);\n", reg,
                        inner);
            break;
        }
        case AU_OP_CLASS_SET_INNER: {
            uint8_t reg = bc(pos);
            DEF_BC16(inner, 1);
            comp_printf(state, "COPY_VALUE(self->v[%d],r%d);\n", inner,
                        reg);
            break;
        }
        // Other
        case AU_OP_NOP:
            break;
        case AU_OP_PRINT: {
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

    au_data_free(labelled_lines);
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
        case AU_VALUE_STR: {
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
        case AU_VALUE_INT: {
            comp_printf(state, "return au_value_int(%d);\n",
                        au_value_get_int(val->real_value));
            break;
        }
        case AU_VALUE_DOUBLE: {
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
            if ((fn->flags & AU_FN_FLAG_EXPORTED) == 0) {
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
            break;
        }
        case AU_FN_NONE: {
            au_fatal("generating none function");
        }
        }
    }

    for (size_t i = 0; i < program->data.fns.len; i++) {
        const struct au_fn *fn = &program->data.fns.data[i];
        if (fn->type != AU_FN_DISPATCH)
            continue;
        const struct au_dispatch_func *dispatch_fn = &fn->as.dispatch_func;
        comp_printf(state,
                    "au_value_t _M%ld_f%ld"
                    "(const au_value_t*args) {\n",
                    module_idx, i);
        comp_printf(state, INDENT
                    "struct au_struct*s=au_struct_coerce(args[0]);\n");
        for (size_t i = 0; i < dispatch_fn->data.len; i++) {
            const struct au_dispatch_func_instance *data =
                &dispatch_fn->data.data[i];
            comp_printf(state,
                        INDENT "if(s->vdata==&_struct_M%ld_%d_vdata)",
                        module_idx, data->class_idx);
            comp_printf(state, "return _M%ld_f%d(args);\n", module_idx,
                        data->function_idx);
        }
        comp_printf(state, INDENT "abort();\n");
        comp_printf(state, "}\n");
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
            au_c_comp_func(state, bcs, &program->data, module_idx, i,
                           g_state);
            comp_printf(state, "}\n");
        }
    }

    for (size_t i = 0; i < program->data.classes.len; i++) {
        const struct au_class_interface *interface =
            program->data.classes.data[i];
        if (interface == 0) {
            continue;
        }

        // Struct declaration
        comp_printf(&g_state->header_file, "struct _M%ld_%d{\n",
                    module_idx, i);
        comp_printf(&g_state->header_file,
                    INDENT "struct au_struct header;\n");
        if (interface->map.entries_occ > 0) {
            comp_printf(&g_state->header_file,
                        INDENT "au_value_t v[%d];\n",
                        interface->map.entries_occ);
        }
        comp_printf(&g_state->header_file, "};\n");

        // Delete function
        comp_printf(&g_state->header_file,
                    "void _struct_M%ld_%d_del_fn("
                    "struct _M%ld_%d*s"
                    "){\n",
                    module_idx, i, module_idx, i);
        for (size_t i = 0; i < interface->map.entries_occ; i++) {
            comp_printf(&g_state->header_file,
                        INDENT "au_value_deref(s->v[%d]);\n", i);
        }
        comp_printf(&g_state->header_file, "}\n");

        // Virtual data function
        comp_printf(&g_state->header_file,
                    "static int _struct_M%ld_%d_vdata_init=0;\n",
                    module_idx, i);
        comp_printf(&g_state->header_file,
                    "static struct au_struct_vdata"
                    " _struct_M%ld_%d_vdata={0};\n",
                    module_idx, i);
        comp_printf(&g_state->header_file,
                    "struct au_struct_vdata *"
                    "_struct_M%ld_%d_vdata_get(){\n",
                    module_idx, i);
        comp_printf(&g_state->header_file,
                    INDENT "if(_struct_M%ld_%d_vdata_init)"
                           "return &_struct_M%ld_%d_vdata;\n",
                    module_idx, i, module_idx, i);
#define VDATA_FUNC(NAME)                                                  \
    comp_printf(&g_state->header_file,                                    \
                INDENT "_struct_M%ld_%d_vdata." NAME "="                  \
                       "(au_struct_" NAME "_t)_struct_M%ld_%d_" NAME      \
                       ";\n",                                             \
                module_idx, i, module_idx, i, module_idx, i);
        VDATA_FUNC("del_fn")
#undef VDATA_FUNC
        comp_printf(&g_state->header_file,
                    INDENT "_struct_M%ld_%d_vdata_init=1;"
                           "return &_struct_M%ld_%d_vdata;\n}\n",
                    module_idx, i, module_idx, i);

        comp_printf(&g_state->header_file,
                    "struct _M%ld_%d *_struct_M%ld_%d_new(){\n",
                    module_idx, i, module_idx, i);
        comp_printf(&g_state->header_file,
                    INDENT "struct _M%ld_%d*k="
                           "au_obj_malloc(sizeof(struct _M%ld_%d),"
                           "_struct_M%ld_%d_del_fn);\n",
                    module_idx, i, module_idx, i, module_idx, i);
        comp_printf(&g_state->header_file, INDENT "k->header.rc=1;\n");
        comp_printf(&g_state->header_file,
                    INDENT
                    "k->header.vdata=_struct_M%ld_%d_vdata_get();\n",
                    module_idx, i);
        for (size_t i = 0; i < interface->map.entries_occ; i++) {
            comp_printf(&g_state->header_file,
                        INDENT "k->v[%d]=au_value_none();\n", i);
        }
        comp_printf(&g_state->header_file, INDENT "return k;\n}\n");
    }

    comp_printf(state, "static int _M%ld_main_init=0;\n", module_idx);
    comp_printf(state, "static au_value_t _M%ld_main() {\n", module_idx);
    comp_printf(state,
                INDENT "if(_M%ld_main_init){return au_value_none();}\n",
                module_idx);
    comp_printf(state, INDENT "_M%ld_main_init=1;\n", module_idx);
    au_c_comp_func(state, &program->main, &program->data, module_idx,
                   AU_SM_FUNC_ID_MAIN, g_state);
    comp_printf(state, "}\n");
    comp_printf(&g_state->header_file, "static au_value_t _M%ld_main();\n",
                module_idx);
}

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

extern const char AU_RT_CODE[];
extern const size_t AU_RT_CODE_LEN;

#ifdef AU_TEST_RT_CODE
char *TEST_RT_CODE;
size_t TEST_RT_CODE_LEN;
#endif

void au_c_comp(struct au_c_comp_state *state,
               const struct au_program *program,
               struct au_c_comp_options options) {
    comp_write(state, AU_RT_HDR, AU_RT_HDR_LEN);
    comp_putc(state, '\n');

    comp_write(state, AU_C_COMP_EXTERN_FUNC_DECL,
               strlen(AU_C_COMP_EXTERN_FUNC_DECL));
    comp_putc(state, '\n');

    struct au_c_comp_global_state g_state =
        (struct au_c_comp_global_state){0};
    g_state.options = options;
    g_state.header_file = (struct au_c_comp_state){0};

    if (g_state.options.with_debug) {
        struct au_mmap_info mmap;
        assert(au_mmap_read(program->data.file, &mmap));
        create_line_info_array(&g_state.main_line_info, mmap.bytes,
                               mmap.size);
        au_mmap_del(&mmap);
    }

    struct au_c_comp_state main_mod_state = {
        .str = (struct au_char_array){0},
    };
    au_c_comp_module(&main_mod_state, program, 0, &g_state);

    comp_printf(state, "%.*s\n", g_state.header_file.str.len,
                g_state.header_file.str.data);
    comp_printf(state, "%.*s\n", main_mod_state.str.len,
                main_mod_state.str.data);

    au_c_comp_state_del(&main_mod_state);
    comp_printf(state, "int main() { _M0_main(); return 0; }\n");

    for (size_t i = 0; i < g_state.modules.len; i++) {
        comp_printf(state, "%.*s\n", g_state.modules.data[i].c_source.len,
                    g_state.modules.data[i].c_source.data);
    }

    comp_write(state, AU_RT_CODE, AU_RT_CODE_LEN);
#ifdef AU_TEST_RT_CODE
    comp_putc(state, '\n');
    comp_write(state, TEST_RT_CODE, TEST_RT_CODE_LEN);
#endif

    // Cleanup
    for (size_t i = 0; i < g_state.modules.len; i++) {
        struct au_c_comp_module *module = &g_state.modules.data[i];
        au_hm_vars_del(&module->fn_map);
        for (size_t i = 0; i < module->fns.len; i++) {
            au_fn_del(&module->fns.data[i]);
        }
        au_data_free(module->fns.data);
        au_data_free(module->c_source.data);
        au_data_free(module->line_info.data);
    }
    au_data_free(g_state.modules.data);
    au_hm_vars_del(&g_state.modules_map);
    au_data_free(g_state.main_line_info.data);
    au_c_comp_state_del(&g_state.header_file);
}