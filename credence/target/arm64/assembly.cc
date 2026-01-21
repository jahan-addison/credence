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
 * ARM64 Assembly Instructions and Mnemonics
 *
 * Defines ARM64 instructions, registers, and mnemonics. Provides
 * instruction formatting and operand helpers for ARM64 ISA.
 *
 * Example instructions:
 *   Data movement: mov, ldr, str, ldp, stp
 *   Arithmetic: add, sub, mul, sdiv, udiv
 *   Bitwise: and, orr, eor, mvn, lsl, lsr
 *   Comparison: cmp, tst
 *   Control flow: b, b.eq, b.ne, b.gt, b.lt, bl, ret
 *
 *****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x6  = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x15      = Second data section register
 *   x7       = multiplication scratch register
 *   x8       = The default "accumulator" register for expression expansion
 *   x10      = The stack move register; additional scratch register
 *   x9 - x18 = If there are no function calls in a stack frame, local scope
 *             variables are stored in x9-x18, after which the stack is used
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

#define MAKE_D() auto directives = make_directives()
#define MAKE_I() auto inst = make_empty()
#define DONE_D() return directives
#define DONE_I() return inst
#define DONE_D_L()        \
    return                \
    {                     \
        label, directives \
    }

namespace credence::target::arm64::assembly {

namespace m = matchit;

std::tuple<std::deque<Register>,
    std::deque<Register>,
    std::deque<Register>,
    std::deque<Register>>
get_available_argument_register()
{
    std::deque<assembly::Register> x = { assembly::Register::x7,
        assembly::Register::x6,
        assembly::Register::x5,
        assembly::Register::x4,
        assembly::Register::x3,
        assembly::Register::x2,
        assembly::Register::x1,
        assembly::Register::x0 };

    std::deque<assembly::Register> w = { assembly::Register::w7,
        assembly::Register::w6,
        assembly::Register::w5,
        assembly::Register::w4,
        assembly::Register::w3,
        assembly::Register::w2,
        assembly::Register::w1,
        assembly::Register::w0 };

    std::deque<assembly::Register> s = { assembly::Register::s7,
        assembly::Register::s6,
        assembly::Register::s5,
        assembly::Register::s4,
        assembly::Register::s3,
        assembly::Register::s2,
        assembly::Register::s1,
        assembly::Register::s0 };

    std::deque<assembly::Register> d = { assembly::Register::d7,
        assembly::Register::d6,
        assembly::Register::d5,
        assembly::Register::d4,
        assembly::Register::d3,
        assembly::Register::d2,
        assembly::Register::d1,
        assembly::Register::d0 };

    return { x, w, s, d };
}

/*********************************/
/* arm64 directive constructors */
/*********************************/

Directive_Pair asciz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    MAKE_D();
    auto label =
        type::semantic::Label{ fmt::format("._L_str{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::asciz, rvalue });
    DONE_D_L();
}

Directive_Pair floatz(std::size_t* index, type::semantic::RValue const& rvalue)
{
    MAKE_D();
    auto label =
        type::semantic::Label{ fmt::format("._L_float{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    DONE_D_L();
}

Directive_Pair doublez(std::size_t* index, type::semantic::RValue const& rvalue)
{
    MAKE_D();
    auto label =
        type::semantic::Label{ fmt::format("._L_double{}__", ++(*index)) };
    directives.emplace_back(label);
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    DONE_D_L();
}

Directives xword(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::xword, rvalue });
    DONE_D();
}

Directives word(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::word, rvalue });
    DONE_D();
}

Directives hword(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::word, rvalue });
    DONE_D();
}

Directives zero(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::space, rvalue });
    DONE_D();
}

Directives align(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::align, rvalue });
    DONE_D();
}

Directives p2align(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::p2align, rvalue });
    DONE_D();
}

Directives float_(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::float_, rvalue });
    DONE_D();
}

Directives double_(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::double_, rvalue });
    DONE_D();
}

Directives long_(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::long_, rvalue });
    DONE_D();
}

