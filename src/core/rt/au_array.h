// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform/platform.h"
#include "value.h"
#include <stdlib.h>
#endif

struct au_obj_array;

_Public struct au_obj_array *au_obj_array_new(size_t capacity);

_Public void au_obj_array_del(struct au_obj_array *obj_array);

_Public void au_obj_array_push(struct au_obj_array *obj_array,
                               au_value_t el);

_Public int au_obj_array_get(struct au_obj_array *obj_array,
                             const au_value_t idx, au_value_t *result);

_Public int au_obj_array_set(struct au_obj_array *obj_array,
                             au_value_t idx, au_value_t value);

_Public int32_t au_obj_array_len(struct au_obj_array *obj_array);

#ifdef _AUMENT_H
_Public struct au_obj_array *au_obj_array_coerce(au_value_t value);
#else
extern struct au_struct_vdata au_obj_array_vdata;
static inline struct au_obj_array *au_obj_array_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_array_vdata)
        return 0;
    return (struct au_obj_array *)au_value_get_struct(value);
}
#endif