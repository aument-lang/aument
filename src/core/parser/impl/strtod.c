// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#ifdef _WIN32
#include <locale.h>
#else
#define _GNU_SOURCE
#include <locale.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define LC_NUMERIC_MASK LC_NUMERIC
typedef _locale_t locale_t;
static inline locale_t newlocale(int category_mask, const char *locale,
                                 locale_t _locale) {
    (void)_locale;
    return _create_locale(category_mask, locale);
}
#define strtod_l _strtod_l
#endif

double au_parser_strtod(const char *src, size_t len) {
    // FIXME: check for length
    (void)len;
    locale_t locale = newlocale(LC_NUMERIC_MASK, "C", NULL);
    return strtod_l(src, 0, locale);
}