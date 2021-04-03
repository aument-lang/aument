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

struct au_obj_dict;

AU_PUBLIC struct au_obj_dict *au_obj_dict_new();

AU_PUBLIC void au_obj_dict_del(struct au_obj_dict *obj_dict);

AU_PUBLIC int au_obj_dict_get(struct au_obj_dict *obj_dict,
                              const au_value_t idx, au_value_t *result);

AU_PUBLIC int au_obj_dict_set(struct au_obj_dict *obj_dict, au_value_t idx,
                              au_value_t value);

AU_PUBLIC int32_t au_obj_dict_len(struct au_obj_dict *obj_dict);

#ifdef _AUMENT_H
AU_PUBLIC struct au_obj_dict *au_obj_dict_coerce(au_value_t value);
#else
extern struct au_struct_vdata au_obj_dict_vdata;
static inline struct au_obj_dict *au_obj_dict_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_dict_vdata)
        return 0;
    return (struct au_obj_dict *)au_value_get_struct(value);
}
#endif