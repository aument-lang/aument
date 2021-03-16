// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#pragma once
#endif

struct au_fn_value;

/// Increases the reference count of an au_string
/// @param header the au_string instance
void au_fn_value_ref(struct au_fn_value *header);

/// Decreases the reference count of an au_string.
///     Automatically frees the au_string if the
///     reference count reaches 0.
/// @param header the au_string instance
void au_fn_value_deref(struct au_fn_value *header);
