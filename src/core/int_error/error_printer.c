// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdio.h>

#include "../parser/lexer.h"
#include "error_printer.h"

#ifdef AU_TEST_EXE
static const char *error_path(const char *path) {
    (void)path;
    return "-";
}
#else
static const char *error_path(const char *path) {
    if (path == 0)
        return "(source)";
    return path;
}
#endif

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
    int padding = fprintf(stderr, "%d | ", lines);
    fprintf(stderr, "%.*s\n", line_len, &loc.src[line_begin]);
    if (error_len > 0) {
        for (int i = 0; i < (padding + cols); i++)
            fprintf(stderr, " ");
        if ((int)error_len < max_cols)
            max_cols = (int)error_len;
        for (int i = 0; i < max_cols; i++)
            fprintf(stderr, "^");
        fprintf(stderr, "\n");
    }
}

void au_print_parser_error(struct au_parser_result res,
                           struct au_error_location loc) {
    fprintf(stderr, "parser error(%d) in %s: ", res.type,
            error_path(loc.path));
    struct au_token errored_token;
    errored_token.type = AU_TOK_EOF;
#define X(NAME) AU_PARSER_RES_##NAME
    switch (res.type) {
    case X(OK): {
        return;
    }
    case X(UNEXPECTED_TOKEN): {
        fprintf(stderr, "unexpected token '%.*s'",
                (int)res.data.unexpected_token.got_token.len,
                res.data.unexpected_token.got_token.src);
        if (res.data.unexpected_token.expected != 0) {
            fprintf(stderr, ", expected %s",
                    res.data.unexpected_token.expected);
        }
        errored_token = res.data.unexpected_token.got_token;
        break;
    }
    case X(BYTECODE_GEN): {
        fprintf(stderr, "bytecode generation failure\n");
        return;
    }
    case X(UNKNOWN_FUNCTION): {
        fprintf(stderr, "unknown function %.*s",
                (int)res.data.unknown_id.name_token.len,
                res.data.unknown_id.name_token.src);
        errored_token = res.data.unknown_id.name_token;
        break;
    }
    case X(WRONG_ARGS): {
        fprintf(stderr, "wrong number of arguments (expected %d, got %d)",
                res.data.wrong_args.expected_args,
                res.data.wrong_args.got_args);
        errored_token = res.data.wrong_args.at_token;
        break;
    }
    case X(UNKNOWN_VAR): {
        fprintf(stderr, "unknown variable '%.*s'",
                (int)res.data.unknown_id.name_token.len,
                res.data.unknown_id.name_token.src);
        errored_token = res.data.unknown_id.name_token;
        break;
    }
    case X(EXPECT_GLOBAL_SCOPE): {
        fprintf(stderr, "this statement must be used in the global scope");
        errored_token = res.data.expect_global.at_token;
        break;
    }
    case X(DUPLICATE_CLASS): {
        fprintf(stderr, "this struct is already declared");
        errored_token = res.data.duplicate_id.name_token;
        break;
    }
    case X(CLASS_SCOPE_ONLY): {
        fprintf(stderr, "this variable can only be accessed in a method");
        errored_token = res.data.class_scope.at_token;
        break;
    }
    case X(DUPLICATE_MODULE): {
        fprintf(stderr, "this module is already imported");
        errored_token = res.data.duplicate_id.name_token;
        break;
    }
    case X(UNKNOWN_CLASS): {
        fprintf(stderr, "unknown struct %.*s",
                (int)res.data.unknown_id.name_token.len,
                res.data.unknown_id.name_token.src);
        errored_token = res.data.unknown_id.name_token;
        break;
    }
    case X(UNKNOWN_MODULE): {
        fprintf(stderr, "unknown module %.*s",
                (int)res.data.unknown_id.name_token.len,
                res.data.unknown_id.name_token.src);
        errored_token = res.data.unknown_id.name_token;
        break;
    }
    case X(DUPLICATE_ARG): {
        fprintf(stderr, "this argument is already declared");
        errored_token = res.data.duplicate_id.name_token;
        break;
    }
    case X(DUPLICATE_PROP): {
        fprintf(stderr, "this property is already declared");
        errored_token = res.data.duplicate_id.name_token;
        break;
    }
    case X(DUPLICATE_CONST): {
        fprintf(stderr, "this constant is already declared");
        errored_token = res.data.duplicate_id.name_token;
        break;
    }
    }
#undef X
    fprintf(stderr, "\n");
    if (errored_token.type != AU_TOK_EOF) {
        print_source(loc, errored_token.src - loc.src, errored_token.len);
    }
}

void au_print_interpreter_error(struct au_interpreter_result res,
                                struct au_error_location loc) {
    fprintf(stderr, "interpreter error(%d) in %s: ", res.type,
            error_path(loc.path));
    switch (res.type) {
#define X(NAME) AU_INT_ERR_##NAME
    case X(OK): {
        return;
    }
    case X(INCOMPAT_BIN_OP): {
        fprintf(stderr, "incompatible values for binary operation");
        break;
    }
    case X(INCOMPAT_CALL): {
        fprintf(stderr, "incompatible call");
        break;
    }
    case X(INVALID_INDEX): {
        fprintf(stderr, "trying to index a key that doesn't exist");
        break;
    }
    case X(INDEXING_NON_COLLECTION): {
        fprintf(stderr, "trying to index a value that isn't a collection");
        break;
    }
    case X(IMPORT_PATH): {
        fprintf(stderr, "unable to resolve import path");
        break;
    }
    case X(CIRCULAR_IMPORT): {
        fprintf(stderr, "circular import detected");
        break;
    }
    case X(BACKTRACE): {
        fprintf(stderr, "came from here");
        break;
    }
    case X(UNKNOWN_FUNCTION): {
        fprintf(stderr, "unknown or unexported function '%s'",
                res.data.unknown_id.key);
        break;
    }
    case X(UNKNOWN_CLASS): {
        fprintf(stderr, "unknown or unexported struct '%s'",
                res.data.unknown_id.key);
        break;
    }
    case X(UNKNOWN_CONST): {
        fprintf(stderr, "unknown or unexported constant '%s'",
                res.data.unknown_id.key);
        break;
    }
    case X(WRONG_ARGS): {
        fprintf(stderr,
                "wrong number of arguments for function '%s' (expected "
                "%d, got %d)",
                res.data.wrong_args.key, res.data.wrong_args.expected_args,
                res.data.wrong_args.got_args);
        break;
    }
    case X(RAISED_ERROR): {
        fprintf(stderr, "uncaught exception");
        break;
    }
    }
#undef X
    fprintf(stderr, "\n");
    print_source(loc, res.pos, 0);
}
