// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#include "platform/platform.h"
#include <stdlib.h>
#endif

/// Print fatal exception and abort
/// @param fmt printf formatted string
_Public _NoReturn void au_fatal(const char *fmt, ...);

/// Print fatal exception from perror and exit program
/// @param msg what the program was trying to do when the error occured
_Public _NoReturn void au_perror(const char *msg);

/// Print index error and abort
/// @param array the array
/// @param idx the index the code is accessing
/// @param len the length of the array
_Public _NoReturn void au_fatal_index(const void *array, size_t idx,
                                      size_t len);
