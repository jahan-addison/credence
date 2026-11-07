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

#include "flags.h"

#include <credence/error.h>               // for credence_assert
#include <credence/map.h>                 // for Ordered_Map
#include <credence/target/common/types.h> // for Enum_T, Instructions

/****************************************************************************
 * Instruction Flags
 *
 * Bit flags on the index of an emitted instruction, set by an inserter and
 * read by the emitter. A flag marks what is only known after the instruction
 * is inserted, such as the stack frame size of an alignment operand, or the
 * operand size of a memory access. Flags are combined with bitwise OR.
 *
 *  Example, on x86-64:
 *
 *    sub rsp, 0      // flag::Align
 *
 *  Becomes, once the frame size of the function is known:
 *
 *    sub rsp, 24
 *
 ****************************************************************************/

namespace credence::target::common {

/**
 * @brief Add a flag to the flags of an instruction
 */
void Flag_Accessor::set_instruction_flag(flag::Instruction_Flag set_flag,
    unsigned int index)
{
    if (!instruction_flag.contains(index))
        instruction_flag[index] = set_flag;
    else
        instruction_flag[index] |= set_flag;
}

/**
 * @brief Remove one or more flags from the flags of an instruction
 */
void Flag_Accessor::unset_instruction_flag(flag::Instruction_Flag set_flag,
    unsigned int index)
{
    if (instruction_flag.contains(index)) {
        auto flags = get_instruction_flags_at_index(index);
        if (flags & set_flag)
            set_instruction_flag(flags & ~set_flag, index);
    }
}

void Flag_Accessor::unset_instruction_flag(flag::flags unset_flags,
    unsigned int index)
{
    if (instruction_flag.contains(index)) {
        auto flags = get_instruction_flags_at_index(index);
        if (flags & unset_flags)
            set_instruction_flag(flags & ~unset_flags, index);
    }
}

/**
 * @brief Set the flags of an instruction
 *
 * The flags replace those of the instruction, unless the next instruction
 * has flags, where they are added. The unset functions rely on the
 * replacement.
 */
void Flag_Accessor::set_instruction_flag(flag::flags flags, unsigned int index)
{
    if (!instruction_flag.contains(index + 1))
        instruction_flag[index] = flags;
    else
        instruction_flag[index] |= flags;
}

/**
 * @brief Move the Load flag from the last instruction to the next one
 *
 * An instruction inserted after a load takes its place, so the flag follows
 * the instruction that uses the address
 */
template<Enum_T Mnemonic_T, Enum_T Registers_T>
void Flag_Accessor::set_load_address_from_previous_instruction(
    Instructions<Mnemonic_T, Registers_T>& instructions)
{
    credence_assert(instructions.size() > 1);
    auto last_instruction_flags = instruction_flag.at(instructions.size() - 1);
    if (instructions.size() > 1 and last_instruction_flags & flag::Load) {
        set_instruction_flag(
            last_instruction_flags &= ~flag::Load, instructions.size() - 1);
        set_instruction_flag(flag::Load, instructions.size());
    }
}

}