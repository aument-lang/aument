// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform/platform.h"
#include <stdint.h>
#endif

typedef uint32_t au_hash_t;

/// [func] Hashes a chunk of memory `str` with length `len`
AU_PRIVATE au_hash_t au_hash(const uint8_t *str, const size_t len);