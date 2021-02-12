// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/au_struct.h"
#include "core/rt/extern_fn.h"
#include "core/rt/value.h"

AU_EXTERN_FUNC_DECL(au_std_len) {
    const au_value_t value = args[0];
    switch (au_value_get_type(value)) {
    case VALUE_STRUCT: {
        struct au_struct *s = au_value_get_struct(value);
        struct au_struct_vdata *vdata = s->vdata;
        if (vdata->len_fn != 0)
            return au_value_int(vdata->len_fn(s));
    }
    default: {
        return au_value_int(0);
    }
    }
}