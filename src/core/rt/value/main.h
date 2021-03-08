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

#include "platform/platform.h"

#include "../au_string.h"
#include "../struct/main.h"
#endif

enum au_vtype {
    // VALUE_DOUBLE must be zero because the zero tag in
    // our nan value representation represents actual
    // floating-point infinities and nans
    VALUE_DOUBLE = 0,
    VALUE_NONE = 1,
    VALUE_INT = 2,
    VALUE_BOOL = 3,
    VALUE_FN = 4,
    VALUE_STR = 5,
    VALUE_STRUCT = 6,
    // ...
    VALUE_OP_ERROR = 15,
};

struct au_string;

#ifdef AU_USE_NAN_TAGGING
typedef union {
    double d;
    uint64_t raw;
} au_value_t;

#define AU_REPR_FRACTION(x) ((x)&0xfffffffffffffUL)
#define AU_REPR_EXPONENT(x) ((x) >> 52 & 0x7ff)
#define AU_REPR_SPECIAL_EXPONENT 0x7ff
#define AU_REPR_BOXED(TAG, POINTER)                                       \
    (0x7ff0000000000000UL |                                               \
     ((((uint64_t)TAG) << 48) | ((POINTER)&0xffffffffffffUL)))
#define AU_REPR_GET_POINTER(x) ((x)&0x0000ffffffffffffUL)

#define AU_REPR_OP_ERROR (0x7ff0ffffffffffffUL)
#define AU_REPR_CANONICAL_NAN (0x7ff0000000000001UL)

#define AU_REPR_MAGIC_BITMASK (0x7fffffffffffffffUL)
#define AU_REPR_MAGIC_THRESHOLD (0x7ff0000000000002UL)

static _AlwaysInline enum au_vtype au_value_get_type(const au_value_t v) {
    // OPTIMIZE: this function checks if the value is a not a regular
    // float, nor a special value (the infinity and canonical nan value) by
    // doing some bitwise magic
    const uint64_t magic = v.raw & AU_REPR_MAGIC_BITMASK;
    const int is_not_float = !!(magic >= AU_REPR_MAGIC_THRESHOLD);
    return (enum au_vtype)((magic >> 48) & (0xf * is_not_float));
}

static _AlwaysInline au_value_t au_value_op_error() {
    au_value_t v;
    v.raw = AU_REPR_OP_ERROR;
    return v;
}

static _AlwaysInline int au_value_is_op_error(au_value_t v) {
    return v.raw == AU_REPR_OP_ERROR;
}

static _AlwaysInline au_value_t au_value_none() {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_NONE, 0);
    return v;
}

static _AlwaysInline au_value_t au_value_int(int32_t n) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_INT, (uint64_t)n);
    return v;
}
static _AlwaysInline int32_t au_value_get_int(const au_value_t v) {
    return (int32_t)(AU_REPR_GET_POINTER(v.raw));
}

static _AlwaysInline au_value_t au_value_double(double n) {
    au_value_t v;
    if (_Unlikely(isnan(n))) {
        v.raw = AU_REPR_CANONICAL_NAN;
        if (n < 0) {
            v.raw |= (1UL << 63UL);
        }
    } else {
        v.d = n;
    }
    return v;
}
static _AlwaysInline double au_value_get_double(const au_value_t v) {
    return v.d;
}

static _AlwaysInline au_value_t au_value_bool(int32_t n) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_BOOL, n != 0);
    return v;
}
static _AlwaysInline int32_t au_value_get_bool(const au_value_t v) {
    return v.raw & 1;
}

static _AlwaysInline au_value_t au_value_string(struct au_string *data) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_STR, (uint64_t)data);
    return v;
}
static _AlwaysInline struct au_string *
au_value_get_string(const au_value_t v) {
    return (struct au_string *)(AU_REPR_GET_POINTER(v.raw));
}

static _AlwaysInline au_value_t au_value_fn(uint32_t n) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_FN, (uint64_t)n);
    return v;
}
static _AlwaysInline uint32_t au_value_get_fn(const au_value_t v) {
    return (uint32_t)(AU_REPR_GET_POINTER(v.raw));
}

