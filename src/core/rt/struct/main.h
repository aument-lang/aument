// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifdef DEBUG_RC
#include <stdio.h>
#endif
#endif

struct au_struct {
    uint32_t rc;
    const struct au_struct_vdata *vdata;
};
