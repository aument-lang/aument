// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "../core/str_array.h"

struct au_cc_options {
    struct au_str_array cflags;
    int use_stdlib;
    char *_stdlib_cache;
};

void au_cc_options_default(struct au_cc_options *cc);
void au_cc_options_del(struct au_cc_options *cc);

char *au_get_cc();
int au_spawn_cc(struct au_cc_options *cc, char *output_file,
                char *input_file);