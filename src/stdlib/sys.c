// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include <stdio.h>
#include <stdlib.h>

#include "core/rt/extern_fn.h"

AU_EXTERN_FUNC_DECL(au_std_sys_abort) { abort(); }
