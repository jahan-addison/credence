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
 * ARM64 Assembly
 *
 * The registers, mnemonics, directives, and instruction constructors of the
 * ARM64 ISA. An instruction is a mnemonic and up to four operands, and each
 * constructor returns the register of its result and its instructions.
 *
 *  Example:
 *
 *    auto [result, instructions] = assembly::mul(w8, Immediate{ "2" });
 *
 *  Becomes:
 *
 *    mov w7, #2
 *    mul w8, w8, w7      // result is w8
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x0 - x7  = The arguments of a call, and x0 the return value
 *   x6       = The data section address register, and the second operand
 *              of a binary expression in a temporary
 *      s6    = floating point
 *      d6    = double
 *   x7       = The scratch register of the right-hand-side of a binary
 *              expression
 *   x8       = The default "accumulator" register for expression expansion
 *   x9, x11 - x14, x16, x17
 *            = If there are no function calls in a stack frame, local scope
 *              variables are stored in these registers, after which the
 *              stack is used
 *   x10      = The stack move register, and the address register of a
 *              pointer on the stack
 *   x15      = The address of a vector element on the stack
 *   x18      = The platform register on Darwin, never used
 *   x19      = The argc, argv scratch register
 *
 *   A local whose address is taken, vectors, and vector offsets are always
 *   on the stack
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

/**
 * @brief Get the doubleword, word, float, and double argument registers
 *
 * Each list is in reverse, so the next argument register is at its back
 */
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
/**
 * The instruction constructors. Each returns the register of its result and
 * its instructions, and an immediate right-hand-side of a multiplication or
 * division is moved to w7 or x7 first.
 *
 *  Example:
 *
 *    r_gt(w9, #10, "._L6__main", w8)
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    cmp w8, #10
 *    b.gt ._L6__main
 */

Instruction_Pair mul(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret_with_immediate(mul);
}

Instruction_Pair div(Storage const& ss0, Storage const& ss1)
{
    arm64__make_and_ret_with_immediate(sdiv);
}

/**
 * @brief The remainder of a signed division, the dividend minus the
 * quotient times the divisor
 *
 *  Example:
 *
 *    mod(w8, #5)
 *
 *  Becomes:
 *
 *    mov w7, #5
 *    sdiv w10, w8, w7
 *    msub w8, w10, w7, w8
 */
Instruction_Pair mod(Storage const& ss0, Storage const& ss1)
{
    MAKE_I();
    auto dividend = std::get<Register>(ss0);
    bool doubleword = is_doubleword_register(dividend);
    auto divisor = ss1;
    if (is_variant(Immediate, divisor)) {
        auto scratch = doubleword ? Register::x7 : Register::w7;
        arm64_add__asm(inst, mov, scratch, divisor);
        divisor = scratch;
    }
    auto quotient = doubleword ? Register::x10 : Register::w10;
    if (std::get<Register>(divisor) == quotient)
        quotient = doubleword ? Register::x6 : Register::w6;
    arm64_add__asm(inst, sdiv, quotient, dividend, divisor);
    arm64_add__asm(inst, msub, dividend, quotient, divisor, dividend);
    return { dividend, inst };
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

/**
 * @brief The logical not of an operand, 1 when it is zero and 0 otherwise
 */
Instruction_Pair u_not(Storage const& ss0)
{
    MAKE_I();
    arm64_add__asm(inst, mov, w8, ss0);
    arm64_add__asm(inst, cmp, w8, u32_int_immediate(0));
    arm64_add__asm(inst, cset, w8, direct_immediate("eq"));
    return { arm_rr(w8), inst };
}

/**
 * @brief Store a register to a stack offset
 */
Instruction_Pair store(Storage const& ss0, common::Stack_Offset const& ss1)
{
    arm64__make_and_ret(str, ss0, ss1);
}

} // namespace arm64::detail