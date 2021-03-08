// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "core/int_error/error_location.h"
#include "core/int_error/error_printer.h"
#include "platform/mmap.h"
#include "vm.h"

#include <assert.h>

static void error_loc(const struct au_program_data *p_data,
                      struct au_mmap_info *mmap,
                      struct au_error_location *loc) {
    assert(au_mmap_read(p_data->file, mmap) != 0);
    loc->path = p_data->file;
    loc->src = mmap->bytes;
    loc->len = mmap->size;
}

static size_t resolve_pos(struct au_vm_frame *frame,
                          const struct au_program_data *p_data) {
    const size_t pc = frame->bc - frame->bc_start;
    for (size_t i = 0; i < p_data->source_map.len; i++) {
        const struct au_program_source_map map =
            p_data->source_map.data[i];
        if (map.bc_from <= pc && pc <= map.bc_to) {
            return map.source_start;
        }
    }
    return 0;
}

void au_vm_error(struct au_interpreter_result res,
                 const struct au_program_data *p_data,
                 struct au_vm_frame *frame) {
    res.pos = resolve_pos(frame, p_data);
    struct au_mmap_info mmap;
    struct au_error_location loc;
    error_loc(p_data, &mmap, &loc);
    au_print_interpreter_error(res, loc);
    exit(EXIT_FAILURE);
}
