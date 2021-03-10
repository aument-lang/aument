// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Gets the length of a collection\n\n
/// * For structures (arrays), calling len will use the structure's
/// internal length function.\n
/// * For strings, it returns the number of UTF-8 codepoints in the
/// string\n
/// * For all other inputs, the result is 0.
/// @name len
/// @param collection The collection (an array, string,...)
/// @return The length of the collection.
AU_EXTERN_FUNC_DECL(au_std_len);