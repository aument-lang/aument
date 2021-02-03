// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <libgen.h>
#include <limits.h>

#include "program.h"
#include "bc.h"
#include "vm.h"
#include "c_comp.h"
#include "exception.h"
#include "mmap.h"
#include "tmpfile.h"

#define FLAG_GENERATE_C (1 << 0)
#define FLAG_DUMP_BYTECODE (1 << 1)
#define has_flag(FLAG, MASK) (((FLAG)&(MASK))!=0)

enum au_action {
    ACTION_BUILD,
    ACTION_RUN
};

int main(int argc, char **argv) {
    int flags = 0;

    char *action = NULL;
    char *input_file = NULL;
    char *output_file = NULL;
    for(int i = 1;i < argc; i++) {
        if(argv[i][0] == '-') {
            switch(argv[i][1]) {
                case 'c': {
                    flags |= FLAG_GENERATE_C;
                    break;
                }
                case 'b': {
                    flags |= FLAG_DUMP_BYTECODE;
                    break;
                }
                default: break;
            }
            continue;
        }
        if(!action) action = argv[i];
        else if(action && !input_file) input_file = argv[i];
        else if(action && input_file && !output_file) output_file = argv[i];
        else au_fatal("too many arguments\n");
    }

    if(!action) {
        au_fatal("no action\n");
    }
    if(!input_file) {
        au_fatal("no input file\n");
    }

    enum au_action action_id;
    if(strcmp(action, "run") == 0)
        action_id = ACTION_RUN;
    else if(strcmp(action, "build") == 0)
        action_id = ACTION_BUILD;
    else
        au_fatal("unknown action %s\n", action);

    struct au_mmap_info mmap;
    if(!au_mmap_read(input_file, &mmap))
        au_perror("mmap");

    struct au_program program;
    assert(au_parse(mmap.bytes, mmap.size, &program) == 1);
    au_mmap_close(&mmap);

    char *program_path = realpath(input_file, 0);
    program.data.cwd = dirname(program_path);
    if(has_flag(flags, FLAG_DUMP_BYTECODE))
        au_program_dbg(&program);
    
    if(action_id == ACTION_RUN) {
        struct au_vm_thread_local tl;

        au_vm_thread_local_init(&tl, &program.data);
        au_vm_exec_unverified_main(&tl, &program);

        au_program_del(&program);
        au_vm_thread_local_del(&tl);
    } else if(action_id == ACTION_BUILD) {
        if(has_flag(flags, FLAG_GENERATE_C)) {
            struct au_c_comp_state c_state = {
                .f = fopen(output_file, "w"),
            };
            au_c_comp(&c_state, &program);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);
        } else {
            char c_file[] = TMPFILE_TEMPLATE;
            int fd;
            if((fd = mkstemps(c_file, 2)) == -1)
                au_perror("cannot generate tmpnam");
            struct au_c_comp_state c_state = {
                .f = fdopen(fd, "w"),
            };
            au_c_comp(&c_state, &program);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);

            char *cc = getenv("CC");
            if(cc == 0) cc = "gcc";

            char *args[] = {
                cc,
                "-flto",
                "-O2",
                "-o", output_file,
                "-include", "src/au_string.h",
                "-include", "src/value.h",
                "-include", "src/array.h",
                c_file,
                "src/value.c",
                "src/au_string.c",
                NULL,
            };
            pid_t pid = fork();
            if(pid == -1) {
                au_perror("fork");
            } else if(pid > 0) {
                int status;
                waitpid(pid, &status, 0);
                unlink(c_file);
            } else {
                execvp(args[0],args);
            }
        }
    } 
    return 0;
}
