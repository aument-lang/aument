// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once

#include <stdint.h>
#include <string.h>

#include "../au_string.h"
#include "../struct/vdata.h"
#include "main.h"
#include "platform/platform.h"
#endif

static _AlwaysInline void au_value_ref(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case VALUE_STR: {
        au_string_ref(au_value_get_string(v));
        break;
    }
    case VALUE_STRUCT: {
        au_struct_ref((struct au_struct *)au_value_get_struct(v));
        break;
    }
    default:
        return;
    }
}

static _AlwaysInline void au_value_deref(const au_value_t v) {
    switch (au_value_get_type(v)) {
    case VALUE_STR: {
        au_string_deref(au_value_get_string(v));
        break;
    }
    case VALUE_STRUCT: {
        au_struct_deref(au_value_get_struct(v));
        break;
    }
    default:
        return;
    }
}