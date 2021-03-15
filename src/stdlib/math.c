// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#include "core/rt/extern_fn.h"
#include <math.h>

#ifdef AU_FEAT_MATH_LIB

AU_EXTERN_FUNC_DECL(au_std_math_abs) {
    const au_value_t value = _args[0];
    switch (au_value_get_type(value)) {
    case AU_VALUE_DOUBLE:
        return au_value_double(fabs(au_value_get_double(value)));
    case AU_VALUE_INT:
        return au_value_int(abs(au_value_get_int(value)));
    default: {
        return value;
    }
    }
}

#define MIN_MAX_FUNC(NAME, COMPARISON)                                    \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t left = _args[0];                                 \
        const au_value_t right = _args[1];                                \
        const enum au_vtype left_type = au_value_get_type(left);          \
        const enum au_vtype right_type = au_value_get_type(right);        \
        if (left_type == AU_VALUE_INT && right_type == AU_VALUE_INT) {    \
            if (au_value_get_int(left)                                    \
                    COMPARISON au_value_get_int(right))                   \
                return left;                                              \
            return right;                                                 \
        } else if (left_type == AU_VALUE_DOUBLE &&                        \
                   right_type == AU_VALUE_DOUBLE) {                       \
            if (au_value_get_double(left)                                 \
                    COMPARISON au_value_get_double(right))                \
                return left;                                              \
            return right;                                                 \
        } else if (left_type == AU_VALUE_DOUBLE &&                        \
                   right_type == AU_VALUE_INT) {                          \
            if (au_value_get_double(left) COMPARISON(double)              \
                    au_value_get_int(right))                              \
                return left;                                              \
            return right;                                                 \
        } else if (left_type == AU_VALUE_INT &&                           \
                   right_type == AU_VALUE_DOUBLE) {                       \
            if ((double)au_value_get_int(left)                            \
                    COMPARISON au_value_get_double(right))                \
                return left;                                              \
            return right;                                                 \
        } else {                                                          \
            au_value_deref(right);                                        \
            return left;                                                  \
        }                                                                 \
    }
MIN_MAX_FUNC(au_std_math_max, >)
MIN_MAX_FUNC(au_std_math_min, <)
#undef MIN_MAX_FUNC

#define ONE_ARG_DOUBLE_FUNC(NAME, LIBC_FUNC)                              \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t value = _args[0];                                \
        double d = 0;                                                     \
        switch (au_value_get_type(value)) {                               \
        case AU_VALUE_INT:                                                \
            d = (double)au_value_get_int(value);                          \
            break;                                                        \
        case AU_VALUE_DOUBLE:                                             \
            d = au_value_get_double(value);                               \
            break;                                                        \
        default:                                                          \
            au_value_deref(value);                                        \
            return au_value_none();                                       \
        }                                                                 \
        return au_value_double(LIBC_FUNC(d));                             \
    }

#define TWO_ARG_DOUBLE_FUNC(NAME, LIBC_FUNC)                              \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t v1 = _args[0];                                   \
        const au_value_t v2 = _args[0];                                   \
        double d1 = 0, d2 = 0;                                            \
        switch (au_value_get_type(v1)) {                                  \
        case AU_VALUE_INT:                                                \
            d1 = (double)au_value_get_int(v1);                            \
            break;                                                        \
        case AU_VALUE_DOUBLE:                                             \
            d1 = au_value_get_double(v1);                                 \
            break;                                                        \
        default:                                                          \
            au_value_deref(v1);                                           \
            au_value_deref(v2);                                           \
            return au_value_none();                                       \
        }                                                                 \
        switch (au_value_get_type(v2)) {                                  \
        case AU_VALUE_INT:                                                \
            d2 = (double)au_value_get_int(v2);                            \
            break;                                                        \
        case AU_VALUE_DOUBLE:                                             \
            d2 = au_value_get_double(v2);                                 \
            break;                                                        \
        default:                                                          \
            au_value_deref(v1);                                           \
            au_value_deref(v2);                                           \
            return au_value_none();                                       \
        }                                                                 \
        return au_value_double(LIBC_FUNC(d1, d2));                        \
    }

