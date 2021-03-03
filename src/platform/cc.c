// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

#include "cc.h"
#include "spawn.h"

void au_cc_options_default(struct au_cc_options *cc) {
    *cc = (struct au_cc_options){0};
    cc->use_stdlib = 1;
}

void au_cc_options_del(struct au_cc_options *cc) {
    free(cc->_stdlib_cache);
    free(cc->cflags.data);
}

char *au_get_cc() {
    char *cc = getenv("CC");
    if (cc == 0)
        cc = "gcc";
    return cc;
}

static const char au_lib_file[] = "/libau_runtime.a";

int au_spawn_cc(struct au_cc_options *cc, char *output_file,
                char *input_file) {
    struct au_str_array args = {0};

    char *cc_exec = au_get_cc();
    if (!cc_exec)
        return 1;
    au_str_array_add(&args, cc_exec);

    for (size_t i = 0; i < cc->cflags.len; i++)
        au_str_array_add(&args, cc->cflags.data[i]);

    au_str_array_add(&args, "-o");
    au_str_array_add(&args, output_file);
    au_str_array_add(&args, input_file);

    if (cc->use_stdlib) {
        if (!cc->_stdlib_cache) {
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
            char buffer[BUFSIZ];
            if (readlink("/proc/self/exe", buffer, BUFSIZ) < 0)
                goto fail;
            char *my_path = dirname(buffer);
#endif
            size_t my_len = strlen(my_path);
            size_t stdlib_cache_len = my_len + sizeof(au_lib_file);
            cc->_stdlib_cache = malloc(stdlib_cache_len + 1);
            snprintf(cc->_stdlib_cache, stdlib_cache_len, "%s%s", my_path,
                     au_lib_file);
            cc->_stdlib_cache[stdlib_cache_len] = 0;
        }
        au_str_array_add(&args, cc->_stdlib_cache);
    }

    int retval = au_spawn(&args);
    free(args.data);
    return retval;

fail:
    free(args.data);
    return 1;
}