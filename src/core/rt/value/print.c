// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "main.h"
#include <stdio.h>
#endif

void au_value_print(au_value_t value) {
    switch (au_value_get_type(value)) {
    case VALUE_INT: {
        printf("%d", au_value_get_int(value));
        break;
    }
    case VALUE_DOUBLE: {
        printf("%g", au_value_get_double(value));
        break;
    }
    case VALUE_BOOL: {
        printf("%s", au_value_get_bool(value) ? "(true)" : "(false)");
        break;
    }
    case VALUE_STR: {
        struct au_string *str = au_value_get_string(value);
        printf("%.*s", str->len, str->data);
        break;
    }
    default: {
        printf("(unknown)");
        break;
    }
    }
}