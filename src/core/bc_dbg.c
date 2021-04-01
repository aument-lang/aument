// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include "bc.h"
#include "program.h"
#include "rt/exception.h"

const char *au_opcode_dbg[256] = {"load_self",
                                  "mov",
                                  "mul",
                                  "div",
                                  "add",
                                  "sub",
                                  "mod",
                                  "mov",
                                  "mov",
                                  "print",
                                  "eq",
                                  "neq",
                                  "lt",
                                  "gt",
                                  "leq",
                                  "geq",
                                  "jif",
                                  "jnif",
                                  "jrel",
                                  "jrelb",
                                  "loadc",
                                  "mov",
                                  "nop",
                                  "push_arg",
                                  "call",
                                  "ret",
                                  "ret",
                                  "ret",
                                  "import",
                                  "array_new",
                                  "array_push",
                                  "idx_get",
                                  "idx_set",
                                  "not",
                                  "tuple_new",
                                  "idx_set_static",
                                  "mov_class",
                                  "mov_class",
                                  "class_new",
                                  "call_1",
                                  "set_const",
                                  "load_func",
                                  "bind_arg_to_func",
                                  "call_func_value",
                                  "load_nil",
                                  "bor",
                                  "bxor",
                                  "band",
                                  "bshl",
                                  "bshr",
                                  "bnot",
                                  "neg",
                                  "new",
                                  "dict_new"};

#ifdef AU_COVERAGE
void au_bc_dbg(const struct au_bc_storage *bcs,
               const struct au_program_data *data) {
    (void)bcs;
    (void)data;
}

