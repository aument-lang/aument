// This source file is part of the aulang project
// Copyright (c) 2021 the aulang contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include <string.h>
#include <stdlib.h>
#include "bc.h"

void au_bc_storage_init(struct au_bc_storage *bc_storage) {
    memset(bc_storage, 0, sizeof(struct au_bc_storage));
}

void au_bc_storage_del(struct au_bc_storage *bc_storage) {
    free(bc_storage->bc.data);
    memset(bc_storage, 0, sizeof(struct au_bc_storage));
}