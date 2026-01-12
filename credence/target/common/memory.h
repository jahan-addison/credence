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

#include "stack_frame.h"
#include "types.h"

#include <credence/types.h>
#include <credence/util.h>
#include <memory>
#include <string>

/****************************************************************************
 *
 * Utilities for memory alignment (stack, data sections) and operand type
 * classification during code generation. Ensures ABI compliance for both
 * System V (x86-64) and ARM64 PCS calling conventions.
 *
 * Example - stack alignment:
 *
 *   add(a, b, c, d) {
 *     auto x, y, z;
 *     ...
 *   }
 *
 * Stack must be 16-byte aligned
 *   - Calculate local variable space: 3 * 8 = 24 bytes
 *   - Align to 16: align_up_to(24, 16) = 32 bytes
 *   - Emit: sub rsp, 32 (x86-64) or sub sp, sp, #32 (ARM64)
 *
 ****************************************************************************/

namespace credence::target::common::memory {

/**
 * @brief Align a value up to the nearest multiple of another value.
 */
constexpr std::size_t align_up_to(std::size_t value, std::size_t alignment)
{
    if (alignment == 0) {
        return value;
    }
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Operand type classification for instruction emission
 */
enum class Operand_Type
{
    Destination,
    Source
};

/**
 * @brief Short-form helpers for matchit predicate pattern matching
 *
 * These predicates are used to classify RValue operands during code generation.
 */

/**
 * @brief Check if an RValue is an immediate value
 */
constexpr bool is_immediate(std::string const& rvalue)
{
    return type::is_rvalue_data_type(rvalue);
}

/**
 * @brief Check if an RValue is a temporary value
 */
constexpr bool is_temporary(std::string const& rvalue)
{
    return type::is_temporary(rvalue);
}

/**
 * @brief Check if an RValue is a function parameter
 */
constexpr bool is_parameter(std::string const& rvalue)
{
    return rvalue.starts_with("_p");
}

/**
 * @brief Check if an RValue is a vector/array offset access
 */
constexpr bool is_vector_offset(std::string const& rvalue)
{
    return util::contains(rvalue, "[") and util::contains(rvalue, "]");
}

} // namespace target::common::memory
