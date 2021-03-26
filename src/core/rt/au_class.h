// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <stdlib.h>

#include "core/array.h"
#include "core/hm_vars.h"
#include "platform/platform.h"
#include "value.h"
#endif

#define AU_CLASS_FLAG_EXPORTED (1 << 0)

struct au_class_interface {
    char *name;
    uint32_t flags;
    uint32_t rc;
    struct au_hm_vars map;
};

struct au_obj_class {
    struct au_struct header;
    const struct au_class_interface *interface;
    au_value_t data[];
};

AU_ARRAY_COPY(struct au_class_interface *, au_class_interface_ptr_array, 1)

AU_PUBLIC void
au_class_interface_init(struct au_class_interface *interface, char *name);

/// [func] Increases reference count of an au_class_interface instance.
/// This struct is owned by a au_class_interface_ptr_array and should
/// only be called when initializing it inside a
/// au_class_interface_ptr_array.
AU_PRIVATE void
au_class_interface_ref(struct au_class_interface *interface);

/// [func] Decreases reference count of an au_class_interface instance.
/// This struct is owned by a au_class_interface_ptr_array and should
/// only be called when destroying it inside a
/// au_class_interface_ptr_array.
AU_PRIVATE void
au_class_interface_deref(struct au_class_interface *interface);

struct au_obj_class;

AU_PUBLIC struct au_obj_class *
au_obj_class_new(const struct au_class_interface *interface);

AU_PUBLIC void au_obj_class_del(struct au_obj_class *obj_class);

AU_PUBLIC int au_obj_class_get(struct au_obj_class *obj_class,
                               const au_value_t idx, au_value_t *result);

AU_PUBLIC int au_obj_class_set(struct au_obj_class *obj_class,
                               au_value_t idx, au_value_t value);

AU_PUBLIC int32_t au_obj_class_len(struct au_obj_class *obj_class);

#ifdef _AUMENT_H
AU_PUBLIC struct au_obj_class *au_obj_class_coerce(const au_value_t value);
#else
extern struct au_struct_vdata au_obj_class_vdata;
static inline struct au_obj_class *
au_obj_class_coerce(const au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_class_vdata)
        return 0;
    return (struct au_obj_class *)au_value_get_struct(value);
}
#endif