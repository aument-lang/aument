// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "def.h"
#include "platform/platform.h"

AU_PRIVATE struct au_imported_module *
au_parser_resolve_module(struct au_parser *p, struct au_token module_tok,
                         size_t *module_idx_out);

AU_PRIVATE int
au_parser_resolve_fn(struct au_parser *p, struct au_token module_tok,
                     struct au_token id_tok, int num_args_in,
                     // output
                     size_t *func_idx_out, int *execute_self_out,
                     int *create_new_function_out);
