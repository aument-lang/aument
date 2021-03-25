// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "core/rt/malloc.h"
#include "platform/arithmetic.h"
#include "platform/platform.h"

#include "../au_string.h"
#include "../struct/main.h"
#endif

enum au_vtype {
    // AU_VALUE_DOUBLE must be zero because the zero tag in
    // our nan value representation represents actual
    // floating-point infinities and nans
    AU_VALUE_DOUBLE = 0,
    AU_VALUE_NONE = 1,
    AU_VALUE_INT = 2,
    AU_VALUE_BOOL = 3,
    AU_VALUE_FN = 4,
    AU_VALUE_STR = 5,
    AU_VALUE_STRUCT = 6,
    // ...
    AU_VALUE_ERROR = 15,
};

struct au_string;
struct au_fn_value;

#ifdef AU_USE_NAN_TAGGING
typedef union {
    double d;
    uint64_t raw;
} au_value_t;
#else
union au_value_data {
    int32_t d_int;
    void *d_ptr;
    double d_double;
};
struct _au_value {
    union au_value_data _data;
    enum au_vtype _type;
};
typedef struct _au_value au_value_t;
#endif

static enum au_vtype au_value_get_type(const au_value_t v);
static au_value_t au_value_none();
static au_value_t au_value_error();
static au_value_t au_value_int(int32_t n);
static int32_t au_value_get_int(const au_value_t v);
static au_value_t au_value_double(double n);
static double au_value_get_double(const au_value_t v);
static au_value_t au_value_bool(int32_t n);
static int32_t au_value_get_bool(const au_value_t v);
static au_value_t au_value_string(struct au_string *data);
static struct au_string *au_value_get_string(const au_value_t v);
static au_value_t au_value_fn(struct au_fn_value *data);
static struct au_fn_value *au_value_get_fn(const au_value_t v);
/// [func] Creates a value from a pointer to a structure.
/// The pointer must be allocated using au_obj_malloc.
static au_value_t au_value_struct(struct au_struct *data);
static struct au_struct *au_value_get_struct(const au_value_t v);

#ifdef AU_USE_NAN_TAGGING

#define AU_REPR_FRACTION(x) ((x)&INT64_C(0xfffffffffffff))
#define AU_REPR_EXPONENT(x) ((x) >> 52 & 0x7ff)
#define AU_REPR_SPECIAL_EXPONENT 0x7ff
#define AU_REPR_BOXED(TAG, POINTER)                                       \
    (INT64_C(0x7ff0000000000000) |                                        \
     ((((uint64_t)TAG) << 48) | ((POINTER)&INT64_C(0xffffffffffff))))
#define AU_REPR_GET_POINTER(x) ((x)&INT64_C(0x0000ffffffffffff))

#define AU_REPR_ERROR INT64_C(0x7ff0ffffffffffff)
#define AU_REPR_CANONICAL_NAN INT64_C(0x7ff0000000000001)

#define AU_REPR_MAGIC_BITMASK INT64_C(0x7fffffffffffffff)
#define AU_REPR_MAGIC_THRESHOLD INT64_C(0x7ff0000000000002)

static AU_ALWAYS_INLINE enum au_vtype
au_value_get_type(const au_value_t v) {
    // OPTIMIZE: this function checks if the value is a not a regular
    // float, nor a special value (the infinity and canonical nan value) by
    // doing some bitwise magic
    const uint64_t magic = v.raw & AU_REPR_MAGIC_BITMASK;
    const int is_not_float = !!(magic >= AU_REPR_MAGIC_THRESHOLD);
    return (enum au_vtype)((magic >> 48) & (0xf * is_not_float));
}

static AU_ALWAYS_INLINE au_value_t au_value_error() {
    au_value_t v;
    v.raw = AU_REPR_ERROR;
    return v;
}

static AU_ALWAYS_INLINE int au_value_is_error(au_value_t v) {
    return AU_UNLIKELY(v.raw == AU_REPR_ERROR);
}

static AU_ALWAYS_INLINE au_value_t au_value_none() {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_NONE, 0);
    return v;
}