// ** Exponential functions **
ONE_ARG_DOUBLE_FUNC(au_std_math_exp, exp)
ONE_ARG_DOUBLE_FUNC(au_std_math_ln, log)
ONE_ARG_DOUBLE_FUNC(au_std_math_log2, log2)
ONE_ARG_DOUBLE_FUNC(au_std_math_log10, log10)

// ** Power functions **
ONE_ARG_DOUBLE_FUNC(au_std_math_sqrt, sqrt)
ONE_ARG_DOUBLE_FUNC(au_std_math_cbrt, cbrt)
TWO_ARG_DOUBLE_FUNC(au_std_math_hypot, hypot)
TWO_ARG_DOUBLE_FUNC(au_std_math_pow, pow)

// ** Trigonometric functions **
ONE_ARG_DOUBLE_FUNC(au_std_math_sin, sin)
ONE_ARG_DOUBLE_FUNC(au_std_math_cos, cos)
ONE_ARG_DOUBLE_FUNC(au_std_math_tan, tan)
ONE_ARG_DOUBLE_FUNC(au_std_math_asin, asin)
ONE_ARG_DOUBLE_FUNC(au_std_math_acos, acos)
ONE_ARG_DOUBLE_FUNC(au_std_math_atan, atan)
TWO_ARG_DOUBLE_FUNC(au_std_math_atan2, atan2)

// ** Hyperbolic functions **
ONE_ARG_DOUBLE_FUNC(au_std_math_sinh, sinh)
ONE_ARG_DOUBLE_FUNC(au_std_math_cosh, cosh)
ONE_ARG_DOUBLE_FUNC(au_std_math_tanh, tanh)
ONE_ARG_DOUBLE_FUNC(au_std_math_asinh, asinh)
ONE_ARG_DOUBLE_FUNC(au_std_math_acosh, acosh)
ONE_ARG_DOUBLE_FUNC(au_std_math_atanh, atanh)

// ** Error & gamma functions **
ONE_ARG_DOUBLE_FUNC(au_std_math_erf, erf)
ONE_ARG_DOUBLE_FUNC(au_std_math_erfc, erfc)
ONE_ARG_DOUBLE_FUNC(au_std_math_lgamma, lgamma)
ONE_ARG_DOUBLE_FUNC(au_std_math_tgamma, tgamma)

// ** Rounding **
#define ROUND_FUNC(NAME, LIBC_FUNC)                                       \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t value = _args[0];                                \
        switch (au_value_get_type(value)) {                               \
        case AU_VALUE_INT:                                                \
            return value;                                                 \
        case AU_VALUE_DOUBLE:                                             \
            return au_value_double(                                       \
                LIBC_FUNC(au_value_get_double(value)));                   \
        default:                                                          \
            au_value_deref(value);                                        \
            return au_value_none();                                       \
        }                                                                 \
    }
ROUND_FUNC(au_std_math_ceil, ceil)
ROUND_FUNC(au_std_math_floor, floor)
ROUND_FUNC(au_std_math_trunc, trunc)
ROUND_FUNC(au_std_math_round, round)

// ** Classification **
#define CLASSIFY_FUNC(NAME, LIBC_FUNC)                                    \
    AU_EXTERN_FUNC_DECL(NAME) {                                           \
        const au_value_t value = _args[0];                                \
        switch (au_value_get_type(value)) {                               \
        case AU_VALUE_DOUBLE:                                             \
            return au_value_bool(LIBC_FUNC(au_value_get_double(value)));  \
        default:                                                          \
            au_value_deref(value);                                        \
            return au_value_bool(0);                                      \
        }                                                                 \
    }
CLASSIFY_FUNC(au_std_math_is_finite, isfinite)
CLASSIFY_FUNC(au_std_math_is_infinite, isinf)
CLASSIFY_FUNC(au_std_math_is_nan, isnan)
CLASSIFY_FUNC(au_std_math_is_normal, isnormal)

#endif