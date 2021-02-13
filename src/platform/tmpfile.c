// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "tmpfile.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TMPFILE_TEMPLATE "/tmp/file-XXXXXX.c"
#define TMPFILE_TEMPLATE_EXEC "/tmp/file-XXXXXX"

void au_tmpfile_del(struct au_tmpfile *tmp) {
    if (tmp->f != 0)
        fclose(tmp->f);
    free(tmp->path);
}

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
    char c_file[] = TMPFILE_TEMPLATE_EXEC;
    int fd;
    if ((fd = mkstemps(c_file, 0)) == -1)
        return 0;
    close(fd);
    *tmp = (struct au_tmpfile){
        .f = 0,
        .path = strdup(c_file),
    };
    return 1;
}