// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <libgen.h>
#endif

#include "path.h"

void au_split_path(const char *path, char **file, char **wd) {
#ifdef _WIN32
	char *return_file = malloc(PATH_MAX);
	size_t size = GetFullPathNameA(path, PATH_MAX, return_file, 0);
	if(!size)
		return;
	return_file = realloc(return_file, size);
	*file = return_file;
	char *return_wd = malloc(PATH_MAX);
	_splitpath_s(return_file, 0, 0, return_wd, PATH_MAX, 0, 0, 0, 0);
	*wd = return_wd;
#else
    char *abs_path_proc = realpath(path, 0);
    *file = strdup(abs_path_proc);
    *wd = dirname(abs_path_proc);
#endif
}
