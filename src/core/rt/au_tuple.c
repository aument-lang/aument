// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "au_tuple.h"
#include "au_struct.h"
#include "value.h"
#endif

struct au_obj_tuple {
    struct au_struct header;
    size_t len;
    au_value_t data[];
};

struct au_struct_vdata au_obj_tuple_vdata;
static int au_obj_tuple_vdata_inited = 0;
static void au_obj_tuple_vdata_init() {
    if (!au_obj_tuple_vdata_inited) {
        au_obj_tuple_vdata = (struct au_struct_vdata){
            .del_fn = (au_obj_del_fn_t)au_obj_tuple_del,
            .idx_get_fn = (au_struct_idx_get_fn_t)au_obj_tuple_get,
            .idx_set_fn = (au_struct_idx_set_fn_t)au_obj_tuple_set,
            .len_fn = (au_struct_len_fn_t)au_obj_tuple_len,
        };
        au_obj_tuple_vdata_inited = 1;
    }
}

struct au_obj_tuple *au_obj_tuple_new(size_t len) {
    struct au_obj_tuple *obj_tuple = au_obj_malloc(
        sizeof(struct au_obj_tuple) + sizeof(au_value_t) * len,
        (au_obj_del_fn_t)au_obj_tuple_del);
    au_obj_tuple_vdata_init();
    obj_tuple->header = (struct au_struct){
        .rc = 1,        
        .vdata = &au_obj_tuple_vdata,
    };
    obj_tuple->len = len;
    au_value_clear(obj_tuple->data, len);
    return obj_tuple;
}

void au_obj_tuple_del(struct au_obj_tuple *obj_tuple) {
    for (size_t i = 0; i < obj_tuple->len; i++) {
        au_value_deref(obj_tuple->data[i]);
    }
}

int au_obj_tuple_get(struct au_obj_tuple *obj_tuple, const au_value_t idx,
                     au_value_t *result) {
    const size_t index = au_value_get_int(idx);
    if (index > obj_tuple->len)
        return 0;
    au_value_ref(obj_tuple->data[index]);
    *result = obj_tuple->data[index];
    return 1;
}

int au_obj_tuple_set(struct au_obj_tuple *obj_tuple, au_value_t idx_val,
                     au_value_t value) {
    const size_t idx = au_value_get_int(idx_val);
    if (idx >= obj_tuple->len)
        return 0;
    au_value_ref(value);
    obj_tuple->data[idx] = value;
    au_value_deref(obj_tuple->data[idx]);
    return 1;
}

int32_t au_obj_tuple_len(struct au_obj_tuple *obj_tuple) {
    return (int32_t)obj_tuple->len;
}
