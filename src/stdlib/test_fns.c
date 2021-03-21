// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_test_1) {
    au_value_deref(_args[0]);
    au_value_deref(_args[1]);
    return au_value_int(1);
}

AU_EXTERN_FUNC_DECL(au_std_test_2) {
    au_value_deref(_args[0]);
    au_value_deref(_args[1]);
    return au_value_int(1);
}