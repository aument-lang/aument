// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>

#include "core/rt/extern_fn.h"
#include "core/rt/malloc.h"
#include "core/rt/value.h"
#include "core/vm/vm.h"

AU_EXTERN_FUNC_DECL(au_std_gc_heap_size) {
    return au_value_int(au_malloc_heap_size());
}