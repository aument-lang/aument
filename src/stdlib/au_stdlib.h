// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/extern_fn.h"

extern struct au_lib_func au_stdlib_funcs[];

struct au_program_data;
void au_install_stdlib(struct au_program_data *data);

// IO
AU_EXTERN_FUNC_DECL(au_std_input);
