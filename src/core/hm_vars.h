// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "hash.h"
#include "platform/platform.h"
#endif

typedef uint32_t au_hm_var_value_t;
#define AU_HM_VAR_VALUE_NONE ((au_hm_var_value_t)-1)

struct au_hm_var_el {
    size_t key_idx;
    size_t key_len;
    au_hash_t key_hash;
    au_hm_var_value_t value;
};

struct au_hm_bucket {
    struct au_hm_var_el *data;
    size_t len;
    size_t cap;
};

struct au_hm_vars {
    struct au_hm_bucket *buckets;
    size_t buckets_len;
    size_t entries_occ;
    char *var_name;
    size_t var_name_len;
};

/// Initializes an au_hm_vars instance
/// @param vars instance to be initialized
AU_PUBLIC void au_hm_vars_init(struct au_hm_vars *vars);

/// Deinitializes an au_hm_vars instance
/// @param vars instance to be destroyed
AU_PUBLIC void au_hm_vars_del(struct au_hm_vars *vars);

/// Adds a key-value pair into an au_hm_vars instance
/// @param vars the au_hm_vars instance
/// @param key the key
/// @param len the bytesize length of the key
/// @param value the value to be added
/// @return the old value (if it exists), otherwise `NULL`
AU_PUBLIC au_hm_var_value_t *au_hm_vars_add(struct au_hm_vars *vars,
                                            const char *key, size_t len,
                                            au_hm_var_value_t value);

/// Retrieves a value from an au_hm_vars instance
/// @param vars the au_hm_vars instance
/// @param key the key
/// @param len the bytesize length of the key
/// @return the value (if it exists), otherwise `NULL`
AU_PUBLIC const au_hm_var_value_t *
au_hm_vars_get(const struct au_hm_vars *vars, const char *key, size_t len);

#define AU_HM_VARS_FOREACH_PAIR(HM, NAME_VAR, ENTRY_VAR, BLOCK)           \
    for (size_t _i = 0; _i < (HM)->buckets_len; _i++) {                   \
        for (size_t _el_idx = 0; _el_idx < (HM)->buckets[_i].len;         \
             _el_idx++) {                                                 \
            struct au_hm_var_el *_el = &(HM)->buckets[_i].data[_el_idx];  \
            const char *NAME_VAR = &(HM)->var_name[_el->key_idx];         \
            const size_t NAME_VAR##_len = _el->key_len;                   \
            const au_hm_var_value_t ENTRY_VAR = _el->value;               \
            BLOCK                                                         \
        }                                                                 \
    }
