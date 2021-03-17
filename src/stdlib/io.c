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

AU_EXTERN_FUNC_DECL(au_std_input) {
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

// ** io module functions **

struct au_std_io {
    struct au_struct header;
    FILE *f;
};

static void io_close(struct au_std_io *io) {
    if (io->f != NULL) {
        fclose(io->f);
        io->f = NULL;
    }
}

struct au_struct_vdata io_vdata;
static int io_vdata_inited = 0;
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

AU_EXTERN_FUNC_DECL(au_std_io_open) {
    const au_value_t path_val = _args[0];
    if (au_value_get_type(path_val) != AU_VALUE_STR)
        return au_value_op_error();
    const au_value_t mode_val = _args[0];
    if (au_value_get_type(mode_val) != AU_VALUE_STR)
        return au_value_op_error();

    struct au_std_io *io = 0;

    const struct au_string *path_str = au_value_get_string(path_val);
    const struct au_string *mode_str = au_value_get_string(mode_val);

    FILE *f = 0;

    // Set up mode parameter
    const char *mode = 0;
#define CMP_MODE(EXPECT_MODE_STR)                                         \
    (mode_str->len == strlen(EXPECT_MODE_STR) &&                          \
     memcmp(mode_str->data, EXPECT_MODE_STR, strlen(EXPECT_MODE_STR)) ==  \
         0)
    if (CMP_MODE("r"))
        mode = "rb";
    else if (CMP_MODE("w"))
        mode = "wb";
    else
        goto fail;
#undef CMP_MODE

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

    au_value_deref(path_val);
    au_value_deref(mode_val);
    return au_value_struct((struct au_struct *)io);

fail:
    au_value_deref(path_val);
    au_value_deref(mode_val);
    if (f != 0)
        fclose(f);
    if (io != 0)
        au_obj_free(io);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_io_close) {
    const au_value_t io_val = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_val);
    if (io_struct == NULL || io_struct->vdata != &io_vdata) {
        au_value_deref(io_val);
        return au_value_op_error();
    }
    io_close((struct au_std_io *)io_struct);
    au_value_deref(io_val);
    return au_value_none();
}

AU_EXTERN_FUNC_DECL(au_std_io_read) {
    const au_value_t io_val = _args[0];
    struct au_struct *io_struct = au_struct_coerce(io_val);
    if (io_struct == NULL || io_struct->vdata != &io_vdata) {
        au_value_deref(io_val);
        return au_value_op_error();
    }
    io_close((struct au_std_io *)io_struct);
    au_value_deref(io_val);
    return au_value_none();
}
