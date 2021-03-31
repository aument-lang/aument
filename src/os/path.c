// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <libgen.h>
#include <limits.h>
#include <unistd.h>
#endif

#include "core/rt/malloc.h"
#include "path.h"

int au_split_path(const char *path, char **file, char **wd) {
#ifdef _WIN32
    char *return_file = au_data_malloc(PATH_MAX);
    size_t size = GetFullPathNameA(path, PATH_MAX, return_file, 0);
    if (size == 0)
        goto fail;
    return_file = au_data_realloc(return_file, size + 1);
    return_file[size] = 0;
    *file = return_file;

    char *return_wd = au_data_malloc(PATH_MAX);
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
    char abs_path_proc[PATH_MAX];
    if (realpath(path, abs_path_proc) == 0)
        goto fail;
    *file = au_data_strdup(abs_path_proc);
    char *abs_path_wd = dirname(abs_path_proc);
    *wd = au_data_strdup(abs_path_wd);
#endif
    return 1;

fail:
    au_data_free(*file);
    au_data_free(*wd);
    *file = 0;
    *wd = 0;
    return 0;
}

struct au_char_array au_binary_path() {
#ifdef _WIN32
    char buffer[PATH_MAX];
    if (GetModuleFileNameA(0, buffer, PATH_MAX) == 0) {
        goto fail;
    }
    char my_path[PATH_MAX];
    size_t size = GetFullPathNameA(buffer, PATH_MAX, my_path, 0);
    if (size == 0) {
        goto fail;
    }
    char my_drive[16] = {0};
    if (_splitpath_s(my_path, my_drive, sizeof(my_drive), my_path,
                     PATH_MAX, 0, 0, 0, 0) != 0) {
        goto fail;
    }
    const size_t my_path_len = strlen(my_path);
    const size_t my_drive_len = strlen(my_drive);
    if (my_path_len + my_drive_len > PATH_MAX) {
        goto fail;
    }
    memmove(&my_path[my_drive_len], my_path, my_path_len);
    memcpy(my_path, my_drive, strlen(my_drive));
    my_path[my_path_len + my_drive_len] = 0;
#else
    char buffer[PATH_MAX];
    if (readlink("/proc/self/exe", buffer, PATH_MAX) < 0)
        goto fail;
    char *my_path = dirname(buffer);
#endif
    const size_t ret_path_len = strlen(my_path);
    char *ret_path = au_data_malloc(ret_path_len + 1);
    memcpy(ret_path, my_path, ret_path_len);
    ret_path[ret_path_len] = 0;
    return (struct au_char_array){
        .data = ret_path,
        .cap = ret_path_len,
        .len = ret_path_len,
    };
fail:
    return (struct au_char_array){0};
}
