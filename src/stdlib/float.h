// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Checks if a value is a float
/// @name float::is
/// @param value the value you want to check
/// @return true if the value is an float, else, returns false
AU_EXTERN_FUNC_DECL(au_std_float_is);

/// [func-au] Converts an object into a float.\n\n
/// * For *int* inputs, the result is its float equivalent.\n
/// * For *float* inputs, the result is exactly the same as the input.\n
/// * For *string* inputs, the result is the base-10 conversion of the
/// string.\n
/// * For *boolean* inputs, the result is 1.0 if `true`, 0.0 if `false`.\n
/// * For all other inputs, the result is 0.0.
/// @name float::into
/// @param input Object to be converted into float
/// @return The floating-point equivalent of the `input` object.
AU_EXTERN_FUNC_DECL(au_std_float_into);
