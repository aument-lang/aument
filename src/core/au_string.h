// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdlib.h>
#include <stdint.h>
#ifdef DEBUG_RC
#include <stdio.h>
#endif

struct au_string {
    uint32_t rc;
    uint32_t len;
    char data[];
};

struct au_string *au_string_from_const(
    const char *s,
    size_t len
);

struct au_string *au_string_add(struct au_string *left, struct au_string *right);

static inline void au_string_ref(struct au_string *header) {
    header->rc++;
#ifdef DEBUG_RC
    printf("[%.*s]: [ref] rc now %d\n", header->len, header->data, header->rc);
#endif
}

static inline void au_string_deref(struct au_string *header) {
    header->rc--;
#ifdef DEBUG_RC
    printf("[%.*s]: [deref] rc now %d\n", header->len, header->data, header->rc);
#endif
    if(header->rc == 0) {
        free(header);
    }
}
