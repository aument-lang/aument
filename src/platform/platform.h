// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#ifdef _MSC_VER
#define _Unused
#else
#define _Unused __attribute__((unused))
#endif

#ifdef _MSC_VER
#define _NoReturn __declspec(noreturn)
#else
#define _NoReturn __attribute__((noreturn))
#endif

#ifdef _MSC_VER
#define _AlwaysInline __forceinline
#else
#define _AlwaysInline __attribute__((always_inline)) inline
#endif
