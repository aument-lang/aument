// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <stdint.h>
#include <string.h>

#include "core/rt/au_string.h"
#include "core/rt/malloc.h"
#include "core/rt/struct/vdata.h"
#include "platform/platform.h"

#include "main.h"
#endif

static AU_ALWAYS_INLINE void au_value_ref(const au_value_t v) {
    void *obj_rc = 0;

    switch (au_value_get_type(v)) {
    case AU_VALUE_STR: {
        obj_rc = au_value_get_string(v);
        break;
    }
    case AU_VALUE_FN: {
        obj_rc = au_value_get_fn(v);
        break;
    }
    case AU_VALUE_STRUCT: {
        obj_rc = (struct au_struct *)au_value_get_struct(v);
        break;
    }
    default:
        return;
    }

    if (obj_rc != 0)
        au_obj_ref(obj_rc);
}

static AU_ALWAYS_INLINE void au_value_deref(const au_value_t v) {
    void *obj_rc = 0;

    switch (au_value_get_type(v)) {
    case AU_VALUE_STR: {
        obj_rc = au_value_get_string(v);
        break;
    }
    case AU_VALUE_FN: {
        obj_rc = au_value_get_fn(v);
        break;
    }
    case AU_VALUE_STRUCT: {
        obj_rc = (struct au_struct *)au_value_get_struct(v);
        break;
    }
    default:
        return;
    }

    if (obj_rc != 0)
        au_obj_deref(obj_rc);
}