// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Checks if a value is an array
/// @name array::is
/// @param value the value you want to check
/// @return true if the value is an array, else, returns false
AU_EXTERN_FUNC_DECL(au_std_array_is);

/// [func-au] Repeats an element and stores that list of elements into an
/// array
/// @name array::repeat
/// @param element the element to be repeated
/// @param times the number of times the element is repeated
/// @return The array
AU_EXTERN_FUNC_DECL(au_std_array_repeat);

/// [func-au] Pushes an element into an array
/// @name array::push
/// @param array the array
/// @param element the element
/// @return The array
AU_EXTERN_FUNC_DECL(au_std_array_push);

/// [func-au] Pops an element from an array and returns it
/// @name array::pop
/// @param array array
/// @return The popped element from the array. If the array is empty, then
/// it returns nil.
AU_EXTERN_FUNC_DECL(au_std_array_pop);
