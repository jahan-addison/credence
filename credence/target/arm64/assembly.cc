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

#include "assembly.h"
#include <credence/types.h> // for RValue, Label
#include <matchit.h>        // for matchit

namespace credence::target::arm64::assembly {

namespace m = matchit;

/*********************************/
/* arm64 directive constructors */
/*********************************/

Directive_Pair asciz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_str{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::asciz, rvalue });
    return { label, directives };
}

Directive_Pair floatz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_float{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    return { label, directives };
}

Directive_Pair doublez(std::size_t* index, type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    auto label =
        type::semantic::Label{ fmt::format("._L_double{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    return { label, directives };
}

Directives xword(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::xword, rvalue });
    return directives;
}

Directives word(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::word, rvalue });
    return directives;
}

Directives hword(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::word, rvalue });
    return directives;
}

Directives zero(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::space, rvalue });
    return directives;
}

Directives align(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::align, rvalue });
    return directives;
}

Directives float_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    return directives;
}

Directives double_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    return directives;
}

Directives string(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::string, rvalue });
    return directives;
}
// ---

/***********************************/
/* arm64 instruction constructors */
/***********************************/

// Instruction_Pair is the destination storage device, and std::deque of
// instructions
// ---

Instruction_Pair mul(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret_with_immediate(mul);
}

Instruction_Pair div(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret_with_immediate(sdiv);
}

Instruction_Pair mod(Storage const& s0, Storage const& s1)
{
    auto [sdiv_s, sdiv_i] = div(s0, s1);
    arm64_add__asm(sdiv_i, msub, s0, sdiv_s, s1, s0);
    return { sdiv_s, sdiv_i };
}

Instruction_Pair sub(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(sub, s0, s0, s1);
}

Instruction_Pair add(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(add, s0, s0, s1);
}

Instruction_Pair inc(Storage const& s0)
{
    arm64__make_and_ret(add, s0, s0, u32_int_immediate(1));
}

Instruction_Pair dec(Storage const& s0)
{
    arm64__make_and_ret(sub, s0, s0, u32_int_immediate(1));
}

Instruction_Pair neg(Storage const& s0)
{
    arm64__make_and_ret(neg, s0, s0);
}

Instruction_Pair neg(Storage const& s0, Storage const& s1)
{
    if (is_variant(Immediate, s1)) {
        auto sid = common::assembly::get_storage_as_string<Register>(s1);
        arm64__make_and_ret(
            mov, s0, direct_immediate(fmt::format("#-{}", sid)));
    } else
        arm64__make_and_ret(neg, s0, s1);
}

Instructions r_eq(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_eq, direct_immediate(to));
    return inst;
}

Instructions r_neq(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_ne, direct_immediate(to));
    return inst;
}

Instructions r_lt(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_lt, direct_immediate(to));
    return inst;
}

Instructions r_gt(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_gt, direct_immediate(to));
    return inst;
}

Instructions r_le(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_le, direct_immediate(to));
    return inst;
}

Instructions r_ge(Storage const& s0,
    Storage const& s1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, with, s0);
    arm64_add__asm(inst, cmp, with, s1);
    arm64_add__asm(inst, b_ge, direct_immediate(to));
    return inst;
}

Instruction_Pair rshift(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(lsr, s0, s1);
}

Instruction_Pair lshift(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(lsl, s0, s1);
}

Instruction_Pair b_xor(Storage const& s0, Storage const& s1)
{
    arm64__bitwise_and_ret_with_register_3ary(eor);
}

Instruction_Pair b_and(Storage const& s0, Storage const& s1)
{
    arm64__bitwise_and_ret_with_register_3ary(and_);
}

Instruction_Pair b_or(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(orr, s0, s0, s1);
}

Instruction_Pair b_not(Storage const& s0, Storage const& s1)
{
    if (is_variant(Immediate, s1))
        arm64__make_and_ret(movn, s0, s1);
    else
        arm64__make_and_ret(mvn, s0, s1);
}

Instruction_Pair u_not(Storage const& s0)
{
    auto inst = make_empty();
    arm64_add__asm(inst, mov, w8, s0);
    arm64_add__asm(inst, cmp, w8, u32_int_immediate(0));
    arm64_add__asm(inst, cset, w8, w8, direct_immediate("ne"));
    return { arm_rr(w8), inst };
}

Instruction_Pair lea(Storage const& s0, Storage const& s1)
{
    arm64__make_and_ret(add, s0, s1);
}

} // namespace arm64::detail