// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/array.h"
#include "core/bc.h"
#include "core/program.h"
#include "core/rt/value.h"
#include "core/value_array.h"

#include "tl.h"

enum au_vm_frame_link_type {
    AU_VM_FRAME_LINK_NONE = 0,
    AU_VM_FRAME_LINK_IMPORTER = 1,
    AU_VM_FRAME_LINK_FRAME = 2,
};

struct au_vm_frame_link {
    union {
        const struct au_vm_frame *frame;
        const struct au_program_data *p_data;
    } as;
    enum au_vm_frame_link_type type;
};

struct au_vm_frame {
    au_value_t regs[AU_REGS];
    au_value_t retval;
    au_value_t *locals;
    const uint8_t *bc;
    size_t pc;
    struct au_value_array arg_stack;
    struct au_vm_frame_link link;
};

/// [func] Executes unverified bytecode in a au_bc_storage
/// @param tl thread local storage
/// @param bcs the au_bc_storage to be executed. Bytecode
///     stored here is unverified and should be checked
///     beforehand if it's safe to run.
/// @param p_data global program data
/// @param args argument array
/// @param link link to previous frame or imported program
/// @return return value specified by interpreted aulang's
///      return statement
au_value_t au_vm_exec_unverified(struct au_vm_thread_local *tl,
                                 const struct au_bc_storage *bcs,
                                 const struct au_program_data *p_data,
                                 const au_value_t *args,
                                 struct au_vm_frame_link link);

/// [func] Executes unverified bytecode in a au_program
/// @param tl thread local storage
/// @param program the au_program to be executed. Bytecode
///     stored here is unverified and should be checked
///     beforehand if it's safe to run.
/// @return return value
static inline au_value_t
au_vm_exec_unverified_main(struct au_vm_thread_local *tl,
                           struct au_program *program);

au_value_t au_vm_exec_unverified_main(struct au_vm_thread_local *tl,
                                      struct au_program *program) {
    return au_vm_exec_unverified(tl, &program->main, &program->data, 0,
                                 (struct au_vm_frame_link){0});
}