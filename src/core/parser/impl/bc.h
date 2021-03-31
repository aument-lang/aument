// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "def.h"

AU_PRIVATE void au_parser_emit_bc_u8(struct au_parser *p, uint8_t val);
AU_PRIVATE void au_parser_emit_pad8(struct au_parser *p);

AU_PRIVATE void au_replace_bc_u16(struct au_bc_buf *bc, size_t idx,
                                  uint16_t val);
AU_PRIVATE void au_parser_replace_bc_u16(struct au_parser *p, size_t idx,
                                         uint16_t val);

AU_PRIVATE void au_parser_emit_bc_u16(struct au_parser *p, uint16_t val);
