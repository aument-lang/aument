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

/// [func] Hashes a chunk of memory `str` with length `len`
AU_PRIVATE uint32_t au_hash(const uint8_t *str, const size_t len);

AU_PRIVATE uint32_t au_hash_u32(uint32_t key);

AU_PRIVATE uint32_t au_hash_u64(uint64_t key);

#if UINTPTR_MAX != SIZE_MAX
#define "UINTPTR_MAX != SIZE_MAX"
#endif

#if SIZE_MAX == UINT64_MAX
#define au_hash_usize(x) au_hash_u64((uint64_t)(x))
#else
#if SIZE_MAX == UINT32_MAX
#define au_hash_usize(x) au_hash_u32((uint32_t)(x))
#else
#error "No hash function for this architecture"
#endif
#endif

