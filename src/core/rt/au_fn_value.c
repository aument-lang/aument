// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "au_fn_value.h"
#include "au_struct.h"
#include "value.h"
#endif

struct au_fn_value {
    struct au_struct header;
};

void au_fn_value_del(struct au_fn_value *fn_value) {
    (void)fn_value;
}

static int au_fn_value_get(struct au_fn_value *fn_value,
                           const au_value_t idx, au_value_t *result) {
    (void)fn_value;
    (void)idx;
    (void)result;
    return 0;
}

static int au_fn_value_set(struct au_fn_value *fn_value,
                           au_value_t idx_val, au_value_t value) {
    (void)fn_value;
    (void)idx_val;
    (void)value;
    return 0;
}

static int32_t au_fn_value_len(struct au_fn_value *fn_value) {
    (void)fn_value;
    return 0;
}

struct au_struct_vdata au_fn_value_vdata;
static int au_fn_value_vdata_inited = 0;
static void au_fn_value_vdata_init() {
    if (!au_fn_value_vdata_inited) {
        au_fn_value_vdata = (struct au_struct_vdata){
            .del_fn = (au_obj_del_fn_t)au_fn_value_del,
            .idx_get_fn = (au_struct_idx_get_fn_t)au_fn_value_get,
            .idx_set_fn = (au_struct_idx_set_fn_t)au_fn_value_set,
            .len_fn = (au_struct_len_fn_t)au_fn_value_len,
        };
        au_fn_value_vdata_inited = 1;
    }
}

struct au_fn_value *au_fn_value_new() {
    struct au_fn_value *fn_value = au_obj_malloc(
        sizeof(struct au_fn_value), (au_obj_del_fn_t)au_fn_value_del);
    au_fn_value_vdata_init();
    fn_value->header = (struct au_struct){
        .rc = 1,
        .vdata = &au_fn_value_vdata,
    };
    return fn_value;
}

#ifdef _AUMENT_H
struct au_fn_value *au_fn_value_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_fn_value_vdata)
        return 0;
    return (struct au_fn_value *)au_value_get_struct(value);
}
#endif