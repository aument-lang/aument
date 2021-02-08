#pragma once

#include <string.h>

enum token_type {
    TOK_EOF = 0,
    TOK_INT,
    TOK_DOUBLE,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_OPERATOR,
};

struct token {
    enum token_type type;
    char *src;
    size_t len;
};

/// Checks if a token is a keyword of `str`
/// @param t Token to check
/// @param str Keyword to compare
/// @return 1 if token is identifier with value str, else, 0
static inline int token_keyword_cmp(const struct token *t,
                                    const char *str) {
    if (t->type != TOK_IDENTIFIER)
        return 0;
    if (t->len != strlen(str))
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
    char *src;
    size_t len;
    size_t pos;

    struct token_lookahead lh[LOOKAHEAD_MAX];
    int lh_read, lh_write;
};

/// Initializes a lexer instance
/// @param l instance to be initialized
/// @param src pointer to source code to be lexed
/// @param len byte-size length of the source code
void lexer_init(struct lexer *l, char *src, size_t len);

/// Deinitializes a lexer instance
/// @param l instance to be deinitialized
void lexer_del(struct lexer *l);

/// Gets the next token in the lexer stream
struct token lexer_next(struct lexer *l);

/// Look ahead the next token in the lexer stream without
///     consuming it
struct token lexer_peek(struct lexer *l, int lh_pos);
