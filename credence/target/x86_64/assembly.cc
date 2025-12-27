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

namespace credence::target::x86_64::assembly {

namespace m = matchit;

/**
 * @brief Helper function for trivial 2-ary mnemonic instructions
 *
 *   Example:
 *
 *    add rax, rdi
 *    imul rax, [rbp - 4]
 */
Instruction_Pair add_2ary_inst(Mnemonic mnemonic,
    Storage const& dest,
    Storage const& src)
{
    auto instructions = make_empty();
    x64_add_asm__(instructions, mnemonic, dest, src);
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
    x64_add_asm__(instructions, mnemonic, src, O_NUL);
    return { src, instructions };
}

/*********************************/
/* x86-64 directive constructors */
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

Directives quad(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::quad, rvalue });
    return directives;
}

Directives long_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::long_, rvalue });
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

Directives byte_(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::byte_, rvalue });
    return directives;
}

// ---

/***********************************/
/* x86-64 instruction constructors */
/***********************************/

// Instruction_Pair is the destination storage device, and std::deque of
// instructions
// ---

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(imul), dest, src);
}

Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    x64_asm__zero_o(inst, cdq);
    x64_add_asm__as(inst, mov, dest, src);
    x64_asm__dest(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    x64_asm__zero_o(inst, cdq);
    x64_add_asm__as(inst, mov, dest, src);
    x64_asm__dest(inst, idiv, dest);
    return { x64_rr(edx), inst };
}

Instruction_Pair sub(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(Mnemonic::sub, dest, src);
}

Instruction_Pair add(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(Mnemonic::add, dest, src);
}

Instruction_Pair inc(Storage const& dest)
{
    return add_1ary_inst(x64_mn(inc), dest);
}

Instruction_Pair dec(Storage const& dest)
{
    return add_1ary_inst(x64_mn(dec), dest);
}

Instruction_Pair neg(Storage const& dest)
{
    return add_1ary_inst(x64_mn(neg), dest);
}

Instructions r_eq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, je, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_neq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, jne, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_lt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, jl, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_gt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, jg, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_le(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, jle, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instructions r_ge(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x64_add_asm__as(inst, mov, with, dest);
    x64_add_asm__as(inst, cmp, with, src);
    x64_add_asm__as(
        inst, jge, common::assembly::make_direct_immediate(to), O_NUL);
    return inst;
}

Instruction_Pair rshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(shr), dest, src);
}

Instruction_Pair lshift(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(shl), dest, src);
}

Instruction_Pair b_and(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(and_), dest, src);
}

Instruction_Pair b_or(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(or_), dest, src);
}

Instruction_Pair b_xor(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(xor_), dest, src);
}

Instruction_Pair b_not(Storage const& dest)
{
    return add_1ary_inst(x64_mn(not_), dest);
}

Instruction_Pair u_not(Storage const& dest)
{
    auto inst = make_empty();
    x64_asm__dest_rs(inst, mov, eax, dest);
    x64_asm__dest_rs(
        inst, cmp, eax, common::assembly::make_numeric_immediate(0));
    x64_asm__dest_s(inst, setne, al);
    x64_add_asm__as(
        inst, xor_, x64_rr(al), common::assembly::make_numeric_immediate(-1));
    x64_add_asm__as(
        inst, and_, x64_rr(al), common::assembly::make_numeric_immediate(1));
    x64_asm__short(inst, movzx, eax, al);
    return { x64_rr(eax), inst };
}

Instruction_Pair lea(Storage const& dest, Storage const& src)
{
    return add_2ary_inst(x64_mn(lea), dest, src);
}

} // namespace x86_64::detail