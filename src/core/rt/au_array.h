// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "value.h"
#include <stdlib.h>
#endif

struct au_obj_array;
extern struct au_struct_vdata au_obj_array_vdata;

struct au_obj_array *au_obj_array_new(size_t capacity);
void au_obj_array_del(struct au_obj_array *obj_array);

void au_obj_array_push(struct au_obj_array *obj_array, au_value_t el);
int au_obj_array_get(struct au_obj_array *obj_array, const au_value_t idx,
                     au_value_t *result);
int au_obj_array_set(struct au_obj_array *obj_array, au_value_t idx,
                     au_value_t value);
int32_t au_obj_array_len(struct au_obj_array *obj_array);

static inline struct au_obj_array *au_obj_array_coerce(au_value_t value) {
    if (au_value_get_type(value) != VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_array_vdata)
        return 0;
    return (struct au_obj_array *)au_value_get_struct(value);
}