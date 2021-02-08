// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

struct au_program;

/// [func] Parses source code into an au_program instance
/// @param src source code string
/// @param len the bytesize len of the source code
/// @param program output into a program
/// @return 1 if parsed successfully, 0 if an error occurred
int au_parse(char *src, size_t len, struct au_program *program);