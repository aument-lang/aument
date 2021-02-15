// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>

#include "bc.h"
#include "program.h"
#include "rt/exception.h"

const char *au_opcode_dbg[256] = {
    "(exit)",  "mov",       "mul",        "div",     "add",      "sub",
    "mod",     "mov",       "mov",        "print",   "eq",       "neq",
    "lt",      "gt",        "leq",        "geq",     "jif",      "jnif",
    "jrel",    "jrelb",     "loadc",      "mov",     "nop",      "mul",
    "div",     "add",       "sub",        "mod",     "push_arg", "call.0",
    "call.1",  "call.2",    "call.3",     "call.4",  "call.5",   "call.6",
    "call.7",  "call.8",    "call.9",     "call.10", "call.11",  "call.12",
    "call.13", "call.14",   "call.15",    "ret",     "ret",      "ret",
    "import",  "array_new", "array_push", "idx_get", "idx_set",  "not"};

void au_bc_dbg(const struct au_bc_storage *bcs,
               const struct au_program_data *data) {
#define bc(x) au_bc_buf_at(&bcs->bc, x)
    size_t pos = 0;
    while (pos < bcs->bc.len) {
        assert(pos % 4 == 0);
        uint8_t opcode = bc(pos);
        printf("%5ld: ", pos);

        if (opcode == OP_EXIT) {
            printf("(exit)\n");
            break;
        } else if (opcode >= PRINTABLE_OP_LEN) {
            au_fatal("unknown opcode %d", opcode);
        } else {
            printf("%s", au_opcode_dbg[opcode]);
        }
        pos++;

#define DEF_BC16(VAR, OFFSET)                                             \
    assert(pos + OFFSET + 2 <= bcs->bc.len);                              \
    uint16_t VAR = *((uint16_t *)(&bcs->bc.data[pos + OFFSET]));

        switch (opcode) {
        case OP_MOV_U16: {
            uint8_t reg = bc(pos);
            DEF_BC16(n, 1)
            printf(" #%d -> r%d\n", n, reg);
            pos += 3;
            break;
        }
        case OP_MOV_BOOL: {
            uint8_t n = bc(pos++), reg = bc(pos++);
            printf(" %s -> r%d\n", n ? "true" : "false", reg);
            pos++; // padding
            break;
        }
        case OP_MUL:
        case OP_DIV:
        case OP_MOD:
        case OP_ADD:
        case OP_SUB:
        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_GT:
        case OP_LEQ:
        case OP_GEQ: {
            uint8_t lhs = bc(pos);
            uint8_t rhs = bc(pos + 1);
            uint8_t res = bc(pos + 2);
            printf(" r%d, r%d -> r%d\n", lhs, rhs, res);
            pos += 3;
            break;
        }
        case OP_MOV_REG_LOCAL: {
            uint8_t reg = bc(pos);
            uint8_t local = bc(pos + 1);
            printf(" r%d -> [%d]\n", reg, local);
            pos += 3;
            break;
        }
        case OP_MOV_LOCAL_REG: {
            uint8_t local = bc(pos);
            uint8_t reg = bc(pos + 1);
            printf(" [%d] -> r%d\n", reg, local);
            pos += 3;
            break;
        }
        case OP_PRINT: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        case OP_JIF:
        case OP_JNIF: {
            uint8_t reg = bc(pos);
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            printf(" r%d, &%ld\n", reg, abs_offset);
            pos += 3;
            break;
        }
        case OP_JREL: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 + offset;
            printf(" &%ld\n", abs_offset);
            pos += 3;
            break;
        }
        case OP_JRELB: {
            DEF_BC16(x, 1)
            const size_t offset = x * 4;
            const size_t abs_offset = pos - 1 - offset;
            printf(" &%ld\n", abs_offset);
            pos += 3;
            break;
        }
        case OP_LOAD_CONST: {
            uint8_t reg = bc(pos);
            DEF_BC16(c, 1)
            printf(" c%d -> r%d\n", c, reg);
            pos += 3;
            break;
        }
        case OP_RET: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        case OP_RET_LOCAL: {
            uint8_t local = bc(pos);
            printf(" [%d]\n", local);
            pos += 3;
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
            uint8_t retval = bc(pos);
            DEF_BC16(x, 1)
            printf(" (%d) -> r%d\n", x, retval);
            pos += 3;
            break;
        }
        case OP_MUL_ASG:
        case OP_DIV_ASG:
        case OP_MOD_ASG:
        case OP_ADD_ASG:
        case OP_SUB_ASG: {
            uint8_t reg = bc(pos);
            uint8_t local = bc(pos + 1);
            printf(" r%d -> [%d]\n", reg, local);
            pos += 3;
            break;
        }
        case OP_PUSH_ARG: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        case OP_IMPORT: {
            DEF_BC16(idx, 1)
            printf(" \"%s\"\n", au_str_array_at(&data->imports, idx));
            pos += 3;
            break;
        }
        case OP_ARRAY_NEW: {
            uint8_t reg = bc(pos);
            DEF_BC16(capacity, 1)
            printf(" %d [capacity %d]\n", reg, capacity);
            pos += 3;
            break;
        }
        case OP_ARRAY_PUSH: {
            uint8_t reg = bc(pos);
            uint8_t value = bc(pos + 1);
            printf(" r%d, r%d\n", reg, value);
            pos += 3;
            break;
        }
        case OP_IDX_GET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            printf(" r%d [r%d] -> r%d\n", reg, idx, ret);
            pos += 3;
            break;
        }
        case OP_IDX_SET: {
            uint8_t reg = bc(pos);
            uint8_t idx = bc(pos + 1);
            uint8_t ret = bc(pos + 2);
            printf(" r%d [r%d] <- r%d\n", reg, idx, ret);
            pos += 3;
            break;
        }
        case OP_NOT: {
            uint8_t reg = bc(pos);
            printf(" r%d\n", reg);
            pos += 3;
            break;
        }
        default: {
            printf("\n");
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
            printf("(%ld):\n", i);
            au_bc_dbg(&p->data.fns.data[i].as.bc_func, &p->data);
        }
    }
}
