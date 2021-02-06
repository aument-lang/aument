// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "value.h"

struct au_vm_thread_local;
struct au_program_data;

typedef au_value_t (*au_extern_func_t)(
    struct au_vm_thread_local *tl,
    const struct au_program_data *p_data,
    const au_value_t *args
);

struct au_lib_func {
    au_extern_func_t func;
    const char *name;
    int num_args;
};

#define AU_EXTERN_FUNC_DECL(NAME) \
au_value_t NAME ( \
    struct au_vm_thread_local *tl, \
    const struct au_program_data *p_data, \
    const au_value_t *args \
)
