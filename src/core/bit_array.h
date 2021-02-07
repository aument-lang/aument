// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

typedef char *bit_array;
#define BA_LEN(n) ((n) / 8)
#define GET_BIT(x, n) ((((x)[(n) / 8]) & (0x1 << ((n) % 8))) != 0)
#define SET_BIT(x, n) ((x)[(n) / 8]) |= (0x1 << ((n) % 8))
#define RESET_BIT(x, n) ((x)[(n) / 8]) &= ~(0x1 << ((n) % 8))
