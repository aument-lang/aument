// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "spawn.h"

int au_spawn(struct au_str_array *array) {
    au_str_array_add(array, 0);

    pid_t pid = fork();
    if (pid == -1) {
        au_perror("fork");
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        array->len--;
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return -1;
    } else {
        execvp(au_str_array_at(array, 0), array->data);
        exit(1);
    }
}