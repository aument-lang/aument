// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdlib.h>

#include "core/rt/exception.h"
#include "lexer.h"
#include "platform/platform.h"

static inline int l_isspace(int ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static inline int l_isdigit(int ch) { return ch >= '0' && ch <= '9'; }

static inline int l_isalpha(int ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

static inline int l_is_id_start(int ch) {
    return l_isalpha(ch) || ch == '_';
}

static inline int l_is_id_cont(int ch) {
    return l_is_id_start(ch) || l_isdigit(ch);
}

static inline int l_ishex(int ch) {
    if ('0' <= ch && ch <= '9')
        return 1;
    if ('a' <= ch && ch <= 'f')
        return 1;
    if ('A' <= ch && ch <= 'F')
        return 1;
    return 0;
}

#ifdef DEBUG_LEXER
static const char *token_type_dbg[] = {
    "AU_TOK_EOF",           "AU_TOK_INT",    "AU_TOK_DOUBLE",
    "AU_TOK_IDENTIFIER",    "AU_TOK_STRING", "AU_TOK_OPERATOR",
    "AU_TOK_AT_IDENTIFIER",
};

static void token_dbg(const struct au_token *t);
#endif

void au_lexer_init(struct au_lexer *l, const char *src, size_t len) {
    l->src = src;
    l->pos = 0;
    l->len = len;
    l->lookahead = (struct au_token){.type = AU_TOK_EOF};
    l->has_lookahead = 0;
}

void au_lexer_del(struct au_lexer *l) {
    memset(l, 0, sizeof(struct au_lexer));
}

static struct au_token au_lexer_next_(struct au_lexer *l) {
    if (l->has_lookahead) {
        l->has_lookahead = 0;
        return l->lookahead;
    }

#define L_EOF() (l->pos >= l->len)
    while (!L_EOF()) {
        if (l_isspace(l->src[l->pos])) {
            l->pos++;
        } else if (l->src[l->pos] == '/' && l->pos + 1 < l->len &&
                   l->src[l->pos + 1] == '/') {
            l->pos += 2;
            while (!L_EOF() && l->src[l->pos] != '\n')
                l->pos++;
        } else {
            break;
        }
    }
    if (L_EOF()) {
        return (struct au_token){
            .type = AU_TOK_EOF,
        };
    }

    const size_t start = l->pos;
    const char start_ch = l->src[l->pos];
    if (start_ch == '"' || start_ch == '\'') {
        l->pos++;
        size_t len = 0;
        while (!L_EOF()) {
            if (l->src[l->pos] == '\\') {
                l->pos++;
                len++;
                if (L_EOF()) {
                    goto fail;
                } else {
                    l->pos++;
                    len++;
                }
                continue;
            } else if (l->src[l->pos] == start_ch) {
                l->pos++;
                break;
            }
            l->pos++;
            len++;
        }
        if (!L_EOF() && l->src[l->pos] == 'c') {
            l->pos++;
            len++;
            return (struct au_token){
                .type = AU_TOK_CHAR_STRING,
                .src = l->src + start + 1,
                .len = len,
            };
        }
        return (struct au_token){
            .type = AU_TOK_STRING,
            .src = l->src + start + 1,
            .len = len,
        };
    } else if (start_ch == '0') {
        if (!L_EOF() && l->src[l->pos + 1] == 'x') {
            l->pos += 2;
        } else {
            goto parse_decimal;
        }
        size_t len = 2;
        while (!L_EOF() && l_ishex(l->src[l->pos])) {
            l->pos++;
            len++;
        }
        return (struct au_token){
            .type = AU_TOK_INT,
            .src = l->src + start,
            .len = len,
        };
    } else if (l_isdigit(start_ch)) {
parse_decimal:
        l->pos++;
        size_t len = 1;
        uint32_t type = AU_TOK_INT;
        while (!L_EOF()) {
            if (!l_isdigit(l->src[l->pos])) {
                break;
            }
            l->pos++;
            len++;
        }
        if (!L_EOF() && l->src[l->pos] == '.') {
            l->pos++;
            len++;
            while (!L_EOF()) {
                if (!l_isdigit(l->src[l->pos])) {
                    break;
                }
                l->pos++;
                len++;
            }
            type = AU_TOK_DOUBLE;
        }
        if (!L_EOF() && (l->src[l->pos] == 'e' || l->src[l->pos] == 'E')) {
            l->pos++;
            len++;
            if (!L_EOF() &&
                (l->src[l->pos] == '+' || l->src[l->pos] == '-')) {
                l->pos++;
                len++;
            }
            while (!L_EOF()) {
                if (!l_isdigit(l->src[l->pos])) {
                    break;
                }
                l->pos++;
                len++;
            }
            type = AU_TOK_DOUBLE;
        }
        return (struct au_token){
            .type = type,
            .src = l->src + start,
            .len = len,
        };
    } else if (l_is_id_start(start_ch)) {
        l->pos++;
        size_t len = 1;
        while (!L_EOF()) {
            if (!l_is_id_cont(l->src[l->pos])) {
                break;
            }
            l->pos++;
            len++;
        }
        return (struct au_token){
            .type = AU_TOK_IDENTIFIER,
            .src = l->src + start,
            .len = len,
        };
    } else if (start_ch == '@') {
        l->pos++;
        size_t len = 1;
        if (!L_EOF() && l_is_id_start(l->src[l->pos])) {
            l->pos++;
            len++;
            while (!L_EOF()) {
                if (!l_is_id_cont(l->src[l->pos])) {
                    break;
                }
                l->pos++;
                len++;
            }
            return (struct au_token){
                .type = AU_TOK_AT_IDENTIFIER,
                .src = l->src + start,
                .len = len,
            };
        }
    }

#define X(TOKEN)                                                          \
    if ((l->pos + strlen(TOKEN) <= l->len) &&                             \
        memcmp(&l->src[l->pos], TOKEN, strlen(TOKEN)) == 0) {             \
        size_t _len = strlen(TOKEN);                                      \
        l->pos += _len;                                                   \
        return (struct au_token){                                         \
            .type = AU_TOK_OPERATOR,                                      \
            .src = l->src + start,                                        \
            .len = _len,                                                  \
        };                                                                \
    }
    // clang-format off

    // Two character operators:
    // A/C assignment operators
    else X("+=") else X("-=") else X("*=") else X("/=")
    else X("%=") else X("!=") else X("<=") else X(">=")
    else X("==")
    else X(">>") else X("<<")
    else X("&&") else X("||") else X("&=") else X("|=")
    // Other
    else X("#[") else X("::")
    else X(")?") else X(")!")
    else X("]?")

    // One character operators:
    // Arithmetic/comparison operators
    else X("+")  else X("-")  else X("*")  else X("/")
    else X("%")  else X("!")  else X("<")  else X(">")
    else X("=")
    // Bitwise operators
    else X("&")  else X("|")  else X("^")  else X("~")
    // Other
    else X("(")  else X(")")  else X("{")  else X("}")
    else X(";")  else X(",")  else X(".")  else X(":")
    else X("[")  else X("]")

    else if (start_ch == 0) {
        l->pos = l->len;
        return (struct au_token){
            .type = AU_TOK_EOF,
        };
    }

#undef X // clang-format on

fail:
    return (struct au_token){
        .type = AU_TOK_UNKNOWN,
        .src = l->src + start,
        .len = 1,
    };
#undef L_EOF
}

struct au_token au_lexer_peek(struct au_lexer *l) {
    if (l->has_lookahead) {
        return l->lookahead;
    }
    l->lookahead = au_lexer_next_(l);
    l->has_lookahead = 1;
    return l->lookahead;
}

#ifdef DEBUG_LEXER
void token_dbg(const struct au_token *t) {
    printf("(token) { .type = %s, src = %p, len = %ld, text: [%.*s] }\n",
           token_type_dbg[t->type], t->src, t->len, (int)t->len, t->src);
}

struct au_token au_lexer_next(struct au_lexer *l) {
    struct au_token t = au_lexer_next_(l);
    token_dbg(&t);
    return t;
}
#else
struct au_token au_lexer_next(struct au_lexer *l) {
    return au_lexer_next_(l);
}
#endif
