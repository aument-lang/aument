// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#endif

#include <stdint.h>
#include <assert.h>
#include <string.h>

#ifdef AU_IS_INTERPRETER
#include "platform/platform.h"
#include "au_string.h"
#endif

#ifdef USE_NAN_TAGGING
#else
union au_value_data {
    int32_t d_int;
    void *d_ptr;
    uint32_t d_fn_idx;
};

enum au_vtype {
    VALUE_NONE = 0,
    VALUE_INT = 1,
    VALUE_BOOL = 2,
    VALUE_FN = 3,
    VALUE_STR = 4,
};

struct _au_value {
    union au_value_data _data;
    enum au_vtype _type;
};

typedef struct _au_value au_value_t;

static _AlwaysInline enum au_vtype au_value_get_type(const struct _au_value v) {
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
static _AlwaysInline int32_t au_value_get_int(const struct _au_value v) { return v._data.d_int; }

static _AlwaysInline struct _au_value au_value_bool(int32_t n) {
    struct _au_value v = {0};
    v._type = VALUE_BOOL;
    v._data.d_int = n;
    return v;
}
static _AlwaysInline int32_t au_value_get_bool(const struct _au_value v) { return v._data.d_int; }

struct au_string;
static _AlwaysInline struct _au_value au_value_string(struct au_string *data) {
    struct _au_value v = {0};
    v._type = VALUE_STR;
    v._data.d_ptr = data;
    return v;
}
static _AlwaysInline struct au_string *au_value_get_string(const struct _au_value v) { return v._data.d_ptr; }

static _AlwaysInline struct _au_value au_value_fn(uint32_t n) {
    struct _au_value v = {0};
    v._type = VALUE_FN;
    v._data.d_fn_idx = n;
    return v;
}
static _AlwaysInline uint32_t au_value_get_fn(const struct _au_value v) { return v._data.d_fn_idx; }

static _AlwaysInline void au_value_clear(au_value_t *a, int size) {
    for(int i = 0; i < size; i++)
        a[i] = (au_value_t){0};
}

static _AlwaysInline au_value_t *au_value_calloc(size_t len) {
    return calloc(len, sizeof(au_value_t));
}
#endif

static _AlwaysInline int au_value_is_truthy(const struct _au_value v) {
    switch(au_value_get_type(v)) {
        case VALUE_INT: {
            return au_value_get_int(v) > 0;
        }
        case VALUE_BOOL: {
            return au_value_get_bool(v);
        }
        default: return 0;
    }
}

static _AlwaysInline void au_value_ref(const struct _au_value v) {
    switch(au_value_get_type(v)) {
        case VALUE_STR: {
            au_string_ref(au_value_get_string(v));
            break;
        }
        default: return;
    }
}

static _AlwaysInline void au_value_deref(const struct _au_value v) {
    switch(au_value_get_type(v)) {
        case VALUE_STR: {
            au_string_deref(au_value_get_string(v));
            break;
        }
        default: return;
    }
}

#define _BIN_OP_GENERIC_NUMBER(OP) { \
    if (au_value_get_type(lhs) != au_value_get_type(rhs)) return au_value_none(); \
    switch(au_value_get_type(lhs)) { \
        case VALUE_INT: { \
            return au_value_int(au_value_get_int(lhs) OP au_value_get_int(rhs)); \
        } \
        default: break; \
    } \
    return au_value_none(); \
}
static _AlwaysInline au_value_t au_value_add(au_value_t lhs, au_value_t rhs) {
    if(au_value_get_type(lhs) != au_value_get_type(rhs))
        return au_value_none();
    switch(au_value_get_type(lhs)) {
        case VALUE_INT: {
            return au_value_int(au_value_get_int(lhs) + au_value_get_int(rhs));
        }
        case VALUE_STR: {
            return au_value_string(au_string_add(au_value_get_string(lhs), au_value_get_string(rhs)));
        }
        default: break;
    }
    return au_value_none();
}
static _AlwaysInline au_value_t au_value_sub(au_value_t lhs, au_value_t rhs) _BIN_OP_GENERIC_NUMBER(-)
static _AlwaysInline au_value_t au_value_mul(au_value_t lhs, au_value_t rhs) _BIN_OP_GENERIC_NUMBER(*)
static _AlwaysInline au_value_t au_value_div(au_value_t lhs, au_value_t rhs) _BIN_OP_GENERIC_NUMBER(/)
static _AlwaysInline au_value_t au_value_mod(au_value_t lhs, au_value_t rhs) _BIN_OP_GENERIC_NUMBER(%)

#define _BIN_OP_BOOL_GENERIC_NUMBER(OP) { \
    if (au_value_get_type(lhs) != au_value_get_type(rhs)) return au_value_none(); \
    switch(au_value_get_type(lhs)) { \
        case VALUE_INT: { \
            return au_value_bool(au_value_get_int(lhs) OP au_value_get_int(rhs)); \
        } \
        default: break; \
    } \
    return au_value_bool(0); \
}
static _AlwaysInline au_value_t au_value_lt(au_value_t lhs, au_value_t rhs) _BIN_OP_BOOL_GENERIC_NUMBER(<)
static _AlwaysInline au_value_t au_value_gt(au_value_t lhs, au_value_t rhs) _BIN_OP_BOOL_GENERIC_NUMBER(>)
static _AlwaysInline au_value_t au_value_leq(au_value_t lhs, au_value_t rhs) _BIN_OP_BOOL_GENERIC_NUMBER(<=)
static _AlwaysInline au_value_t au_value_geq(au_value_t lhs, au_value_t rhs) _BIN_OP_BOOL_GENERIC_NUMBER(>=)
#undef _BIN_OP_GENERIC_NUMBER

static _AlwaysInline au_value_t au_value_eq(au_value_t lhs, au_value_t rhs) {
    if(au_value_get_type(lhs) != au_value_get_type(lhs)) return au_value_bool(0);
    switch(au_value_get_type(lhs)) {
        case VALUE_INT: {
            return au_value_bool(au_value_get_int(lhs) == au_value_get_int(rhs));
        }
        case VALUE_BOOL: {
            return au_value_bool(au_value_get_bool(lhs) == au_value_get_bool(rhs));
        }
        case VALUE_STR: {
            return au_value_bool(strcmp(au_value_get_string(lhs)->data, au_value_get_string(rhs)->data) == 0);
        }
        default: break;
    }
    return au_value_bool(0);
}
static _AlwaysInline au_value_t au_value_neq(au_value_t lhs, au_value_t rhs) {
    if(au_value_get_type(lhs) != au_value_get_type(lhs)) return au_value_bool(1);
    switch(au_value_get_type(lhs)) {
        case VALUE_INT: {
            return au_value_bool(au_value_get_int(lhs) != au_value_get_int(rhs));
        }
        case VALUE_BOOL: {
            return au_value_bool(au_value_get_bool(lhs) != au_value_get_bool(rhs));
        }
        case VALUE_STR: {
            return au_value_bool(strcmp(au_value_get_string(lhs)->data, au_value_get_string(rhs)->data) == 0);
        }
        default: break;
    }
    return au_value_bool(1);
}

void au_value_print(au_value_t value);
