// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#include "core/rt/malloc.h"

#include "c_comp.h"

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

#ifdef AU_TEST_RT_CODE
char *TEST_RT_CODE;
size_t TEST_RT_CODE_LEN;
#endif

void au_c_comp_state_del(struct au_c_comp_state *state) {
    au_data_free(state->str.data);
}

struct au_interpreter_result
au_c_comp(struct au_c_comp_state *state, const struct au_program *program,
          const struct au_c_comp_options *options,
          struct au_cc_options *cc) {
    (void)state;
    (void)program;
    (void)options;
    (void)cc;
    struct au_interpreter_result retval =
        (struct au_interpreter_result){.type = AU_INT_ERR_OK};
    return retval;
}
