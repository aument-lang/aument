// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <libgen.h>
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "core/rt/malloc.h"

#include "cc.h"
#include "path.h"
#include "spawn.h"

void au_cc_options_default(struct au_cc_options *cc) {
    *cc = (struct au_cc_options){0};
    cc->use_stdlib = 1;
}

void au_cc_options_del(struct au_cc_options *cc) {
    au_data_free(cc->_stdlib_cache);
    au_data_free(cc->cflags.data);
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
            struct au_char_array bin_path_array = au_binary_path();
            if (bin_path_array.len == 0)
                goto fail;

            char *stdlib_cache = bin_path_array.data;
            const size_t stdlib_cache_len =
                bin_path_array.len + strlen(au_lib_file) + 1;
            stdlib_cache = au_data_realloc(stdlib_cache, stdlib_cache_len);
            memcpy(&stdlib_cache[bin_path_array.len], au_lib_file,
                   strlen(au_lib_file));
            stdlib_cache[stdlib_cache_len - 1] = 0;

            cc->_stdlib_cache = stdlib_cache;
        }
        if (cc->loads_dl) {
#ifdef _WIN32
            au_str_array_add(&args, cc->_stdlib_cache);
#else
            au_str_array_add(&args, "-rdynamic");
            au_str_array_add(&args, "-Wl,--whole-archive");
            au_str_array_add(&args, cc->_stdlib_cache);
            au_str_array_add(&args, "-Wl,--no-whole-archive");
            au_str_array_add(&args, "-Wl,-rpath,$ORIGIN");
#endif
        } else {
            au_str_array_add(&args, cc->_stdlib_cache);
        }
    }

    for (size_t i = 0; i < cc->ldflags.len; i++)
        au_str_array_add(&args, cc->ldflags.data[i]);

    int retval = au_spawn(&args);
    au_data_free(args.data);
    return retval;

fail:
    au_data_free(args.data);
    return 1;
}