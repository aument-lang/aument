// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/au_struct.h"
#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_len) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case VALUE_STRUCT: {
        struct au_struct *s = au_value_get_struct(value);
        const struct au_struct_vdata *vdata = s->vdata;
        if (vdata->len_fn != 0)
            return au_value_int(vdata->len_fn(s));
        return au_value_int(0);
    }
    case VALUE_STR: {
        struct au_string *s = au_value_get_string(value);
        int32_t utf8_len = 0;
        for (size_t i = 0; i < s->len;) {
            uint8_t utf8_char = (uint8_t)s->data[i];
            if (utf8_char >= 0xE0) {
                utf8_len++;
                i += 4;
            } else if (utf8_char >= 0xC0) {
                utf8_len++;
                i += 3;
            } else if (utf8_char >= 0x80) {
                utf8_len++;
                i += 2;
            } else {
                utf8_len++;
                i++;
            }
        }
        return au_value_int(utf8_len);
    }
    default: {
        return au_value_int(0);
    }
    }
}