// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
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

static inline int is_id_start(int ch) {
    return l_isalpha(ch) || ch == '_';
}

static inline int is_id_cont(int ch) {
    return is_id_start(ch) || l_isdigit(ch);
}

#ifdef DEBUG_LEXER
static const char *token_type_dbg[] = {
    "TOK_EOF",    "TOK_INT",      "TOK_DOUBLE",        "TOK_IDENTIFIER",
    "TOK_STRING", "TOK_OPERATOR", "TOK_AT_IDENTIFIER",
};

static void token_dbg(const struct token *t);
#endif

void lexer_init(struct lexer *l, const char *src, size_t len) {
    l->src = src;
    l->pos = 0;
    l->len = len;

    l->lh_read = 0;
    l->lh_write = 0;
}

void lexer_del(struct lexer *l) { memset(l, 0, sizeof(struct lexer)); }

static struct token lexer_next_(struct lexer *l) {
    if (l->lh_read < l->lh_write) {
        struct token_lookahead lh = l->lh[l->lh_read];
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
        return (struct token){
            .type = TOK_EOF,
        };
    }

    const size_t start = l->pos;
    const char start_ch = l->src[l->pos];
    if (start_ch == '"' || start_ch == '\'') {
        l->pos++;
        while (!L_EOF()) {
            if (l->src[l->pos] == start_ch) {
                l->pos++;
                break;
            }
            l->pos++;
        }
        // Subtract 1 for the final quote character
        const size_t len = l->pos - start - 2;
        return (struct token){
            .type = TOK_STRING,
            .src = l->src + start + 1,
            .len = len,
        };
    } else if (l_isdigit(start_ch)) {
        l->pos++;
        while (!L_EOF()) {
            if (!l_isdigit(l->src[l->pos])) {
                break;
            }
            l->pos++;
        }
        if (!L_EOF() && l->src[l->pos] == '.') {
            l->pos++;
            while (!L_EOF()) {
                if (!l_isdigit(l->src[l->pos])) {
                    break;
                }
                l->pos++;
            }
            const size_t len = l->pos - start;
            return (struct token){
                .type = TOK_DOUBLE,
                .src = l->src + start,
                .len = len,
            };
        }
        const size_t len = l->pos - start;
        return (struct token){
            .type = TOK_INT,
            .src = l->src + start,
            .len = len,
        };
    } else if (is_id_start(start_ch)) {
        l->pos++;
        while (!L_EOF()) {
            if (!is_id_cont(l->src[l->pos])) {
                break;
            }
            l->pos++;
        }
        const size_t len = l->pos - start;
        return (struct token){
            .type = TOK_IDENTIFIER,
            .src = l->src + start,
            .len = len,
        };
    } else if (start_ch == '@') {
        l->pos++;
        if (!L_EOF() && is_id_start(l->src[l->pos])) {
            l->pos++;
            while (!L_EOF()) {
                if (!is_id_cont(l->src[l->pos])) {
                    break;
                }
                l->pos++;
            }
            const size_t len = l->pos - start;
            return (struct token){
                .type = TOK_AT_IDENTIFIER,
                .src = l->src + start,
                .len = len,
            };
        }
    } else if (start_ch == '+' || start_ch == '-' || start_ch == '*' ||
               start_ch == '/' || start_ch == '%' || start_ch == '!' ||
               start_ch == '<' || start_ch == '>' || start_ch == '=') {
        l->pos++;
        if (!L_EOF() && l->src[l->pos] == '=') {
            l->pos++;
            return (struct token){
                .type = TOK_OPERATOR,
                .src = l->src + start,
                .len = 2,
            };
        }
        return (struct token){
            .type = TOK_OPERATOR,
            .src = l->src + start,
            .len = 1,
        };
    } else if (start_ch == '&' || start_ch == '|') {
        l->pos++;
        if (!L_EOF() && l->src[l->pos] == start_ch) {
            l->pos++;
            return (struct token){
                .type = TOK_OPERATOR,
                .src = l->src + start,
                .len = 2,
            };
        }
        return (struct token){
            .type = TOK_OPERATOR,
            .src = l->src + start,
            .len = 1,
        };
    } else if (start_ch == '(' || start_ch == ')' || start_ch == ';' ||
               start_ch == ',' || start_ch == '{' || start_ch == '}' ||
               start_ch == '[' || start_ch == ']') {
        l->pos++;
        return (struct token){
            .type = TOK_OPERATOR,
            .src = l->src + start,
            .len = 1,
        };
    } else if (start_ch == ':') {
        l->pos++;
        if (!L_EOF() && l->src[l->pos] == ':') {
            l->pos++;
            return (struct token){
                .type = TOK_OPERATOR,
                .src = l->src + start,
                .len = 2,
            };
        }
        return (struct token){
            .type = TOK_OPERATOR,
            .src = l->src + start,
            .len = 1,
        };
    } else if (start_ch == '#') {
        l->pos++;
        if (!L_EOF() && l->src[l->pos] == '[') {
            l->pos++;
            return (struct token){
                .type = TOK_OPERATOR,
                .src = l->src + start,
                .len = 2,
            };
        }
    } else if (start_ch == 0) {
        l->pos = l->len;
        return (struct token){
            .type = TOK_EOF,
        };
    }

    return (struct token){
        .type = TOK_UNKNOWN,
        .src = l->src + start,
        .len = 1,
    };
#undef L_EOF
}

struct token lexer_peek(struct lexer *l, int lh_pos) {
    const size_t old_pos = l->pos;
    if (lh_pos == 0 && l->lh_write == 0) {
        struct token t = lexer_next_(l);
        struct token_lookahead *lh_new = &l->lh[l->lh_write++];
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
        struct token_lookahead *lh_last = &l->lh[l->lh_write - 1];
#ifdef DEBUG_LEXER
        printf("! from:");
        token_dbg(&lh_last->token);
#endif
        l->pos = lh_last->end_pos;
        struct token t = lexer_next_(l);
        struct token_lookahead *lh_new = &l->lh[l->lh_write++];
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
        struct token_lookahead *lh = &l->lh[lh_pos];
#ifdef DEBUG_LEXER
        printf("peek %d:", lh_pos);
        token_dbg(&lh->token);
#endif
        return lh->token;
    } else {
        _Unreachable;
    }
}

#ifdef DEBUG_LEXER
void token_dbg(const struct token *t) {
    printf("(token) { .type = %s, src = %p, len = %ld, text: [%.*s] }\n",
           token_type_dbg[t->type], t->src, t->len, (int)t->len, t->src);
}

struct token lexer_next(struct lexer *l) {
    struct token t = lexer_next_(l);
    token_dbg(&t);
    return t;
}
#else
struct token lexer_next(struct lexer *l) {
    return lexer_next_(l);
}
#endif
