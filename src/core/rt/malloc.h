// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifndef AU_MALLOC_H
#define AU_MALLOC_H

typedef void (*au_obj_del_fn_t)(void *self);

#if defined(AU_IS_INTERPRETER) && !defined(AU_IS_STDLIB)
#include <stdlib.h>

void au_obj_malloc_init();
void au_obj_malloc_collect();
// [func] Allocates a new object in the heap. The first element of the
// object must be a uint32_t reference counter.
__attribute__((malloc)) void *au_obj_malloc(size_t size,
                                            au_obj_del_fn_t free_fn);
void *au_obj_realloc(void *ptr, size_t size);
void au_obj_free(void *ptr);
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
#endif

#endif