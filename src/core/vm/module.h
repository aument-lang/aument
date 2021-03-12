// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform/mmap.h"
#endif

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
#ifndef AU_IS_STDLIB
        struct au_mmap_info source;
#endif
        struct au_module_lib lib;
    } data;
};

char *au_module_resolve(const char *relpath, const char *parent_dir);

enum au_module_import_result {
    AU_MODULE_IMPORT_SUCCESS = 0,
    AU_MODULE_IMPORT_SUCCESS_NO_MODULE = 1,
    AU_MODULE_IMPORT_FAIL = 2,
    AU_MODULE_IMPORT_FAIL_DLERROR = 3,
};

enum au_module_import_result au_module_import(struct au_module *module,
                                              const char *abspath);

#ifdef AU_IS_STDLIB
void *au_module_get_fn(struct au_module *module, const char *fn);
#endif
