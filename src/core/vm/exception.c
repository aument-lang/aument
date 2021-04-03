// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "core/int_error/error_location.h"
#include "os/mmap.h"
#include "vm.h"

size_t au_vm_locate_error(const size_t pc, const struct au_bc_storage *bcs,
                          const struct au_program_data *p_data) {
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
