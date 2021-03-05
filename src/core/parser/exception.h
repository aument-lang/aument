// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/int_error/error_location.h"
#include "lexer.h"

#define X(NAME) AU_PARSER_RES_##NAME
enum au_parser_result_type {
    X(OK) = 0,
    X(UNEXPECTED_TOKEN) = 1,
    X(BYTECODE_GEN) = 2,
    X(UNKNOWN_FUNCTION) = 3,
    X(WRONG_ARGS) = 4,
    X(UNKNOWN_VAR) = 5,
    X(EXPECT_GLOBAL_SCOPE) = 6,
};
#undef X

struct au_parser_result {
    union {
        struct {
            struct au_token got_token;
            const char *expected;
        } unexpected_token;
        struct {
            struct au_token name_token;
        } unknown_function;
        struct {
            int got_args;
            int expected_args;
            struct au_token call_token;
        } wrong_args;
        struct {
            struct au_token name_token;
        } unknown_var;
        struct {
            struct au_token at_token;
        } expect_global;
    } data;
    enum au_parser_result_type type;
};