Directives string(type::semantic::RValue const& rvalue)
{
    MAKE_D();
    directives.emplace_back(Data_Pair{ Directive::string, rvalue });
    DONE_D();
}
// ---

/***********************************/
/* arm64 instruction constructors */
/***********************************/

Instruction_Pair mul(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret_with_immediate(mul);
}

Instruction_Pair div(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret_with_immediate(sdiv);
}

Instruction_Pair mod(Storage const& ss0, Storage const& ss1)
{
    auto [sdiv_s, sdiv_i] = div(ss0, ss1);
    arm64_add__asm(sdiv_i, msub, ss0, sdiv_s, ss1, ss0);
    return { sdiv_s, sdiv_i };
}

Instruction_Pair sub(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret(sub, ss0, ss0, ss1);
}

Instruction_Pair add(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret(add, ss0, ss0, ss1);
}

Instruction_Pair inc(Storage const& ss0)
{
    arm64__make_and_ret(add, ss0, ss0, u32_int_immediate(1));
}

Instruction_Pair dec(Storage const& ss0)
{
    arm64__make_and_ret(sub, ss0, ss0, u32_int_immediate(1));
}

Instruction_Pair neg(Storage const& ss0)
{
    arm64__make_and_ret(neg, ss0, ss0);
}

Instruction_Pair neg(Storage const& ss0, Storage const& ss1)
{
    if (is_variant(Immediate, ss1)) {
        auto sid = common::assembly::get_storage_as_string<Register>(ss1);
        arm64__make_and_ret(
            mov, ss0, direct_immediate(fmt::format("#-{}", sid)));
    } else
        arm64__make_and_ret(neg, ss0, ss1);
}

Instructions r_eq(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_eq, direct_immediate(to));
    DONE_I();
}

Instructions r_neq(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_ne, direct_immediate(to));
    DONE_I();
}

Instructions r_lt(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_lt, direct_immediate(to));
    DONE_I();
}

Instructions r_gt(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_gt, direct_immediate(to));
    DONE_I();
}

Instructions r_le(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_le, direct_immediate(to));
    DONE_I();
}

Instructions r_ge(Storage const& ss0,
    Storage const& ss1,
    Label const& to,
    arm64::assembly::Register const& with)
{
    MAKE_I();
    arm64_add__asm(inst, mov, with, ss0);
    arm64_add__asm(inst, cmp, with, ss1);
    arm64_add__asm(inst, b_ge, direct_immediate(to));
    DONE_I();
}

Instruction_Pair rshift(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    arm64__make_and_ret(lsr, ss0, ss1, ss2);
}

Instruction_Pair lshift(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    arm64__make_and_ret(lsl, ss0, ss1, ss2);
}

Instruction_Pair b_xor(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    arm64__make_and_ret(eor, ss0, ss1, ss2);
}

Instruction_Pair b_and(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    arm64__make_and_ret(and_, ss0, ss1, ss2);
}

Instruction_Pair b_or(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    arm64__make_and_ret(orr, ss0, ss1, ss2);
}

Instruction_Pair b_not(Storage const& ss0,
    Storage const& ss1,
    Storage const& ss2)
{
    if (is_variant(Immediate, ss2))
        arm64__make_and_ret(movn, ss0, ss1, ss2);
    else
        arm64__make_and_ret(mvn, ss0, ss1, ss2);
}

Instruction_Pair b_not(Storage const& ss0, Storage const& ss1)
{
    if (is_variant(Immediate, ss1))
        arm64__make_and_ret(movn, ss0, ss1);
    else
        arm64__make_and_ret(mvn, ss0, ss1);
}

Instruction_Pair u_not(Storage const& ss0)
{
    MAKE_I();
    arm64_add__asm(inst, mov, w8, ss0);
    arm64_add__asm(inst, cmp, w8, u32_int_immediate(0));
    arm64_add__asm(inst, cset, w8, w8, direct_immediate("ne"));
    return { arm_rr(w8), inst };
}

Instruction_Pair store(Storage const& ss0, common::Stack_Offset const& ss1)
{
    arm64__make_and_ret(str, ss0, ss1);
}

} // namespace arm64::detail