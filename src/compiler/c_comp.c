// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>

#include "os/mmap.h"
#include "os/path.h"

#include "core/bc.h"
#include "core/bit_array.h"
#include "core/hm_vars.h"
#include "core/int_error/error_printer.h"
#include "core/parser/parser.h"
#include "core/program.h"
#include "core/rt/exception.h"
#include "core/rt/malloc.h"
#include "core/vm/module.h"
#include "stdlib/au_stdlib.h"

#include "c_comp.h"

extern const char AU_RT_HDR[];
extern const size_t AU_RT_HDR_LEN;

#ifdef AU_TEST_RT_CODE
char *TEST_RT_CODE;
size_t TEST_RT_CODE_LEN;
#endif

struct au_interpreter_result
au_c_comp(struct au_c_comp_state *state, const struct au_program *program,
          const struct au_c_comp_options *options,
          struct au_cc_options *cc) {
    struct au_interpreter_result retval =
        (struct au_interpreter_result){.type = AU_INT_ERR_OK};
    return retval;
}
