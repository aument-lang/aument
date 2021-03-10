// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "module.h"
#include "core/rt/malloc.h"

#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

#define AU_MODULE_LIB_EXT ".so"
#define AU_MODULE_LOAD_FN "au_module_load"

void au_module_lib_del(struct au_module_lib *lib) { (void)lib; }

char *au_module_resolve(const char *relpath, const char *parent_dir) {
    const char *relpath_canon = 0;
    if (relpath[0] == '.' && relpath[1] == '/') {
        relpath_canon = &relpath[2];
    } else if (relpath[0] == '.' && relpath[1] == '.' &&
               relpath[2] == '/') {
        relpath_canon = relpath;
    }
    if (relpath_canon != 0) {
        const size_t relpath_canon_len = strlen(relpath_canon);
        const size_t abspath_len =
            strlen(parent_dir) + relpath_canon_len + 2;
        char *abspath = au_data_malloc(abspath_len);
        snprintf(abspath, abspath_len, "%s/%s", parent_dir, relpath_canon);
        return abspath;
    }
    return 0;
}

typedef struct au_program_data *module_load_fn_ret_t;
typedef module_load_fn_ret_t (*module_load_fn_t)();

enum au_module_import_result au_module_import(struct au_module *module,
                                              const char *abspath) {
    const size_t abspath_len = strlen(abspath);
    if (abspath_len > strlen(AU_MODULE_LIB_EXT) &&
        memcmp(&abspath[abspath_len - strlen(AU_MODULE_LIB_EXT)],
               AU_MODULE_LIB_EXT, strlen(AU_MODULE_LIB_EXT)) == 0) {
#ifdef AU_FEAT_LIBDL
        module->type = AU_MODULE_LIB;
        module->data.lib.dl_handle =
            dlopen(abspath, RTLD_LAZY | RTLD_LOCAL);
        if (module->data.lib.dl_handle == 0) {
            return AU_MODULE_IMPORT_FAIL_DLERROR;
        }
        module_load_fn_t loader = (module_load_fn_t)dlsym(
            module->data.lib.dl_handle, AU_MODULE_LOAD_FN);
        if (loader == 0) {
            dlclose(module->data.lib.dl_handle);
            return AU_MODULE_IMPORT_FAIL_DLERROR;
        }
        module->data.lib.lib = loader();
        if (module->data.lib.lib == 0) {
            dlclose(module->data.lib.dl_handle);
            return AU_MODULE_IMPORT_SUCCESS_NO_MODULE;
        }
#else
        return AU_MODULE_IMPORT_FAIL;
#endif
    } else {
        module->type = AU_MODULE_SOURCE;
        struct au_mmap_info mmap;
        if (!au_mmap_read(abspath, &mmap)) {
            return AU_MODULE_IMPORT_FAIL;
        }
        module->data.source = mmap;
    }
    return AU_MODULE_IMPORT_SUCCESS;
}
