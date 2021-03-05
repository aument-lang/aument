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
#include "platform/path.h"
#include "platform/tmpfile.h"

#include "core/bc.h"
#include "core/parser/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "core/vm/vm.h"

#ifdef AU_FEAT_COMPILER
#include "compiler/c_comp.h"
#include "platform/cc.h"
#endif

#ifndef _WIN32
#include <libgen.h>
#include <signal.h>
#include <unistd.h>
#endif

#define FLAG_GENERATE_C (1 << 0)
#define FLAG_DUMP_BYTECODE (1 << 1)
#define FLAG_GENERATE_DEBUG (1 << 2)
#define has_flag(FLAG, MASK) (((FLAG) & (MASK)) != 0)

#include "core/int_error/error_printer.h"

#include "help.h"
#include "version.h"

enum au_action { ACTION_BUILD, ACTION_RUN };

int main(int argc, char **argv) {
    uint32_t flags = 0;

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
            case 'g': {
                flags |= FLAG_GENERATE_DEBUG;
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
#ifdef AU_FEAT_COMPILER
        action_id = ACTION_BUILD;
#else
        au_fatal("the compiler feature isn't enabled");
#endif
        if (!input_file) {
            au_fatal("no input file\n");
        }
        if (!output_file) {
            au_fatal("no output file\n");
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
    struct au_parser_result parse_res =
        au_parse(mmap.bytes, mmap.size, &program);
    if (parse_res.type != AU_PARSER_RES_OK) {
        au_print_parser_error(parse_res, (struct au_error_location){
                                             .src = mmap.bytes,
                                             .len = mmap.size,
                                             .path = input_file,
                                         });
        au_mmap_del(&mmap);
        return 1;
    }
    au_mmap_del(&mmap);

    program.data.file = 0;
    program.data.cwd = 0;
    if (!au_split_path(input_file, &program.data.file, &program.data.cwd))
        au_perror("au_split_path");

    if (has_flag(flags, FLAG_DUMP_BYTECODE))
        au_program_dbg(&program);

    if (action_id == ACTION_RUN) {
        au_obj_malloc_init();

        struct au_vm_thread_local tl;
        au_vm_thread_local_init(&tl, &program.data);
        au_vm_thread_local_set(&tl);

        au_vm_exec_unverified_main(&tl, &program);

#ifdef AU_FEAT_DELAYED_RC
        au_vm_thread_local_del_const_cache(&tl);
        au_obj_malloc_collect();
#endif

        au_program_del(&program);
        au_vm_thread_local_del(&tl);
        au_vm_thread_local_set(0);
    }
#ifdef AU_FEAT_COMPILER
    else if (action_id == ACTION_BUILD) {
        struct au_c_comp_options options = {0};
        options.with_debug = has_flag(flags, FLAG_GENERATE_DEBUG);
        if (has_flag(flags, FLAG_GENERATE_C)) {
            struct au_c_comp_state c_state = {
                .as.f = fopen(output_file, "w"),
                .type = AU_C_COMP_FILE,
            };
            au_c_comp(&c_state, &program, options);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);
        } else {
            struct au_tmpfile tmp;
            if (!au_tmpfile_new(&tmp))
                au_perror("unable to create tmpfile");
            struct au_c_comp_state c_state = {
                .as.f = tmp.f,
                .type = AU_C_COMP_FILE,
            };
            tmp.f = 0;
            au_c_comp(&c_state, &program, options);
            au_c_comp_state_del(&c_state);
            au_program_del(&program);

            struct au_cc_options cc;
            au_cc_options_default(&cc);

            au_str_array_add(&cc.cflags, "-flto");
            au_str_array_add(&cc.cflags, "-O2");

            int retval = au_spawn_cc(&cc, output_file, tmp.path);
            if (retval != 0) {
                printf("c compiler returned with exit code %d\n", retval);
            }
            au_tmpfile_del(&tmp);
            au_cc_options_del(&cc);
            return retval;
        }
    }
#endif
    return 0;
}
