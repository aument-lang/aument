// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#pragma once

#include <stdlib.h>

struct au_mmap_info {
    char *bytes;
    size_t size;
#ifdef USE_MMAP
    int _fd;
#endif
};

/// Loads a file into memory and stores a reference into a au_mmap_info instance
/// @param path path of file
/// @param info info
/// @return 1 if success, 0 if errored
int au_mmap_read(const char *path, struct au_mmap_info *info);

void au_mmap_close(struct au_mmap_info *info);