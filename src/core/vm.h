// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "program.h"
#include "bc.h"
#include "array.h"
#include "rt/value.h"

ARRAY_TYPE(au_value_t, au_value_stack, 1)

struct au_vm_frame {
    au_value_t regs[AU_REGS];
    au_value_t retval;
    au_value_t *locals;
    /// This object does not own the bytecode pointer
    uint8_t *bc;
    size_t pc;
    struct au_value_stack arg_stack;
};

struct au_vm_thread_local {
    au_value_t *const_cache;
    size_t const_len;
};
void au_vm_thread_local_init(struct au_vm_thread_local *tl, const struct au_program_data *p_data);
void au_vm_thread_local_del(struct au_vm_thread_local *tl);

/// Executes unverified bytecode in a au_bc_storage
/// @param tl
/// @param bcs
/// @param p_data
/// @param args
/// @return return value
au_value_t au_vm_exec_unverified(
    struct au_vm_thread_local *tl,
    const struct au_bc_storage *bcs,
    const struct au_program_data *p_data,
    const au_value_t *args
);

/// Executes unverified bytecode in a au_program
/// @param tl
/// @param program
/// @return return value
static inline au_value_t au_vm_exec_unverified_main(
    struct au_vm_thread_local *tl,
    struct au_program *program
) {
    return au_vm_exec_unverified(tl, &program->main, &program->data, 0);
}
