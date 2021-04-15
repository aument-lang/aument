// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
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

#include "malloc.h"
#include "platform/platform.h"
#endif

struct au_string {
    uint32_t len;
    char data[];
};

typedef struct au_string *au_string_ptr;

/// [func] Creates an au_string from a constant array of chars
/// @param s Pointer to the array of chars
/// @param len Byte size of the string
/// @returns An au_string instance
AU_PUBLIC struct au_string *au_string_from_const(const char *s,
                                                 size_t len);

/// [func] Creates an au_string from concatenating 2 au_string(s)
/// @param left First string
/// @param right Second string
/// @return Concatenation of `left` and `right`
AU_PUBLIC struct au_string *au_string_add(const struct au_string *left,
                                          const struct au_string *right);

/// [func] Compares 2 au_string instances
/// @param left First string
/// @param right Second string
/// @return Comparison of `left` and `right`
static AU_UNUSED inline int au_string_cmp(const struct au_string *left,
                                          const struct au_string *right);

int au_string_cmp(const struct au_string *left,
                  const struct au_string *right) {
    if (left->len > right->len) {
        return 1;
    } else if (left->len < right->len) {
        return -1;
    } else {
        return memcmp(left->data, right->data, left->len);
    }
}

/// [func] Compares an au_string instance with a zero-terminated string
/// @param str au_string instance
/// @param cstr C string
/// @return Comparison of `left` and `right`
static AU_UNUSED inline int au_string_cmp_cstr(const struct au_string *str,
                                               const char *cstr);

int au_string_cmp_cstr(const struct au_string *str, const char *cstr) {
    const size_t clen = strlen(cstr);
    if (str->len > clen) {
        return 1;
    } else if (str->len < clen) {
        return -1;
    } else {
        return memcmp(str->data, cstr, clen);
    }
}
