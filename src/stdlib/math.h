// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

AU_EXTERN_FUNC_DECL(au_std_math_abs);
AU_EXTERN_FUNC_DECL(au_std_math_max);
AU_EXTERN_FUNC_DECL(au_std_math_min);

// ** Exponential functions **
AU_EXTERN_FUNC_DECL(au_std_math_exp);
AU_EXTERN_FUNC_DECL(au_std_math_ln);
AU_EXTERN_FUNC_DECL(au_std_math_log2);
AU_EXTERN_FUNC_DECL(au_std_math_log10);

// ** Power functions **
AU_EXTERN_FUNC_DECL(au_std_math_sqrt);
AU_EXTERN_FUNC_DECL(au_std_math_cbrt);
AU_EXTERN_FUNC_DECL(au_std_math_hypot);
AU_EXTERN_FUNC_DECL(au_std_math_pow);

// ** Trigonometric functions **
AU_EXTERN_FUNC_DECL(au_std_math_sin);
AU_EXTERN_FUNC_DECL(au_std_math_cos);
AU_EXTERN_FUNC_DECL(au_std_math_tan);
AU_EXTERN_FUNC_DECL(au_std_math_asin);
AU_EXTERN_FUNC_DECL(au_std_math_acos);
AU_EXTERN_FUNC_DECL(au_std_math_atan);
AU_EXTERN_FUNC_DECL(au_std_math_atan2);

// ** Hyperbolic functions **
AU_EXTERN_FUNC_DECL(au_std_math_sinh);
AU_EXTERN_FUNC_DECL(au_std_math_cosh);
AU_EXTERN_FUNC_DECL(au_std_math_tanh);
AU_EXTERN_FUNC_DECL(au_std_math_asinh);
AU_EXTERN_FUNC_DECL(au_std_math_acosh);
AU_EXTERN_FUNC_DECL(au_std_math_atanh);

// ** Error & gamma functions **
AU_EXTERN_FUNC_DECL(au_std_math_erf);
AU_EXTERN_FUNC_DECL(au_std_math_erfc);
AU_EXTERN_FUNC_DECL(au_std_math_lgamma);
AU_EXTERN_FUNC_DECL(au_std_math_tgamma);

// ** Rounding **
AU_EXTERN_FUNC_DECL(au_std_math_ceil);
AU_EXTERN_FUNC_DECL(au_std_math_floor);
AU_EXTERN_FUNC_DECL(au_std_math_trunc);
AU_EXTERN_FUNC_DECL(au_std_math_round);

// ** Classification **
AU_EXTERN_FUNC_DECL(au_std_math_is_finite);
AU_EXTERN_FUNC_DECL(au_std_math_is_infinite);
AU_EXTERN_FUNC_DECL(au_std_math_is_nan);
AU_EXTERN_FUNC_DECL(au_std_math_is_normal);