static _AlwaysInline au_value_t au_value_struct(struct au_struct *data) {
    au_value_t v;
    v.raw = AU_REPR_BOXED(VALUE_STRUCT, (uint64_t)data);
    return v;
}
static _AlwaysInline struct au_struct *
au_value_get_struct(const au_value_t v) {
    return (struct au_struct *)(AU_REPR_GET_POINTER(v.raw));
}
#else
union au_value_data {
    int32_t d_int;
    void *d_ptr;
    uint32_t d_fn_idx;
    double d_double;
};

struct _au_value {
    union au_value_data _data;
    enum au_vtype _type;
};

typedef struct _au_value au_value_t;

static _AlwaysInline enum au_vtype au_value_get_type(const au_value_t v) {
    return v._type;
}

static _AlwaysInline au_value_t au_value_none() {
    au_value_t v = {0};
    v._type = VALUE_NONE;
    return v;
}

static _AlwaysInline au_value_t au_value_op_error() {
    au_value_t v = {0};
    v._type = VALUE_OP_ERROR;
    return v;
}

static _AlwaysInline int au_value_is_op_error(au_value_t v) {
    return v._type == VALUE_OP_ERROR;
}

static _AlwaysInline au_value_t au_value_int(int32_t n) {
    au_value_t v = {0};
    v._type = VALUE_INT;
    v._data.d_int = n;
    return v;
}
static _AlwaysInline int32_t au_value_get_int(const au_value_t v) {
    return v._data.d_int;
}

static _AlwaysInline au_value_t au_value_double(double n) {
    au_value_t v = {0};
    v._type = VALUE_DOUBLE;
    v._data.d_double = n;
    return v;
}
static _AlwaysInline double au_value_get_double(const au_value_t v) {
    return v._data.d_double;
}

static _AlwaysInline au_value_t au_value_bool(int32_t n) {
    au_value_t v = {0};
    v._type = VALUE_BOOL;
    v._data.d_int = n;
    return v;
}
static _AlwaysInline int32_t au_value_get_bool(const au_value_t v) {
    return v._data.d_int;
}

static _AlwaysInline au_value_t au_value_string(struct au_string *data) {
    au_value_t v = {0};
    v._type = VALUE_STR;
    v._data.d_ptr = data;
    return v;
}
static _AlwaysInline struct au_string *
au_value_get_string(const au_value_t v) {
    return v._data.d_ptr;
}

static _AlwaysInline au_value_t au_value_fn(uint32_t n) {
    au_value_t v = {0};
    v._type = VALUE_FN;
    v._data.d_fn_idx = n;
    return v;
}
static _AlwaysInline uint32_t au_value_get_fn(const au_value_t v) {
    return v._data.d_fn_idx;
}

static _AlwaysInline au_value_t au_value_struct(struct au_struct *data) {
    au_value_t v = {0};
    v._type = VALUE_STRUCT;
    v._data.d_ptr = data;
    return v;
}
static _AlwaysInline struct au_struct *
au_value_get_struct(const au_value_t v) {
    return v._data.d_ptr;
}
#endif

static enum au_vtype au_value_get_type(const au_value_t v);
static au_value_t au_value_none();
static au_value_t au_value_op_error();
static int au_value_is_op_error(au_value_t v);
static au_value_t au_value_int(int32_t n);
static int32_t au_value_get_int(const au_value_t v);
static au_value_t au_value_double(double n);
static double au_value_get_double(const au_value_t v);
static au_value_t au_value_bool(int32_t n);
static int32_t au_value_get_bool(const au_value_t v);
static au_value_t au_value_string(struct au_string *data);
static struct au_string *au_value_get_string(const au_value_t v);
static au_value_t au_value_fn(uint32_t n);
static uint32_t au_value_get_fn(const au_value_t v);
/// [func] Creates a value from a pointer to a structure.
///     The pointer must be allocated using au_obj_malloc.
static au_value_t au_value_struct(struct au_struct *data);
static struct au_struct *au_value_get_struct(const au_value_t v);

static _AlwaysInline void au_value_clear(au_value_t *a, int size) {
    for (int i = 0; i < size; i++)
        a[i] = au_value_none();
}

static _AlwaysInline au_value_t *au_value_calloc(size_t len) {
    au_value_t *array = malloc(sizeof(au_value_t) * len);
    au_value_clear(array, len);
    return array;
}

