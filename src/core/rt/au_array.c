// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "au_array.h"
#include "au_struct.h"
#include "value.h"

#include "../value_array.h"
#endif

struct au_obj_array {
    struct au_struct header;
    struct au_value_array array;
};

struct au_struct_vdata au_obj_array_vdata = (struct au_struct_vdata){
    .del_fn = (au_struct_del_fn_t)au_obj_array_del,
    .idx_get_fn = (au_struct_idx_get_fn_t)au_obj_array_get,
    .idx_set_fn = (au_struct_idx_set_fn_t)au_obj_array_set,
    .len_fn = (au_struct_len_fn_t)au_obj_array_len,
};

static struct au_struct array_header = (struct au_struct){
    .rc = 1,
    .vdata = &au_obj_array_vdata,
};

struct au_obj_array *au_obj_array_new(size_t capacity) {
    struct au_obj_array *obj_array = malloc(sizeof(struct au_obj_array));
    obj_array->header = array_header;
    obj_array->array.data = au_value_calloc(capacity);
    obj_array->array.len = 0;
    obj_array->array.cap = capacity;
    return obj_array;
}

void au_obj_array_del(struct au_obj_array *obj_array) {
    for (size_t i = 0; i < obj_array->array.len; i++) {
        au_value_deref(obj_array->array.data[i]);
    }
    free(obj_array->array.data);
    free(obj_array);
}

void au_obj_array_push(struct au_obj_array *obj_array, au_value_t el) {
    au_value_array_add(&obj_array->array, el);
}

au_value_t au_obj_array_get(struct au_obj_array *obj_array,
                            au_value_t idx) {
    return au_value_array_at(&obj_array->array, au_value_get_int(idx));
}

int au_obj_array_set(struct au_obj_array *obj_array, au_value_t idx_val,
                     au_value_t value) {
    const size_t idx = au_value_get_int(idx_val);
    if (idx >= obj_array->array.len)
        return 0;
    au_value_ref(value);
    obj_array->array.data[idx] = value;
    return 1;
}

int32_t au_obj_array_len(struct au_obj_array *obj_array) {
    return (int32_t)obj_array->array.len;
}
