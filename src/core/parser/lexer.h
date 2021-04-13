// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdint.h>
#include <string.h>

#include "platform/platform.h"

#define AU_TOK_EOF 0
#define AU_TOK_INT 1
#define AU_TOK_DOUBLE 2
#define AU_TOK_IDENTIFIER 3
#define AU_TOK_STRING 4
#define AU_TOK_OPERATOR 5
#define AU_TOK_AT_IDENTIFIER 6
#define AU_TOK_CHAR_STRING 7
#define AU_TOK_UNKNOWN 8

struct au_token {
    uint32_t type;
    uint32_t len;
    const char *src;
};

/// Checks if a token is a keyword of `str`
/// @param t Token to check
/// @param str Keyword to compare
/// @return 1 if token is identifier with value str, else, 0
static inline int token_keyword_cmp(const struct au_token *t,
                                    const char *str) {
    if (t->type != AU_TOK_IDENTIFIER)
        return 0;
    if (t->len != (uint32_t)strlen(str))
        return 0;
    return memcmp(t->src, str, t->len) == 0;
}

struct au_lexer {
    /// This object does not own the src pointer
    const char *src;
    size_t len;
    size_t pos;
    struct au_token lookahead;
    int has_lookahead;
};

/// [func] Initializes a lexer instance
/// @param l instance to be initialized
/// @param src pointer to source code to be lexed
/// @param len byte-size length of the source code
AU_PUBLIC void au_lexer_init(struct au_lexer *l, const char *src,
                             size_t len);

/// [func] Deinitializes a lexer instance
/// @param l instance to be deinitialized
AU_PUBLIC void au_lexer_del(struct au_lexer *l);

/// [func] Gets the next token in the lexer stream
AU_PUBLIC struct au_token au_lexer_next(struct au_lexer *l);

/// [func] Look ahead the next token in the lexer stream without
/// consuming it
AU_PUBLIC struct au_token au_lexer_peek(struct au_lexer *l);
