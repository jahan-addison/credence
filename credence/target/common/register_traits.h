/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include <credence/types.h>

namespace credence::target::common {

/**
 * @brief Register traits policy for architecture-specific register mapping
 *
 * Each architecture should specialize this template to provide:
 * - default_accumulator: The default accumulator register
 * - get_accumulator_from_size: Map storage size to accumulator register
 * - get_second_register_from_size: Map size to a second working register
 */
template<typename Register_Type, typename Size_Type>
struct Register_Traits;

} // namespace credence::target::common
