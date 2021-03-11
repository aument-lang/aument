// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "exception.h"

void au_fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
#ifdef AU_FUZZ_VM
    exit(1);
#else
    abort();
#endif
}

void au_perror(const char *msg) {
    perror(msg);
#ifdef AU_FUZZ_VM
    exit(1);
#else
    abort();
#endif
}

void au_fatal_index(const void *array, size_t idx, size_t len) {
    au_fatal("trying to access array %p (with len %ld) at idx %ld", array,
             len, idx);
}
