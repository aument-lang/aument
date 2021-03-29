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

    l->lh_read = 0;
    l->lh_write = 0;
}

void au_lexer_del(struct au_lexer *l) {
    memset(l, 0, sizeof(struct au_lexer));
}

static struct au_token au_lexer_next_(struct au_lexer *l) {
    if (l->lh_read < l->lh_write) {
        assert(l->lh_read >= 0 && l->lh_read <= LOOKAHEAD_MAX);
        struct au_token_lookahead lh = l->lh[l->lh_read];
        l->lh_read++;
        if (l->lh_read == l->lh_write) {
            l->lh_read = 0;
            l->lh_write = 0;
        }

        if (l->pos == lh.start_pos) {
#ifdef DEBUG_LEXER
            printf("(returns) ");
            token_dbg(&lh.token);
#endif
            l->pos = lh.end_pos;
            return lh.token;
        }
        l->lh_read = 0;
        l->lh_write = 0;
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
        l->pos++;
        size_t len = 1;
        if (!L_EOF() && l->src[l->pos] == 'x') {
            l->pos++;
            len++;
        } else {
            goto fail;
        }
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
        l->pos++;
        size_t len = 1;
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
            return (struct au_token){
                .type = AU_TOK_DOUBLE,
                .src = l->src + start,
                .len = len,
            };
        }
        return (struct au_token){
            .type = AU_TOK_INT,
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

struct au_token au_lexer_peek(struct au_lexer *l, int lh_pos) {
    const size_t old_pos = l->pos;
    if (lh_pos == 0 && l->lh_write == 0) {
        struct au_token t = au_lexer_next_(l);
        struct au_token_lookahead *lh_new = &l->lh[l->lh_write++];
        assert(l->lh_write <= LOOKAHEAD_MAX);
        lh_new->token = t;
        lh_new->start_pos = old_pos;
        lh_new->end_pos = l->pos;
        l->pos = old_pos;
#ifdef DEBUG_LEXER
        printf("peek %d:", lh_pos);
        token_dbg(&t);
#endif
        return t;
    } else if (lh_pos == l->lh_write) {
        struct au_token_lookahead *lh_last = &l->lh[l->lh_write - 1];
#ifdef DEBUG_LEXER
        printf("! from:");
        token_dbg(&lh_last->token);
#endif
        l->pos = lh_last->end_pos;
        struct au_token t = au_lexer_next_(l);
        struct au_token_lookahead *lh_new = &l->lh[l->lh_write++];
        assert(l->lh_write <= LOOKAHEAD_MAX);
        lh_new->token = t;
        lh_new->start_pos = lh_last->end_pos;
        lh_new->end_pos = l->pos;
        l->pos = old_pos;
#ifdef DEBUG_LEXER
        printf("! peek %d:", lh_pos);
        token_dbg(&t);
#endif
        return t;
    } else if (lh_pos < l->lh_write) {
        struct au_token_lookahead *lh = &l->lh[lh_pos];
#ifdef DEBUG_LEXER
        printf("peek %d:", lh_pos);
        token_dbg(&lh->token);
#endif
        return lh->token;
    } else {
        AU_UNREACHABLE;
    }
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
