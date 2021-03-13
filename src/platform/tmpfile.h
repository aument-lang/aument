// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdio.h>

struct au_tmpfile {
    FILE *f;
    char *path;
};

/// [func] Closes an au_tmpfile instance
/// @param tmp instance to be closed
void au_tmpfile_close(struct au_tmpfile *tmp);

/// [func] Deinitializes an au_tmpfile instance
/// @param tmp instance to be deinitialized
void au_tmpfile_del(struct au_tmpfile *tmp);

/// [func] Creates a new temporary file with the `.c` extension
int au_tmpfile_new(struct au_tmpfile *tmp);

/// [func] Creates an empty executable file
int au_tmpfile_exec(struct au_tmpfile *tmp);
