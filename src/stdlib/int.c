// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_int_is) {
    const au_value_t value = _args[0];
    const au_value_t retval =
        au_value_bool(au_value_get_type(value) == AU_VALUE_INT);
    au_value_deref(value);
    return retval;
}

AU_EXTERN_FUNC_DECL(au_std_int_into) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_INT: {
        return value;
    }
    case AU_VALUE_DOUBLE: {
        return au_value_int((int)au_value_get_double(value));
    }
    case AU_VALUE_STR: {
        const struct au_string *header = au_value_get_string(value);
        int num = 0;
        for (size_t i = 0; i < header->len; i++) {
            if (header->data[i] >= '0' && header->data[i] <= '9') {
                num = num * 10 + (header->data[i] - '0');
            } else {
                break;
            }
        }
        au_value_deref(value);
        return au_value_int(num);
    }
    default: {
        au_value_deref(value);
        return au_value_int(0);
    }
    }
}
