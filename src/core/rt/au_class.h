// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "core/array.h"
#include "core/hm_vars.h"
#include "value.h"
#include <stdlib.h>
#endif

struct au_class_interface {
    struct au_hm_vars map;
    char *name;
};

ARRAY_TYPE_STRUCT(struct au_class_interface, au_class_interface_array, 1)

void au_class_interface_init(struct au_class_interface *interface,
                             char *name);
void au_class_interface_del(struct au_class_interface *interface);

struct au_obj_class;
extern struct au_struct_vdata au_obj_class_vdata;

struct au_obj_class *
au_obj_class_new(const struct au_class_interface *interface);
void au_obj_class_del(struct au_obj_class *obj_class);

int au_obj_class_get(struct au_obj_class *obj_class, const au_value_t idx,
                     au_value_t *result);
int au_obj_class_set(struct au_obj_class *obj_class, au_value_t idx,
                     au_value_t value);
int32_t au_obj_class_len(struct au_obj_class *obj_class);

static inline struct au_obj_class *au_obj_class_coerce(au_value_t value) {
    if (au_value_get_type(value) != VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_class_vdata)
        return 0;
    return (struct au_obj_class *)au_value_get_struct(value);
}