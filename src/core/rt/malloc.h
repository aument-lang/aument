// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifndef AU_MALLOC_H
#define AU_MALLOC_H

#ifdef AU_IS_INTERPRETER
#include "platform/platform.h"
#include <stdlib.h>
#include <string.h>
#endif

typedef void (*au_obj_del_fn_t)(void *self);

#if defined(AU_IS_INTERPRETER) && !defined(AU_IS_STDLIB)
void au_malloc_init();
void au_malloc_set_collect(int do_collect);

void au_obj_malloc_collect();
// [func] Allocates a new object in the heap. The first element of the
// object must be a uint32_t reference counter.
__attribute__((malloc)) void *au_obj_malloc(size_t size,
                                            au_obj_del_fn_t free_fn);
void *au_obj_realloc(void *ptr, size_t size);
void au_obj_free(void *ptr);

__attribute__((malloc)) void *au_data_malloc(size_t size);
void *au_data_realloc(void *ptr, size_t size);
void au_data_free(void *ptr);

static _Unused inline char *au_data_strdup(const char *other) {
    const size_t len = strlen(other);
    char *dest = au_data_malloc(len + 1);
    memcpy(dest, other, len);
    dest[len] = 0;
    return dest;
}

static _Unused char *au_data_strndup(const char *str, size_t len) {
    char *output = au_data_malloc(len + 1);
    memcpy(output, str, len);
    output[len] = 0;
    return output;
}
#else
__attribute__((malloc)) static inline void *au_obj_malloc(size_t size,
                                                          void *free_fn) {
    (void)free_fn;
    return malloc(size);
}
static inline void *au_obj_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}
static inline void au_obj_free(void *ptr) { free(ptr); }

__attribute__((malloc)) static inline void *au_data_malloc(size_t size) {
    return malloc(size);
}
static inline void *au_data_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}
static inline void au_data_free(void *ptr) { free(ptr); }
#endif

#endif