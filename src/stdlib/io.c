// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/malloc.h"
#include "core/rt/struct/coerce.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

#include "lib/string_builder.h"

AU_EXTERN_FUNC_DECL(au_std_io_input) {
    int ch = -1;
    struct au_string_builder builder;
    au_string_builder_init(&builder);
    while ((ch = fgetc(stdin)) != EOF) {
        if (ch == '\n')
            break;
        au_string_builder_add(&builder, ch);
    }
    return au_value_string(au_string_builder_into_string(&builder));
}

// ** mock testing **
#ifdef AU_TEST
static FILE *_test_fopen(const char *path, const char *mode) {
    fprintf(stderr, "fopen %s, %s\n", path, mode);
    return (FILE *)((uintptr_t)1);
}

static int _test_fclose(FILE *ptr) {
    (void)ptr;
    fprintf(stderr, "fclose\n");
    return 0;
}

static size_t _test_fwrite(const void *ptr, size_t size, size_t count,
                           FILE *stream) {
    const char *bytes = (const char *)ptr;
    const size_t num_bytes = size * count;
    (void)stream;
    fprintf(stderr, "fwrite [%.*s]\n", (int)num_bytes, bytes);
    return num_bytes;
}

static AU_THREAD_LOCAL int nread = 0;
#define TEST_NREAD_MAX 10
static int _test_fgetc(FILE *stream) {
    (void)stream;
    fprintf(stderr, "getc\n");
    if (nread < TEST_NREAD_MAX) {
        nread++;
        return 'a';
    }
    return EOF;
}

static int _test_fflush(FILE *stream) {
    (void)stream;
    fprintf(stderr, "fflush\n");
    return 0;
}

#define fopen _test_fopen
#define fclose _test_fclose
#define fgetc _test_fgetc
#define fread _test_fread
#define fwrite _test_fwrite
#define fflush _test_fflush
#endif

// ** io module functions **

struct au_std_io {
    struct au_struct header;
    FILE *f;
    int can_close;
};

static void io_close(struct au_std_io *io) {
    if (io->f != NULL) {
        if (io->can_close) {
            fclose(io->f);
        }
        io->f = NULL;
    }
}

static AU_THREAD_LOCAL struct au_struct_vdata io_vdata;
static AU_THREAD_LOCAL int io_vdata_inited = 0;
static void io_vdata_init() {
    if (!io_vdata_inited) {
        io_vdata = (struct au_struct_vdata){
            .del_fn = (au_obj_del_fn_t)io_close,
            .idx_get_fn = 0,
            .idx_set_fn = 0,
            .len_fn = 0,
        };
        io_vdata_inited = 1;
    }
}

#define MAX_SMALL_PATH 256

AU_EXTERN_FUNC_DECL(au_std_io_stdout) {
    struct au_std_io *io =
        au_obj_malloc(sizeof(struct au_std_io), (au_obj_del_fn_t)io_close);
    io_vdata_init();
    io->header = (struct au_struct){
        .rc = 1,
        .vdata = &io_vdata,
    };
    io->f = stdout;
    io->can_close = 0;
    return au_value_struct((struct au_struct *)io);
}

AU_EXTERN_FUNC_DECL(au_std_io_stdin) {
    struct au_std_io *io =
        au_obj_malloc(sizeof(struct au_std_io), (au_obj_del_fn_t)io_close);
    io_vdata_init();
    io->header = (struct au_struct){
        .rc = 1,
        .vdata = &io_vdata,
    };
    io->f = stdin;
    io->can_close = 0;
    return au_value_struct((struct au_struct *)io);
}

AU_EXTERN_FUNC_DECL(au_std_io_stderr) {
    struct au_std_io *io =
        au_obj_malloc(sizeof(struct au_std_io), (au_obj_del_fn_t)io_close);
    io_vdata_init();
    io->header = (struct au_struct){
        .rc = 1,
        .vdata = &io_vdata,
    };
    io->f = stderr;
    io->can_close = 0;
    return au_value_struct((struct au_struct *)io);
}

