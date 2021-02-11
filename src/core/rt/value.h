// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <stdint.h>
#include <string.h>

#include "au_string.h"
#include "au_struct.h"
#include "platform/platform.h"
#endif

enum au_vtype {
    VALUE_INVALID = 0,
    VALUE_NONE = 1,
    VALUE_INT = 2,
    VALUE_BOOL = 3,
    VALUE_FN = 4,
    VALUE_STR = 5,
    VALUE_DOUBLE = 6,
    VALUE_STRUCT = 7,
};

#ifdef USE_NAN_TAGGING
__attribute__((__packed__)) struct _au_value {
    union {
        struct __attribute__((__packed__)) {
            uint64_t sign : 1;
            uint64_t exponent : 11;
            uint64_t fraction : 52;
        } bits;
        double d;
    } _as;
};

#define AU_SPECIAL_DOUBLE 0x7ff
#define AU_INF_FRACTION 0x0
#define AU_NAN_FRACTION 0xfffffffffffff
#define AU_REPR_GET_PTR(x) (((uint64_t)(x)) & 0xffffffffffff)
#define AU_REPR_GET_TAG(x) (((x) >> 48) & 0xf)
#define AU_REPR_FRACTION(TAG, PTR)                                        \
    ((((uint64_t)(TAG)) << 48) | AU_REPR_GET_PTR(PTR))

typedef struct _au_value au_value_t;

static _AlwaysInline enum au_vtype
au_value_get_type(const struct _au_value v) {
    if (v._as.bits.exponent == AU_SPECIAL_DOUBLE &&
        v._as.bits.fraction != AU_INF_FRACTION &&
        v._as.bits.fraction != AU_NAN_FRACTION && v._as.bits.sign) {
        return (enum au_vtype)(AU_REPR_GET_TAG(v._as.bits.fraction));
    } else {
        return VALUE_DOUBLE;
    }
}

static _AlwaysInline struct _au_value au_value_none() {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_NONE, 0);
    return v;
}

static _AlwaysInline struct _au_value au_value_int(int32_t n) {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_INT, ((uint64_t)n));
    return v;
}
static _AlwaysInline int32_t au_value_get_int(const struct _au_value v) {
    return (int32_t)(AU_REPR_GET_PTR(v._as.bits.fraction));
}

static _AlwaysInline struct _au_value au_value_double(double n) {
    struct _au_value v = {0};
    // TODO: check nans and inf values
    v._as.d = n;
    return v;
}
static _AlwaysInline double au_value_get_double(const struct _au_value v) {
    return v._as.d;
}

static _AlwaysInline struct _au_value au_value_bool(int32_t n) {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_BOOL, n);
    return v;
}
static _AlwaysInline int32_t au_value_get_bool(const struct _au_value v) {
    return (int32_t)(AU_REPR_GET_PTR(v._as.bits.fraction));
}

struct au_string;
static _AlwaysInline struct _au_value
au_value_string(struct au_string *data) {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_STR, (uintptr_t)data);
    return v;
}
static _AlwaysInline struct au_string *
au_value_get_string(const struct _au_value v) {
    return (struct au_string *)((uintptr_t)AU_REPR_GET_PTR(
        v._as.bits.fraction));
}

static _AlwaysInline struct _au_value au_value_fn(uint32_t n) {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_FN, (uint64_t)n);
    return v;
}
static _AlwaysInline uint32_t au_value_get_fn(const struct _au_value v) {
    return (uint32_t)(AU_REPR_GET_PTR(v._as.bits.fraction));
}

struct au_string;
static _AlwaysInline struct _au_value
au_value_struct(struct au_struct *data) {
    struct _au_value v = {0};
    v._as.bits.sign = 1;
    v._as.bits.exponent = AU_SPECIAL_DOUBLE;
    v._as.bits.fraction = AU_REPR_FRACTION(VALUE_STRUCT, (uintptr_t)data);
    return v;
}
static _AlwaysInline struct au_struct *
au_value_get_struct(const struct _au_value v) {
    return (void *)((uintptr_t)AU_REPR_GET_PTR(v._as.bits.fraction));
}

static _AlwaysInline void au_value_clear(au_value_t *a, int size) {
    for (int i = 0; i < size; i++)
        a[i] = au_value_none();
}

