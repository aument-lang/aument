// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <stdint.h>
#include <stdlib.h>

#include "hash.h"

hash_t au_hash(const uint8_t *str, const size_t len) {
    uint32_t hash = 2166136261;
    for (size_t i = 0; i < len; ++i)
        hash = 16777619 * (hash ^ str[i]);
    return hash ^ (hash >> 16);
}