// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "core/rt/value.h"

struct au_vm_thread_local;
struct au_bc_storage;
struct au_program_data;
struct au_vm_frame;

#define X(NAME) AU_INT_ERR_##NAME
enum au_interpreter_result_type {
    X(OK),
    X(INCOMPAT_BIN_OP),
};
#undef X

struct au_interpreter_result {
    union {
        struct {
            au_value_t left;
            au_value_t right;
        } incompat_bin_op;
    } data;
    size_t pos;
    enum au_interpreter_result_type type;
};

/// [func] Throws an interpreter error
/// @param tl thread local storage
/// @param bcs the bytecode storage
/// @param p_data program data
/// @param frame the frame at which the error occurred
void au_vm_error(struct au_interpreter_result res,
                 const struct au_program_data *p_data,
                 struct au_vm_frame *frame);