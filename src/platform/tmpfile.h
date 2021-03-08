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

void au_tmpfile_del(struct au_tmpfile *);
int au_tmpfile_new(struct au_tmpfile *tmp);
int au_tmpfile_exec(struct au_tmpfile *tmp);
