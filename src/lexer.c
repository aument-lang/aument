// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
enum token_type {
    TOK_EOF = 0,
    TOK_INT,
    TOK_FLOAT,
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_OPERATOR,
};

struct token {
    enum token_type type;
    char *src;
    size_t len;
};

#ifdef DEBUG_LEXER
static const char *token_type_dbg[] = {
    "TOK_EOF",
    "TOK_INT",
    "TOK_FLOAT",
    "TOK_IDENTIFIER",
    "TOK_STRING",
    "TOK_OPERATOR",
};

static void token_dbg(const struct token *t);
#endif

/// Returns 1 if token is identifier with value str, else, returns 0
static inline int token_keyword_cmp(const struct token *t, const char *str) {
    if(t->type != TOK_IDENTIFIER) return 0;
    if(t->len != strlen(str)) return 0;
    return memcmp(t->src, str, t->len) == 0;
}

struct token_lookahead {
    struct token token;
    size_t start_pos;
    size_t end_pos;
};

#define LOOKAHEAD_MAX 2

struct lexer {
    /// This object does not own the src pointer
    char *src;
    size_t len;
    size_t pos;

    struct token_lookahead lh[LOOKAHEAD_MAX];
    int lh_read, lh_write;
};

static void lexer_init(struct lexer *l, char *src, size_t len) {
    l->src = src;
    l->pos = 0;
    l->len = len;

    l->lh_read = 0;
    l->lh_write = 0;
}

static void lexer_del(struct lexer *l) {
    memset(l, 0, sizeof(struct lexer));
}

static struct token lexer_next_(struct lexer *l) {
    if(l->lh_read < l->lh_write) {
        struct token_lookahead lh = l->lh[l->lh_read];
        l->lh_read++;
        if(l->lh_read == l->lh_write) {
            l->lh_read = 0;
            l->lh_write = 0;
        }

        if(l->pos == lh.start_pos) {
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

#define L_EOF (l->pos >= l->len)
    while(isspace(l->src[l->pos]) && !L_EOF) {
        l->pos++;
    }
    if(L_EOF) {
        return (struct token){
            .type = TOK_EOF,
        };
    }

    const size_t start = l->pos;
    const char start_ch = l->src[l->pos];
    if(start_ch == '"' || start_ch == '\'') {
        l->pos++;
        while(!L_EOF) {
            if(l->src[l->pos] == start_ch) {
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
    } else if (isdigit(start_ch)) {
        l->pos++;
        while(!L_EOF) {
            if(!isdigit(l->src[l->pos])) {
                break;
            }
            l->pos++;
        }
        const size_t len = l->pos - start;
        return (struct token){
            .type = TOK_INT,
            .src = l->src + start,
            .len = len,
        };
    } else if (isalpha(start_ch)) {
        l->pos++;
        while(!L_EOF) {
            if(!isalnum(l->src[l->pos])) {
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
    } else if (
        start_ch == '+' ||
        start_ch == '-' ||
        start_ch == '*' ||
        start_ch == '/' ||
        start_ch == '%' ||
        start_ch == '!' ||
        start_ch == '<' ||
        start_ch == '>' ||
        start_ch == '='
    ) {
        l->pos++;
        if(!L_EOF && l->src[l->pos] == '=') {
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
        if(!L_EOF && (l->src[l->pos] == start_ch || l->src[l->pos] == '=')) {
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
    } else if (
        start_ch == '(' ||
        start_ch == ')' ||
        start_ch == ';' ||
        start_ch == ',' ||
        start_ch == ':' ||
        start_ch == '{' ||
        start_ch == '}'
    ) {
        l->pos++;
        return (struct token){
            .type = TOK_OPERATOR,
            .src = l->src + start,
            .len = 1,
        };
    }
    
    au_fatal("unexpected character %c", start_ch);
#undef L_EOF
}

static struct token lexer_peek(struct lexer *l, int lh_pos) {
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
    } else if(lh_pos == l->lh_write) {
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
        __builtin_unreachable();
    }
}

#ifdef DEBUG_LEXER
void token_dbg(const struct token *t) {
    printf("(token) { .type = %s, src = %p, len = %ld, text: [%.*s] }\n",
        token_type_dbg[t->type],
        t->src,
        t->len,
        (int)t->len,
        t->src
    );
}

static struct token lexer_next(struct lexer *l) {
    struct token t = lexer_next_(l);
    token_dbg(&t);
    return t;
}
#else
#define lexer_next lexer_next_
#endif
