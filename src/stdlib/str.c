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
