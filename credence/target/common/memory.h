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
 * Memory Utilities
 *
 * Stack alignment and the classification of IR operands during code
 * generation. The stack pointer is 16-byte aligned at each call, as the
 * System V and ARM64 PCS calling conventions require.
 *
 *  Example:
 *
 *    main() {
 *      auto x, y;
 *      x = 5;
 *      y = x + 10;
 *      printf("%d %d\n", compute(1, 2), y);
 *    }
 *
 *  Becomes, three 4-byte locals and the 16-byte frame record on arm64:
 *
 *    stp x29, x30, [sp, #-32]!     // align_up_to(16 + 12, 16)
 *
 *  And on x86-64, where rbp is pushed before the locals:
 *
 *    push rbp
 *    mov rbp, rsp
 *    sub rsp, 24
 *
 ****************************************************************************/

namespace credence::target::common::memory {

/**
 * @brief Align a value up to the nearest multiple of a power of two
 *
 *  Examples:
 *
 *    align_up_to(24, 16)   // 32
 *    align_up_to(5, 0)     // 5
 */
constexpr std::size_t align_up_to(std::size_t value, std::size_t alignment)
{
    if (alignment == 0) {
        return value;
    }
    return (value + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief The position of an operand in an emitted instruction
 */
enum class Operand_Type
{
    Destination,
    Source
};

/**
 * Short-form predicates of IR operands, used in matchit patterns
 */

/**
 * @brief Check if an rvalue is a literal in data type form, `(5:int:4)`
 */
constexpr bool is_immediate(std::string const& rvalue)
{
    return type::is_rvalue_data_type(rvalue);
}

/**
 * @brief Check if an rvalue is a temporary, `_t1`
 */
constexpr bool is_temporary(std::string const& rvalue)
{
    return type::is_temporary(rvalue);
}

/**
 * @brief Check if an rvalue is an argument slot of a call, `_p1_1`
 *
 * An argument slot holds an argument before it is pushed, and is not a
 * parameter of the function under translation
 */
constexpr bool is_parameter(std::string const& rvalue)
{
    return rvalue.starts_with("_p");
}

/**
 * @brief Check if an rvalue is a vector element, `mess[1]`
 */
constexpr bool is_vector_offset(std::string const& rvalue)
{
    return util::contains(rvalue, "[") and util::contains(rvalue, "]");
}

} // namespace target::common::memory
