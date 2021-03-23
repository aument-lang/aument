// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "core/rt/malloc.h"
#include "platform/platform.h"
#include "rt/exception.h"
#endif

#define AU_ARRAY_COPY(INNER, NAME, IN_CAP)                                \
    struct NAME {                                                         \
        INNER *data;                                                      \
        size_t len;                                                       \
        size_t cap;                                                       \
    };                                                                    \
    static AU_UNUSED void NAME##_add(struct NAME *array, INNER el) {        \
        if (array->cap == 0) {                                            \
            array->data =                                                 \
                (INNER *)au_data_malloc(sizeof(INNER) * IN_CAP);          \
            array->cap = IN_CAP;                                          \
        } else if (array->len == array->cap) {                            \
            array->data = (INNER *)au_data_realloc(                       \
                array->data, array->cap * 2 * sizeof(INNER));             \
            array->cap *= 2;                                              \
        }                                                                 \
        array->data[array->len++] = el;                                   \
    }                                                                     \
    static AU_UNUSED AU_ALWAYS_INLINE INNER NAME##_at(                         \
        const struct NAME *array, size_t idx) {                           \
        if (AU_UNLIKELY(idx >= array->len))                                 \
            au_fatal_index((void *)array, idx, array->len);               \
        return array->data[idx];                                          \
    }                                                                     \
    static AU_UNUSED AU_ALWAYS_INLINE void NAME##_set(                         \
        const struct NAME *array, size_t idx, INNER thing) {              \
        if (AU_UNLIKELY(idx >= array->len))                                 \
            au_fatal_index((void *)array, idx, array->len);               \
        array->data[idx] = thing;                                         \
    }

#define AU_ARRAY_STRUCT(INNER, NAME, IN_CAP)                              \
    AU_ARRAY_COPY(INNER, NAME, IN_CAP)                                    \
    static AU_UNUSED AU_ALWAYS_INLINE const INNER *NAME##_at_ptr(              \
        const struct NAME *array, size_t idx) {                           \
        if (AU_UNLIKELY(idx >= array->len))                                 \
            au_fatal_index((void *)array, idx, array->len);               \
        return &array->data[idx];                                         \
    }                                                                     \
    static AU_UNUSED AU_ALWAYS_INLINE INNER *NAME##_at_mut(struct NAME *array, \
                                                      size_t idx) {       \
        if (AU_UNLIKELY(idx >= array->len))                                 \
            au_fatal_index((void *)array, idx, array->len);               \
        return &array->data[idx];                                         \
    }
