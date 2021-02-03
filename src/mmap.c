// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "mmap.h"

#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <fcntl.h>

int au_mmap_read(const char *path, struct au_mmap_info *info) {
    info->_fd = open(path, O_RDONLY);
    if(info->_fd < 0) return 0;
    info->size = lseek(info->_fd, 0, SEEK_END);
    char *bytes = mmap(NULL, info->size, PROT_READ, MAP_PRIVATE, info->_fd, 0);
    if(bytes==(char*)-1) return 0;
    info->bytes = bytes;
    return 1;
}

void au_mmap_close(struct au_mmap_info *info) {
    munmap(info->bytes, info->size);
    close(info->_fd);
}