static AU_ALWAYS_INLINE au_value_t au_value_int(int32_t n) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_INT, (uint64_t)n);
    return v;
}
static AU_ALWAYS_INLINE int32_t au_value_get_int(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_INT)
        abort();
    return (int32_t)(AU_REPR_GET_POINTER(v.raw));
}

static AU_ALWAYS_INLINE au_value_t au_value_double(double n) {
    au_value_t v;
    v.d = n;
    if (AU_UNLIKELY((AU_REPR_EXPONENT(v.raw) == AU_REPR_SPECIAL_EXPONENT) &
                    (AU_REPR_FRACTION(v.raw) != 0))) {
        v.raw = AU_REPR_CANONICAL_NAN;
        if (n < 0) {
            v.raw |= (INT64_C(1) << INT64_C(63));
        }
    } else {
        v.d = n;
    }
    return v;
}
static AU_ALWAYS_INLINE double au_value_get_double(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_DOUBLE)
        abort();
    return v.d;
}

static AU_ALWAYS_INLINE au_value_t au_value_bool(int32_t n) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_BOOL, n != 0);
    return v;
}
static AU_ALWAYS_INLINE int32_t au_value_get_bool(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_BOOL)
        abort();
    return v.raw & 1;
}

static AU_ALWAYS_INLINE au_value_t
au_value_string(struct au_string *data) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_STR, (uint64_t)data);
    return v;
}
static AU_ALWAYS_INLINE struct au_string *
au_value_get_string(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_STR)
        abort();
    return (struct au_string *)(AU_REPR_GET_POINTER(v.raw));
}

static AU_ALWAYS_INLINE au_value_t au_value_fn(struct au_fn_value *data) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_FN, (uint64_t)data);
    return v;
}
static AU_ALWAYS_INLINE struct au_fn_value *
au_value_get_fn(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_FN)
        abort();
    return (struct au_fn_value *)(AU_REPR_GET_POINTER(v.raw));
}

static AU_ALWAYS_INLINE au_value_t
au_value_struct(struct au_struct *data) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(AU_VALUE_STRUCT, (uint64_t)data);
    return v;
}
static AU_ALWAYS_INLINE struct au_struct *
au_value_get_struct(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_STRUCT)
        abort();
    return (struct au_struct *)(AU_REPR_GET_POINTER(v.raw));
}
#else
static AU_ALWAYS_INLINE enum au_vtype
au_value_get_type(const au_value_t v) {
    return v._type;
}

static AU_ALWAYS_INLINE au_value_t au_value_none() {
    au_value_t v = {0};
    v._type = AU_VALUE_NONE;
    return v;
}

static AU_ALWAYS_INLINE au_value_t AU_VALUE_ERROR() {
    au_value_t v = {0};
    v._type = AU_VALUE_ERROR;
    return v;
}

static AU_ALWAYS_INLINE int au_value_is_error(au_value_t v) {
    return AU_UNLIKELY(v._type == AU_VALUE_ERROR);
}

static AU_ALWAYS_INLINE au_value_t au_value_int(int32_t n) {
    au_value_t v = {0};
    v._type = AU_VALUE_INT;
    v._data.d_int = n;
    return v;
}
static AU_ALWAYS_INLINE int32_t au_value_get_int(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_INT)
        abort();
    return v._data.d_int;
}

static AU_ALWAYS_INLINE au_value_t au_value_double(double n) {
    au_value_t v = {0};
    v._type = AU_VALUE_DOUBLE;
    v._data.d_double = n;
    return v;
}
static AU_ALWAYS_INLINE double au_value_get_double(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_DOUBLE)
        abort();
    return v._data.d_double;
}

static AU_ALWAYS_INLINE au_value_t au_value_bool(int32_t n) {
    au_value_t v = {0};
    v._type = AU_VALUE_BOOL;
    v._data.d_int = n;
    return v;
}
static AU_ALWAYS_INLINE int32_t au_value_get_bool(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_BOOL)
        abort();
    return v._data.d_int;
}

static AU_ALWAYS_INLINE au_value_t
au_value_string(struct au_string *data) {
    au_value_t v = {0};
    v._type = AU_VALUE_STR;
    v._data.d_ptr = data;
    return v;
}
static AU_ALWAYS_INLINE struct au_string *
au_value_get_string(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_STR)
        abort();
    return (struct au_string *)v._data.d_ptr;
}

