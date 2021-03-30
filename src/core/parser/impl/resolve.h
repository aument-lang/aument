#pragma once

#include "def.h"
#include "platform/platform.h"

AU_PRIVATE struct au_imported_module *
au_parser_resolve_module(struct au_parser *p, struct au_token module_tok,
                         size_t *module_idx_out);

AU_PRIVATE int au_parser_resolve_fn(struct au_parser *p,
                                    struct au_token module_tok,
                                    struct au_token id_tok,
                                    int num_args_in, size_t *func_idx_out,
                                    int *execute_self_out);
