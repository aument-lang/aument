// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_int) {
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

AU_EXTERN_FUNC_DECL(au_std_float) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_INT: {
        return au_value_double((double)au_value_get_int(value));
    }
    case AU_VALUE_DOUBLE: {
        return value;
    }
    default: {
        au_value_deref(value);
        return au_value_double(0);
    }
    }
}

AU_EXTERN_FUNC_DECL(au_std_bool) {
    return au_value_bool(au_value_is_truthy(_args[0]));
}

AU_EXTERN_FUNC_DECL(au_std_str) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_STR: {
        au_string_ref(au_value_get_string(value));
        return value;
    }
    case AU_VALUE_BOOL: {
        const char *repr = au_value_get_bool(value) ? "(true)" : "(false)";
        return au_value_string(au_string_from_const(repr, strlen(repr)));
    }
    case AU_VALUE_INT: {
        int32_t abs_num = au_value_get_int(value);
        struct au_string *header =
            au_obj_malloc(sizeof(struct au_string) + 1, 0);
        header->rc = 1;
        header->len = 1;
        uint32_t pos = 0, cap = 1;
        int is_neg = 0;
        if (abs_num < 0) {
            is_neg = 1;
            abs_num = -abs_num;
        }
        while (abs_num != 0) {
            if (pos == cap) {
                cap *= 2;
                header =
                    au_obj_realloc(header, sizeof(struct au_string) + cap);
            }
            header->data[pos] = ((abs_num % 10) + '0');
            pos++;
            abs_num /= 10;
        }
        if (is_neg) {
            if (pos == cap) {
                cap *= 2;
                header =
                    au_obj_realloc(header, sizeof(struct au_string) + cap);
            }
            header->data[pos] = '-';
            pos++;
        }
        for (int i = 0, j = pos - 1; i < j; i++, j--) {
            char c = header->data[i];
            header->data[i] = header->data[j];
            header->data[j] = c;
        }
        header->len = pos;
        return au_value_string(header);
    }
    default: {
        au_value_deref(value);
        return au_value_string(au_string_from_const("", 0));
    }
    }
}