static _AlwaysInline int au_value_is_truthy(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case VALUE_INT: {
        return au_value_get_int(v) > 0;
    }
    case VALUE_DOUBLE: {
        return au_value_get_double(v) > 0;
    }
    case VALUE_BOOL: {
        return au_value_get_bool(v);
    }
    case VALUE_STR: {
        return au_value_get_string(v)->len > 0;
    }
    default:
        return 0;
    }
}

static _AlwaysInline au_value_t au_value_add(au_value_t lhs,
                                             au_value_t rhs) {
    if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_op_error();
    switch (au_value_get_type(lhs)) {
    case VALUE_INT: {
        return au_value_int(au_value_get_int(lhs) + au_value_get_int(rhs));
    }
    case VALUE_DOUBLE: {
        return au_value_double(au_value_get_double(lhs) +
                               au_value_get_double(rhs));
    }
    case VALUE_STR: {
        return au_value_string(au_string_add(au_value_get_string(lhs),
                                             au_value_get_string(rhs)));
    }
    default:
        break;
    }
    return au_value_op_error();
}

#define _BIN_OP_GENERIC_NUMBER(NAME, OP)                                  \
    static _AlwaysInline au_value_t NAME(au_value_t lhs,                  \
                                         au_value_t rhs) {                \
        if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))  \
            return au_value_op_error();                                   \
        switch (au_value_get_type(lhs)) {                                 \
        case VALUE_INT: {                                                 \
            return au_value_int(au_value_get_int(lhs)                     \
                                    OP au_value_get_int(rhs));            \
        }                                                                 \
        case VALUE_DOUBLE: {                                              \
            return au_value_double(au_value_get_double(lhs)               \
                                       OP au_value_get_double(rhs));      \
        }                                                                 \
        default:                                                          \
            break;                                                        \
        }                                                                 \
        return au_value_op_error();                                       \
    }
_BIN_OP_GENERIC_NUMBER(au_value_sub, -)
_BIN_OP_GENERIC_NUMBER(au_value_mul, *)
_BIN_OP_GENERIC_NUMBER(au_value_div, /)

static _AlwaysInline au_value_t au_value_mod(au_value_t lhs,
                                             au_value_t rhs) {
    if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_op_error();
    switch (au_value_get_type(lhs)) {
    case VALUE_INT: {
        return au_value_int(au_value_get_int(lhs) % au_value_get_int(rhs));
    }
    default:
        break;
    }
    return au_value_op_error();
}

#define _BIN_OP_BOOL_GENERIC(NAME, OP)                                    \
    static _AlwaysInline au_value_t NAME(au_value_t lhs,                  \
                                         au_value_t rhs) {                \
        if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))  \
            return au_value_bool(0);                                      \
        switch (au_value_get_type(lhs)) {                                 \
        case VALUE_INT: {                                                 \
            return au_value_bool(au_value_get_int(lhs)                    \
                                     OP au_value_get_int(rhs));           \
        }                                                                 \
        case VALUE_STR: {                                                 \
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

static _AlwaysInline au_value_t au_value_eq(au_value_t lhs,
                                            au_value_t rhs) {
    if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_bool(0);
    switch (au_value_get_type(lhs)) {
    case VALUE_INT: {
        return au_value_bool(au_value_get_int(lhs) ==
                             au_value_get_int(rhs));
    }
    case VALUE_BOOL: {
        return au_value_bool(au_value_get_bool(lhs) ==
                             au_value_get_bool(rhs));
    }
    case VALUE_STR: {
        return au_value_bool(au_string_cmp(au_value_get_string(lhs),
                                           au_value_get_string(rhs)) == 0);
    }
    default:
        break;
    }
    return au_value_bool(0);
}
static _AlwaysInline au_value_t au_value_neq(au_value_t lhs,
                                             au_value_t rhs) {
    if (_Unlikely(au_value_get_type(lhs) != au_value_get_type(rhs)))
        return au_value_bool(1);
    switch (au_value_get_type(lhs)) {
    case VALUE_INT: {
        return au_value_bool(au_value_get_int(lhs) !=
                             au_value_get_int(rhs));
    }
    case VALUE_BOOL: {
        return au_value_bool(au_value_get_bool(lhs) !=
                             au_value_get_bool(rhs));
    }
    case VALUE_STR: {
        return au_value_bool(au_string_cmp(au_value_get_string(lhs),
                                           au_value_get_string(rhs)) != 0);
    }
    default:
        break;
    }
    return au_value_bool(1);
}

void au_value_print(au_value_t value);