void au_program_dbg(const struct au_program *p) { (void)p; }
#else
void au_bc_dbg(const struct au_bc_storage *bcs,
               const struct au_program_data *data) {
#define bc(x) au_bc_buf_at(&bcs->bc, x)
    size_t pos = 0;
    while (pos < bcs->bc.len) {
        assert(pos % 4 == 0);
        uint8_t opcode = bc(pos);
        printf("%5" PRIdPTR ": ", pos);
        if (opcode >= AU_OP_MAX_PRINTABLE) {
            au_fatal("unknown opcode %d", opcode);
        } else {
            printf("%s", au_opcode_dbg[opcode]);
        }
        pos++;

#define DEF_BC16(VAR, OFFSET)                                             \
    uint16_t VAR;                                                         \
    do {                                                                  \
        assert(pos + OFFSET + 2 <= bcs->bc.len);                          \
        VAR = *((uint16_t *)(&bcs->bc.data[pos + OFFSET]));               \
    } while (0)

        switch (opcode) {
        case AU_OP_LOAD_SELF: {
            printf("\n");
            pos += 3;
            break;
        }
        // Move instructions
        case AU_OP_MOV_U16: {
            uint8_t reg = bc(pos);
            DEF_BC16(n, 1);
            printf(" #%d -> r%d\n", n, reg);
            pos += 3;
            break;
        }
        case AU_OP_MOV_REG_LOCAL: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);
            printf(" r%d -> [%d]\n", reg, local);
            pos += 3;
            break;
        }
        case AU_OP_MOV_LOCAL_REG: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);
            printf(" [%d] -> r%d\n", local, reg);
            pos += 3;
            break;
        }
        case AU_OP_MOV_BOOL: {
            uint8_t n = bc(pos), reg = bc(pos + 1);
            printf(" %s -> r%d\n", n ? "true" : "false", reg);
            pos += 3;
            break;
        }
        case AU_OP_LOAD_NIL: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        // Load/store constants
        case AU_OP_LOAD_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1);
            printf(" c%d -> r%d\n", c, reg);
            pos += 3;
            break;
        }
        case AU_OP_SET_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1);
            printf(" r%d -> c%d\n", reg, c);
            pos += 3;
            break;
        }
        // Binary operations
        case AU_OP_MUL:
        case AU_OP_DIV:
        case AU_OP_MOD:
        case AU_OP_ADD:
        case AU_OP_SUB:
        case AU_OP_EQ:
        case AU_OP_NEQ:
        case AU_OP_LT:
        case AU_OP_GT:
        case AU_OP_LEQ:
        case AU_OP_GEQ:
        case AU_OP_BAND:
        case AU_OP_BOR:
        case AU_OP_BSHL:
        case AU_OP_BSHR: {
            uint8_t lhs = bc(pos);
            uint8_t rhs = bc(pos + 1);
            uint8_t res = bc(pos + 2);
            printf(" r%d, r%d -> r%d\n", lhs, rhs, res);
            pos += 3;
            break;
        }
        // Unary instructions
        case AU_OP_NOT:
        case AU_OP_NEG:
        case AU_OP_BNOT: {
            uint8_t reg = bc(pos);
            uint8_t ret = bc(pos + 1);
            printf(" r%d <- r%d\n", ret, reg);
            pos += 3;
            break;
        }
        // Jump instructions
        case AU_OP_JIF:
        case AU_OP_JNIF: {
            uint8_t reg = bc(pos);
            DEF_BC16(x, 1);
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            printf(" r%d, &%" PRIdPTR "\n", reg, abs_offset);
            pos += 3;
            break;
        }
        case AU_OP_JREL: {
            DEF_BC16(x, 1);
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            printf(" &%" PRIdPTR "\n", abs_offset);
            pos += 3;
            break;
        }
        case AU_OP_JRELB: {
            DEF_BC16(x, 1);
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            printf(" &%" PRIdPTR "\n", abs_offset);
            pos += 3;
            break;
        }
        // Call instructions
        case AU_OP_PUSH_ARG: {
            uint8_t reg1 = bc(pos);
            uint8_t reg2 = bc(pos + 1);
            uint8_t reg3 = bc(pos + 2);
            printf(" r%d, r%d, r%d\n", reg1, reg2, reg3);
            pos += 3;
            break;
        }
        case AU_OP_CALL:
        case AU_OP_CALL_CATCH: {
            uint8_t retval = bc(pos);
            DEF_BC16(x, 1);
            printf(" (%d) -> r%d\n", x, retval);
            pos += 3;
            break;
        }
        // Return instructions
        case AU_OP_RET: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        case AU_OP_RET_LOCAL: {
            DEF_BC16(local, 1);
            printf(" [%d]\n", local);
            pos += 3;
            break;
        }
        // Modules
        case AU_OP_IMPORT: {
            DEF_BC16(idx, 1);
            assert(idx < data->imports.len);
            printf(
                " \"%s\"\n",
                au_program_import_array_at_ptr(&data->imports, idx)->path);
            pos += 3;
            break;
        }
        // Array instructions
        case AU_OP_ARRAY_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(capacity, 1);
            printf(" -> r%d [capacity %d]\n", reg, capacity);
            pos += 3;
            break;
        }
        case AU_OP_ARRAY_PUSH: {
            uint8_t reg = bc(pos);
            uint8_t value = bc(pos + 1);
            printf(" r%d, r%d\n", reg, value);
            pos += 3;
            break;
        }
        case AU_OP_IDX_GET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            printf(" r%d [r%d] -> r%d\n", reg, idx, ret);
            pos += 3;
            break;
        }
        case AU_OP_IDX_SET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            printf(" r%d [r%d] <- r%d\n", reg, idx, ret);
            pos += 3;
            break;
        }
        // Tuple instructions
        case AU_OP_TUPLE_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(len, 1);
            printf(" -> r%d [length %d]\n", reg, len);
            pos += 3;
            break;
        }
        case AU_OP_IDX_SET_STATIC: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            printf(" r%d [%d] <- r%d\n", reg, idx, ret);
            pos += 3;
            break;
        }
        // Dictionary instructions
        case AU_OP_DICT_NEW: {
            uint8_t reg = bc(pos);
            printf(" -> r%d\n", reg);
            pos += 3;
            break;
        }
        // Class instructions
        case AU_OP_CLASS_NEW:
        case AU_OP_CLASS_NEW_INITIALZIED: {
            uint8_t reg = bc(pos);
            DEF_BC16(class_idx, 1);
            printf(" (id %d) -> r%d\n", class_idx, reg);
            pos += 3;
            break;
        }
        case AU_OP_CLASS_GET_INNER: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);
            printf(" @%d -> r%d\n", local, reg);
            pos += 3;
            break;
        }
        case AU_OP_CLASS_SET_INNER: {
            uint8_t reg = bc(pos);
            DEF_BC16(local, 1);
            printf(" r%d -> @%d\n", reg, local);
            pos += 3;
            break;
        }
        // Other
        case AU_OP_PRINT: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        default: {
            printf("\n");
            pos += 3;
            break;
        }
        }
    }
}

void au_program_dbg(const struct au_program *p) {
    printf("(main):\n");
    au_bc_dbg(&p->main, &p->data);
    for (size_t i = 0; i < p->data.fns.len; i++) {
        if (p->data.fns.data[i].type == AU_FN_BC) {
            printf("(%" PRIdPTR "):\n", i);
            au_bc_dbg(&p->data.fns.data[i].as.bc_func, &p->data);
        }
    }
}
#endif
