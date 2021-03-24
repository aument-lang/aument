// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Splits a string into an array of characters
/// @name str
/// @param input Object to split
/// @return The array of characters inside the string
AU_EXTERN_FUNC_DECL(au_std_str_chars);

/// [func-au] Convert an integer-typed Unicode code point into a string.
/// @name str::char
/// @param input Object to be converted into string
/// @return The string equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_str_char);
