// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef DEBUG_RC
#include <stdio.h>
#endif
#endif

struct au_string {
    uint32_t rc;
    uint32_t len;
    char data[];
};

/// Creates an au_string from a constant array of chars
/// @param s Pointer to the array of chars
/// @param len Byte size of the string
/// @returns An au_string instance
struct au_string *au_string_from_const(const char *s, size_t len);

/// Creates an au_string from concatenating 2 au_string(s)
/// @param left First string
/// @param right Second string
/// @return Concatenation of `left` and `right`
struct au_string *au_string_add(struct au_string *left,
                                struct au_string *right);

/// Compares 2 au_string instances
/// @param left First string
/// @param right Second string
/// @return Comparison of `left` and `right`
int au_string_cmp(struct au_string *left, struct au_string *right);

/// Increases the reference count of an au_string
/// @param header the au_string instance
static inline void au_string_ref(struct au_string *header) {
    header->rc++;
#ifdef DEBUG_RC
    printf("[%.*s]: [ref] rc now %d\n", header->len, header->data,
           header->rc);
#endif
}

/// Decreases the reference count of an au_string.
///     Automatically frees the au_string if the
///     reference count reaches 0.
/// @param header the au_string instance
static inline void au_string_deref(struct au_string *header) {
    header->rc--;
#ifdef DEBUG_RC
    printf("[%.*s]: [deref] rc now %d\n", header->len, header->data,
           header->rc);
#endif
    if (header->rc == 0) {
        free(header);
    }
}
