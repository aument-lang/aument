// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "../malloc.h"
#endif

void au_malloc_init() {}
void au_malloc_set_collect(int collect) { (void)collect; }
size_t au_malloc_heap_size() { return 0; }

// ** objects **

struct au_obj_malloc_header {
    au_obj_del_fn_t del_fn;
    size_t rc;
    char data[];
};

#define MAX_RC (SIZE_MAX)

#define PTR_TO_OBJ_HEADER(PTR)                                            \
    (struct au_obj_malloc_header *)((uintptr_t)PTR -                      \
                                    sizeof(struct au_obj_malloc_header))

void *au_obj_malloc(size_t size, au_obj_del_fn_t del_fn) {
    AU_STATIC_ASSERT(
        sizeof(struct au_obj_malloc_header) % alignof(max_align_t) == 0,
        "struct au_obj_malloc_header must divisible by the maximum "
        "alignment");

    struct au_obj_malloc_header *header =
        malloc(sizeof(struct au_obj_malloc_header) + size);
    header->del_fn = del_fn;
    header->rc = 1;
    return header->data;
}

void *au_obj_realloc(void *ptr, size_t size) {
    struct au_obj_malloc_header *old_header = PTR_TO_OBJ_HEADER(ptr);
    if (old_header->rc > 1)
        abort();
    struct au_obj_malloc_header *header =
        realloc(old_header, sizeof(struct au_obj_malloc_header) + size);
    return header->data;
}

void au_obj_free(void *ptr) {
    struct au_obj_malloc_header *header = PTR_TO_OBJ_HEADER(ptr);
    if (header->rc != 0)
        abort();
    if (header->del_fn != 0)
        header->del_fn(ptr);
    free(header);
}

void au_obj_ref(void *ptr) {
    struct au_obj_malloc_header *header = PTR_TO_OBJ_HEADER(ptr);
    if (AU_UNLIKELY(header->rc == MAX_RC))
        abort();
    header->rc++;
}

void au_obj_deref(void *ptr) {
    struct au_obj_malloc_header *header = PTR_TO_OBJ_HEADER(ptr);
    if (AU_UNLIKELY(header->rc == 0))
        abort();
    header->rc--;
    if (header->rc == 0)
        au_obj_free(ptr);
}

// ** data **

void *au_data_malloc(size_t size) { return malloc(size); }
void *au_data_calloc(size_t count, size_t size) {
    return calloc(count, size);
}
void *au_data_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}
void au_data_free(void *ptr) { free(ptr); }
