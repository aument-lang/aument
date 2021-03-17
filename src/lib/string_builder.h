// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/rt/au_string.h"
#include "core/rt/malloc.h"

#include "platform/platform.h"

struct au_string_builder {
    struct au_string *_string;
    size_t pos;
    size_t cap;
};

static _Unused inline void
au_string_builder_init(struct au_string_builder *builder) {
    builder->_string = au_obj_malloc(sizeof(struct au_string) + 1, 0);
    builder->_string->rc = 1;
    builder->_string->len = 1;
    builder->pos = 0;
    builder->cap = 1;
}

static _Unused inline void
au_string_builder_del(struct au_string_builder *builder) {
    if (builder->_string->rc != 0) {
        builder->_string->rc--;
        if (builder->_string->rc == 0)
            au_obj_free(builder->_string);
    }
    builder->_string = 0;
}

static _Unused inline void
au_string_builder_add(struct au_string_builder *builder, char ch) {
    if (builder->pos == builder->cap) {
        builder->cap *= 2;
        builder->_string = au_obj_realloc(
            builder->_string, sizeof(struct au_string) + builder->cap);
    }
    builder->_string->data[builder->pos] = ch;
    builder->pos++;
}

static _Unused inline struct au_string *
au_string_builder_into_string(struct au_string_builder *builder) {
    struct au_string *ret = builder->_string;
    ret->len = builder->pos;
    builder->_string = 0;
    return ret;
}