static AU_ALWAYS_INLINE au_value_t au_value_fn(struct au_fn_value *data) {
    au_value_t v = {0};
    v._type = AU_VALUE_FN;
    v._data.d_ptr = data;
    return v;
}
static AU_ALWAYS_INLINE struct au_fn_value *
au_value_get_fn(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_FN)
        abort();
    return (struct au_fn_value *)v._data.d_ptr;
}

static AU_ALWAYS_INLINE au_value_t
au_value_struct(struct au_struct *data) {
    au_value_t v = {0};
    v._type = AU_VALUE_STRUCT;
    v._data.d_ptr = data;
    return v;
}
static AU_ALWAYS_INLINE struct au_struct *
au_value_get_struct(const au_value_t v) {
    if (au_value_get_type(v) != AU_VALUE_STRUCT)
        abort();
    return (struct au_struct *)v._data.d_ptr;
}
#endif

static AU_ALWAYS_INLINE void au_value_clear(au_value_t *a, int size) {
    for (int i = 0; i < size; i++)
        a[i] = au_value_none();
}

static AU_ALWAYS_INLINE au_value_t *au_value_calloc(size_t len) {
    au_value_t *array =
        (au_value_t *)au_data_malloc(sizeof(au_value_t) * len);
    au_value_clear(array, len);
    return array;
}

static AU_ALWAYS_INLINE int au_value_is_truthy(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case AU_VALUE_INT: {
        return au_value_get_int(v) > 0;
    }
    case AU_VALUE_DOUBLE: {
        return au_value_get_double(v) > 0;
    }
    case AU_VALUE_BOOL: {
        return au_value_get_bool(v);
    }
    case AU_VALUE_STR: {
        return au_value_get_string(v)->len > 0;
    }
    default:
        return 0;
    }
}

