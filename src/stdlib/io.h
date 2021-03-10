// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Read a string from standard input without
/// a newline character
/// @name input
/// @return string read from stdin
AU_EXTERN_FUNC_DECL(au_std_input);

/// [func-au] Opens a file
/// @name io::open
/// @param file path of file to be opened
/// @param perm fopen permissions
/// @return file object
AU_EXTERN_FUNC_DECL(au_std_io_open);

/// [func-au] Closes a file
/// @name io::close
/// @param file file object to be closed
AU_EXTERN_FUNC_DECL(au_std_io_close);