// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>

#include "bc.h"
#include "bc_vars.h"
#include "value.h"
#include "array.h"
#include "str_array.h"

struct au_program_data_val {
    au_value_t real_value;
    uint32_t buf_idx;
    uint32_t buf_len;
};

ARRAY_TYPE(struct au_program_data_val, au_program_data_vals, 1)
ARRAY_TYPE(struct au_bc_storage, au_bc_storage_vals, 1)

struct au_program_data {
    struct au_bc_storage_vals fns;
    struct au_bc_vars fn_map;
    struct au_program_data_vals data_val;
    uint8_t *data_buf;
    size_t data_buf_len;
    struct au_str_array imports;
    char *cwd;
};

void au_program_data_init(struct au_program_data *data);
void au_program_data_del(struct au_program_data *data);

int au_program_data_add_data(
    struct au_program_data *p_data,
    au_value_t value,
    uint8_t *v_data,
    size_t len
);

struct au_program {
    struct au_bc_storage main;
    struct au_program_data data;
};

void au_program_dbg(const struct au_program *p);
void au_program_del(struct au_program *p);