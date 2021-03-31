// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "tmpfile.h"
#include "core/rt/malloc.h"
#include "platform/platform.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <fcntl.h>
#include <time.h>
#include <windows.h>
#endif

void au_tmpfile_close(struct au_tmpfile *tmp) {
    if (tmp->f != 0) {
        fclose(tmp->f);
        tmp->f = 0;
    }
}

void au_tmpfile_del(struct au_tmpfile *tmp) {
    au_tmpfile_close(tmp);
#ifdef _WIN32
    DeleteFileA(tmp->path);
#else
    unlink(tmp->path);
#endif
    au_data_free(tmp->path);
}

#ifdef _WIN32
#define TMPFILE_PREFIX "au-"
#define TMPFILE_RAND_CHARS 6

const char rand_chars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int new_tmpfile(AU_UNUSED struct au_tmpfile *tmp,
                       AU_UNUSED const char *ext) {
    char name[MAX_PATH] = {0};
    if (GetTempPathA(MAX_PATH, name) == 0)
        return 0;

    size_t len = strlen(name);
    if ((len + sizeof(TMPFILE_PREFIX) + TMPFILE_RAND_CHARS +
         strlen(ext)) >= MAX_PATH)
        return 0;

    memcpy(&name[len], TMPFILE_PREFIX, sizeof(TMPFILE_PREFIX));
    len += sizeof(TMPFILE_PREFIX) - 1;

    srand(time(0));
    int tries = 0;
    const size_t ext_len = strlen(ext);
    while (tries < 128) {
        for (int i = 0; i < TMPFILE_RAND_CHARS; i++) {
            name[len + i] = rand_chars[rand() % sizeof(rand_chars)];
        }
        len += TMPFILE_RAND_CHARS;
        memcpy(&name[len], ext, ext_len);
        HANDLE hfile =
            CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE) {
            tries++;
            continue;
        }

        int fd = _open_osfhandle((intptr_t)hfile, _O_TEXT);
        tmp->f = _fdopen(fd, "w");
        tmp->path = au_data_strdup(name);
        return 1;
    }

    return 0;
}

int au_tmpfile_new(struct au_tmpfile *tmp) {
    return new_tmpfile(tmp, ".c");
}

int au_tmpfile_exec(struct au_tmpfile *tmp) {
    return new_tmpfile(tmp, ".exe");
}
#else
#define TMPFILE_TEMPLATE "/tmp/au-XXXXXX"

static int new_tmpfile(struct au_tmpfile *tmp, const char *ext) {
    const size_t ext_len = strlen(ext);

    const size_t c_file_len = strlen(TMPFILE_TEMPLATE) + ext_len;
    char c_file[c_file_len + 1];
    memcpy(c_file, TMPFILE_TEMPLATE, strlen(TMPFILE_TEMPLATE));
    memcpy(&c_file[strlen(TMPFILE_TEMPLATE)], ext, ext_len);
    c_file[c_file_len] = 0;

    int fd;
    if ((fd = mkstemps(c_file, ext_len)) == -1)
        return 0;
    *tmp = (struct au_tmpfile){
        .f = fdopen(fd, "w"),
        .path = au_data_strdup(c_file),
    };
    return 1;
}

int au_tmpfile_new(struct au_tmpfile *tmp) {
    return new_tmpfile(tmp, ".c");
}

int au_tmpfile_exec(struct au_tmpfile *tmp) {
    return new_tmpfile(tmp, "");
}
#endif
