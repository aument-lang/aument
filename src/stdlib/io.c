// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/malloc.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_input) {
    int ch = -1;
    struct au_string *header =
        au_obj_malloc(sizeof(struct au_string) + 1, 0);
    header->rc = 1;
    header->len = 1;
    uint32_t pos = 0, cap = 1;
    while ((ch = fgetc(stdin)) != EOF) {
        if (ch == '\n')
            break;
        if (pos == cap) {
            cap *= 2;
            header =
                au_obj_realloc(header, sizeof(struct au_string) + cap);
        }
        header->data[pos] = ch;
        pos++;
    }
    header->len = pos;
    return au_value_string(header);
}