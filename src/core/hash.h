// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

typedef uint32_t hash_t;

/// [func] Hashes a chunk of memory `str` with length `len`
hash_t au_hash(const uint8_t *str, const size_t len);