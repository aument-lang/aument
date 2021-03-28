// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <errhandlingapi.h>
#include <libloaderapi.h>
#include <sys/stat.h>
#define AU_MODULE_LIB_EXT ".dll"
#else
#include <sys/stat.h>
#define AU_MODULE_LIB_EXT ".so"
#ifdef AU_FEAT_LIBDL
#include <dlfcn.h>
#endif
#endif

#define AU_MODULE_UNIV_LIB_EXT ".lib"

#include "core/rt/malloc.h"
#include "module.h"

#define AU_MODULE_LOAD_FN "au_extern_module_load"

#ifdef AU_IS_STDLIB
#include "core/fn/main.h"
#include "core/hm_vars.h"
#include "core/program.h"

au_extern_func_t au_module_get_fn(struct au_module *module,
                                  const char *fn_name) {
    const au_hm_var_value_t *value = au_hm_vars_get(
        &module->data.lib.lib->fn_map, fn_name, strlen(fn_name));
    if (value == 0)
        return 0;
    const struct au_fn fn =
        au_fn_array_at(&module->data.lib.lib->fns, *value);
    if (fn.type != AU_FN_LIB)
        return 0;
    return fn.as.lib_func.func;
}
#endif

void au_module_resolve_result_del(
    struct au_module_resolve_result *result) {
    if (result->abspath != 0)
        au_data_free(result->abspath);
    result->abspath = 0;

    if (result->subpath != 0)
        au_data_free(result->subpath);
    result->subpath = 0;
}

void au_module_lib_del(struct au_module_lib *lib) {
    (void)lib;
#ifdef _WIN32
    FreeLibrary((HMODULE)lib->dl_handle);
#else
#ifdef AU_FEAT_LIBDL
    dlclose(lib->dl_handle);
#endif
#endif
}

void au_module_lib_perror() {
#ifdef _WIN32
    fprintf(stderr, "win32 error: %d\n", (int)GetLastError());
#else
#ifdef AU_FEAT_LIBDL
    fprintf(stderr, "libdl: %s\n", dlerror());
#endif
#endif
}

int au_module_resolve(struct au_module_resolve_result *result,
                      const char *import_path, const char *parent_dir) {
    const char *canon_path = 0;
    if (import_path[0] == '.' && import_path[1] == '/') {
        canon_path = &import_path[2];
    } else if (import_path[0] == '.' && import_path[1] == '.' &&
               import_path[2] == '/') {
        canon_path = import_path;
    } else {
        *result = (struct au_module_resolve_result){0};
        return 0;
    }

    int canon_path_len = 0;
    int subpath_pos = 0, subpath_len = 0;
    for (; canon_path[canon_path_len] != 0; canon_path_len++) {
        if (canon_path[canon_path_len] == ':') {
            subpath_pos = canon_path_len + 1;
            subpath_len = 0;
            for (size_t i = subpath_pos; canon_path[i] != 0; i++) {
                subpath_len++;
            }
            break;
        }
    }

    const size_t abspath_len = strlen(parent_dir) + canon_path_len + 2;

    char *abspath = au_data_malloc(abspath_len + 1);
    snprintf(abspath, abspath_len, "%s/%.*s", parent_dir, canon_path_len,
             canon_path);
    abspath[abspath_len] = 0;

    char *subpath = 0;
    if (subpath_len != 0) {
        subpath = au_data_malloc(subpath_len + 1);
        memcpy(subpath, &canon_path[subpath_pos], subpath_len);
        subpath[subpath_len] = 0;
    }

    *result = (struct au_module_resolve_result){
        .abspath = abspath,
        .subpath = subpath,
    };
    return 1;
}

typedef struct au_program_data *module_load_fn_ret_t;

#ifdef _WIN32
#define MODULE_LOAD_CALLCONV __stdcall
#else
#define MODULE_LOAD_CALLCONV
#endif

typedef module_load_fn_ret_t(MODULE_LOAD_CALLCONV *module_load_fn_t)(
    struct au_extern_module_options *);

static int endswith(const char *str, size_t len, const char *needle) {
    const size_t needle_len = strlen(needle);
    return len >= needle_len &&
           memcmp(&str[len - needle_len], needle, needle_len) == 0;
}

