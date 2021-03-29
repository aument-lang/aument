// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/au_array.h"
#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/utf8.h"
#include "core/vm/vm.h"
#include "lib/string_builder.h"

// ** Implementation **

AU_EXTERN_FUNC_DECL(au_std_str_is) {
    const au_value_t value = _args[0];
    const au_value_t retval =
        au_value_bool(au_value_get_type(value) == AU_VALUE_STR);
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

AU_EXTERN_FUNC_DECL(au_std_str_ord) {
    const au_value_t str_value = _args[0];
    if (au_value_get_type(str_value) != AU_VALUE_STR)
        goto fail;
    struct au_string *str = au_value_get_string(str_value);
    int32_t out = 0;
    if (utf8_codepoint(str->data, str->len, &out) == 0)
        goto fail;
    au_value_deref(str_value);
    return au_value_int(out);
fail:
    au_value_deref(str_value);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_str_bytes) {
    const au_value_t value = _args[0];
    if (au_value_get_type(value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *str = au_value_get_string(value);
    struct au_obj_array *array = au_obj_array_new(str->len);
    for (size_t i = 0; i < str->len; i++) {
        au_obj_array_push(array, au_value_int(str->data[i]));
    }
    return au_value_struct((struct au_struct *)array);
fail:
    au_value_deref(value);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_str_code_points) {
    const au_value_t value = _args[0];

    if (au_value_get_type(value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *str = au_value_get_string(value);

    struct au_obj_array *array = au_obj_array_new(1);
    const char *str_cur = &str->data[0];
    int str_cur_size = 0;
    const char *str_next = 0;

    const char *str_max = &str->data[str->len];

    while ((str_next = utf8_next(str_cur, str_max, &str_cur_size)) != 0) {
        struct au_string_builder builder = {0};
        au_string_builder_init(&builder);
        for (int i = 0; i < str_cur_size; i++) {
            au_string_builder_add(&builder, str_cur[i]);
        }
        struct au_string *newstr = au_string_builder_into_string(&builder);
        au_value_t newstr_value = au_value_string(newstr);
        au_obj_array_push(array, newstr_value);
        str_cur = str_next;
    }

    return au_value_struct((struct au_struct *)array);
fail:
    au_value_deref(value);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_str_index_of) {
    au_value_t haystack_value = au_value_none();
    au_value_t needle_value = au_value_none();

    haystack_value = _args[0];
    if (au_value_get_type(haystack_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *haystack = au_value_get_string(haystack_value);

    needle_value = _args[1];
    if (au_value_get_type(needle_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *needle = au_value_get_string(needle_value);

    const char *found =
        utf8_str(haystack->data, &haystack->data[haystack->len],
                 needle->data, &needle->data[needle->len]);

    if (found == 0) {
        goto not_found;
    } else {
        au_value_deref(haystack_value);
        au_value_deref(needle_value);
        uintptr_t haystack_intptr = (uintptr_t)(haystack->data);
        uintptr_t found_intptr = (uintptr_t)found;
        return au_value_int(found_intptr - haystack_intptr);
    }
not_found:
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_int(-1);
}

AU_EXTERN_FUNC_DECL(au_std_str_contains) {
    au_value_t haystack_value = au_value_none();
    au_value_t needle_value = au_value_none();

    haystack_value = _args[0];
    if (au_value_get_type(haystack_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *haystack = au_value_get_string(haystack_value);

    needle_value = _args[1];
    if (au_value_get_type(needle_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *needle = au_value_get_string(needle_value);

    const char *found =
        utf8_str(haystack->data, &haystack->data[haystack->len],
                 needle->data, &needle->data[needle->len]);

    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(found != 0);
fail:
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(0);
}

AU_EXTERN_FUNC_DECL(au_std_str_starts_with) {
    au_value_t haystack_value = au_value_none();
    au_value_t needle_value = au_value_none();

    haystack_value = _args[0];
    if (au_value_get_type(haystack_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *haystack = au_value_get_string(haystack_value);

    needle_value = _args[1];
    if (au_value_get_type(needle_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *needle = au_value_get_string(needle_value);

    if (needle->len > haystack->len) {
        goto not_found;
    }

    int found = memcmp(haystack->data, needle->data, needle->len) == 0;
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(found);
not_found:
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(0);
}

AU_EXTERN_FUNC_DECL(au_std_str_ends_with) {
    au_value_t haystack_value = au_value_none();
    au_value_t needle_value = au_value_none();

    haystack_value = _args[0];
    if (au_value_get_type(haystack_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *haystack = au_value_get_string(haystack_value);

    needle_value = _args[1];
    if (au_value_get_type(needle_value) != AU_VALUE_STR)
        goto not_found;
    const struct au_string *needle = au_value_get_string(needle_value);

    if (needle->len > haystack->len) {
        goto not_found;
    }

    int found = memcmp(&haystack->data[haystack->len - needle->len],
                       needle->data, needle->len) == 0;
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(found);
not_found:
    au_value_deref(haystack_value);
    au_value_deref(needle_value);
    return au_value_bool(0);
}

#define CHAR_TYPE_FN(NAME, COND)                                          \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t str_value = _args[0];                            \
        const struct au_string *str = 0;                                  \
        if (au_value_get_type(str_value) != AU_VALUE_STR)                 \
            goto is_false;                                                \
        str = au_value_get_string(str_value);                             \
        if (str->len == 0)                                                \
            goto is_false;                                                \
        if ((COND)) {                                                     \
            au_value_deref(str_value);                                    \
            return au_value_bool(1);                                      \
        }                                                                 \
is_false:                                                                 \
        au_value_deref(str_value);                                        \
        return au_value_bool(0);                                          \
    }

CHAR_TYPE_FN(au_std_str_is_space,
             (str->data[0] == ' ' || str->data[0] == '\t' ||
              str->data[0] == '\r' || str->data[0] == '\n'))

CHAR_TYPE_FN(au_std_str_is_digit,
             ('0' <= str->data[0] && str->data[0] <= '9'))
