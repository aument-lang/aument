// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "au_stdlib.h"
#include "core/program.h"
#include "core/vm/vm.h"

#include "array.h"
#include "gc.h"
#include "io.h"
#include "list.h"
#include "math.h"
#include "str.h"
#include "types.h"

#ifdef AU_TEST
#include "test_fns.h"
#endif

au_extern_module_t au_stdlib_module() {
    au_extern_module_t module = au_extern_module_new();
    return module;
}

void au_install_stdlib(struct au_program_data *data) {
    au_hm_var_value_t *old =
        au_hm_vars_add(&data->imported_module_map, "std",3,
                       data->imported_modules.len);
    assert(old == 0);
    struct au_imported_module module = {0};
    au_imported_module_init(&module, 0);
    au_imported_module_array_add(&data->imported_modules, module);
}