static AU_ALWAYS_INLINE au_value_t au_value_add(au_value_t lhs,
                                                au_value_t rhs) {
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_int(au_platform_iadd_wrap(
                au_value_get_int(lhs), au_value_get_int(rhs)));
        case AU_VALUE_DOUBLE:
            return au_value_double((double)au_value_get_int(lhs) +
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    case AU_VALUE_DOUBLE: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_double(au_value_get_double(lhs) +
                                   (double)au_value_get_int(rhs));
        case AU_VALUE_DOUBLE:
            return au_value_double(au_value_get_double(lhs) +
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    case AU_VALUE_STR: {
        if (AU_UNLIKELY(au_value_get_type(rhs) != AU_VALUE_STR))
            return au_value_error();
        return au_value_string(au_string_add(au_value_get_string(lhs),
                                             au_value_get_string(rhs)));
    }
    default:
        break;
    }
    return au_value_error();
}

static AU_ALWAYS_INLINE au_value_t au_value_sub(au_value_t lhs,
                                                au_value_t rhs) {
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_int(au_platform_isub_wrap(
                au_value_get_int(lhs), au_value_get_int(rhs)));
        case AU_VALUE_DOUBLE:
            return au_value_double((double)au_value_get_int(lhs) -
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    case AU_VALUE_DOUBLE: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_double(au_value_get_double(lhs) -
                                   (double)au_value_get_int(rhs));
        case AU_VALUE_DOUBLE:
            return au_value_double(au_value_get_double(lhs) -
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    default:
        break;
    }
    return au_value_error();
}

static AU_ALWAYS_INLINE au_value_t au_value_mul(au_value_t lhs,
                                                au_value_t rhs) {
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_int(au_platform_imul_wrap(
                au_value_get_int(lhs), au_value_get_int(rhs)));
        case AU_VALUE_DOUBLE:
            return au_value_double((double)au_value_get_int(lhs) *
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    case AU_VALUE_DOUBLE: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_double(au_value_get_double(lhs) *
                                   (double)au_value_get_int(rhs));
        case AU_VALUE_DOUBLE:
            return au_value_double(au_value_get_double(lhs) *
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    default:
        break;
    }
    return au_value_error();
}

static AU_ALWAYS_INLINE au_value_t au_value_div(au_value_t lhs,
                                                au_value_t rhs) {
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_double((double)au_value_get_int(lhs) /
                                   (double)au_value_get_int(rhs));
        case AU_VALUE_DOUBLE:
            return au_value_double((double)au_value_get_int(lhs) /
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    case AU_VALUE_DOUBLE: {
        switch (au_value_get_type(rhs)) {
        case AU_VALUE_INT:
            return au_value_double(au_value_get_double(lhs) /
                                   (double)au_value_get_int(rhs));
        case AU_VALUE_DOUBLE:
            return au_value_double(au_value_get_double(lhs) /
                                   au_value_get_double(rhs));
        default:
            return au_value_error();
        }
    }
    default:
        break;
    }
    return au_value_error();
}

static AU_ALWAYS_INLINE au_value_t au_value_mod(au_value_t lhs,
                                                au_value_t rhs) {
    if (AU_UNLIKELY(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_error();
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        return au_value_int(au_value_get_int(lhs) % au_value_get_int(rhs));
    }
    default:
        break;
    }
    return au_value_error();
}

#define _BIN_OP_BOOL_GENERIC(NAME, OP)                                    \
    static AU_ALWAYS_INLINE au_value_t NAME(au_value_t lhs,               \
                                            au_value_t rhs) {             \
        if (AU_UNLIKELY(au_value_get_type(lhs) !=                         \
                        au_value_get_type(rhs)))                          \
            return au_value_bool(0);                                      \
        switch (au_value_get_type(lhs)) {                                 \
        case AU_VALUE_INT: {                                              \
            return au_value_bool(au_value_get_int(lhs)                    \
                                     OP au_value_get_int(rhs));           \
        }                                                                 \
        case AU_VALUE_DOUBLE: {                                           \
            return au_value_bool(au_value_get_double(lhs)                 \
                                     OP au_value_get_double(rhs));        \
        }                                                                 \
        case AU_VALUE_STR: {                                              \
            return au_value_bool(au_string_cmp(au_value_get_string(lhs),  \
                                               au_value_get_string(rhs))  \
                                     OP 0);                               \
        }                                                                 \
        default:                                                          \
            break;                                                        \
        }                                                                 \
        return au_value_bool(0);                                          \
    }
_BIN_OP_BOOL_GENERIC(au_value_lt, <)
_BIN_OP_BOOL_GENERIC(au_value_gt, >)
_BIN_OP_BOOL_GENERIC(au_value_leq, <=)
_BIN_OP_BOOL_GENERIC(au_value_geq, >=)
#undef _BIN_OP_BOOL_GENERIC

static AU_ALWAYS_INLINE au_value_t au_value_eq(au_value_t lhs,
                                               au_value_t rhs) {
    if (AU_UNLIKELY(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_bool(0);
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        return au_value_bool(au_value_get_int(lhs) ==
                             au_value_get_int(rhs));
    }
    case AU_VALUE_BOOL: {
        return au_value_bool(au_value_get_bool(lhs) ==
                             au_value_get_bool(rhs));
    }
    case AU_VALUE_STR: {
        return au_value_bool(au_string_cmp(au_value_get_string(lhs),
                                           au_value_get_string(rhs)) == 0);
    }
    default:
        break;
    }
    return au_value_bool(0);
}
static AU_ALWAYS_INLINE au_value_t au_value_neq(au_value_t lhs,
                                                au_value_t rhs) {
    if (AU_UNLIKELY(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_bool(1);
    switch (au_value_get_type(lhs)) {
    case AU_VALUE_INT: {
        return au_value_bool(au_value_get_int(lhs) !=
                             au_value_get_int(rhs));
    }
    case AU_VALUE_BOOL: {
        return au_value_bool(au_value_get_bool(lhs) !=
                             au_value_get_bool(rhs));
    }
    case AU_VALUE_STR: {
        return au_value_bool(au_string_cmp(au_value_get_string(lhs),
                                           au_value_get_string(rhs)) != 0);
    }
    default:
        break;
    }
    return au_value_bool(1);
}

void au_value_print(au_value_t value);
