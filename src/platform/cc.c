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
#include <sys/wait.h>
#include <unistd.h>

#include "spawn.h"
#include "cc.h"

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
            // TODO: look up standard library file
            char buffer[BUFSIZ];
            readlink("/proc/self/exe", buffer, BUFSIZ);
            char *my_path = dirname(buffer);
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
}