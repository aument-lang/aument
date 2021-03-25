// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "core/int_error/error_location.h"
#include "platform/mmap.h"
#include "vm.h"

#include <assert.h>

size_t au_vm_locate_error(const struct au_vm_frame *frame,
                          const struct au_bc_storage *bcs,
                          const struct au_program_data *p_data) {
    const size_t pc = frame->bc - frame->bc_start;

    for (size_t i = 0; i < p_data->source_map.len; i++) {
        const struct au_program_source_map map =
            p_data->source_map.data[i];
        if (map.func_idx == bcs->func_idx && map.bc_from <= pc &&
            pc <= map.bc_to) {
            return map.source_start;
        }
    }

    return 0;
}
