// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "mmap.h"
#include "core/rt/malloc.h"

#ifdef AU_USE_MMAP
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <stdio.h>
#endif

int au_mmap_read(const char *path, struct au_mmap_info *info) {
#ifdef AU_USE_MMAP
    info->_fd = open(path, O_RDONLY);
    if (info->_fd < 0)
        return 0;
    info->size = lseek(info->_fd, 0, SEEK_END);
    if (info->size == 0) {
        info->bytes = 0;
        return 1;
    }
    char *bytes =
        mmap(NULL, info->size, PROT_READ, MAP_PRIVATE, info->_fd, 0);
    if (bytes == (char *)-1)
        return 0;
    info->bytes = bytes;
#else
    FILE *file = fopen(path, "r");
    fseek(file, 0, SEEK_END);
    info->size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (info->size == 0) {
        info->bytes = 0;
        fclose(file);
        return 1;
    }
    char *bytes = au_data_malloc(info->size);
    fread(bytes, 1, info->size, file);
    fclose(file);
    info->bytes = bytes;
#endif
    return 1;
}

void au_mmap_del(struct au_mmap_info *info) {
#ifdef AU_USE_MMAP
    munmap(info->bytes, info->size);
    close(info->_fd);
#else
    au_data_free(info->bytes);
#endif
}