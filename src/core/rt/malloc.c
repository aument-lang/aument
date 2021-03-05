// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "malloc.h"
#include "core/vm/tl.h"
#include "core/vm/vm.h"
#include "platform/platform.h"

#include <stdint.h>

#define PTR_TO_HEADER(PTR)                                                \
    (struct au_obj_malloc_header *)((uintptr_t)PTR -                      \
                                    sizeof(struct au_obj_malloc_header))

struct au_obj_rc {
    uint32_t rc;
};

struct au_obj_malloc_header {
    struct au_obj_malloc_header *next;
    au_obj_del_fn_t del_fn;
    int32_t marked;
    uint32_t size;
    char data[];
};

#define INITIAL_HEAP_THRESHOLD 1000000
#define HEAP_THRESHOLD_GROWTH 1.5

struct malloc_data {
    struct au_obj_malloc_header *obj_list;
    size_t heap_size;
    size_t heap_threshold;
};

static _TLStorage struct malloc_data malloc_data;

void au_obj_malloc_init() {
    malloc_data = (struct malloc_data){0};
    malloc_data.heap_threshold = INITIAL_HEAP_THRESHOLD;
}

void *au_obj_malloc(size_t size, au_obj_del_fn_t del_fn) {
    if (size > UINT32_MAX)
        return 0;
    if (malloc_data.heap_size + size > malloc_data.heap_threshold) {
        au_obj_malloc_collect();
        if (malloc_data.heap_size + size > malloc_data.heap_threshold) {
            malloc_data.heap_threshold *= HEAP_THRESHOLD_GROWTH;
        }
    }
    struct au_obj_malloc_header *header =
        malloc(sizeof(struct au_obj_malloc_header) + size);
    header->next = 0;
    header->marked = 0;
    header->size = size;
    header->del_fn = del_fn;
    malloc_data.heap_size += size;

    header->next = malloc_data.obj_list;
    malloc_data.obj_list = header;

    void *ptr = (void *)(header->data);
    return ptr;
}

void *au_obj_realloc(void *ptr, size_t size) {
    if (ptr == 0 || size == 0) {
        return 0;
    }

    struct au_obj_malloc_header *old_header = PTR_TO_HEADER(ptr);
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
    case VALUE_STR: {
        struct au_obj_malloc_header *header =
            PTR_TO_HEADER(au_value_get_string(value));
        header->marked = 1;
        break;
    }
    case VALUE_STRUCT: {
        struct au_obj_malloc_header *header =
            PTR_TO_HEADER(au_value_get_struct(value));
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
        mark(link.frame->retval);
        for (int i = 0; i < link.bcs->num_registers; i++) {
            mark(link.frame->regs[i]);
        }
        for (int i = 0; i < link.bcs->locals_len; i++) {
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
}
