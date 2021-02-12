// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "../value/main.h"
#include "main.h"
#endif

static inline struct au_struct *au_struct_coerce(au_value_t value) {
    if (au_value_get_type(value) != VALUE_STRUCT)
        return 0;
    return (struct au_struct *)au_value_get_struct(value);
}