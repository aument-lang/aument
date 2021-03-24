// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/au_array.h"
#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"
#include "lib/string_builder.h"

AU_EXTERN_FUNC_DECL(au_std_str_is) {
    const au_value_t value = _args[0];
    const au_value_t retval = au_value_bool(au_value_get_type(value) == AU_VALUE_STR);
    au_value_deref(value);
    return retval;
}

AU_EXTERN_FUNC_DECL(au_std_str_into) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_STR: {
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

AU_EXTERN_FUNC_DECL(au_std_str_chars) {
    const au_value_t value = _args[0];
    if (au_value_get_type(value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *str = au_value_get_string(value);

    struct au_obj_array *array = au_obj_array_new(1);

    // FIXME: support Unicode characters

    for (size_t i = 0; i < str->len; i++) {
        struct au_string_builder builder = {0};
        au_string_builder_init(&builder);
        au_string_builder_add(&builder, str->data[i]);
        struct au_string *newstr = au_string_builder_into_string(&builder);
        au_value_t newstr_value = au_value_string(newstr);
        au_obj_array_push(array, newstr_value);
    }

    return au_value_struct((struct au_struct *)array);
fail:
    au_value_deref(value);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_str_char) {
    const au_value_t char_value = _args[0];
    if (au_value_get_type(char_value) != AU_VALUE_INT)
        goto fail;
    const int32_t char_point = au_value_get_int(char_value);

    // FIXME: support Unicode characters

    struct au_string_builder builder = {0};
    au_string_builder_init(&builder);
    au_string_builder_add(&builder, (char)char_point);
    return au_value_string(au_string_builder_into_string(&builder));
fail:
    au_value_deref(char_value);
    return au_value_none();
}
