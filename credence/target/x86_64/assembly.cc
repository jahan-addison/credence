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

/****************************************************************************
 *
 * x86-64 Assembly Constructors and Mnemonics
 *
 * x86-64 instructions, registers, and mnemonics in intel syntax. Provides
 * instruction string formatting and operand constructors and macros.
 *
 * Example registers:
 *   64-bit: rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8-r15
 *   32-bit: eax, ebx, ecx, edx, esi, edi, ...
 *   16-bit: ax, bx, cx, dx, ...
 *   8-bit:  al, bl, cl, dl, ...
 *
 * Example instructions:
 *   Data movement: mov, lea, push, pop
 *   Arithmetic: add, sub, mul, imul, div, idiv
 *   Bitwise: and, or, xor, not, shl, shr
 *   Comparison: cmp, test
 *   Control flow: jmp, je, jne, jg, jl, call, ret
 *
 *****************************************************************************/

namespace credence::target::x86_64::assembly {

namespace m = matchit;

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

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(imul, dest, src);
}

Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    x8664_add__asm(inst, cdq);
    x8664_add__asm(inst, mov, dest, src);
    x8664_add__asm(inst, idiv, dest);
    return { src, inst };
}

Instruction_Pair mod(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    x8664_add__asm(inst, cdq);
    x8664_add__asm(inst, mov, dest, src);
    x8664_add__asm(inst, idiv, dest);
    return { x64_rr(edx), inst };
}

Instruction_Pair sub(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(sub, dest, src);
}

Instruction_Pair add(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(add, dest, src);
}

Instruction_Pair inc(Storage const& dest)
{
    x8664__make_and_ret(inc, dest);
}

Instruction_Pair dec(Storage const& dest)
{
    x8664__make_and_ret(dec, dest);
}

Instruction_Pair neg(Storage const& dest)
{
    x8664__make_and_ret(neg, dest);
}

Instructions r_eq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, je, direct_immediate(to));
    return inst;
}

Instructions r_neq(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, jne, direct_immediate(to));
    return inst;
}

Instructions r_lt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, jl, direct_immediate(to));
    return inst;
}

Instructions r_gt(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, jg, direct_immediate(to));
    return inst;
}

Instructions r_le(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, jle, direct_immediate(to));
    return inst;
}

Instructions r_ge(Storage const& dest,
    Storage const& src,
    Label const& to,
    x86_64::assembly::Register const& with)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, with, dest);
    x8664_add__asm(inst, cmp, with, src);
    x8664_add__asm(inst, jge, direct_immediate(to));
    return inst;
}

Instruction_Pair rshift(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(shr, dest, src);
}

Instruction_Pair lshift(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(shl, dest, src);
}

Instruction_Pair b_and(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(and_, dest, src);
}

Instruction_Pair b_or(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(or_, dest, src);
}

Instruction_Pair b_xor(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(xor_, dest, src);
}

Instruction_Pair b_not(Storage const& dest)
{
    x8664__make_and_ret(not_, dest);
}

Instruction_Pair u_not(Storage const& dest)
{
    auto inst = make_empty();
    x8664_add__asm(inst, mov, eax, dest);
    x8664_add__asm(inst, cmp, eax, common::assembly::make_numeric_immediate(0));
    x8664_add__asm(inst, setne, al);
    x8664_add__asm(
        inst, xor_, al, common::assembly::make_numeric_immediate(-1));
    x8664_add__asm(inst, and_, al, common::assembly::make_numeric_immediate(1));
    x8664_add__asm(inst, movzx, eax, al);
    return { x64_rr(eax), inst };
}

Instruction_Pair lea(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(lea, dest, src);
}

} // namespace x86_64::detail