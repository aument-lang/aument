// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once
#include "platform/platform.h"
#include <stdint.h>

typedef uint32_t au_hash_t;

/// [func] Hashes a chunk of memory `str` with length `len`
_Private au_hash_t au_hash(const uint8_t *str, const size_t len);