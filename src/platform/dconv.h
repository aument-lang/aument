#pragma once

#include "core/rt/malloc.h"
#include "platform.h"
#include <stdlib.h>

AU_PRIVATE double au_dconv_strtod(const char *str, char **ptr);
AU_PRIVATE char *au_dconv_dtoa(double d, int mode, int ndigits, int *decpt,
                               int *sign, char **rve);
AU_PRIVATE void au_dconv_freedtoa(char *s);
AU_PRIVATE double au_dconv_stdnan(int sign);
AU_PRIVATE double au_dconv_infinity(int sign);

#define AU_DCONV_SMALL_SIZE 32

/// Parses a string with specified size
/// @param str string to parse
/// @param size byte size of string
/// @param output the corresponding double value
/// @return number of bytes parsed
static AU_ALWAYS_INLINE size_t au_dconv_strtod_s(const char *str,
                                                 size_t size,
                                                 double *output) {
    if (AU_LIKELY(size < AU_DCONV_SMALL_SIZE)) {
        char buf[AU_DCONV_SMALL_SIZE];
        for (size_t i = 0; i < size; i++)
            buf[i] = str[i];
        buf[size] = 0;
        char *endptr;
        *output = au_dconv_strtod(buf, &endptr);
        return (size_t)((uintptr_t)endptr - (uintptr_t)buf);
    } else {
        char *buf = au_data_strndup(str, size);
        char *endptr;
        *output = au_dconv_strtod(buf, &endptr);
        size_t retval = (size_t)((uintptr_t)endptr - (uintptr_t)buf);
        au_data_free(buf);
        return retval;
    }
}
