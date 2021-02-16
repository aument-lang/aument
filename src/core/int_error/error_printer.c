// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdio.h>

#include "../parser/lexer.h"
#include "error_printer.h"

static const char *error_path(const char *path) {
    if (path == 0)
        return "(source)";
    return path;
}

static void print_source(struct au_error_location loc, size_t error_pos,
                         size_t error_len) {
    int lines = 1, cols = 0;
    size_t line_begin = 0, line_end = 0;
    for (size_t i = 0; i < error_pos; i++) {
        if (loc.src[i] == '\n') {
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
    int line_len = (int)(line_end - line_begin);
    int padding = printf("%d | ", lines);
    printf("%.*s\n", line_len, &loc.src[line_begin]);
    if (error_len > 0) {
        for (int i = 0; i < (padding + cols); i++)
            printf(" ");
        if (error_len < max_cols)
            max_cols = error_len;
        for (int i = 0; i < max_cols; i++)
            printf("^");
        printf("\n");
    }
}

void au_print_parser_error(struct au_parser_result res,
                           struct au_error_location loc) {
    printf("parser error(%d) in %s: ", res.type, error_path(loc.path));
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
        print_source(loc, errored_token.src - loc.src, errored_token.len);
    }
}

void au_print_interpreter_error(struct au_interpreter_result res,
                                struct au_error_location loc) {
    printf("interpreter error(%d) in %s: ", res.type,
           error_path(loc.path));
    switch (res.type) {
    case AU_INT_ERR_INCOMPAT_BIN_OP: {
        printf("incompatible values for binary operation: ");
        au_value_print(res.data.incompat_bin_op.left);
        printf(" and ");
        au_value_print(res.data.incompat_bin_op.right);
        break;
    }
    default:
        break;
    }
    printf("\n");
    print_source(loc, res.pos, 0);
}
