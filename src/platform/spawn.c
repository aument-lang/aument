// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <sys/types.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#endif

#include "spawn.h"

int au_spawn(struct au_str_array *array) {
    au_str_array_add(array, 0);
#ifdef _WIN32
    return (int)_spawnvp(P_WAIT, au_str_array_at(array, 0),
                         (const char *const *)array->data);
#else
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
#endif
}