// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/rt/au_fn_value.h"

#define au_call_from_module(FN_VALUE, ARG_ARRAY)                          \
    au_fn_value_call_vm(FN_VALUE, _tl, ARG_ARRAY->data, ARG_ARRAY->len, 0)