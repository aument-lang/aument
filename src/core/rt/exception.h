// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform/platform.h"
#endif

/// Print fatal exception and exit program
/// @param fmt printf formatted string
_NoReturn void au_fatal(const char *fmt, ...);

/// Print fatal exception from perror and exit program
/// @param msg from where the error occured?
_NoReturn void au_perror(const char *msg);
