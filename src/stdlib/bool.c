// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_bool_is) {
    const au_value_t value = _args[0];
    const au_value_t retval =
        au_value_bool(au_value_get_type(value) == AU_VALUE_BOOL);
    au_value_deref(value);
    return retval;
}

AU_EXTERN_FUNC_DECL(au_std_bool_into) {
    au_value_t value = au_value_bool(au_value_is_truthy(_args[0]));
    au_value_deref(_args[0]);
    return value;
}
