// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include <stdlib.h>

#include "platform/platform.h"
#include "value.h"
#endif

struct au_obj_tuple;

_Public struct au_obj_tuple *au_obj_tuple_new(size_t len);

_Public void au_obj_tuple_del(struct au_obj_tuple *obj_tuple);

_Public int au_obj_tuple_get(struct au_obj_tuple *obj_tuple,
                             const au_value_t idx, au_value_t *result);

_Public int au_obj_tuple_set(struct au_obj_tuple *obj_tuple,
                             au_value_t idx, au_value_t value);

_Public int32_t au_obj_tuple_len(struct au_obj_tuple *obj_tuple);

#ifdef _AUMENT_H
_Public struct au_obj_tuple *au_obj_tuple_coerce(au_value_t value);
#else
extern struct au_struct_vdata au_obj_tuple_vdata;
static inline struct au_obj_tuple *au_obj_tuple_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_tuple_vdata)
        return 0;
    return (struct au_obj_tuple *)au_value_get_struct(value);
}
#endif