AU_EXTERN_FUNC_DECL(au_std_io_open) {
    au_value_t path_value = au_value_none();
    au_value_t mode_value = au_value_none();

    struct au_std_io *io = 0;
    FILE *f = 0;

    path_value = _args[0];
    if (au_value_get_type(path_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *path_str = au_value_get_string(path_value);

    mode_value = _args[1];
    if (au_value_get_type(mode_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *mode_str = au_value_get_string(mode_value);

    // Set up mode parameter
    const char *mode = 0;
    if (au_string_cmp_cstr(mode_str, "r") == 0)
        mode = "rb";
    else if (au_string_cmp_cstr(mode_str, "w") == 0)
        mode = "wb";
    else if (au_string_cmp_cstr(mode_str, "a") == 0)
        mode = "ab";
    else
        goto fail;

    // Set up path parameter & open the file
    if (path_str->len < MAX_SMALL_PATH) {
        // Path parameter will usually be smaller than or equal to this
        // size. This is an optimization to reduce heap allocation
        char path_param[MAX_SMALL_PATH] = {0};
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        f = fopen(path_param, mode);
    } else {
        char *path_param = au_data_malloc(path_str->len + 1);
        memcpy(path_param, path_str->data, path_str->len);
        path_param[path_str->len] = 0;
        f = fopen(path_param, mode);
        au_data_free(path_param);
    }

    if (f == 0)
        goto fail;

    io =
        au_obj_malloc(sizeof(struct au_std_io), (au_obj_del_fn_t)io_close);
    io_vdata_init();
    io->header = (struct au_struct){
        .rc = 1,
        .vdata = &io_vdata,
    };
    io->f = f;
    f = 0;
    io->can_close = 1;

    au_value_deref(path_value);
    au_value_deref(mode_value);
    return au_value_struct((struct au_struct *)io);

fail:
    au_value_deref(path_value);
    au_value_deref(mode_value);
    if (f != 0)
        fclose(f);
    if (io != 0)
        au_obj_free(io);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_io_close) {
    const au_value_t io_valueue = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_valueue);
    if (io_struct == NULL || io_struct->vdata != &io_vdata) {
        au_value_deref(io_valueue);
        return au_value_op_error();
    }
    io_close((struct au_std_io *)io_struct);
    au_value_deref(io_valueue);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_io_read) {
    const au_value_t io_valueue = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_valueue);
    if (io_struct == NULL || io_struct->vdata != &io_vdata)
        goto fail;
    struct au_std_io *io = (struct au_std_io *)io_struct;

    int ch = -1;
    struct au_string_builder builder;
    au_string_builder_init(&builder);
    while ((ch = fgetc(io->f)) != EOF) {
        au_string_builder_add(&builder, ch);
    }
    au_value_deref(io_valueue);
    return au_value_string(au_string_builder_into_string(&builder));

fail:
    au_value_deref(io_valueue);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(au_std_io_read_up_to) {
    au_value_t io_value = au_value_none();
    au_value_t n_value = au_value_none();

    io_value = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_value);
    if (io_struct == NULL || io_struct->vdata != &io_vdata)
        goto fail;
    struct au_std_io *io = (struct au_std_io *)io_struct;

    n_value = _args[1];
    if (au_value_get_type(n_value) != AU_VALUE_INT)
        goto fail;
    const int32_t n = au_value_get_int(n_value);

    struct au_string_builder builder;
    au_string_builder_init(&builder);
    for (int i = 0; i < n; i++) {
        int ch = fgetc(io->f);
        if (ch == EOF)
            break;
        au_string_builder_add(&builder, ch);
    }
    au_value_deref(io_value);
    return au_value_string(au_string_builder_into_string(&builder));

fail:
    au_value_deref(io_value);
    au_value_deref(n_value);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(au_std_io_write) {
    au_value_t io_value = au_value_none();
    au_value_t out_value = au_value_none();

    io_value = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_value);
    if (io_struct == NULL || io_struct->vdata != &io_vdata)
        goto fail;
    struct au_std_io *io = (struct au_std_io *)io_struct;

    out_value = _args[1];
    if (au_value_get_type(out_value) != AU_VALUE_STR)
        goto fail;
    const struct au_string *out = au_value_get_string(out_value);

    int32_t retval = (int32_t)fwrite(out->data, 1, out->len, io->f);
    au_value_deref(io_value);
    au_value_deref(out_value);
    return au_value_int(retval);

fail:
    au_value_deref(io_value);
    au_value_deref(out_value);
    return au_value_op_error();
}

AU_EXTERN_FUNC_DECL(au_std_io_flush) {
    const au_value_t io_value = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_value);
    if (io_struct == NULL || io_struct->vdata != &io_vdata)
        goto fail;
    struct au_std_io *io = (struct au_std_io *)io_struct;
    fflush(io->f);
    au_value_deref(io_value);
    return au_value_none();

fail:
    au_value_deref(io_value);
    return au_value_op_error();
}
