// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform/mmap.h"
#include "platform/tmpfile.h"

#include "core/bc.h"
#include "core/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/vm.h"

#ifdef FEAT_COMPILER
#include <libgen.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "compiler/c_comp.h"
#endif

#ifdef _WIN32
#else
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#endif

#define FLAG_GENERATE_C (1 << 0)
#define FLAG_DUMP_BYTECODE (1 << 1)
#define has_flag(FLAG, MASK) (((FLAG) & (MASK)) != 0)

#include "help.h"
#include "version.h"

enum au_action { ACTION_BUILD, ACTION_RUN };

int main(int argc, char **argv) {
    int flags = 0;

    char *action = NULL;
    char *input_file = NULL;
    char *output_file = NULL;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'c': {
                flags |= FLAG_GENERATE_C;
                break;
            }
            case 'b': {
                flags |= FLAG_DUMP_BYTECODE;
                break;
            }
            default:
                break;
            }
            continue;
        }
        if (!action)
            action = argv[i];
        else if (action && !input_file)
            input_file = argv[i];
        else if (action && input_file && !output_file)
            output_file = argv[i];
        else
            au_fatal("too many arguments\n");
    }

    if (!action) {
        fputs(AU_HELP_MAIN, stdout);
        return 0;
    }

    enum au_action action_id = ACTION_RUN;

    if (strcmp(action, "run") == 0) {
        action_id = ACTION_RUN;
        if (!input_file) {
            au_fatal("no input file\n");
        }
    } else if (strcmp(action, "build") == 0) {
#ifdef FEAT_COMPILER
        action_id = ACTION_BUILD;
#else
        au_fatal("the compiler feature isn't enabled");
#endif
        if (!input_file) {
            au_fatal("no input file\n");
        }
    } else if (strcmp(action, "help") == 0) {
        if (!input_file) {
            fputs(AU_HELP_MAIN, stdout);
        } else {
            fputs(help_text(input_file), stdout);
        }
        return 0;
    } else if (strcmp(action, "version") == 0) {
        fputs("aulang " AU_VERSION "\n", stdout);
        return 0;
    } else {
        fputs(AU_HELP_MAIN, stdout);
        return 0;
    }

    struct au_mmap_info mmap;
    if (!au_mmap_read(input_file, &mmap))
        au_perror("mmap");

    struct au_program program;
    assert(au_parse(mmap.bytes, mmap.size, &program) == 1);
    au_mmap_del(&mmap);

#ifdef _WIN32
    {
        char program_path[_MAX_DIR];
        _splitpath_s(input_file, 0, 0, program_path, _MAX_DIR, 0, 0, 0, 0);
        program.data.cwd = strdup(program_path);
    }
#else
    {
        char *program_path = realpath(input_file, 0);
        program.data.cwd = dirname(program_path);
    }
#endif

    if (has_flag(flags, FLAG_DUMP_BYTECODE))
        au_program_dbg(&program);

    if (action_id == ACTION_RUN) {
        struct au_vm_thread_local tl;
        au_vm_thread_local_init(&tl, &program.data);
        au_vm_exec_unverified_main(&tl, &program);

        au_program_del(&program);
        au_vm_thread_local_del(&tl);
    }
#ifdef FEAT_COMPILER
    else if (action_id == ACTION_BUILD) {
        if (has_flag(flags, FLAG_GENERATE_C)) {
            struct au_c_comp_state c_state = {
                .as.f = fopen(output_file, "w"),
                .type = AU_C_COMP_FILE,
            };
            au_c_comp(&c_state, &program);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);
        } else {
            char c_file[] = TMPFILE_TEMPLATE;
            int fd;
            if ((fd = mkstemps(c_file, 2)) == -1)
                au_perror("cannot generate tmpnam");
            struct au_c_comp_state c_state = {
                .as.f = fdopen(fd, "w"),
                .type = AU_C_COMP_FILE,
            };
            au_c_comp(&c_state, &program);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);

            char *cc = getenv("CC");
            if (cc == 0)
                cc = "gcc";

            char *args[] = {
                cc, "-flto", "-O2", "-o", output_file, c_file, NULL,
            };
            pid_t pid = fork();
            if (pid == -1) {
                au_perror("fork");
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                unlink(c_file);
                return status;
            } else {
                execvp(args[0], args);
                return 1;
            }
        }
    }
#endif
    return 0;
}
