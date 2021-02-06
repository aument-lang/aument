// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdio.h>
#include <assert.h>
#include <libgen.h>

#include "platform/tmpfile.h"
#include "platform/mmap.h"

#include "core/bc.h"
#include "core/program.h"
#include "core/bit_array.h"
#include "core/rt/exception.h"

#include "c_comp.h"

struct au_c_comp_global_state {
    struct au_str_array includes;
};

static void au_c_comp_module(
    struct au_c_comp_state *state,
    const struct au_program *program,
    const size_t module_idx,
    struct au_c_comp_global_state *g_state
);

void au_c_comp_state_del(struct au_c_comp_state *state) {
    fclose(state->f);
}

#define INDENT "    "

static void au_c_comp_func(
  struct au_c_comp_state *state,
  const struct au_bc_storage *bcs,
  const struct au_program_data *p_data,
  const size_t module_idx,
  struct au_c_comp_global_state *g_state
) {
    fprintf(state->f, INDENT "au_value_t t;\n");
    if(bcs->num_registers > 0) {
        fprintf(state->f, INDENT "au_value_t r0 = au_value_none()");
        for(int i = 1; i < bcs->num_registers; i++) {
            fprintf(state->f, ", r%d = au_value_none()", i);
        }
        fprintf(state->f, ";\n");
    }

    if(bcs->locals_len > 0) {
        fprintf(state->f, INDENT "au_value_t l0 = au_value_none()");
        for(int i = 1; i < bcs->locals_len; i++) {
            fprintf(state->f, ", l%d = au_value_none()", i);
        }
        fprintf(state->f, ";\n");
        if(bcs->num_args > 0) {
            for(int i = 0; i < bcs->num_args; i++) {
                fprintf(state->f, INDENT "l%d=args[%d];\n", i, i);
            }
        }
    }

#define bc(x) au_bc_buf_at(&bcs->bc, x)
#define DEF_BC16(VAR, n) \
assert(pos + n + 2 <= bcs->bc.len); \
uint16_t VAR = *((uint16_t*)(&bcs->bc.data[pos+n]));

    bit_array labelled_lines = calloc(1, BA_LEN(bcs->bc.len / 4));
    int has_args = 0;
    for(size_t pos = 0; pos < bcs->bc.len;) {
        uint8_t opcode = bc(pos);
        pos++;

        switch(opcode) {
            case OP_JIF: 
            case OP_JNIF: 
            case OP_JREL: {
                DEF_BC16(x, 1)
                const size_t offset = x * 4;
                const size_t abs_offset = pos - 1 + offset;
                SET_BIT(labelled_lines, (abs_offset/4));
                break;
            }
            case OP_JRELB: {
                DEF_BC16(x, 1)
                const size_t offset = x * 4;
                const size_t abs_offset = pos - 1 - offset;
                SET_BIT(labelled_lines, (abs_offset/4));
                break;
            }
            case OP_PUSH_ARG: {
                has_args = 1;
                break;
            }
            default: break;
        }
        pos += 3;
    }

    if(has_args) {
        fprintf(state->f, INDENT "struct au_value_stack s = {0};\n");
    }

    for(size_t pos = 0; pos < bcs->bc.len;) {
        if(GET_BIT(labelled_lines, pos/4)) {
            fprintf(state->f, INDENT "L%ld: ", pos);
        } else {
            fprintf(state->f, INDENT);
        }

        uint8_t opcode = bc(pos);
        if(opcode >= PRINTABLE_OP_LEN) {
            au_fatal("unknown opcode %d", opcode);
        }
        pos++;

        switch(opcode) {
            case OP_EXIT: {
                for(int i = 0; i < bcs->num_registers; i++)
                    fprintf(state->f, "au_value_deref(r%d);", i);
                for(int i = 0; i < bcs->locals_len; i++)
                    fprintf(state->f, "au_value_deref(l%d);", i);
                fprintf(state->f, "return au_value_none();\n");
                break;
            }
            case OP_MOV_U16: {
                uint8_t reg = bc(pos);
                DEF_BC16(n, 1)
                fprintf(state->f, "au_value_deref(r%d); r%d = au_value_int(%d);\n", reg, reg, n);
                break;
            }
#define BIN_OP(NAME) \
{ \
    uint8_t lhs = bc(pos); \
    uint8_t rhs = bc(pos+1); \
    uint8_t res = bc(pos+2); \
    fprintf(state->f, "t=r%d;r%d = au_value_" NAME "(r%d, r%d);au_value_deref(t);\n", res, res, lhs, rhs); \
    break; \
}
            case OP_MUL: BIN_OP("mul")
            case OP_DIV: BIN_OP("div")
            case OP_ADD: BIN_OP("add")
            case OP_SUB: BIN_OP("sub")
            case OP_MOD: BIN_OP("mod")
            case OP_EQ: BIN_OP("eq")
            case OP_NEQ: BIN_OP("neq")
            case OP_LT: BIN_OP("lt")
            case OP_GT: BIN_OP("gt")
            case OP_LEQ: BIN_OP("leq")
            case OP_GEQ: BIN_OP("geq")
#undef BIN_OP
            case OP_PRINT: {
                uint8_t lhs = bc(pos);
                fprintf(state->f, "au_value_print(r%d);\n", lhs);
                break;
            }
            case OP_JIF:
            case OP_JNIF: {
                uint8_t reg = bc(pos);
                DEF_BC16(x, 1)
                const size_t offset = x * 4;
                const size_t abs_offset = pos - 1 + offset;
                if(opcode == OP_JIF)
                    fprintf(state->f, "if(au_value_is_truthy(r%d)) goto L%ld;\n", reg, abs_offset);
                else
                    fprintf(state->f, "if(!au_value_is_truthy(r%d)) goto L%ld;\n", reg, abs_offset);
                break;
            }
            case OP_JREL: {
                DEF_BC16(x, 1)
                const size_t offset = x * 4;
                const size_t abs_offset = pos - 1 + offset;
                fprintf(state->f, "goto L%ld;\n", abs_offset);
                break;
            }
            case OP_JRELB: {
                DEF_BC16(x, 1)
                const size_t offset = x * 4;
                const size_t abs_offset = pos - 1 - offset;
                fprintf(state->f, "goto L%ld;\n", abs_offset);
                break;
            }
            case OP_MOV_REG_LOCAL: {
                uint8_t reg = bc(pos), local = bc(pos+1);
                fprintf(state->f, "au_value_deref(l%d); l%d = r%d; au_value_ref(l%d);\n", local, local, reg, local);
                break;
            }
            case OP_MOV_LOCAL_REG: {
                uint8_t local = bc(pos), reg= bc(pos+1);
                fprintf(state->f, "au_value_deref(r%d); r%d = l%d; au_value_ref(r%d);\n", reg, reg, local, reg);
                break;
            }
            case OP_LOAD_CONST: {
                uint8_t c = bc(pos);
                uint8_t reg = bc(pos+1);
                fprintf(state->f, "r%d = _M%ld_c%d();\n", reg, module_idx, c);
                break;
            }
            case OP_MOV_BOOL: {
                uint8_t n = bc(pos), reg = bc(pos+1);
                fprintf(state->f, "au_value_deref(r%d); r%d = au_value_bool(%d);\n", reg, reg, n);
                break;
            }
            case OP_CALL0:
            case OP_CALL1:
            case OP_CALL2:
            case OP_CALL3:
            case OP_CALL4:
            case OP_CALL5:
            case OP_CALL6:
            case OP_CALL7:
            case OP_CALL8:
            case OP_CALL9:
            case OP_CALL10:
            case OP_CALL11:
            case OP_CALL12:
            case OP_CALL13:
            case OP_CALL14:
            case OP_CALL15: {
                const uint8_t n_args = opcode - OP_CALL0;
                uint8_t reg = bc(pos);
                DEF_BC16(func_id, 1)

                assert(func_id <= p_data->fns.len);
                const struct au_fn *fn = &p_data->fns.data[func_id];
                if(fn->type == AU_FN_BC) {
                    const struct au_bc_storage *bcs = &fn->as.bc_func;
                    assert(n_args == bcs->num_args);
                    fprintf(state->f, "au_value_deref(r%d); r%d =", reg, reg);
                    if(n_args > 0) {
                        // TODO: deref arguments
                        fprintf(state->f, "_M%ld_f%d(&s.data[s.len-%d]); s.len-=%d;\n", module_idx, func_id, n_args, n_args);
                    } else {
                        fprintf(state->f, "_M%ld_f%d();\n", module_idx, n_args);
                    }
                } else if(fn->type == AU_FN_NATIVE) {
                    assert(0);
                }
                break;
            }
#define BIN_OP_ASG(NAME) \
{ \
    uint8_t reg = bc(pos); \
    uint8_t local = bc(pos+1); \
    fprintf(state->f, "t=l%d;l%d = au_value_" NAME "(l%d,r%d);au_value_deref(t);\n", local, local, local, reg); \
    break; \
}
            case OP_MUL_ASG: BIN_OP_ASG("mul")
            case OP_DIV_ASG: BIN_OP_ASG("div")
            case OP_ADD_ASG: BIN_OP_ASG("add")
            case OP_SUB_ASG: BIN_OP_ASG("sub")
            case OP_MOD_ASG: BIN_OP_ASG("mod")
#undef BIN_OP_ASG
            case OP_RET: {
                uint8_t reg = bc(pos);
                for(int i = 0; i < bcs->num_registers; i++)
                    if(i != reg)
                        fprintf(state->f, "au_value_deref(r%d);", i);
                for(int i = 0; i < bcs->locals_len; i++)
                    fprintf(state->f, "au_value_deref(l%d);", i);
                fprintf(state->f, "return r%d;\n", reg);
                break;
            }
            case OP_RET_LOCAL: {
                uint8_t local = bc(pos);
                for(int i = 0; i < bcs->num_registers; i++)
                    fprintf(state->f, "au_value_deref(r%d);", i);
                for(int i = 0; i < bcs->locals_len; i++)
                    if(i != local)
                        fprintf(state->f, "au_value_deref(l%d);", i);
                fprintf(state->f, "return l%d;\n", local);
                break;
            }
            case OP_RET_NULL: {
                for(int i = 0; i < bcs->num_registers; i++)
                    fprintf(state->f, "au_value_deref(r%d);", i);
                for(int i = 0; i < bcs->locals_len; i++)
                    fprintf(state->f, "au_value_deref(l%d);", i);
                fprintf(state->f, "return au_value_none();\n");
                break;
            }
            case OP_PUSH_ARG: {
                uint8_t reg = bc(pos);
                fprintf(state->f, "au_value_stack_add(&s, r%d); au_value_ref(r%d);\n", reg, reg);
                break;
            }
            case OP_IMPORT: {
                DEF_BC16(idx, 1)
                const char *relpath = au_str_array_at(&p_data->imports, idx);

                struct au_mmap_info mmap;
                char *abspath;

                if (relpath[0] == '.' && relpath[1] == '/') {
                    const char *relpath_canon = &relpath[2];
                    const size_t abspath_len = strlen(p_data->cwd) + strlen(relpath_canon) + 2;
                    abspath = malloc(abspath_len);
                    snprintf(abspath, abspath_len, "%s/%s", p_data->cwd, relpath_canon);
                    if(!au_mmap_read(abspath, &mmap))
                        au_perror("mmap");
                } else {
                    assert(0);
                }

                // 0 is main module
                const size_t module_idx = g_state->includes.len + 1;

                struct au_program program;
                assert(au_parse(mmap.bytes, mmap.size, &program) == 1);
                au_mmap_close(&mmap);
                program.data.cwd = dirname(abspath);
                // abspath is transferred to program.data

                char c_file[] = TMPFILE_TEMPLATE;
                int fd;
                if((fd = mkstemps(c_file, 2)) == -1)
                    au_perror("cannot generate tmpnam");
                au_str_array_add(&g_state->includes, strdup(c_file));
                struct au_c_comp_state mod_state = {
                    .f = fdopen(fd, "w"),
                };

                au_c_comp_module(&mod_state, &program, module_idx, g_state);

                au_program_del(&program);
                au_c_comp_state_del(&mod_state);

                fprintf(state->f, "extern au_value_t _M%ld_main(); _M%ld_main();\n", module_idx, module_idx);
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

void au_c_comp_module(
    struct au_c_comp_state *state,
    const struct au_program *program,
    const size_t module_idx,
    struct au_c_comp_global_state *g_state
) {
    for(size_t i = 0; i < program->data.data_val.len; i++) {
        const struct au_program_data_val *val = &program->data.data_val.data[i];
        switch(au_value_get_type(val->real_value)) {
            case VALUE_STR: {
                fprintf(state->f, "static inline au_value_t _M%ld_c%ld() {\n", module_idx, i);
                fprintf(state->f, INDENT "return au_value_string(au_string_from_const((const char[]){");
                for(size_t i = 0; i < val->buf_len; i++) {
                    fprintf(state->f, "%d,", program->data.data_buf[val->buf_idx + i]);
                }
                fprintf(state->f, "}, %d));\n", val->buf_len);
                fprintf(state->f, "}\n");
                break;
            }
            default: break;
        }
    }
    for(size_t i = 0; i < program->data.fns.len; i++) {
        if(program->data.fns.data[i].type == AU_FN_BC) {
            if (program->data.fns.data[i].as.bc_func.num_args > 0)  {
                fprintf(state->f, "static au_value_t _M%ld_f%ld(const au_value_t *args);\n", module_idx, i);
            } else {
                fprintf(state->f, "static au_value_t _M%ld_f%ld();\n", module_idx, i);
            }
        }
    }
    for(size_t i = 0; i < program->data.fns.len; i++) {
        const struct au_fn *fn = &program->data.fns.data[i];
        if (fn->type == AU_FN_BC) {
            const struct au_bc_storage *bcs = &fn->as.bc_func;
            if(bcs->num_args > 0) {
                fprintf(state->f, "au_value_t _M%ld_f%ld(const au_value_t *args) {\n", module_idx, i);
            } else {
                fprintf(state->f, "au_value_t _M%ld_f%ld() {\n", module_idx, i);
            }
            au_c_comp_func(state, bcs, &program->data, module_idx, g_state);
            fprintf(state->f, "}\n");
        }
    }
    fprintf(state->f, "au_value_t _M%ld_main() {\n", module_idx);
    au_c_comp_func(state, &program->main, &program->data, module_idx, g_state);
    fprintf(state->f, "}\n");
}

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

extern const char AU_RT_CODE[];
extern const size_t AU_RT_CODE_LEN;

void au_c_comp(struct au_c_comp_state *state, const struct au_program *program) {
    fwrite(AU_RT_HDR, 1, AU_RT_HDR_LEN, state->f);
    fprintf(state->f, "ARRAY_TYPE(au_value_t, au_value_stack, 1)\n");

    struct au_c_comp_global_state g_state = (struct au_c_comp_global_state){0};
    au_c_comp_module(state, program, 0, &g_state);

    fprintf(state->f, "int main() { _M0_main(); return 0; }\n");
    for(size_t i = 0; i < g_state.includes.len; i++) {
        fprintf(state->f, "#include \"%s\"\n", g_state.includes.data[i]);
    }
    fwrite(AU_RT_CODE, 1, AU_RT_CODE_LEN, state->f);

    // Cleanup
    for(size_t i = 0; i < g_state.includes.len; i++) {
        free(g_state.includes.data[i]);
    }
    free(g_state.includes.data);
}