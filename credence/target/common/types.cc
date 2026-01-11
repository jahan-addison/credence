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

/****************************************************************************
 *
 * Common type implementation
 *
 * Implements PIMPL for base_stack_pointer to reduce compilation dependencies
 * and improve build times. Base class for architecture-specific stack
 * implementations.
 *
 *****************************************************************************/

#include "types.h"

namespace credence::target::common::detail {

struct base_stack_pointer::impl
{
    using Offset = Stack_Offset;
};

base_stack_pointer::base_stack_pointer()
    : pimpl{ std::make_unique<impl>() }
{
}
base_stack_pointer::~base_stack_pointer() = default;
base_stack_pointer::base_stack_pointer(base_stack_pointer&&) noexcept = default;
base_stack_pointer& base_stack_pointer::operator=(
    base_stack_pointer&&) noexcept = default;

} // namespace credence::target::common::detail
