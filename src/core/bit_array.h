// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

typedef char *au_bit_array;
#define AU_BA_LEN(n) ((n + 7) / 8)
#define AU_BA_GET_BIT(x, n) ((((x)[(n) / 8]) & (0x1 << ((n) % 8))) != 0)
#define AU_BA_SET_BIT(x, n) ((x)[(n) / 8]) |= (0x1 << ((n) % 8))
#define AU_BA_RESET_BIT(x, n) ((x)[(n) / 8]) &= ~(0x1 << ((n) % 8))
