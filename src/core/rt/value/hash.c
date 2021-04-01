// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#include "core/hash.h"
#include "main.h"

uint32_t au_hash_value(au_value_t key) {
#ifdef AU_USE_NAN_TAGGING
    if (au_value_get_type(key) == AU_VALUE_STR) {
        struct au_string *ptr = au_value_get_string(key);
        return au_hash((uint8_t *)ptr->data, ptr->len);
    }
    return au_hash_u64(key._raw);
#else
    switch (au_value_get_type(key)) {
    case AU_VALUE_DOUBLE: {
        union {
            double d;
            uint64_t u;
        } u;
        u.d = au_value_get_double(key);
        return au_hash_u64(u.u);
    }
    case AU_VALUE_NONE: {
        return 2;
    }
    case AU_VALUE_INT: {
        int32_t value = au_value_get_int(key);
        return au_hash_u32((uint32_t)value);
    }
    case AU_VALUE_BOOL: {
        return au_value_get_bool(key);
    }
    case AU_VALUE_FN: {
        return au_hash_usize(au_value_get_fn(key));
    }
    case AU_VALUE_STR: {
        struct au_string *ptr = au_value_get_string(key);
        return au_hash((uint8_t *)ptr->data, ptr->len);
    }
    case AU_VALUE_STRUCT: {
        return au_hash_usize(au_value_get_struct(key));
    }
    case AU_VALUE_ERROR: {
        AU_UNREACHABLE;
    }
    }
    return 0;
#endif
}
