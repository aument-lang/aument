// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "os/mmap.h"
#include "platform/platform.h"
#endif

enum au_module_type {
    AU_MODULE_SOURCE,
    AU_MODULE_LIB,
};

struct au_module_lib {
    void *dl_handle;
    struct au_program_data *lib;
};

AU_PUBLIC void au_module_lib_del(struct au_module_lib *lib);

struct au_module {
    enum au_module_type type;
    union {
        struct au_mmap_info source;
        struct au_module_lib lib;
    } data;
};

struct au_module_resolve_result {
    char *abspath;
    char *subpath;
};

AU_PUBLIC void
au_module_resolve_result_del(struct au_module_resolve_result *result);

AU_PUBLIC int au_module_resolve(struct au_module_resolve_result *result,
                                const char *import_path,
                                const char *parent_dir);

enum au_module_import_result {
    AU_MODULE_IMPORT_SUCCESS = 0,
    AU_MODULE_IMPORT_SUCCESS_NO_MODULE = 1,
    AU_MODULE_IMPORT_FAIL = 2,
    AU_MODULE_IMPORT_FAIL_DL = 3,
};

AU_PRIVATE void au_module_lib_perror();

AU_PUBLIC enum au_module_import_result
au_module_import(struct au_module *module,
                 const struct au_module_resolve_result *resolved);

struct au_extern_module_options {
    const char *subpath;
};

#ifdef AU_IS_STDLIB
#ifdef AU_IS_INTERPRETER
#include "core/rt/extern_fn.h"
#endif
AU_PUBLIC au_extern_func_t au_module_get_fn(struct au_module *module,
                                            const char *fn);
#endif