static _AlwaysInline au_value_t *au_value_calloc(size_t len) {
    au_value_t *array = malloc(sizeof(au_value_t) * len);
    au_value_clear(array, len);
    return array;
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

static _AlwaysInline enum au_vtype
au_value_get_type(const struct _au_value v) {
    return v._type;
}

static _AlwaysInline struct _au_value au_value_none() {
    struct _au_value v = {0};
    return v;
}

static _AlwaysInline struct _au_value au_value_int(int32_t n) {
    struct _au_value v = {0};
    v._type = VALUE_INT;
    v._data.d_int = n;
    return v;
}
static _AlwaysInline int32_t au_value_get_int(const struct _au_value v) {
    return v._data.d_int;
}

static _AlwaysInline struct _au_value au_value_double(double n) {
    struct _au_value v = {0};
    v._type = VALUE_DOUBLE;
    v._data.d_double = n;
    return v;
}
static _AlwaysInline double au_value_get_double(const struct _au_value v) {
    return v._data.d_double;
}

static _AlwaysInline struct _au_value au_value_bool(int32_t n) {
    struct _au_value v = {0};
    v._type = VALUE_BOOL;
    v._data.d_int = n;
    return v;
}
static _AlwaysInline int32_t au_value_get_bool(const struct _au_value v) {
    return v._data.d_int;
}

struct au_string;
static _AlwaysInline struct _au_value
au_value_string(struct au_string *data) {
    struct _au_value v = {0};
    v._type = VALUE_STR;
    v._data.d_ptr = data;
    return v;
}
static _AlwaysInline struct au_string *
au_value_get_string(const struct _au_value v) {
    return v._data.d_ptr;
}

static _AlwaysInline struct _au_value au_value_fn(uint32_t n) {
    struct _au_value v = {0};
    v._type = VALUE_FN;
    v._data.d_fn_idx = n;
    return v;
}
static _AlwaysInline uint32_t au_value_get_fn(const struct _au_value v) {
    return v._data.d_fn_idx;
}

struct au_string;
static _AlwaysInline struct _au_value au_value_struct(void *data) {
    struct _au_value v = {0};
    v._type = VALUE_STR;
    v._data.d_ptr = data;
    return v;
}
static _AlwaysInline void *au_value_get_struct(const struct _au_value v) {
    return v._data.d_ptr;
}

static _AlwaysInline void au_value_clear(au_value_t *a, int size) {
    for (int i = 0; i < size; i++)
        a[i] = (au_value_t){0};
}

static _AlwaysInline au_value_t *au_value_calloc(size_t len) {
    return calloc(len, sizeof(au_value_t));
}
#endif

static _AlwaysInline int au_value_is_truthy(const struct _au_value v) {
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

static _AlwaysInline void au_value_ref(const struct _au_value v) {
    switch (au_value_get_type(v)) {
    case VALUE_STR: {
        au_string_ref(au_value_get_string(v));
        break;
    }
    case VALUE_STRUCT: {
        au_struct_ref((struct au_struct *)au_value_get_struct(v));
        break;
    }
    default:
        return;
    }
}

static _AlwaysInline void au_value_deref(const struct _au_value v) {
    switch (au_value_get_type(v)) {
    case VALUE_STR: {
        au_string_deref(au_value_get_string(v));
        break;
    }
    default:
        return;
    }
}

static _AlwaysInline au_value_t au_value_add(au_value_t lhs,
                                             au_value_t rhs) {
    if (au_value_get_type(lhs) != au_value_get_type(rhs))
        return au_value_none();
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
    return au_value_none();
}

#define _BIN_OP_GENERIC_NUMBER(NAME, OP)                                  \
    static _AlwaysInline au_value_t NAME(au_value_t lhs,                  \
                                         au_value_t rhs) {                \
        if (au_value_get_type(lhs) != au_value_get_type(rhs))             \
            return au_value_none();                                       \
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
        return au_value_none();                                           \
    }
_BIN_OP_GENERIC_NUMBER(au_value_sub, -)
_BIN_OP_GENERIC_NUMBER(au_value_mul, *)
_BIN_OP_GENERIC_NUMBER(au_value_div, /)

static _AlwaysInline au_value_t au_value_mod(au_value_t lhs,
                                             au_value_t rhs) {
    if (au_value_get_type(lhs) != au_value_get_type(rhs))
        return au_value_none();
    switch (au_value_get_type(lhs)) {
    case VALUE_INT: {
        return au_value_int(au_value_get_int(lhs) % au_value_get_int(rhs));
    }
    default:
        break;
    }
    return au_value_none();
}

#define _BIN_OP_BOOL_GENERIC(NAME, OP)                                    \
    static _AlwaysInline au_value_t NAME(au_value_t lhs,                  \
                                         au_value_t rhs) {                \
        if (au_value_get_type(lhs) != au_value_get_type(rhs))             \
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
    if (au_value_get_type(lhs) != au_value_get_type(lhs))
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
    if (au_value_get_type(lhs) != au_value_get_type(lhs))
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
