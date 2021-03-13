// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

/// [func] Splits a path into file and directory components
int au_split_path(const char *path, char **file, char **wd);