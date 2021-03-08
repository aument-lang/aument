// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdlib.h>

#include "core/parser/exception.h"
#include "core/vm/exception.h"
#include "error_location.h"

void au_print_parser_error(struct au_parser_result res,
                           struct au_error_location loc);

void au_print_interpreter_error(struct au_interpreter_result type,
                                struct au_error_location loc);
