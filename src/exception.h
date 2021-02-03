// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

__attribute__((noreturn)) void au_fatal(const char *fmt, ...);
__attribute__((noreturn)) void au_perror(const char *msg);