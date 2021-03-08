// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "helper.h"
#include "core/rt/exception.h"
#include "vdata.h"
#endif

au_value_t au_struct_idx_get(au_value_t value, au_value_t idx) {
    au_value_t retval;
    struct au_struct *s = au_value_get_struct(value);
    if (!s->vdata->idx_get_fn(s, idx, &retval)) {
        au_fatal("au_struct_idx_get: accessing invalid index");
    }
    return retval;
}

void au_struct_idx_set(au_value_t value, au_value_t idx, au_value_t item) {
    struct au_struct *s = au_value_get_struct(value);
    if (!s->vdata->idx_set_fn(s, idx, item)) {
        au_fatal("au_struct_idx_set: setting invalid index");
    }
}
