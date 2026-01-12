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

#include <credence/target/common/flags.h>

#pragma once

/****************************************************************************
 *
 * ARM64-specific Instruction Flags
 *
 * Additional instruction flags specific to ARM64 code generation.
 * Extends common flags with ARM64-specific addressing modes and
 * instruction variants.
 *
 * Used to mark instructions that need additional flags for ARM64's
 * load/store addressing modes, immediate encoding restrictions, and
 * register pair operations.
 *
 *****************************************************************************/

#define set_alignment_flag_inline(flag_name, index) \
    flag_accessor.set_instruction_flag(arm64::detail::flags::flag_name, index)

#define set_alignment_flag(flag_name)              \
    accessor_->flag_accessor.set_instruction_flag( \
        arm64::detail::flags::flag_name,           \
        accessor_->instruction_accessor->size())

namespace credence::target::arm64::detail::flags {

enum ARM64_Instruction_Flag : common::flag::flags
{

    Align_Folded = 1 << 7,
    Align_SP = 1 << 8,
    Align_SP_Folded = 1 << 9,
    Align_S3_Folded = 1 << 10,
    Align_SP_Local = 1 << 11,
    Callee_Saved = 1 << 12
};

} // namespace credence::target::arm64::detail::flags