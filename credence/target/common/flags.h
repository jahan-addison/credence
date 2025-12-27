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

#include "types.h"        // for Enum_T, Instructions
#include <credence/map.h> // for Ordered_Map

#pragma once

namespace credence::target::common {

namespace flag {

using flags = unsigned int;

/**
 * @brief Common instruction flags shared across architectures
 */
enum Instruction_Flag : flags
{
    None = 0,
    Address = 1 << 0,
    Indirect = 1 << 1,
    Indirect_Source = 1 << 2,
    Align = 1 << 3,
    Argument = 1 << 4,
    QWord_Dest = 1 << 5,
    Load = 1 << 6
};

} // namespace flags

/**
 * @brief Flag accessor for bit flags to emitter settings on instruction indices
 */
struct Flag_Accessor
{
    void set_instruction_flag(flag::Instruction_Flag set_flag,
        unsigned int index);
    void unset_instruction_flag(flag::Instruction_Flag set_flag,
        unsigned int index);
    template<Enum_T Mnemonic_T, Enum_T Registers_T>
    void set_load_address_from_previous_instruction(
        Instructions<Mnemonic_T, Registers_T>& instructions);
    void set_instruction_flag(flag::flags flags, unsigned int index);
    constexpr bool index_contains_flag(unsigned int index,
        flag::Instruction_Flag flag)
    {
        if (!instruction_flag.contains(index))
            return false;
        return instruction_flag.at(index) & flag;
    }
    flag::flags get_instruction_flags_at_index(unsigned int index)
    {
        if (!instruction_flag.contains(index))
            return 0;
        return instruction_flag.at(index);
    }

  private:
    Ordered_Map<unsigned int, flag::flags> instruction_flag{};
};

} // namespace target::common::flag