static char *find_native_lib(const char *abspath) {
    const size_t filename_len =
        strlen(abspath) - strlen(AU_MODULE_UNIV_LIB_EXT);
    const size_t full_len = filename_len + strlen(AU_MODULE_LIB_EXT);

    char *native_lib = au_data_malloc(full_len + 1);
    memcpy(native_lib, abspath, filename_len);
    memcpy(&native_lib[filename_len], AU_MODULE_LIB_EXT,
           strlen(AU_MODULE_LIB_EXT));
    native_lib[full_len] = 0;

#ifdef _WIN32
    struct _stat buf;
    if (_stat(native_lib, &buf) == -1)
        goto fail;
#else
    struct stat buf;
    if (stat(native_lib, &buf) == -1)
        goto fail;
#endif

    return native_lib;

fail:
    au_data_free(native_lib);
    return 0;
}

enum au_module_import_result
au_module_import(struct au_module *module,
                 const struct au_module_resolve_result *resolved) {
    (void)module;
    const size_t abspath_len = strlen(resolved->abspath);
    const int is_universal_lib =
        endswith(resolved->abspath, abspath_len, AU_MODULE_UNIV_LIB_EXT);
    if (is_universal_lib ||
        endswith(resolved->abspath, abspath_len, AU_MODULE_LIB_EXT)) {
#ifdef _WIN32
        module->type = AU_MODULE_LIB;

        HMODULE handle = 0;
        if (is_universal_lib) {
            char *native_lib = find_native_lib(resolved->abspath);
            if (native_lib == 0) {
                return AU_MODULE_IMPORT_FAIL;
            } else {
                handle = LoadLibraryA(native_lib);
                au_data_free(native_lib);
            }
        } else {
            handle = LoadLibraryA(resolved->abspath);
        }
        if (handle == 0) {
            return AU_MODULE_IMPORT_FAIL_DL;
        }
        module->data.lib.dl_handle = (void *)handle;

        module_load_fn_t loader = (module_load_fn_t)(
            (void *)GetProcAddress(handle, AU_MODULE_LOAD_FN));
        if (loader == 0) {
            FreeLibrary(handle);
            module->data.lib.dl_handle = 0;
            module->data.lib.lib = 0;
            return AU_MODULE_IMPORT_FAIL_DL;
        }

        struct au_extern_module_options options = {0};
        options.subpath = resolved->subpath;
        module->data.lib.lib = loader(&options);

        if (module->data.lib.lib == 0) {
            FreeLibrary(handle);
            module->data.lib.dl_handle = 0;
            module->data.lib.lib = 0;
            return AU_MODULE_IMPORT_SUCCESS_NO_MODULE;
        }
        return AU_MODULE_IMPORT_SUCCESS;
#else
#ifdef AU_FEAT_LIBDL
        module->type = AU_MODULE_LIB;

        if (is_universal_lib) {
            char *native_lib = find_native_lib(resolved->abspath);
            if (native_lib == 0) {
                return AU_MODULE_IMPORT_FAIL;
            } else {
                module->data.lib.dl_handle =
                    dlopen(native_lib, RTLD_LAZY | RTLD_LOCAL);
                au_data_free(native_lib);
            }
        } else {
            module->data.lib.dl_handle =
                dlopen(resolved->abspath, RTLD_LAZY | RTLD_LOCAL);
        }
        if (module->data.lib.dl_handle == 0) {
            return AU_MODULE_IMPORT_FAIL_DL;
        }

        module_load_fn_t loader = (module_load_fn_t)dlsym(
            module->data.lib.dl_handle, AU_MODULE_LOAD_FN);
        if (loader == 0) {
            dlclose(module->data.lib.dl_handle);
            return AU_MODULE_IMPORT_FAIL_DL;
        }

        struct au_extern_module_options options = {0};
        options.subpath = resolved->subpath;
        module->data.lib.lib = loader(&options);

        if (module->data.lib.lib == 0) {
            dlclose(module->data.lib.dl_handle);
            module->data.lib.dl_handle = 0;
            module->data.lib.lib = 0;
            return AU_MODULE_IMPORT_SUCCESS_NO_MODULE;
        }
#else
        return AU_MODULE_IMPORT_FAIL;
#endif
#endif
    }
#ifndef AU_IS_STDLIB
    else {
        module->type = AU_MODULE_SOURCE;
        struct au_mmap_info mmap;
        if (!au_mmap_read(resolved->abspath, &mmap)) {
            return AU_MODULE_IMPORT_FAIL;
        }
        module->data.source = mmap;
    }
#endif
    return AU_MODULE_IMPORT_SUCCESS;
}
