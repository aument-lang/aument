// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"

struct au_lib_func au_stdlib_funcs[] = {
    (struct au_lib_func){ .name = "input", .func = au_std_input },
    (struct au_lib_func){ 0 },
};
