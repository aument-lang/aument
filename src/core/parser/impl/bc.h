#pragma once

#include "def.h"

AU_PRIVATE void au_parser_emit_bc_u8(struct au_parser *p, uint8_t val);
AU_PRIVATE void au_parser_emit_pad8(struct au_parser *p);

AU_PRIVATE void au_replace_bc_u16(struct au_bc_buf *bc, size_t idx,
                                  uint16_t val);
AU_PRIVATE void au_parser_replace_bc_u16(struct au_parser *p, size_t idx,
                                         uint16_t val);

AU_PRIVATE void au_parser_emit_bc_u16(struct au_parser *p, uint16_t val);
