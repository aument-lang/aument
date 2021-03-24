// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"
#include "lib/module.h"
#include "platform/platform.h"

AU_PRIVATE void au_stdlib_export(struct au_program_data *data);
extern AU_PRIVATE const size_t au_stdlib_modules_len;
AU_PRIVATE au_extern_module_t au_stdlib_module(size_t idx);
