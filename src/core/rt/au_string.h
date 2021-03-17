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
    uint32_t rc;
    uint32_t len;
    char data[];
};

/// [func] Creates an au_string from a constant array of chars
/// @param s Pointer to the array of chars
/// @param len Byte size of the string
/// @returns An au_string instance
_Public struct au_string *au_string_from_const(const char *s, size_t len);

/// [func] Creates an au_string from concatenating 2 au_string(s)
/// @param left First string
/// @param right Second string
/// @return Concatenation of `left` and `right`
_Public struct au_string *au_string_add(struct au_string *left,
                                        struct au_string *right);

/// [func] Compares 2 au_string instances
/// @param left First string
/// @param right Second string
/// @return Comparison of `left` and `right`
_Public int au_string_cmp(struct au_string *left, struct au_string *right);
