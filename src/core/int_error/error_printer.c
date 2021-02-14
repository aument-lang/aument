// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "error_printer.h"
#include "../parser/lexer.h"
#include <stdio.h>

static const char *error_path(struct au_error_location *loc) {
    if (loc->path == 0)
        return "(source)";
    return loc->path;
}

void au_print_parser_error(struct au_parser_result res,
                           struct au_error_location loc) {
    printf("parser error(%d) in %s: ", res.type, error_path(&loc));
    struct token errored_token;
    errored_token.type = TOK_EOF;
#define X(NAME) AU_PARSER_RES_##NAME
    switch (res.type) {
    case X(OK): {
        return;
    }
    case X(UNEXPECTED_TOKEN): {
        printf("unexpected token '%.*s'",
               (int)res.data.unexpected_token.got_token.len,
               res.data.unexpected_token.got_token.src);
        errored_token = res.data.unexpected_token.got_token;
        break;
    }
    case X(BYTECODE_GEN): {
        printf("bytecode generation failure\n");
        return;
    }
    case X(UNKNOWN_FUNCTION): {
        printf("unknown function %.*s",
               (int)res.data.unknown_function.name_token.len,
               res.data.unknown_function.name_token.src);
        errored_token = res.data.unknown_function.name_token;
        break;
    }
    case X(WRONG_ARGS): {
        printf("wrong number of arguments (expected %d, got %d)",
               res.data.wrong_args.expected_args,
               res.data.wrong_args.got_args);
        errored_token = res.data.wrong_args.call_token;
        break;
    }
    case X(UNKNOWN_VAR): {
        printf("unknown variable '%.*s'",
               (int)res.data.unknown_var.name_token.len,
               res.data.unknown_var.name_token.src);
        errored_token = res.data.unknown_var.name_token;
        break;
    }
    }
#undef X
    printf("\n");
    if (errored_token.type != TOK_EOF) {
        size_t lines = 1, cols = 0;
        size_t line_begin = 0, line_end = 0;
        for (size_t i = 0; i < loc.len; i++) {
            if (&loc.src[i] == errored_token.src) {
                break;
            } else if (loc.src[i] == '\n') {
                lines++;
                cols = 0;
                line_begin = i + 1;
            } else {
                cols++;
            }
        }
        int max_cols = 0;
        for (line_end = line_begin; line_end < loc.len; line_end++) {
            if (loc.src[line_end] == '\n')
                break;
            max_cols++;
        }
        size_t line_len = line_end - line_begin;
        int padding = printf("%ld | ", lines);
        printf("%.*s\n", (int)line_len, &loc.src[line_begin]);
        for (int i = 0; i < (padding + cols); i++)
            printf(" ");
        if (errored_token.len < max_cols)
            max_cols = errored_token.len;
        for (int i = 0; i < max_cols; i++)
            printf("^");
        printf("\n");
    }
}