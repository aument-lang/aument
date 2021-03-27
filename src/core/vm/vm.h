// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/array.h"
#include "core/bc.h"
#include "core/program.h"
#include "core/rt/value.h"
#include "core/value_array.h"

#include "frame_link.h"
#include "tl.h"

struct au_vm_frame {
#ifdef AU_USE_ALLOCA
    au_value_t *regs;
#else
    au_value_t regs[AU_REGS];
#endif
    au_value_t *locals;
    const uint8_t *bc;
    const uint8_t *bc_start;
    struct au_obj_class *self;
    struct au_vm_frame_link link;
    au_value_t retval;
};

/// [func] Executes unverified bytecode in a au_bc_storage
/// @param tl thread local storage
/// @param bcs the au_bc_storage to be executed. Bytecode
///     stored here is unverified and should be checked
///     beforehand if it's safe to run.
/// @param p_data global program data
/// @param args argument array
/// @param link link to previous frame or imported program
/// @return return value specified by interpreted aument's
///      return statement
AU_PRIVATE au_value_t au_vm_exec_unverified(
    struct au_vm_thread_local *tl, const struct au_bc_storage *bcs,
    const struct au_program_data *p_data, const au_value_t *args);

/// [func] Executes unverified bytecode in a au_program
/// @param tl thread local storage
/// @param program the au_program to be executed. Bytecode
///     stored here is unverified and should be checked
///     beforehand if it's safe to run.
/// @return return value
AU_PUBLIC au_value_t au_vm_exec_unverified_main(
    struct au_vm_thread_local *tl, struct au_program *program);
