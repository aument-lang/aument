// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdint.h>
#include <stdlib.h>

#include "platform/platform.h"

static inline AU_UNUSED const char *
utf8_codepoint(const char *s, const size_t max_len,
               int32_t *out_codepoint) {
    if (max_len == 0)
        return 0;

    if (0xf0 == (0xf8 & s[0])) {
        // 4 byte utf8 codepoint
        if (max_len < 4)
            return 0;
        *out_codepoint = ((0x07 & s[0]) << 18) | ((0x3f & s[1]) << 12) |
                         ((0x3f & s[2]) << 6) | (0x3f & s[3]);
        s += 4;
    } else if (0xe0 == (0xf0 & s[0])) {
        // 3 byte utf8 codepoint
        if (max_len < 3)
            return 0;
        *out_codepoint =
            ((0x0f & s[0]) << 12) | ((0x3f & s[1]) << 6) | (0x3f & s[2]);
        s += 3;
    } else if (0xc0 == (0xe0 & s[0])) {
        // 2 byte utf8 codepoint
        if (max_len < 2)
            return 0;
        *out_codepoint = ((0x1f & s[0]) << 6) | (0x3f & s[1]);
        s += 2;
    } else {
        // 1 byte utf8 codepoint otherwise
        *out_codepoint = s[0];
        s += 1;
    }

    return (void *)s;
}

static inline AU_UNUSED const char *utf8_next(const char *s,
                                              const char *max, int *size) {
    uintptr_t s_intptr = (uintptr_t)s;
    uintptr_t max_intptr = (uintptr_t)max;

    if (0xf0 == (0xf8 & s[0])) {
        // 4 byte utf8 codepoint
        if (s_intptr + 4 > max_intptr)
            return 0;
        *size = 4;
        s += 4;
    } else if (0xe0 == (0xf0 & s[0])) {
        // 3 byte utf8 codepoint
        if (s_intptr + 3 > max_intptr)
            return 0;
        *size = 3;
        s += 3;
    } else if (0xc0 == (0xe0 & s[0])) {
        // 2 byte utf8 codepoint
        if (s_intptr + 2 > max_intptr)
            return 0;
        *size = 2;
        s += 2;
    } else {
        // 1 byte utf8 codepoint otherwise
        if (s_intptr + 1 > max_intptr)
            return 0;
        *size = 1;
        s += 1;
    }

    return s;
}

static inline AU_UNUSED const char *utf8_str(const char *h,
                                             const char *h_max,
                                             const char *n,
                                             const char *n_max) {
    while (h != h_max) {
        const char *maybe_match = h;

        while (*h == *n && (h != h_max && n != n_max)) {
            n++;
            h++;
        }

        if (n == n_max) {
            // we found the whole utf8 string for needle in haystack at
            // maybeMatch, so return it
            return maybe_match;
        } else {
            // h could be in the middle of an unmatching utf8 codepoint,
            // so we need to march it on to the next character beginning
            // starting from the current character
            int throwaway;
            h = (const char *)utf8_next(h, h_max, &throwaway);
            if (h == 0)
                return 0;
        }
    }

    // no match
    return 0;
}
