// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "array.h"
#include "bc.h"
#include "bc_vars.h"
#include "str_array.h"

#include "rt/extern_fn.h"
#include "rt/value.h"

struct au_program_data_val {
    au_value_t real_value;
    uint32_t buf_idx;
    uint32_t buf_len;
};

ARRAY_TYPE(struct au_program_data_val, au_program_data_vals, 1)

enum au_fn_type { AU_FN_NATIVE, AU_FN_BC };

struct au_fn {
    enum au_fn_type type;
    union {
        struct au_lib_func native_func;
        struct au_bc_storage bc_func;
    } as;
};
void au_fn_del(struct au_fn *fn);

ARRAY_TYPE(struct au_fn, au_fn_array, 1)

struct au_program_data {
    struct au_fn_array fns;
    struct au_bc_vars fn_map;
    struct au_program_data_vals data_val;
    uint8_t *data_buf;
    size_t data_buf_len;
    struct au_str_array imports;
    char *cwd;
};

void au_program_data_init(struct au_program_data *data);
void au_program_data_del(struct au_program_data *data);

/// Adds data into a au_program_data instance
/// @param p_data
/// @param value
/// @param v_data
/// @param len
/// @return index of the data
int au_program_data_add_data(struct au_program_data *p_data,
                             au_value_t value, uint8_t *v_data,
                             size_t len);

struct au_program {
    struct au_bc_storage main;
    struct au_program_data data;
};

/// Debugs an au_program
void au_program_dbg(const struct au_program *p);
void au_program_del(struct au_program *p);