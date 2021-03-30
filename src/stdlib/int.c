// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

#define MAX_SMALL_STRING 256

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
        int32_t num;
        if (header->len < MAX_SMALL_STRING) {
            char string[MAX_SMALL_STRING] = {0};
            memcpy(string, header->data, header->len);
            string[header->len] = 0;
            num = atoi(string);
        } else {
            char *string = au_data_strndup(header->data, header->len);
            num = atoi(string);
            au_data_free(string);
        }
        return au_value_int(num);
    }
    default: {
        au_value_deref(value);
        return au_value_int(0);
    }
    }
}
