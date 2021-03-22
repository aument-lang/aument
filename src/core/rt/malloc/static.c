// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include <stdlib.h>
#include <string.h>
#endif

void au_malloc_init() {}
void au_malloc_set_collect(int collect) { (void)collect; }
size_t au_malloc_heap_size() { return 0; }

void *au_obj_malloc(size_t size, void *free_fn) {
    (void)free_fn;
    return malloc(size);
}

void *au_obj_realloc(void *ptr, size_t size) { return realloc(ptr, size); }
void au_obj_free(void *ptr) { free(ptr); }

void *au_data_malloc(size_t size) { return malloc(size); }
void *au_data_calloc(size_t count, size_t size) { return calloc(count, size); }
void *au_data_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}
void au_data_free(void *ptr) { free(ptr); }