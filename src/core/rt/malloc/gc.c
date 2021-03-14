// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "core/vm/tl.h"
#include "core/vm/vm.h"
#include "malloc.h"
#include "platform/platform.h"

#include <assert.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
// #include <stdio.h>

struct au_obj_rc {
    uint32_t rc;
};

#define PTR_TO_OBJ_HEADER(PTR)                                            \
    (struct au_obj_malloc_header *)((uintptr_t)PTR -                      \
                                    sizeof(struct au_obj_malloc_header))

struct au_obj_malloc_header {
    struct au_obj_malloc_header *next;
    au_obj_del_fn_t del_fn;
    size_t marked;
    size_t size;
    char data[];
};

#define PTR_TO_DATA_HEADER(PTR)                                           \
    (struct au_data_malloc_header *)((uintptr_t)PTR -                     \
                                     sizeof(                              \
                                         struct au_data_malloc_header))

struct au_data_malloc_header {
    size_t size;
    size_t _align;
    char data[];
};

#ifdef AU_DEBUG_GC
#define INITIAL_HEAP_THRESHOLD 0
#else
#define INITIAL_HEAP_THRESHOLD 1000000
#endif
#define HEAP_THRESHOLD_GROWTH 1.5

struct malloc_data {
    struct au_obj_malloc_header *obj_list;
    size_t heap_size;
    size_t heap_threshold;
    int do_collect;
};

static _TLStorage struct malloc_data malloc_data;

void au_malloc_init() {
#if __STDC_VERSION__ >= 201112L
    static_assert(
        sizeof(struct au_obj_malloc_header) % alignof(max_align_t) == 0,
        "struct au_obj_malloc_header must divisible by the maximum "
        "alignment");
    static_assert(
        sizeof(struct au_data_malloc_header) % alignof(max_align_t) == 0,
        "struct au_data_malloc_header must divisible by the maximum "
        "alignment");
#else
    // FIXME: Implement the equivalent of static_assert for C99.
    // Right now, we trusting the developer to define the header structs
    // correctly.
#endif
    malloc_data = (struct malloc_data){0};
    malloc_data.heap_threshold = INITIAL_HEAP_THRESHOLD;
    malloc_data.do_collect = 0;
}

void au_malloc_set_collect(int do_collect) {
    malloc_data.do_collect = do_collect;
}

size_t au_malloc_heap_size() { return malloc_data.heap_size; }

static void collect_if_needed(size_t size) {
    if (malloc_data.do_collect &&
        malloc_data.heap_size + size > malloc_data.heap_threshold) {
        au_obj_malloc_collect();
        if (malloc_data.heap_size + size > malloc_data.heap_threshold) {
            malloc_data.heap_threshold *= HEAP_THRESHOLD_GROWTH;
        }
    }
}

void *au_obj_malloc(size_t size, au_obj_del_fn_t del_fn) {
    collect_if_needed(size);

    struct au_obj_malloc_header *header =
        malloc(sizeof(struct au_obj_malloc_header) + size);
    header->next = 0;
    header->marked = 0;
    header->size = size;
    header->del_fn = del_fn;
    malloc_data.heap_size += size;

    header->next = malloc_data.obj_list;
    malloc_data.obj_list = header;

    return (void *)(header->data);
}

void *au_obj_realloc(void *ptr, size_t size) {
    if (ptr == 0 || size == 0) {
        return 0;
    }

    struct au_obj_malloc_header *old_header = PTR_TO_OBJ_HEADER(ptr);
    if (size <= old_header->size) {
        return old_header;
    }

    void *reallocated = au_obj_malloc(size, old_header->del_fn);
    memcpy(reallocated, old_header->data, old_header->size);
    ((struct au_obj_rc *)(old_header->data))->rc = 0;
    return reallocated;
}

void au_obj_free(void *ptr) {
    if (ptr == 0)
        return;
}

static void mark(au_value_t value) {
    switch (au_value_get_type(value)) {
    case AU_VALUE_STR: {
        struct au_obj_malloc_header *header =
            PTR_TO_OBJ_HEADER(au_value_get_string(value));
        header->marked = 1;
        break;
    }
    case AU_VALUE_STRUCT: {
        struct au_obj_malloc_header *header =
            PTR_TO_OBJ_HEADER(au_value_get_struct(value));
        header->marked = 1;
        break;
    }
    default:
        break;
    }
}

static int should_free(struct au_obj_malloc_header *header) {
    if (header->marked)
        return 0;
    return ((struct au_obj_rc *)(header->data))->rc == 0;
}

void au_obj_malloc_collect() {
    // fprintf(stderr, "collecting heap of %ld bytes\n",
    // malloc_data.heap_size);
    struct au_vm_thread_local *tl = au_vm_thread_local_get();
    {
        struct au_obj_malloc_header *cur = malloc_data.obj_list;
        while (cur != 0) {
            cur->marked = 0;
            cur = cur->next;
        }
    }
    assert(tl != 0);
    struct au_vm_frame_link link = tl->current_frame;
    while (link.frame != 0) {
        if (link.frame->self) {
            struct au_obj_malloc_header *header =
                PTR_TO_OBJ_HEADER(link.frame->self);
            header->marked = 1;
        }
        mark(link.frame->retval);
        for (int i = 0; i < link.bcs->num_registers; i++) {
            mark(link.frame->regs[i]);
        }
        for (int i = 0; i < link.bcs->num_locals; i++) {
            mark(link.frame->locals[i]);
        }
        link = link.frame->link;
    }
    struct au_obj_malloc_header *prev = 0;
    struct au_obj_malloc_header *cur = malloc_data.obj_list;
    while (cur != 0) {
        if (should_free(cur)) {
            struct au_obj_malloc_header *next = cur->next;
            if (prev) {
                prev->next = next;
            } else {
                malloc_data.obj_list = next;
            }
            malloc_data.heap_size -= cur->size;
            if (cur->del_fn != 0)
                cur->del_fn(&cur->data);
            free(cur);
            cur = next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    // fprintf(stderr, "heap is now %ld bytes\n", malloc_data.heap_size);
}

void *au_data_malloc(size_t size) {
    collect_if_needed(size);
    malloc_data.heap_size += size;

    struct au_data_malloc_header *header =
        malloc(sizeof(struct au_data_malloc_header) + size);
    header->size = size;
    return (void *)header->data;
}

void *au_data_calloc(size_t size) {
    collect_if_needed(size);
    malloc_data.heap_size += size;

    struct au_data_malloc_header *header =
        calloc(sizeof(struct au_data_malloc_header) + size, 1);
    header->size = size;
    return (void *)header->data;
}

void *au_data_realloc(void *ptr, size_t size) {
    if (ptr == 0)
        return au_data_malloc(size);
    struct au_data_malloc_header *old_header = PTR_TO_DATA_HEADER(ptr);
    const size_t old_size = old_header->size;
    struct au_data_malloc_header *header =
        realloc(old_header, sizeof(struct au_data_malloc_header) + size);
    header->size = size;
    if (header != old_header) {
        malloc_data.heap_size -= old_size;
        malloc_data.heap_size += size;
        collect_if_needed(0);
    }
    return (void *)header->data;
}

void au_data_free(void *ptr) {
    if (ptr == 0)
        return;
    struct au_data_malloc_header *header = PTR_TO_DATA_HEADER(ptr);
    malloc_data.heap_size -= header->size;
    free(header);
}