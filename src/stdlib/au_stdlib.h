// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/extern_fn.h"

extern struct au_lib_func au_stdlib_funcs[];

// IO
au_value_t au_std_input(const au_value_t *args, const size_t n_args);
