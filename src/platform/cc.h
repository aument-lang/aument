// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "../core/str_array.h"

struct au_cc_options {
    struct au_str_array cflags;
    struct au_str_array ldflags;
    char *_stdlib_cache;
    int use_stdlib;
    int loads_dl;
};

/// Initializes an au_cc_options instance with default parameters
/// @param cc instance to be initialized
void au_cc_options_default(struct au_cc_options *cc);

/// Deinitializes an au_cc_options instance
/// @param cc instance to be destroyed
void au_cc_options_del(struct au_cc_options *cc);

/// Returns the path of the C compiler
char *au_get_cc();

/// Spawns a C compiler with specified options, input and output.
/// @param cc options passed into the compiler
/// @param output_file path to the output file
/// @param input_file path to the input file
/// @return exit code of the C compiler
int au_spawn_cc(struct au_cc_options *cc, char *output_file,
                char *input_file);