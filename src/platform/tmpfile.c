// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "tmpfile.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef _WIN32
#include <fcntl.h>
#include <time.h>
#include <windows.h>
#endif

void au_tmpfile_del(struct au_tmpfile *tmp) {
    if (tmp->f != 0)
        fclose(tmp->f);
    free(tmp->path);
}

#ifdef _WIN32
#define TMPFILE_PREFIX "au-"
#define TMPFILE_RAND_CHARS 6

const char rand_chars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int new_tmpfile(_Unused struct au_tmpfile *tmp,
                       _Unused const char *ext) {
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
    while (tries < 128) {
        for (int i = 0; i < TMPFILE_RAND_CHARS; i++) {
            name[len + i] = rand_chars[rand() % sizeof(rand_chars)];
        }
        len += TMPFILE_RAND_CHARS;
        strcat(&name[len], ext);
        HANDLE hfile =
            CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (hfile == INVALID_HANDLE_VALUE) {
            tries++;
            continue;
        }

        int fd = _open_osfhandle((intptr_t)hfile, _O_TEXT);
        tmp->f = _fdopen(fd, "w");
        tmp->path = strdup(name);
        printf("FILE %s\n", tmp->path);
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
#define TMPFILE_TEMPLATE "/tmp/au-XXXXXX.c"
#define TMPFILE_TEMPLATE_EXEC "/tmp/au-XXXXXX"

int au_tmpfile_new(struct au_tmpfile *tmp) {
    char c_file[] = TMPFILE_TEMPLATE;
    int fd;
    if ((fd = mkstemps(c_file, 2)) == -1)
        return 0;
    *tmp = (struct au_tmpfile){
        .f = fdopen(fd, "w"),
        .path = strdup(c_file),
    };
    return 1;
}

int au_tmpfile_exec(struct au_tmpfile *tmp) {
    char exe_file[] = TMPFILE_TEMPLATE_EXEC;
    int fd;
    if ((fd = mkstemps(exe_file, 0)) == -1)
        return 0;
    close(fd);
    *tmp = (struct au_tmpfile){
        .f = 0,
        .path = strdup(exe_file),
    };
    return 1;
}
#endif