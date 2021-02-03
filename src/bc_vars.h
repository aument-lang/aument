// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include <stdint.h>
#include <stddef.h>

#include "hash.h"

struct au_bc_var_value {
    uint32_t idx;
};

struct au_bc_var_el {
    size_t key_idx;
    size_t key_len;
    hash_t key_hash;
    struct au_bc_var_value value;
};

struct au_bc_bucket {
    struct au_bc_var_el *data;
    size_t len;
    size_t cap;
};

struct au_bc_vars {
    struct au_bc_bucket *buckets;
    size_t buckets_len;
    size_t entries_occ;
    char *var_name;
    size_t var_name_len;
};

void au_bc_vars_init(struct au_bc_vars *vars);
void au_bc_vars_del(struct au_bc_vars *vars);
struct au_bc_var_value *au_bc_vars_add(struct au_bc_vars *vars,
    const char *key, size_t len,
    const struct au_bc_var_value *value);
const struct au_bc_var_value *au_bc_vars_get(const struct au_bc_vars *vars, const char *key, size_t len);
