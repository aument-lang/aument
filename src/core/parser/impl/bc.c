// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "bc.h"

void au_parser_emit_bc_u8(struct au_parser *p, uint8_t val) {
    au_bc_buf_add(&p->bc, val);
}

void au_replace_bc_u16(struct au_bc_buf *bc, size_t idx, uint16_t val) {
    if (idx + 1 >= bc->len)
        abort();
    uint16_t *ptr = (uint16_t *)(&bc->data[idx]);
    ptr[0] = val;
}

void au_parser_replace_bc_u16(struct au_parser *p, size_t idx,
                              uint16_t val) {
    au_replace_bc_u16(&p->bc, idx, val);
}

void au_parser_emit_bc_u16(struct au_parser *p, uint16_t val) {
    const size_t offset = p->bc.len;
    au_parser_emit_bc_u8(p, 0);
    au_parser_emit_bc_u8(p, 0);
    uint16_t *ptr = (uint16_t *)(&p->bc.data[offset]);
    ptr[0] = val;
}

void au_parser_emit_pad8(struct au_parser *p) {
    au_parser_emit_bc_u8(p, 0);
}