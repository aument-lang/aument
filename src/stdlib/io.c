// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/extern_fn.h"
#include "core/value.h"

AU_EXTERN_FUNC_DECL(au_std_input) {
    int ch = -1;
    struct au_string *header = malloc(sizeof(struct au_string) + 1);
    header->rc = 1;
    header->len = 1;
    uint32_t pos = 0, cap = 1;
    while((ch = fgetc(stdin)) != EOF) {
        if(ch == '\n') break;
        if(pos == cap) {
            cap *= 2;
            header = realloc(header, sizeof(struct au_string) + cap);
        }
        header->data[pos] = ch;
        pos++;
    }
    header->len = pos;
    return au_value_string(header);
}