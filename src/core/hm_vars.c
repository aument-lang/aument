// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rt/malloc.h"
#include "hash.h"
#include "hm_vars.h"
#include "rt/exception.h"

void au_hm_vars_init(struct au_hm_vars *vars) {
    vars->buckets = 0;
    vars->buckets_len = 0;
    vars->entries_occ = 0;
    vars->var_name = 0;
    vars->var_name_len = 0;
}

void au_hm_vars_del(struct au_hm_vars *vars) {
    for (size_t i = 0; i < vars->buckets_len; i++) {
        struct au_hm_bucket *bucket = &vars->buckets[i];
        au_data_free(bucket->data);
    }
    au_data_free(vars->buckets);
    au_data_free(vars->var_name);
    memset(vars, 0, sizeof(struct au_hm_vars));
}

static void rehash_table(struct au_hm_vars *vars) {
    const int new_len = vars->buckets_len * 2;
    struct au_hm_bucket *new_buckets =
        au_data_malloc(new_len * sizeof(struct au_hm_bucket));
    for (int i = 0; i < new_len; i++)
        new_buckets[i] = (struct au_hm_bucket){0};
    for (size_t i = 0; i < vars->buckets_len; i++) {
        const struct au_hm_bucket *old_bucket = &vars->buckets[i];
        for (size_t i = 0; i < old_bucket->len; i++) {
            const struct au_hm_var_el *el = &old_bucket->data[i];
            const int new_bucket_idx = el->key_hash & (new_len - 1);
            struct au_hm_bucket *bucket = &new_buckets[new_bucket_idx];
            if (bucket->cap == 0) {
                bucket->data = au_data_malloc(sizeof(struct au_hm_var_el));
                bucket->cap = 1;
            } else if (bucket->len == bucket->cap) {
                bucket->data = au_data_realloc(
                    bucket->data,
                    bucket->cap * 2 * sizeof(struct au_hm_var_el));
                bucket->cap *= 2;
            }
            bucket->data[bucket->len++] = *el;
        }
    }

    for (size_t i = 0; i < vars->buckets_len; i++) {
        struct au_hm_bucket *bucket = &vars->buckets[i];
        au_data_free(bucket->data);
    }
    au_data_free(vars->buckets);
    vars->buckets = new_buckets;
    vars->buckets_len = new_len;
}

au_hm_var_value_t *au_hm_vars_add(struct au_hm_vars *vars, const char *key,
                                  size_t len, au_hm_var_value_t value) {
    const au_hash_t hash = au_hash((const uint8_t *)key, len);
    if (vars->buckets_len > 0) {
        const int bucket_idx = ((int)hash) & (vars->buckets_len - 1);
        const struct au_hm_bucket *bucket = &vars->buckets[bucket_idx];
        for (size_t i = 0; i < bucket->len; i++) {
            struct au_hm_var_el *el = &bucket->data[i];
            if (el->key_len == len && memcmp(&vars->var_name[el->key_idx],
                                             key, el->key_len) == 0) {
                return &el->value;
            }
        }
        while (vars->entries_occ > vars->buckets_len * 0.75) {
            rehash_table(vars);
        }
    }

    const size_t key_idx = vars->var_name_len;
    vars->var_name =
        au_data_realloc(vars->var_name, vars->var_name_len + len);
    memcpy(&vars->var_name[vars->var_name_len], key, len);
    vars->var_name_len += len;

    const struct au_hm_var_el el = (struct au_hm_var_el){
        .key_idx = key_idx,
        .key_len = len,
        .key_hash = hash,
        .value = value,
    };

    if (vars->buckets_len == 0) {
        vars->buckets = au_data_malloc(sizeof(struct au_hm_bucket));
        vars->buckets_len = 1;

        struct au_hm_bucket *bucket = &vars->buckets[0];
        bucket->data = au_data_malloc(sizeof(struct au_hm_var_el));
        bucket->data[0] = el;
        bucket->len = 1;
        bucket->cap = 1;
    } else {
        // We can't guarantee vars->buckets_len is constant
        // as we may have rehashed the table
        const size_t bucket_idx = (size_t)hash & (vars->buckets_len - 1);
        struct au_hm_bucket *bucket = &vars->buckets[bucket_idx];
        if (bucket->cap == 0) {
            bucket->data = au_data_malloc(sizeof(struct au_hm_var_el));
            bucket->cap = 1;
        } else if (bucket->len == bucket->cap) {
            bucket->data = au_data_realloc(
                bucket->data,
                bucket->cap * 2 * sizeof(struct au_hm_var_el));
            bucket->cap *= 2;
        }
        bucket->data[bucket->len++] = el;
    }
    vars->entries_occ++;
    return 0;
}

const au_hm_var_value_t *au_hm_vars_get(const struct au_hm_vars *vars,
                                        const char *key, size_t len) {
    if (vars->buckets_len == 0) {
        return 0;
    }

    const au_hash_t hash = au_hash((const uint8_t *)key, len);
    const size_t bucket_idx = ((size_t)hash) & (vars->buckets_len - 1);
    const struct au_hm_bucket *bucket = &vars->buckets[bucket_idx];
    for (size_t i = 0; i < bucket->len; i++) {
        const struct au_hm_var_el *el = &bucket->data[i];
        if (el->key_len == len &&
            memcmp(&vars->var_name[el->key_idx], key, el->key_len) == 0) {
            return &el->value;
        }
    }
    return 0;
}