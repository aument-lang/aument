// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

struct au_program_data;
void au_install_stdlib(struct au_program_data *data);

// ** io.c **
/// [func-au] Read a string from standard input without
/// a newline character
/// @name input
/// @return string read from stdin
AU_EXTERN_FUNC_DECL(au_std_input);

// ** types.c **
/// [func-au] Converts an object into an integer.\n\n
/// * For *integer* inputs, the result is exactly the same as the input.\n
/// * For *string* inputs, the result is the base-10 conversion of the
/// string.\n
/// * For *boolean* inputs, the result is 1 if `true`, 0 if `false`.\n
/// * For all other inputs, the result is 0.
/// @name int
/// @param input Object to be converted into integer
/// @return The integer equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_int);

/// [func-au] Converts an object into a boolean\n\n
/// * For *integer* and *floating-point* inputs, the result is `true` when
/// the input is greater than 0.\n
/// * For *boolean* inputs, the result is exactly the same as the input.\n
/// * For *string* inputs, the result is `false` when the string is empty,
/// otherwise it's true.\n
/// * For all other inputs, the result is 0.
/// @name bool
/// @param input Object to be converted into a boolean
/// @return The boolean equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_bool);

/// [func-au] Converts an object into a string
/// @name str
/// @param input Object to be converted into string
/// @return The string equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_str);

/// [func-au] Gets the length of a collection
/// @name len
/// @param collection The collection (an array, string,...)
/// @return The length of the collection.
AU_EXTERN_FUNC_DECL(au_std_len);
