// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "core/rt/value.h"
#include "platform/platform.h"

struct au_vm_thread_local;
struct au_bc_storage;
struct au_program_data;
struct au_vm_frame;

#define X(NAME) AU_INT_ERR_##NAME
enum au_interpreter_result_type {
    X(OK) = 0,
    X(INCOMPAT_BIN_OP) = 1,
    X(INCOMPAT_CALL) = 2,
    X(INDEXING_NON_COLLECTION) = 3,
    X(INVALID_INDEX) = 4,
    X(IMPORT_PATH) = 5,
    X(CIRCULAR_IMPORT) = 6,
    X(BACKTRACE) = 7,
    X(UNKNOWN_FUNCTION) = 8,
    X(UNKNOWN_CLASS) = 9,
    X(UNKNOWN_CONST) = 10,
    X(WRONG_ARGS) = 11,
    X(RAISED_ERROR) = 12,
};
#undef X

struct au_interpreter_result {
    union {
        struct {
            au_value_t left;
            au_value_t right;
        } incompat_bin_op;
        struct {
            au_value_t value;
        } invalid_collection;
        struct {
            au_value_t collection;
            au_value_t idx;
        } invalid_index;
        struct {
            char *key;
        } unknown_id;
        struct {
            char *key;
            int got_args;
            int expected_args;
        } wrong_args;
        struct {
            au_value_t value;
        } raised_error;
    } data;
    size_t pos;
    enum au_interpreter_result_type type;
};

AU_PRIVATE size_t au_vm_locate_error(const size_t pc,
                                     const struct au_bc_storage *bcs,
                                     const struct au_program_data *p_data);