// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef DEBUG_RC
#include <stdio.h>
#endif
#endif

typedef void (*au_struct_del_fn_t)(void *self);

struct au_struct_vdata {
    au_struct_del_fn_t del_fn;
};

struct au_struct {
    uint32_t rc;
    struct au_struct_vdata *vdata;
};

/// Increases the reference count of an au_struct
/// @param header the au_struct instance
static inline void au_struct_ref(struct au_struct *header) {
    header->rc++;
#ifdef DEBUG_RC
    printf("[%p]: [ref] rc now %d\n", header, header->rc);
#endif
}

/// Decreases the reference count of an au_struct.
///     Automatically frees the au_struct if the
///     reference count reaches 0.
/// @param header the au_struct instance
static inline void au_struct_deref(struct au_struct *header) {
    header->rc--;
#ifdef DEBUG_RC
    printf("[%p]: [ref] rc now %d\n", header, header->rc);
#endif
    if (header->rc == 0) {
        header->vdata->del_fn(header);
    }
}