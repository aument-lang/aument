// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "path.h"

void au_split_path(const char *path, char **file, char **wd) {
    char *abs_path_proc = realpath(path, 0);
    *file = strdup(abs_path_proc);
    *wd = dirname(abs_path_proc);
}