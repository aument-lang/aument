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

AU_PUBLIC struct au_obj_tuple *au_obj_tuple_new(size_t len);

AU_PUBLIC void au_obj_tuple_del(struct au_obj_tuple *obj_tuple);

AU_PUBLIC int au_obj_tuple_get(struct au_obj_tuple *obj_tuple,
                             const au_value_t idx, au_value_t *result);

AU_PUBLIC int au_obj_tuple_set(struct au_obj_tuple *obj_tuple,
                             au_value_t idx, au_value_t value);

AU_PUBLIC int32_t au_obj_tuple_len(struct au_obj_tuple *obj_tuple);

#ifdef _AUMENT_H
AU_PUBLIC struct au_obj_tuple *au_obj_tuple_coerce(au_value_t value);
#else
extern struct au_struct_vdata au_obj_tuple_vdata;
static inline struct au_obj_tuple *au_obj_tuple_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_tuple_vdata)
        return 0;
    return (struct au_obj_tuple *)au_value_get_struct(value);
}
#endif