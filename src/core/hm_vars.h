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

typedef size_t au_hm_var_value_t;
#define AU_HM_VAR_VALUE_NONE ((au_hm_var_value_t)-1)

struct au_hm_var_el_bucket {
    size_t key_idx;
    size_t key_len;
    uint32_t hash;
    uint32_t psl;
    au_hm_var_value_t value;
};

// A zero-initialized au_hm_vars structure should be considered empty
struct au_hm_vars {
    uint64_t divinfo;
    size_t size;
    size_t nitems;
    size_t minsize;
    struct au_hm_var_el_bucket *buckets;
    char *key_bytes;
    size_t key_bytes_len;
    int initialized;
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
    if ((HM)->initialized) {                                              \
        for (size_t _i = 0; _i < (HM)->size; _i++) {                      \
            struct au_hm_var_el_bucket *_bucket = &(HM)->buckets[_i];     \
            if (_bucket->key_len == 0)                                    \
                continue;                                                 \
            const char *NAME_VAR = &(HM)->key_bytes[_bucket->key_idx];    \
            const size_t NAME_VAR##_len = _bucket->key_len;               \
            const au_hm_var_value_t ENTRY_VAR = _bucket->value;           \
            BLOCK                                                         \
        }                                                                 \
    }
