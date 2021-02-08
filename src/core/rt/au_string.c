// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "au_string.h"
#include <string.h>
#endif

struct au_string *au_string_from_const(const char *s, size_t len) {
    struct au_string *header = malloc(sizeof(struct au_string) + len);
    header->rc = 1;
    header->len = len;
    memcpy(header->data, s, len);
    return header;
}

struct au_string *au_string_add(struct au_string *left,
                                struct au_string *right) {
    const size_t len = left->len + right->len;
    struct au_string *header = malloc(sizeof(struct au_string) + len);
    header->rc = 1;
    header->len = len;
    memcpy(header->data, left->data, left->len);
    memcpy(&header->data[left->len], right->data, right->len);
    return header;
}

int au_string_cmp(struct au_string *left, struct au_string *right) {
    if (left->len > right->len) {
        return 1;
    } else if (left->len < right->len) {
        return -1;
    } else if (left->len == right->len) {
        return memcmp(left->data, right->data, left->len);
    }
    return -1;
}
