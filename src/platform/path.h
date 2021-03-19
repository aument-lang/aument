// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "platform.h"
#include "core/char_array.h"

/// [func] Splits a path into file and directory components
_Private int au_split_path(const char *path, char **file, char **wd);

/// [func] Get the path that contains the running Aument executable
_Private struct au_char_array au_binary_path();
