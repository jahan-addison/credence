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

/**
 * @brief Helper function for trivial 2-ary mnemonic instructions
 *
 *   Example:
 *
 *    add x0, x1
 *    mul x0, [sp, #-4]
 */
Instruction_Pair add_2ary_inst(Mnemonic mnemonic,
    Storage const& dest,
    Storage const& src)
{
    auto instructions = make_empty();
    arm_add_asm__(instructions, mnemonic, dest, src);
    return { dest, instructions };
}

/**
 * @brief Helper function for trivial 1-ary mnemonic instructions
 *
 *   Example:
 *
 *    idiv edi
 */
Instruction_Pair add_1ary_inst(Mnemonic mnemonic, Storage const& src)
{
    auto instructions = make_empty();
    arm_add_asm__(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

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

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, mul, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, sdiv, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair mod(Storage const& /*dest*/, Storage const& /*src*/)
{
    // TODO: Implement modulo for ARM64
    return {};
}

Instruction_Pair sub(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, sub, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair add(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, add, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair inc(Storage const& dest)
{
    auto inst = make_empty();
    arm_add_asm__as3(
        inst, add, dest, dest, common::assembly::make_numeric_immediate(1));
    return { dest, inst };
}

Instruction_Pair dec(Storage const& dest)
{
    auto inst = make_empty();
    arm_add_asm__as3(
        inst, sub, dest, dest, common::assembly::make_numeric_immediate(1));
    return { dest, inst };
}

Instruction_Pair neg(Storage const& dest)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, sub, dest, arm_rr(xzr), dest);
    return { dest, inst };
}

Instructions r_eq(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_eq, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_neq(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_ne, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_lt(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_lt, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_gt(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_gt, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_le(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_le, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_ge(Storage const& dest,
    Storage const& src,
    Label const& to,
    arm64::assembly::Register const& with)
{
    auto inst = make_empty();
    arm_add_asm__as(inst, mov, with, dest);
    arm_add_asm__as(inst, cmp, with, src);
    arm_add_asm__as(
        inst, b_ge, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instruction_Pair rshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(arm_mn(lsr), dest, src);
}

Instruction_Pair lshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(arm_mn(lsl), dest, src);
}

Instruction_Pair b_and(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, and_, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair b_or(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, orr, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair b_xor(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    arm_add_asm__as3(inst, eor, dest, dest, src);
    return { dest, inst };
}

Instruction_Pair b_not(Storage const& dest)
{
    return add_1ary_inst(arm_mn(mvn), dest);
}

Instruction_Pair u_not(Storage const& dest)
{
    auto inst = make_empty();
    arm_asm__dest_rs(inst, mov, w0, dest);
    arm_asm__dest_rs(
        inst, cmp, w0, common::assembly::make_numeric_immediate(0));
    arm_add_asm__as3(inst,
        cset,
        arm_rr(w0),
        arm_rr(w0),
        common::assembly::make_direct_immediate("ne"));
    return { arm_rr(w0), inst };
}

Instruction_Pair lea(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(arm_mn(add), dest, src);
}

} // namespace arm64::detail