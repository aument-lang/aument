// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "def.h"

struct au_parser;
struct au_lexer;

AU_PRIVATE int au_parser_exec_expr(struct au_parser *p,
                                   struct au_lexer *l);
AU_PRIVATE int au_parser_exec_assign(struct au_parser *p,
                                     struct au_lexer *l);
AU_PRIVATE int au_parser_exec_logical(struct au_parser *p,
                                      struct au_lexer *l);
AU_PRIVATE int au_parser_emit_bc_binary_expr(struct au_parser *p);
AU_PRIVATE int au_parser_exec_eq(struct au_parser *p, struct au_lexer *l);
AU_PRIVATE int au_parser_exec_cmp(struct au_parser *p, struct au_lexer *l);
AU_PRIVATE int au_parser_exec_addsub(struct au_parser *p,
                                     struct au_lexer *l);
AU_PRIVATE int au_parser_exec_muldiv(struct au_parser *p,
                                     struct au_lexer *l);
AU_PRIVATE int au_parser_exec_bitwise_logic(struct au_parser *p,
                                            struct au_lexer *l);
AU_PRIVATE int au_parser_exec_bitwise_shift(struct au_parser *p,
                                            struct au_lexer *l);
AU_PRIVATE int au_parser_exec_unary_expr(struct au_parser *p,
                                         struct au_lexer *l);
AU_PRIVATE int au_parser_exec_index_expr(struct au_parser *p,
                                         struct au_lexer *l);
AU_PRIVATE int au_parser_exec_value(struct au_parser *p,
                                    struct au_lexer *l);
AU_PRIVATE int au_parser_exec_call(struct au_parser *p, struct au_lexer *l,
                                   struct au_token module_tok,
                                   struct au_token id_tok,
                                   int has_self_argument);
AU_PRIVATE int au_parser_exec_array_or_tuple(struct au_parser *p,
                                             struct au_lexer *l,
                                             int is_tuple);
AU_PRIVATE int au_parser_exec_dict(struct au_parser *p,
                                   struct au_lexer *l);
AU_PRIVATE int au_parser_exec_new_expr(struct au_parser *p,
                                       struct au_lexer *l);
