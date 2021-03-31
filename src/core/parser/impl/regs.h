// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "def.h"

/// Allocates a new register in the register stack and copies it into the
/// `out` argument
/// @return 1 if a new register is available, 0 if no more registers can be
/// allocated
AU_PRIVATE int au_parser_new_reg(struct au_parser *p, uint8_t *out);

/// Returns the last register in the register stack. The caller must NOT
/// call any opcode that changes the result in the last register
AU_PRIVATE uint8_t au_parser_last_reg(struct au_parser *p);

/// Swaps the top 2 registers
AU_PRIVATE void au_parser_swap_top_regs(struct au_parser *p);

/// Pushes the register `reg` onto the register stack
AU_PRIVATE void au_parser_push_reg(struct au_parser *p, uint8_t reg);

/// Pops a register from the register stack
AU_PRIVATE uint8_t au_parser_pop_reg(struct au_parser *p);

/// Flushes cached registers
AU_PRIVATE void au_parser_flush_cached_regs(struct au_parser *p);

/// Flushes the register stack
AU_PRIVATE void au_parser_flush_free_regs(struct au_parser *p);

AU_PRIVATE void au_parser_set_reg_unused(struct au_parser *p, uint8_t reg);

struct owned_reg_state {
    uint8_t reg;
    uint8_t is_owned;
};

AU_ARRAY_COPY(struct owned_reg_state, owned_reg_state_array, 1)

AU_PRIVATE struct owned_reg_state
au_parser_pop_reg_take_ownership(struct au_parser *p);

AU_PRIVATE void
au_parser_del_reg_from_ownership(struct au_parser *p,
                                 const struct owned_reg_state state);

static inline void
owned_reg_state_array_del(struct au_parser *p,
                          struct owned_reg_state_array *array) {
    for (size_t i = 0; i < array->len; i++)
        au_parser_del_reg_from_ownership(p, array->data[i]);
    au_data_free(array->data);
}
