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
#include "core/rt/fn_value/ref.h"
#include "core/rt/struct/vdata.h"
#include "platform/platform.h"

#include "main.h"
#endif

static _AlwaysInline void au_value_ref(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case AU_VALUE_STR: {
        au_string_ref(au_value_get_string(v));
        break;
    }
    case AU_VALUE_FN: {
        au_fn_value_ref(au_value_get_fn(v));
        break;
    }
    case AU_VALUE_STRUCT: {
        au_struct_ref((struct au_struct *)au_value_get_struct(v));
        break;
    }
    default:
        return;
    }
}

static _AlwaysInline void au_value_deref(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case AU_VALUE_STR: {
        au_string_deref(au_value_get_string(v));
        break;
    }
    case AU_VALUE_FN: {
        au_fn_value_deref(au_value_get_fn(v));
        break;
    }
    case AU_VALUE_STRUCT: {
        au_struct_deref(au_value_get_struct(v));
        break;
    }
    default:
        return;
    }
}