// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information

#pragma once

#include "core/rt/extern_fn.h"

/// [func-au] Returns the absolute value of a number
/// @name math::abs
/// @param n a number (integer/float)
/// @return the absolute value of `n`
AU_EXTERN_FUNC_DECL(au_std_math_abs);

/// [func-au] Returns the maximum value between 2 arguments
/// @name math::max
/// @param a a number (integer/float)
/// @param b a number (integer/float)
/// @return maximum of a or b
AU_EXTERN_FUNC_DECL(au_std_math_max);

/// [func-au] Returns the minimum value between 2 arguments
/// @name math::min
/// @param a a number (integer/float)
/// @param b a number (integer/float)
/// @return minimum of a or b
AU_EXTERN_FUNC_DECL(au_std_math_min);

// ** Exponential functions **
/// [func-au]
/// @name math::exp
AU_EXTERN_FUNC_DECL(au_std_math_exp);

/// [func-au]
/// @name math::exp
AU_EXTERN_FUNC_DECL(au_std_math_ln);

/// [func-au]
/// @name math::log2
AU_EXTERN_FUNC_DECL(au_std_math_log2);

/// [func-au]
/// @name math::log10
AU_EXTERN_FUNC_DECL(au_std_math_log10);

// ** Power functions **
/// [func-au]
/// @name math::sqrt
AU_EXTERN_FUNC_DECL(au_std_math_sqrt);

/// [func-au]
/// @name math::cbrt
AU_EXTERN_FUNC_DECL(au_std_math_cbrt);

/// [func-au]
/// @name math::hypot
AU_EXTERN_FUNC_DECL(au_std_math_hypot);

/// [func-au]
/// @name math::pow
AU_EXTERN_FUNC_DECL(au_std_math_pow);

// ** Trigonometric functions **
/// [func-au]
/// @name math::sin
AU_EXTERN_FUNC_DECL(au_std_math_sin);

/// [func-au]
/// @name math::cos
AU_EXTERN_FUNC_DECL(au_std_math_cos);

/// [func-au]
/// @name math::tan
AU_EXTERN_FUNC_DECL(au_std_math_tan);

/// [func-au]
/// @name math::asin
AU_EXTERN_FUNC_DECL(au_std_math_asin);

/// [func-au]
/// @name math::acos
AU_EXTERN_FUNC_DECL(au_std_math_acos);

/// [func-au]
/// @name math::atan
AU_EXTERN_FUNC_DECL(au_std_math_atan);

/// [func-au]
/// @name math::atan2
AU_EXTERN_FUNC_DECL(au_std_math_atan2);

// ** Hyperbolic functions **
/// [func-au]
/// @name math::sinh
AU_EXTERN_FUNC_DECL(au_std_math_sinh);

/// [func-au]
/// @name math::cosh
AU_EXTERN_FUNC_DECL(au_std_math_cosh);

/// [func-au]
/// @name math::tanh
AU_EXTERN_FUNC_DECL(au_std_math_tanh);

/// [func-au]
/// @name math::asinh
AU_EXTERN_FUNC_DECL(au_std_math_asinh);

/// [func-au]
/// @name math::acosh
AU_EXTERN_FUNC_DECL(au_std_math_acosh);

/// [func-au]
/// @name math::atanh
AU_EXTERN_FUNC_DECL(au_std_math_atanh);

// ** Error & gamma functions **
/// [func-au]
/// @name math::erf
AU_EXTERN_FUNC_DECL(au_std_math_erf);

/// [func-au]
/// @name math::erfc
AU_EXTERN_FUNC_DECL(au_std_math_erfc);

/// [func-au]
/// @name math::lgamma
AU_EXTERN_FUNC_DECL(au_std_math_lgamma);

/// [func-au]
/// @name math::tgamma
AU_EXTERN_FUNC_DECL(au_std_math_tgamma);

// ** Rounding **
/// [func-au]
/// @name math::ceil
AU_EXTERN_FUNC_DECL(au_std_math_ceil);
/// [func-au]
/// @name math::floor
AU_EXTERN_FUNC_DECL(au_std_math_floor);
/// [func-au]
/// @name math::trunc
AU_EXTERN_FUNC_DECL(au_std_math_trunc);
/// [func-au]
/// @name math::round
AU_EXTERN_FUNC_DECL(au_std_math_round);

// ** Classification **
/// [func-au]
/// @name math::is_finite
AU_EXTERN_FUNC_DECL(au_std_math_is_finite);
/// [func-au]
/// @name math::is_infinite
AU_EXTERN_FUNC_DECL(au_std_math_is_infinite);
/// [func-au]
/// @name math::is_nan
AU_EXTERN_FUNC_DECL(au_std_math_is_nan);
/// [func-au]
/// @name math::is_normal
AU_EXTERN_FUNC_DECL(au_std_math_is_normal);
