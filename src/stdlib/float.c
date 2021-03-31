// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

#define MAX_SMALL_STRING 32

AU_EXTERN_FUNC_DECL(au_std_float_is) {
    const au_value_t value = _args[0];
    const au_value_t retval =
        au_value_bool(au_value_get_type(value) == AU_VALUE_DOUBLE);
    au_value_deref(value);
    return retval;
}

AU_EXTERN_FUNC_DECL(au_std_float_into) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_INT: {
        return au_value_double((double)au_value_get_int(value));
    }
    case AU_VALUE_DOUBLE: {
        return value;
    }
    case AU_VALUE_STR: {
        const struct au_string *header = au_value_get_string(value);
        double num;
        if (header->len < MAX_SMALL_STRING) {
            char string[MAX_SMALL_STRING];
            memcpy(string, header->data, header->len);
            string[header->len] = 0;
            num = strtod(string, 0);
        } else {
            char *string = au_data_strndup(header->data, header->len);
            num = strtod(string, 0);
            au_data_free(string);
        }
        return au_value_double(num);
    }
    default: {
        au_value_deref(value);
        return au_value_double(0);
    }
    }
}