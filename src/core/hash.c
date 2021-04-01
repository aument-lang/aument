// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include <stdint.h>
#include <stdlib.h>

#include "hash.h"
#endif

uint32_t au_hash(const uint8_t *str, const size_t len) {
    // FNV-1a hash. See: http://www.isthe.com/chongo/tech/comp/fnv/    
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    return hash;
}

uint32_t au_hash_u32(uint32_t key) {
    uint32_t c2 = 0x27d4eb2d;
    key = (key ^ 61) ^ (key >> 16);
    key = key + (key << 3);
    key = key ^ (key >> 4);
    key = key * c2;
    key = key ^ (key >> 15);
    return key;
}

uint32_t au_hash_u64(uint64_t key) {
    key = (~key) + (key << 18); // key = (key << 18) - key - 1;
    key = key ^ (key >> 31);
    key = key * 21; // key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 11);
    key = key + (key << 6);
    key = key ^ (key >> 22);
    return (uint32_t) key;
}
