// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include "core/str_array.h"
#include "platform.h"

/// [func] Spawns a program with arguments specified
_Public int au_spawn(struct au_str_array *args);