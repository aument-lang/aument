// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#endif

struct au_vm_frame;
struct au_program_data;

struct au_vm_frame_link {
    const struct au_vm_frame *frame;
    const struct au_program_data *data;
};