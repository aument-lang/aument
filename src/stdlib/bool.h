// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Checks if a value is a boolean
/// @name bool::is
/// @param value the value you want to check
/// @return true if the value is a boolean, else, returns false
AU_EXTERN_FUNC_DECL(au_std_bool_is);

/// [func-au] Converts an object into a boolean\n\n
/// * For *integer* and *floating-point* inputs, the result is `true` when
/// the input is greater than 0.\n
/// * For *boolean* inputs, the result is exactly the same as the input.\n
/// * For *string* inputs, the result is `false` when the string is empty,
/// otherwise it's true.\n
/// * For all other inputs, the result is 0.
/// @name bool::into
/// @param input Object to be converted into a boolean
/// @return The boolean equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_bool_into);
