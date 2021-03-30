#pragma once

#include "def.h"

AU_PRIVATE int au_parser_new_reg(struct au_parser *p, uint8_t *out);

/// The caller must NOT call any opcode that changes the result in the last
/// register
AU_PRIVATE uint8_t au_parser_last_reg(struct au_parser *p);

AU_PRIVATE void au_parser_swap_top_regs(struct au_parser *p);

AU_PRIVATE void au_parser_push_reg(struct au_parser *p, uint8_t reg);

AU_PRIVATE void au_parser_set_reg_unused(struct au_parser *p, uint8_t reg);

AU_PRIVATE uint8_t au_parser_pop_reg(struct au_parser *p);

AU_PRIVATE uint8_t au_parser_pop_reg_no_consume(struct au_parser *p);

AU_PRIVATE void au_parser_flush_cached_regs(struct au_parser *p);

AU_PRIVATE void au_parser_flush_free_regs(struct au_parser *p);
