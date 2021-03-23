// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform.h"
#endif

static AU_UNUSED AU_ALWAYS_INLINE int au_platform_iadd_wrap(int a, int b) {
    int res;
    __builtin_sadd_overflow(a, b, &res);
    return res;
}

static AU_UNUSED AU_ALWAYS_INLINE int au_platform_isub_wrap(int a, int b) {
    int res;
    __builtin_ssub_overflow(a, b, &res);
    return res;
}

static AU_UNUSED AU_ALWAYS_INLINE int au_platform_imul_wrap(int a, int b) {
    int res;
    __builtin_smul_overflow(a, b, &res);
    return res;
}
