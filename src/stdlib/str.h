// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Checks if an object is a string
/// @name str::is
/// @param object Object to check
/// @return True if this object is a string, otherwise false.
AU_EXTERN_FUNC_DECL(au_std_str_is);

/// [func-au] Converts an object into a string
/// @name str::into
/// @param input Object to be converted into string
/// @return The string equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_str_into);

/// [func-au] Convert an integer-typed Unicode code point into a string.
/// @name str::char
/// @param input Object to be converted into string
/// @return The string equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_str_char);

/// [func-au] Splits a string into an array of UTF-8 code points
/// @name str::code_points
/// @param input Object to split
/// @return The array of code points inside the string
AU_EXTERN_FUNC_DECL(au_std_str_code_points);

/// [func-au] Splits a string into an array of integer bytes
/// @name str::bytes
/// @param input Object to split
/// @return The array of code points inside the string
AU_EXTERN_FUNC_DECL(au_std_str_bytes);

AU_EXTERN_FUNC_DECL(au_std_str_index_of);

AU_EXTERN_FUNC_DECL(au_std_str_contains);

AU_EXTERN_FUNC_DECL(au_std_str_starts_with);

AU_EXTERN_FUNC_DECL(au_std_str_ends_with);

AU_EXTERN_FUNC_DECL(au_std_str_is_space);

AU_EXTERN_FUNC_DECL(au_std_str_is_digit);
