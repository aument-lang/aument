// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdio.h>

#include "core/bc.h"
#include "core/char_array.h"
#include "core/str_array.h"

struct au_c_comp_state {
    struct au_char_array str;
};

/// [func] Deinitializes an au_c_comp_state instance
/// @param state instance to be initialized
void au_c_comp_state_del(struct au_c_comp_state *state);

struct au_program;

struct au_c_comp_options {
    int with_debug;
};

/// [func] Compiles a program into file specified by
/// an au_c_comp_state instance
void au_c_comp(struct au_c_comp_state *state,
               const struct au_program *program,
               struct au_c_comp_options options);
