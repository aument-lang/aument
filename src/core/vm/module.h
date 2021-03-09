// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "platform/mmap.h"

enum au_module_type {
    AU_MODULE_SOURCE,
    AU_MODULE_LIB,
};

struct au_module_lib {
    void *dl_handle;
    struct au_program_data *lib;
};

void au_module_lib_del(struct au_module_lib *lib);

struct au_module {
    enum au_module_type type;
    union {
        struct au_mmap_info source;
        struct au_module_lib lib;
    } data;
};

char *au_module_resolve(const char *relpath, const char *parent_dir);
int au_module_import(struct au_module *module, const char *abspath);
