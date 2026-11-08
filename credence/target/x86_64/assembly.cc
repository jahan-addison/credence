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
 * x86-64 Assembly
 *
 * The registers, mnemonics, directives, and instruction constructors of the
 * x86-64 ISA, in Intel syntax. An instruction is a mnemonic and up to two
 * operands, and each constructor returns the register of its result and
 * its instructions.
 *
 *  Example:
 *
 *    auto [result, instructions] = assembly::mul(eax, Immediate{ "2" });
 *
 *  Becomes:
 *
 *    imul eax, 2      // result is eax
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   rax      = The default "accumulator" register for expression expansion,
 *              the return value, and the number of a syscall
 *   rdi, r8, r9, rsi, rdx, rcx
 *            = The arguments of a call to a function of the program
 *   rdi, rsi, rdx, rcx, r8, r9
 *            = The arguments of a call to the standard library, System V
 *   rdi, rsi, rdx, r10, r8, r9
 *            = The arguments of a syscall
 *   xmm0 - xmm7
 *            = The float and double arguments of the standard library
 *   xmm7     = The store register of a float or double literal
 *   rcx      = The address register of the address-of operator, and the
 *              shift count of a shift by a register, in cl
 *   rdx      = The high half of the dividend and the remainder of `idiv`
 *   r10      = The address register of a dereference
 *   r11      = The scratch register of the right-hand-side of a binary
 *              expression, the divisor of `idiv`, and the value of a
 *              global argument
 *   r15      = The address of argc, argv on the stack
 *   rbp      = The frame pointer
 *
 *   Locals, parameters, vectors, and vector offsets are always on the
 *   stack below rbp
 *
 *****************************************************************************/

namespace credence::target::x86_64::assembly {

namespace m = matchit;

/**
 * The directive constructors of the data section. A string, float, or
 * double literal has its own label, from the counter of literal labels.
 *
 *  Example:
 *
 *    asciz(&index, "hello")
 *
 *  Becomes:
 *
 *    ._L_str1__:
 *        .asciz "hello"
 */

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

Directives align(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::align, rvalue });
    return directives;
}

Directives p2align(type::semantic::RValue const& rvalue)
{
    auto directives = make_directives();
    directives.emplace_back(Data_Pair{ Directive::p2align, rvalue });
    return directives;
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

/**
 * The instruction constructors. Each returns the register of its result and
 * its instructions.
 *
 *  Example:
 *
 *    r_gt(dword ptr [rbp - 12], 10, "._L6__main", eax)
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 12]
 *    cmp eax, 10
 *    jg ._L6__main
 */

Instruction_Pair mul(Storage const& dest, Storage const& src)
{
    x8664__make_and_ret(imul, dest, src);
}

/**
 * @brief The signed division of the accumulator by a divisor
 *
 * The dividend is sign extended into edx:eax, or rdx:rax, and an immediate
 * divisor is moved to r11 first, since `idiv` takes no immediate
 *
 *  Example:
 *
 *    div(eax, 5)
 *
 *  Becomes:
 *
 *    mov r11d, 5
 *    cdq
 *    idiv r11d          // the quotient is in eax
 */
Instruction_Pair div(Storage const& dest, Storage const& src)
{
    auto inst = make_empty();
    bool qword = is_variant(Register, dest) and
                 is_qword_register(std::get<Register>(dest));
    auto divisor = src;
    if (is_variant(Immediate, divisor)) {
        auto scratch = qword ? Register::r11 : Register::r11d;
        x8664_add__asm(inst, mov, scratch, divisor);
        divisor = scratch;
    }
    if (qword)
        x8664_add__asm(inst, cqo);
    else
        x8664_add__asm(inst, cdq);
    x8664_add__asm(inst, idiv, divisor);
    return { qword ? x64_rr(rax) : x64_rr(eax), inst };
}

/**
 * @brief The remainder of the signed division of the accumulator
 *
 * The remainder of `idiv` is in edx, or rdx, and is moved to the
 * accumulator
 */
Instruction_Pair mod(Storage const& dest, Storage const& src)
{
    auto [quotient, inst] = div(dest, src);
    if (quotient == Storage{ Register::rax })
        x8664_add__asm(inst, mov, rax, rdx);
    else
        x8664_add__asm(inst, mov, eax, edx);
    return { quotient, inst };
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

/**
 * @brief The logical not of an operand, 1 when it is zero and 0 otherwise
 */
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