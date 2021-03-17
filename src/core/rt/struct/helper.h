// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "../value/main.h"
#include "main.h"
#include "platform/platform.h"
#endif

_Public au_value_t au_struct_idx_get(au_value_t value, au_value_t idx);

_Public void au_struct_idx_set(au_value_t value, au_value_t idx,
                               au_value_t item);