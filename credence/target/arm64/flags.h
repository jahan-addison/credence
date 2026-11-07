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

#include <credence/target/common/flags.h> // for Flag_Accessor, flags

/****************************************************************************
 * ARM64 Instruction Flags
 *
 * The flags of ARM64 instructions, after the common flags. Most mark an
 * operand that depends on the frame size, which is only known at the end of
 * the function, such as the pre-indexed store of the frame record.
 *
 *  Example:
 *
 *    stp x29, x30, [sp, #0]      // Align_SP
 *
 *  Becomes, in a frame of 32 bytes:
 *
 *    stp x29, x30, [sp, #-32]!
 *
 ****************************************************************************/

/**
 * Set an ARM64 flag on an instruction index, or on the next instruction
 */
#define set_alignment_flag_inline(flag_name, index) \
    flag_accessor.set_instruction_flag(arm64::detail::flags::flag_name, index)

#define set_alignment_flag(flag_name)              \
    accessor_->flag_accessor.set_instruction_flag( \
        arm64::detail::flags::flag_name,           \
        accessor_->instruction_accessor->size())

namespace credence::target::arm64::detail::flags {

/**
 * @brief ARM64 instruction flags
 *
 *  * Align_Folded - the second operand is the stack slot of a pointer, from
 *    the top of the frame
 *
 *  * Align_SP - the third operand is the pre-indexed frame allocation,
 *    `[sp, #-32]!`
 *
 *  * Align_SP_Folded - the third operand is the offset of a pointer, from
 *    the top of the frame
 *
 *  * Align_S2_Folded - the third operand is the frame size, such as the
 *    address of `argc` on Linux, `add x19, sp, #32`
 *
 *  * Align_S3_Folded - the fourth operand is the frame size
 *
 *  * Vector_Storage - the instruction accesses an element of a vector on
 *    the stack
 */
enum ARM64_Instruction_Flag : common::flag::flags
{
    Align_Folded = 1 << 7,
    Align_SP = 1 << 8,
    Align_SP_Folded = 1 << 9,
    Align_S3_Folded = 1 << 10,
    Vector_Storage = 1 << 13,
    Align_S2_Folded = 1 << 14,
};

} // namespace credence::target::arm64::detail::flags