// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdio.h>

#include "core/bc.h"
#include "core/str_array.h"

struct au_c_comp_state {
    FILE *f;
};
void au_c_comp_state_del(struct au_c_comp_state *state);

struct au_program;
void au_c_comp(struct au_c_comp_state *state,
               const struct au_program *program);
