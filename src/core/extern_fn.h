// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "value.h"

typedef au_value_t (*au_extern_func_t)(const au_value_t *args, const size_t n_args);

struct au_lib_func {
    const char *name;
    au_extern_func_t func;
};
