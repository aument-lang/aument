// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "core/char_array.h"
#include "platform.h"

/// [func] Splits a path into file and directory components
/// @param path the path to be split
/// @param file output pointer to the file component
/// @param wd output pointer to the directory component
/// @return 1 if successful, 0 if failed
AU_PRIVATE int au_split_path(const char *path, char **file, char **wd);

/// [func] Gets the path containing the currently running Aument executable
AU_PRIVATE struct au_char_array au_binary_path();
