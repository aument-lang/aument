// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#endif

#ifdef _MSC_VER
#define AU_UNUSED
#else
#define AU_UNUSED __attribute__((unused))
#endif

#ifdef _MSC_VER
#define AU_NO_RETURN __declspec(noreturn)
#else
#define AU_NO_RETURN __attribute__((noreturn))
#endif

#ifdef _MSC_VER
#define AU_ALWAYS_INLINE __forceinline
#else
#define AU_ALWAYS_INLINE __attribute__((always_inline)) inline
#endif

#ifdef _MSC_VER
#define AU_UNREACHABLE __assume(0)
#else
#define AU_UNREACHABLE __builtin_unreachable()
#endif

#define AU_LIKELY(x) __builtin_expect(!!(x), 1)
#define AU_UNLIKELY(x) __builtin_expect(!!(x), 0)

#ifdef _Thread_local
#define AU_THREAD_LOCAL _Thread_local
#else
#define AU_THREAD_LOCAL
#endif

#if defined(_WIN32)
#ifdef _AUMENT_H
#define AU_PUBLIC __declspec(dllimport)
#define AU_PRIVATE
#else
#define AU_PUBLIC __declspec(dllexport)
#define AU_PRIVATE
#endif
#else
#define AU_PUBLIC __attribute__((visibility("default")))
#define AU_PRIVATE __attribute__((visibility("hidden")))
#endif

#ifdef static_assert
#define AU_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define AU_STATIC_ASSERT(cond, msg) assert(cond, msg)
#endif
