// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdint.h>
#include <string.h>

#define TOK_EOF 0
#define TOK_INT 1
#define TOK_DOUBLE 2
#define TOK_IDENTIFIER 3
#define TOK_STRING 4
#define TOK_OPERATOR 5

struct token {
    uint32_t type;
    uint32_t len;
    const char *src;
};

/// Checks if a token is a keyword of `str`
/// @param t Token to check
/// @param str Keyword to compare
/// @return 1 if token is identifier with value str, else, 0
static inline int token_keyword_cmp(const struct token *t,
                                    const char *str) {
    if (t->type != TOK_IDENTIFIER)
        return 0;
    if (t->len != (uint32_t)strlen(str))
        return 0;
    return memcmp(t->src, str, t->len) == 0;
}

#define LOOKAHEAD_MAX 2

struct token_lookahead {
    struct token token;
    size_t start_pos;
    size_t end_pos;
};

struct lexer {
    /// This object does not own the src pointer
    const char *src;
    size_t len;
    size_t pos;
    int lh_read, lh_write;
    struct token_lookahead lh[LOOKAHEAD_MAX];
};

/// Initializes a lexer instance
/// @param l instance to be initialized
/// @param src pointer to source code to be lexed
/// @param len byte-size length of the source code
void lexer_init(struct lexer *l, const char *src, size_t len);

/// Deinitializes a lexer instance
/// @param l instance to be deinitialized
void lexer_del(struct lexer *l);

/// Gets the next token in the lexer stream
struct token lexer_next(struct lexer *l);

/// Look ahead the next token in the lexer stream without
///     consuming it
struct token lexer_peek(struct lexer *l, int lh_pos);
