// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Get the size of the heap.
/// @name gc::heap_size
/// @return size of the heap
AU_EXTERN_FUNC_DECL(au_std_gc_heap_size);