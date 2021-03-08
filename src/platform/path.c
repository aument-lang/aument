// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <libgen.h>
#endif

#include "path.h"

int au_split_path(const char *path, char **file, char **wd) {
#ifdef _WIN32
    char *return_file = malloc(PATH_MAX);
    size_t size = GetFullPathNameA(path, PATH_MAX, return_file, 0);
    if (size == 0)
        goto fail;
    return_file = realloc(return_file, size);
    *file = return_file;

    char *return_wd = malloc(PATH_MAX);
    char return_wd_drive[PATH_MAX];
    if (_splitpath_s(return_file, return_wd_drive, PATH_MAX, return_wd,
                     PATH_MAX, 0, 0, 0, 0) != 0)
        goto fail;
    const size_t return_wd_len = strlen(return_wd);
    const size_t return_wd_drive_len = strlen(return_wd_drive);
    if (return_wd_len + return_wd_drive_len >= PATH_MAX)
        goto fail;
    memmove(&return_wd[return_wd_drive_len], return_wd, return_wd_len);
    memcpy(return_wd, return_wd_drive, return_wd_drive_len);
    return_wd[return_wd_len + return_wd_drive_len] = 0;
    *wd = return_wd;
#else
    char *abs_path_proc = realpath(path, 0);
    if (abs_path_proc == 0)
        goto fail;
    *file = strdup(abs_path_proc);
    *wd = dirname(abs_path_proc);
#endif
    return 1;

fail:
    free(*file);
    free(*wd);
    *file = 0;
    *wd = 0;
    return 0;
}
