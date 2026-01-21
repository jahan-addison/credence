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

#pragma once

#include <credence/ir/object.h>              // for Size, Type, Label
#include <credence/target/common/assembly.h> // for COMMON_REGISTER_OSTREAM
#include <credence/target/common/types.h>    // for Label, Assignment_Operands_T
#include <credence/types.h>                  // for RValue, get_type_from_r...
#include <credence/util.h>                   // for STRINGIFY, range_contains
#include <cstddef>                           // for size_t
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <initializer_list>                  // for initializer_list
#include <matchit.h>                         // for pattern, PatternHelper
#include <ostream>                           // for basic_ostream, operator<<
#include <sstream>                           // for ostream
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view, stri...
#include <tuple>                             // for get
#include <type_traits>                       // for underlying_type_t
#include <utility>                           // for pair
#include <variant>                           // for variant, monostate, get

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

namespace credence::target::arm64::assembly {
enum class Directive;
enum class Mnemonic;
enum class Register;
}

/**
 * @brief
 *
 *  Macros and helpers to construct arm64 instructions
 *
 */

#define arm_mn(n) arm64::assembly::Mnemonic::n
#define arm_rr(n) arm64::assembly::Register::n
#define arm_dd(n) arm64::assembly::Directive::n

#define get_arm64_storage_as_string(str) \
    common::assembly::get_storage_as_string<arm64::assembly::Register>(str)

#define ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    arm64::assembly::Instruction_Pair name(                       \
        arm64::assembly::Storage const& ss0,                      \
        arm64::assembly::Storage const& ss1)
#define ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(name)                  \
    arm64::assembly::Instructions name(arm64::assembly::Storage const& ss0, \
        arm64::assembly::Storage const& ss1,                                \
        target::common::Label const& to,                                    \
        arm64::assembly::Register const& with)
#define ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    arm64::assembly::Instruction_Pair name(                       \
        arm64::assembly::Storage const& ss0,                      \
        arm64::assembly::Storage const& ss1,                      \
        arm64::assembly::Storage const& ss2)
#define ARM64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    arm64::assembly::Instruction_Pair name(arm64::assembly::Storage const& ss1)

#define ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_TEMPLATE(name) \
    arm64::assembly::Directive_Pair name(                            \
        std::size_t* index, type::semantic::RValue const& rvalue)

#define ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_LITERAL_TEMPLATE(name) \
    arm64::assembly::Directives name(type::semantic::RValue const& rvalue)

#define ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE( \
    name, type)                                                         \
    arm64::assembly::Directive_Pair name(std::size_t* index, type const& rvalue)

#define ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(name) \
    arm64::assembly::Directives name(type::semantic::RValue const& rvalue)

/**
 * @brief
 *
 *  Instruction expansion macros for code generation
 *
 */

#define arm64__first_storage_device(first, ...) first

#define arm64__make_and_ret(op, ...)                                   \
    do {                                                               \
        auto instructions = make_empty();                              \
        auto s = __VA_OPT__(arm64__first_storage_device(__VA_ARGS__)); \
        arm64_add__asm(instructions, op __VA_OPT__(, ) __VA_ARGS__);   \
        return { s, std::move(instructions) };                         \
    } while (false)

#define arm64__make_and_ret_with_immediate(op)                 \
    do {                                                       \
        if (is_variant(Immediate, ss1)) {                      \
            auto inst = make_empty();                          \
            auto size = get_operand_size_from_rvalue_datatype( \
                std::get<Immediate>(ss1));                     \
            if (size == Operand_Size::Doubleword) {            \
                arm64_add__asm(inst, mov, x7, ss1);            \
                arm64_add__asm(inst, op, ss0, ss0, x7);        \
                return { ss0, inst };                          \
            } else {                                           \
                arm64_add__asm(inst, mov, w7, ss1);            \
                arm64_add__asm(inst, op, ss0, ss0, w7);        \
                return { ss0, inst };                          \
            }                                                  \
        } else                                                 \
            arm64__make_and_ret(op, ss0, ss0, ss1);            \
    } while (false)

#define arm64_add__asm(inst, op, ...)                               \
    do {                                                            \
        using enum arm64::assembly::Register;                       \
        using enum arm64::assembly::Mnemonic;                       \
        arm64::assembly::ARM64_ASSEMBLY_INSERTER::insert<           \
            arm64::assembly::Instruction,                           \
            arm64::assembly::ARM64_ASSEMBLY_INSERTER::nary::ary_4>( \
            inst, op __VA_OPT__(, ) __VA_ARGS__);                   \
    } while (false)

/**
 * @brief
 *
 *  Register and directive string std::ostream macros
 *
 */

#define ARM64_REGISTER_OSTREAM(reg) COMMON_REGISTER_OSTREAM(arm_rr, reg)

#define ARM64_REGISTER_STRING(reg) COMMON_REGISTER_STRING(arm_rr, reg)

#define ARM64_DIRECTIVE_OSTREAM(d) COMMON_DIRECTIVE_OSTREAM(arm_dd, d)

#define ARM64_DIRECTIVE_OSTREAM_2ARY(d, g) \
    COMMON_DIRECTIVE_OSTREAM_2ARY(arm_dd, d, g)

#define ARM64_MNEMONIC_OSTREAM(mnem)                         \
    case arm_mn(mnem): {                                     \
        auto mnem_str = std::string_view{ STRINGIFY(mnem) }; \
        if (mnem_str.starts_with("b_")) {                    \
            os << "b." << mnem_str.substr(2);                \
        } else if (mnem_str.ends_with("_")) {                \
            os << mnem_str.substr(0, mnem_str.length() - 1); \
        } else {                                             \
            os << mnem_str;                                  \
        }                                                    \
    }; break

namespace credence::target::arm64::assembly {

/**
 * @brief Push a platform-dependent newline character to an ostream
 */
inline void newline(std::ostream& os, int amount = 1)
{
    for (; amount > 0; amount--)
        os << std::endl;
}

/**
 * @brief
 *
 * arm64 architecture implementation details.
 *
 */

enum class Register
{
    x0,
    x1,
    x2,
    x3,
    x4,
    x5,
    x6,
    x7,
    x8,
    x9,
    x10,
    x11,
    x12,
    x13,
    x14,
    x15,
    x16,
    x17,
    x18,
    x19,
    x20,
    x21,
    x22,
    x23,
    x24,
    x25,
    x26,
    x27,
    x28,
    x29,
    x30,
    sp,
    xzr,

    w0,
    w1,
    w2,
    w3,
    w4,
    w5,
    w6,
    w7,
    w8,
    w9,
    w10,
    w11,
    w12,
    w13,
    w14,
    w15,
    w16,
    w17,
    w18,
    w19,
    w20,
    w21,
    w22,
    w23,
    w24,
    w25,
    w26,
    w27,
    w28,
    w29,
    w30,
    wsp,
    wzr,

    d0,
    d1,
    d2,
    d3,
    d4,
    d5,
    d6,
    d7,
    d8,
    d9,
    d10,
    d11,
    d12,
    d13,
    d14,
    d15,
    d16,
    d17,
    d18,
    d19,
    d20,
    d21,
    d22,
    d23,
    d24,
    d25,
    d26,
    d27,
    d28,
    d29,
    d30,
    d31,

    s0,
    s1,
    s2,
    s3,
    s4,
    s5,
    s6,
    s7,
    s8,
    s9,
    s10,
    s11,
    s12,
    s13,
    s14,
    s15,
    s16,
    s17,
    s18,
    s19,
    s20,
    s21,
    s22,
    s23,
    s24,
    s25,
    s26,
    s27,
    s28,
    s29,
    s30,
    s31,

    v0,
    v1,
    v2,
    v3,
    v4,
    v5,
    v6,
    v7,
    v8,
    v9,
    v10,
    v11,
    v12,
    v13,
    v14,
    v15,
    v16,
    v17,
    v18,
    v19,
    v20,
    v21,
    v22,
    v23,
    v24,
    v25,
    v26,
    v27,
    v28,
    v29,
    v30,
    v31,

};

constexpr const auto DOUBLEWORD_REGISTER = { Register::x0,
    Register::x1,
    Register::x2,
    Register::x3,
    Register::x4,
    Register::x5,
    Register::x6,
    Register::x7,
    Register::x8,
    Register::x9,
    Register::x10,
    Register::x11,
    Register::x12,
    Register::x13,
    Register::x14,
    Register::x15,
    Register::x16,
    Register::x17,
    Register::x18,
    Register::x19,
    Register::x20,
    Register::x21,
    Register::x22,
    Register::x23,
    Register::x24,
    Register::x25,
    Register::x26,
    Register::x27,
    Register::x28,
    Register::x29,
    Register::x30,
    Register::sp };

constexpr const auto WORD_REGISTER = {
    Register::w0,
    Register::w1,
    Register::w2,
    Register::w3,
    Register::w4,
    Register::w5,
    Register::w6,
    Register::w7,
    Register::w8,
    Register::w9,
    Register::w10,
    Register::w11,
    Register::w12,
    Register::w13,
    Register::w14,
    Register::w15,
    Register::w16,
    Register::w17,
    Register::w18,
    Register::w19,
    Register::w20,
    Register::w21,
    Register::w22,
    Register::w23,
    Register::w24,
    Register::w25,
    Register::w26,
    Register::w27,
    Register::w28,
    Register::w29,
    Register::w30,
    Register::wsp,
};

constexpr const auto DOUBLE_REGISTER = {
    Register::d0,
    Register::d1,
    Register::d2,
    Register::d3,
    Register::d4,
    Register::d5,
    Register::d6,
    Register::d7,
    Register::d8,
    Register::d9,
    Register::d10,
    Register::d11,
    Register::d12,
    Register::d13,
    Register::d14,
    Register::d15,
    Register::d16,
    Register::d17,
    Register::d18,
    Register::d19,
    Register::d20,
    Register::d21,
    Register::d22,
    Register::d23,
    Register::d24,
    Register::d25,
    Register::d26,
    Register::d27,
    Register::d28,
    Register::d29,
    Register::d30,
};

constexpr const auto FLOAT_REGISTER = {
    Register::s0,
    Register::s1,
    Register::s2,
    Register::s3,
    Register::s4,
    Register::s5,
    Register::s6,
    Register::s7,
    Register::s8,
    Register::s9,
    Register::s10,
    Register::s11,
    Register::s12,
    Register::s13,
    Register::s14,
    Register::s15,
    Register::s16,
    Register::s17,
    Register::s18,
    Register::s19,
    Register::s20,
    Register::s21,
    Register::s22,
    Register::s23,
    Register::s24,
    Register::s25,
    Register::s26,
    Register::s27,
    Register::s28,
    Register::s29,
    Register::s30,
};

constexpr const auto VECTOR_REGISTER = { Register::v0,
    Register::v1,
    Register::v2,
    Register::v3,
    Register::v4,
    Register::v5,
    Register::v6,
    Register::v7,
    Register::v8,
    Register::v9,
    Register::v10,
    Register::v11,
    Register::v12,
    Register::v13,
    Register::v14,
    Register::v15,
    Register::v16,
    Register::v17,
    Register::v18,
    Register::v19,
    Register::v20,
    Register::v21,
    Register::v22,
    Register::v23,
    Register::v24,
    Register::v25,
    Register::v26,
    Register::v27,
    Register::v28,
    Register::v29,
    Register::v30,
    Register::v31 };

enum class Mnemonic
{
    add,
    adds,
    sub,
    subs,
    mul,
    sdiv,
    udiv,
    msub,
    and_,
    ands,
    orr,
    eor,
    mvn,
    lsl,
    lsr,
    asr,
    ror,
    ldr,
    str,
    neg,
    ldp,
    stp,
    b,
    bl,
    br,
    blr,
    cbz,
    cbnz,
    tbz,
    tbnz,
    b_eq,
    b_ne,
    b_lt,
    b_le,
    b_gt,
    b_ge,
    svc,
    adr,
    adrp,
    ret,
    mov,
    fmov,
    movn,
    cmp,
    cmn,
    tst,
    cset,
    nop
};

enum class Directive
{
    asciz,
    global,
    data,
    text,
    xword,
    word,
    dword,
    string,
    space,
    align,
    p2align,
    float_,
    double_,
    long_,
    start,
    extern_,
};

#define ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(name) \
    arm64::assembly::Directives name(type::semantic::RValue const& rvalue)

/**
 * @brief
 *
 * Word and operand size implementation helpers
 *
 */

enum class Operand_Size : std::size_t
{
    Empty = 0,
    Byte = 1,
    Halfword = 2,
    Word = 4,
    Doubleword = 8,
};

constexpr bool is_doubleword_register(Register r)
{
    return util::range_contains(r, DOUBLEWORD_REGISTER);
}
constexpr bool is_word_register(Register r)
{
    return util::range_contains(r, WORD_REGISTER);
}
constexpr bool is_vector_register(Register r)
{
    return util::range_contains(r, VECTOR_REGISTER);
}

constexpr Register get_doubleword_register_from_word(Register r)
{
    switch (r) {
        case Register::w0:
            return Register::x0;
        case Register::w1:
            return Register::x1;
        case Register::w2:
            return Register::x2;
        case Register::w3:
            return Register::x3;
        case Register::w4:
            return Register::x4;
        case Register::w5:
            return Register::x5;
        case Register::w6:
            return Register::x6;
        case Register::w7:
            return Register::x7;
        case Register::w8:
            return Register::x8;
        case Register::w9:
            return Register::x9;
        case Register::w10:
            return Register::x10;
        case Register::w11:
            return Register::x11;
        case Register::w12:
            return Register::x12;
        case Register::w13:
            return Register::x13;
        case Register::w14:
            return Register::x14;
        case Register::w15:
            return Register::x15;
        case Register::w16:
            return Register::x16;
        case Register::w17:
            return Register::x17;
        case Register::w18:
            return Register::x18;
        case Register::w19:
            return Register::x19;
        case Register::w20:
            return Register::x20;
        case Register::w21:
            return Register::x21;
        case Register::w22:
            return Register::x22;
        case Register::w23:
            return Register::x23;
        case Register::w24:
            return Register::x24;
        case Register::w25:
            return Register::x25;
        case Register::w26:
            return Register::x26;
        case Register::w27:
            return Register::x27;
        case Register::w28:
            return Register::x28;
        case Register::w29:
            return Register::x29;
        case Register::w30:
            return Register::x30;
        case Register::wsp:
            return Register::sp;
        default:
            return r; // Already a dword or unmapped
    }
}
constexpr Register get_double_register_from_doubleword(Register r)
{
    switch (r) {
        case Register::x0:
            return Register::d0;
        case Register::x1:
            return Register::d1;
        case Register::x2:
            return Register::d2;
        case Register::x3:
            return Register::d3;
        case Register::x4:
            return Register::d4;
        case Register::x5:
            return Register::d5;
        case Register::x6:
            return Register::d6;
        case Register::x7:
            return Register::d7;
        case Register::x8:
            return Register::d8;
        case Register::x9:
            return Register::d9;
        case Register::x10:
            return Register::d10;
        case Register::x11:
            return Register::d11;
        case Register::x12:
            return Register::d12;
        case Register::x13:
            return Register::d13;
        case Register::x14:
            return Register::d14;
        case Register::x15:
            return Register::d15;
        case Register::x16:
            return Register::d16;
        case Register::x17:
            return Register::d17;
        case Register::x18:
            return Register::d18;
        case Register::x19:
            return Register::d19;
        case Register::x20:
            return Register::d20;
        case Register::x21:
            return Register::d21;
        case Register::x22:
            return Register::d22;
        case Register::x23:
            return Register::d23;
        case Register::x24:
            return Register::d24;
        case Register::x25:
            return Register::d25;
        case Register::x26:
            return Register::d26;
        case Register::x27:
            return Register::d27;
        case Register::x28:
            return Register::d28;
        case Register::x29:
            return Register::d29;
        case Register::x30:
            return Register::d30;
        default:
            return r;
    }
}

constexpr Register get_float_register_from_doubleword(Register r)
{
    switch (r) {
        case Register::x0:
            return Register::s0;
        case Register::x1:
            return Register::s1;
        case Register::x2:
            return Register::s2;
        case Register::x3:
            return Register::s3;
        case Register::x4:
            return Register::s4;
        case Register::x5:
            return Register::s5;
        case Register::x6:
            return Register::s6;
        case Register::x7:
            return Register::s7;
        case Register::x8:
            return Register::s8;
        case Register::x9:
            return Register::s9;
        case Register::x10:
            return Register::s10;
        case Register::x11:
            return Register::s11;
        case Register::x12:
            return Register::s12;
        case Register::x13:
            return Register::s13;
        case Register::x14:
            return Register::s14;
        case Register::x15:
            return Register::s15;
        case Register::x16:
            return Register::s16;
        case Register::x17:
            return Register::s17;
        case Register::x18:
            return Register::s18;
        case Register::x19:
            return Register::s19;
        case Register::x20:
            return Register::s20;
        case Register::x21:
            return Register::s21;
        case Register::x22:
            return Register::s22;
        case Register::x23:
            return Register::s23;
        case Register::x24:
            return Register::s24;
        case Register::x25:
            return Register::s25;
        case Register::x26:
            return Register::s26;
        case Register::x27:
            return Register::s27;
        case Register::x28:
            return Register::s28;
        case Register::x29:
            return Register::s29;
        case Register::x30:
            return Register::s30;
        default:
            return r;
    }
}

constexpr Register get_word_register_from_doubleword(Register r)
{
    switch (r) {
        case Register::x0:
            return Register::w0;
        case Register::x1:
            return Register::w1;
        case Register::x2:
            return Register::w2;
        case Register::x3:
            return Register::w3;
        case Register::x4:
            return Register::w4;
        case Register::x5:
            return Register::w5;
        case Register::x6:
            return Register::w6;
        case Register::x7:
            return Register::w7;
        case Register::x8:
            return Register::w8;
        case Register::x9:
            return Register::w9;
        case Register::x10:
            return Register::w10;
        case Register::x11:
            return Register::w11;
        case Register::x12:
            return Register::w12;
        case Register::x13:
            return Register::w13;
        case Register::x14:
            return Register::w14;
        case Register::x15:
            return Register::w15;
        case Register::x16:
            return Register::w16;
        case Register::x17:
            return Register::w17;
        case Register::x18:
            return Register::w18;
        case Register::x19:
            return Register::w19;
        case Register::x20:
            return Register::w20;
        case Register::x21:
            return Register::w21;
        case Register::x22:
            return Register::w22;
        case Register::x23:
            return Register::w23;
        case Register::x24:
            return Register::w24;
        case Register::x25:
            return Register::w25;
        case Register::x26:
            return Register::w26;
        case Register::x27:
            return Register::w27;
        case Register::x28:
            return Register::w28;
        case Register::x29:
            return Register::w29;
        case Register::x30:
            return Register::w30;
        case Register::sp:
            return Register::wsp;
        default:
            return r; // Return as-is if already dword or other register
    }
}

constexpr Operand_Size get_operand_size_from_register(Register acc)
{
    if (is_doubleword_register(acc)) {
        return Operand_Size::Doubleword;
    } else if (is_word_register(acc)) {
        return Operand_Size::Word;
    } else if (is_vector_register(acc)) {
        return Operand_Size::Halfword;
    } else {
        return Operand_Size::Empty;
    }
}

constexpr Operand_Size get_operand_size_from_rvalue_datatype(
    type::Data_Type const& rvalue)
{
    namespace m = matchit;
    using T = type::semantic::Type;
    T type = type::get_type_from_rvalue_data_type(rvalue);
    return m::match(type)(
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Doubleword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Word; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | T{ "string" } = [&] { return Operand_Size::Doubleword; },
        m::pattern | m::_ = [&] { return Operand_Size::Word; });
}

constexpr Operand_Size get_operand_size_from_type(type::semantic::Type type)
{
    namespace m = matchit;
    using T = type::semantic::Type;
    return m::match(type)(
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Doubleword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Word; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | T{ "string" } = [&] { return Operand_Size::Doubleword; },
        m::pattern | m::_ = [&] { return Operand_Size::Word; });
}

constexpr std::size_t get_size_from_operand_size(Operand_Size size)
{
    return static_cast<std::underlying_type_t<Operand_Size>>(size);
}

/**
 * @brief Get operand size from numeric byte size
 */
constexpr Operand_Size get_operand_size_from_size(std::size_t size)
{
    switch (size) {
        case 1:
            return Operand_Size::Byte;
        case 2:
            return Operand_Size::Halfword;
        case 4:
            return Operand_Size::Word;
        case 8:
            return Operand_Size::Doubleword;
        default:
            return Operand_Size::Empty;
    }
}

/**
 * @brief operator<< function for emission of directives
 */
constexpr std::ostream& operator<<(std::ostream& os, Directive d)
{
    switch (d) {
        // the special ".global _start" directive
        ARM64_DIRECTIVE_OSTREAM_2ARY(start, _start);

        ARM64_DIRECTIVE_OSTREAM(asciz);
        ARM64_DIRECTIVE_OSTREAM(global);
        ARM64_DIRECTIVE_OSTREAM(data);
        ARM64_DIRECTIVE_OSTREAM(text);
        ARM64_DIRECTIVE_OSTREAM(xword);
        ARM64_DIRECTIVE_OSTREAM(word);
        ARM64_DIRECTIVE_OSTREAM(dword);
        ARM64_DIRECTIVE_OSTREAM(string);
        ARM64_DIRECTIVE_OSTREAM(space);
        ARM64_DIRECTIVE_OSTREAM(align);
        ARM64_DIRECTIVE_OSTREAM(p2align);
        ARM64_DIRECTIVE_OSTREAM(float_);
        ARM64_DIRECTIVE_OSTREAM(double_);
        ARM64_DIRECTIVE_OSTREAM(long_);
        ARM64_DIRECTIVE_OSTREAM(extern_);
    }
    return os;
}

/**
 * @brief operator<< function for emission of registers
 */
constexpr std::ostream& operator<<(std::ostream& os, Register reg)
{
    switch (reg) {
        ARM64_REGISTER_OSTREAM(x0);
        ARM64_REGISTER_OSTREAM(x1);
        ARM64_REGISTER_OSTREAM(x2);
        ARM64_REGISTER_OSTREAM(x3);
        ARM64_REGISTER_OSTREAM(x4);
        ARM64_REGISTER_OSTREAM(x5);
        ARM64_REGISTER_OSTREAM(x6);
        ARM64_REGISTER_OSTREAM(x7);
        ARM64_REGISTER_OSTREAM(x8);
        ARM64_REGISTER_OSTREAM(x9);
        ARM64_REGISTER_OSTREAM(x10);
        ARM64_REGISTER_OSTREAM(x11);
        ARM64_REGISTER_OSTREAM(x12);
        ARM64_REGISTER_OSTREAM(x13);
        ARM64_REGISTER_OSTREAM(x14);
        ARM64_REGISTER_OSTREAM(x15);
        ARM64_REGISTER_OSTREAM(x16);
        ARM64_REGISTER_OSTREAM(x17);
        ARM64_REGISTER_OSTREAM(x18);
        ARM64_REGISTER_OSTREAM(x19);
        ARM64_REGISTER_OSTREAM(x20);
        ARM64_REGISTER_OSTREAM(x21);
        ARM64_REGISTER_OSTREAM(x22);
        ARM64_REGISTER_OSTREAM(x23);
        ARM64_REGISTER_OSTREAM(x24);
        ARM64_REGISTER_OSTREAM(x25);
        ARM64_REGISTER_OSTREAM(x26);
        ARM64_REGISTER_OSTREAM(x27);
        ARM64_REGISTER_OSTREAM(x28);
        ARM64_REGISTER_OSTREAM(x29);
        ARM64_REGISTER_OSTREAM(x30);
        ARM64_REGISTER_OSTREAM(sp);
        ARM64_REGISTER_OSTREAM(xzr);
        ARM64_REGISTER_OSTREAM(w0);
        ARM64_REGISTER_OSTREAM(w1);
        ARM64_REGISTER_OSTREAM(w2);
        ARM64_REGISTER_OSTREAM(w3);
        ARM64_REGISTER_OSTREAM(w4);
        ARM64_REGISTER_OSTREAM(w5);
        ARM64_REGISTER_OSTREAM(w6);
        ARM64_REGISTER_OSTREAM(w7);
        ARM64_REGISTER_OSTREAM(w8);
        ARM64_REGISTER_OSTREAM(w9);
        ARM64_REGISTER_OSTREAM(w10);
        ARM64_REGISTER_OSTREAM(w11);
        ARM64_REGISTER_OSTREAM(w12);
        ARM64_REGISTER_OSTREAM(w13);
        ARM64_REGISTER_OSTREAM(w14);
        ARM64_REGISTER_OSTREAM(w15);
        ARM64_REGISTER_OSTREAM(w16);
        ARM64_REGISTER_OSTREAM(w17);
        ARM64_REGISTER_OSTREAM(w18);
        ARM64_REGISTER_OSTREAM(w19);
        ARM64_REGISTER_OSTREAM(w20);
        ARM64_REGISTER_OSTREAM(w21);
        ARM64_REGISTER_OSTREAM(w22);
        ARM64_REGISTER_OSTREAM(w23);
        ARM64_REGISTER_OSTREAM(w24);
        ARM64_REGISTER_OSTREAM(w25);
        ARM64_REGISTER_OSTREAM(w26);
        ARM64_REGISTER_OSTREAM(w27);
        ARM64_REGISTER_OSTREAM(w28);
        ARM64_REGISTER_OSTREAM(w29);
        ARM64_REGISTER_OSTREAM(w30);
        ARM64_REGISTER_OSTREAM(wsp);
        ARM64_REGISTER_OSTREAM(wzr);
        ARM64_REGISTER_OSTREAM(v0);
        ARM64_REGISTER_OSTREAM(v1);
        ARM64_REGISTER_OSTREAM(v2);
        ARM64_REGISTER_OSTREAM(v3);
        ARM64_REGISTER_OSTREAM(v4);
        ARM64_REGISTER_OSTREAM(v5);
        ARM64_REGISTER_OSTREAM(v6);
        ARM64_REGISTER_OSTREAM(v7);
        ARM64_REGISTER_OSTREAM(v8);
        ARM64_REGISTER_OSTREAM(v9);
        ARM64_REGISTER_OSTREAM(v10);
        ARM64_REGISTER_OSTREAM(v11);
        ARM64_REGISTER_OSTREAM(v12);
        ARM64_REGISTER_OSTREAM(v13);
        ARM64_REGISTER_OSTREAM(v14);
        ARM64_REGISTER_OSTREAM(v15);
        ARM64_REGISTER_OSTREAM(v16);
        ARM64_REGISTER_OSTREAM(v17);
        ARM64_REGISTER_OSTREAM(v18);
        ARM64_REGISTER_OSTREAM(v19);
        ARM64_REGISTER_OSTREAM(v20);
        ARM64_REGISTER_OSTREAM(v21);
        ARM64_REGISTER_OSTREAM(v22);
        ARM64_REGISTER_OSTREAM(v23);
        ARM64_REGISTER_OSTREAM(v24);
        ARM64_REGISTER_OSTREAM(v25);
        ARM64_REGISTER_OSTREAM(v26);
        ARM64_REGISTER_OSTREAM(v27);
        ARM64_REGISTER_OSTREAM(v28);
        ARM64_REGISTER_OSTREAM(v29);
        ARM64_REGISTER_OSTREAM(v30);
        ARM64_REGISTER_OSTREAM(v31);

        ARM64_REGISTER_OSTREAM(s0);
        ARM64_REGISTER_OSTREAM(s1);
        ARM64_REGISTER_OSTREAM(s2);
        ARM64_REGISTER_OSTREAM(s3);
        ARM64_REGISTER_OSTREAM(s4);
        ARM64_REGISTER_OSTREAM(s5);
        ARM64_REGISTER_OSTREAM(s6);
        ARM64_REGISTER_OSTREAM(s7);
        ARM64_REGISTER_OSTREAM(s8);
        ARM64_REGISTER_OSTREAM(s9);
        ARM64_REGISTER_OSTREAM(s10);
        ARM64_REGISTER_OSTREAM(s11);
        ARM64_REGISTER_OSTREAM(s12);
        ARM64_REGISTER_OSTREAM(s13);
        ARM64_REGISTER_OSTREAM(s14);
        ARM64_REGISTER_OSTREAM(s15);
        ARM64_REGISTER_OSTREAM(s16);
        ARM64_REGISTER_OSTREAM(s17);
        ARM64_REGISTER_OSTREAM(s18);
        ARM64_REGISTER_OSTREAM(s19);
        ARM64_REGISTER_OSTREAM(s20);
        ARM64_REGISTER_OSTREAM(s21);
        ARM64_REGISTER_OSTREAM(s22);
        ARM64_REGISTER_OSTREAM(s23);
        ARM64_REGISTER_OSTREAM(s24);
        ARM64_REGISTER_OSTREAM(s25);
        ARM64_REGISTER_OSTREAM(s26);
        ARM64_REGISTER_OSTREAM(s27);
        ARM64_REGISTER_OSTREAM(s28);
        ARM64_REGISTER_OSTREAM(s29);
        ARM64_REGISTER_OSTREAM(s30);
        ARM64_REGISTER_OSTREAM(s31);

        ARM64_REGISTER_OSTREAM(d0);
        ARM64_REGISTER_OSTREAM(d1);
        ARM64_REGISTER_OSTREAM(d2);
        ARM64_REGISTER_OSTREAM(d3);
        ARM64_REGISTER_OSTREAM(d4);
        ARM64_REGISTER_OSTREAM(d5);
        ARM64_REGISTER_OSTREAM(d6);
        ARM64_REGISTER_OSTREAM(d7);
        ARM64_REGISTER_OSTREAM(d8);
        ARM64_REGISTER_OSTREAM(d9);
        ARM64_REGISTER_OSTREAM(d10);
        ARM64_REGISTER_OSTREAM(d11);
        ARM64_REGISTER_OSTREAM(d12);
        ARM64_REGISTER_OSTREAM(d13);
        ARM64_REGISTER_OSTREAM(d14);
        ARM64_REGISTER_OSTREAM(d15);
        ARM64_REGISTER_OSTREAM(d16);
        ARM64_REGISTER_OSTREAM(d17);
        ARM64_REGISTER_OSTREAM(d18);
        ARM64_REGISTER_OSTREAM(d19);
        ARM64_REGISTER_OSTREAM(d20);
        ARM64_REGISTER_OSTREAM(d21);
        ARM64_REGISTER_OSTREAM(d22);
        ARM64_REGISTER_OSTREAM(d23);
        ARM64_REGISTER_OSTREAM(d24);
        ARM64_REGISTER_OSTREAM(d25);
        ARM64_REGISTER_OSTREAM(d26);
        ARM64_REGISTER_OSTREAM(d27);
        ARM64_REGISTER_OSTREAM(d28);
        ARM64_REGISTER_OSTREAM(d29);
        ARM64_REGISTER_OSTREAM(d30);
        ARM64_REGISTER_OSTREAM(d31);
    }
    return os;
}

/**
 * @brief Get register as an std::string
 */
constexpr std::string register_as_string(Register reg)
{
    switch (reg) {
        ARM64_REGISTER_STRING(x0);
        ARM64_REGISTER_STRING(x1);
        ARM64_REGISTER_STRING(x2);
        ARM64_REGISTER_STRING(x3);
        ARM64_REGISTER_STRING(x4);
        ARM64_REGISTER_STRING(x5);
        ARM64_REGISTER_STRING(x6);
        ARM64_REGISTER_STRING(x7);
        ARM64_REGISTER_STRING(x8);
        ARM64_REGISTER_STRING(x9);
        ARM64_REGISTER_STRING(x10);
        ARM64_REGISTER_STRING(x11);
        ARM64_REGISTER_STRING(x12);
        ARM64_REGISTER_STRING(x13);
        ARM64_REGISTER_STRING(x14);
        ARM64_REGISTER_STRING(x15);
        ARM64_REGISTER_STRING(x16);
        ARM64_REGISTER_STRING(x17);
        ARM64_REGISTER_STRING(x18);
        ARM64_REGISTER_STRING(x19);
        ARM64_REGISTER_STRING(x20);
        ARM64_REGISTER_STRING(x21);
        ARM64_REGISTER_STRING(x22);
        ARM64_REGISTER_STRING(x23);
        ARM64_REGISTER_STRING(x24);
        ARM64_REGISTER_STRING(x25);
        ARM64_REGISTER_STRING(x26);
        ARM64_REGISTER_STRING(x27);
        ARM64_REGISTER_STRING(x28);
        ARM64_REGISTER_STRING(x29);
        ARM64_REGISTER_STRING(x30);
        ARM64_REGISTER_STRING(sp);
        ARM64_REGISTER_STRING(xzr);
        ARM64_REGISTER_STRING(w0);
        ARM64_REGISTER_STRING(w1);
        ARM64_REGISTER_STRING(w2);
        ARM64_REGISTER_STRING(w3);
        ARM64_REGISTER_STRING(w4);
        ARM64_REGISTER_STRING(w5);
        ARM64_REGISTER_STRING(w6);
        ARM64_REGISTER_STRING(w7);
        ARM64_REGISTER_STRING(w8);
        ARM64_REGISTER_STRING(w9);
        ARM64_REGISTER_STRING(w10);
        ARM64_REGISTER_STRING(w11);
        ARM64_REGISTER_STRING(w12);
        ARM64_REGISTER_STRING(w13);
        ARM64_REGISTER_STRING(w14);
        ARM64_REGISTER_STRING(w15);
        ARM64_REGISTER_STRING(w16);
        ARM64_REGISTER_STRING(w17);
        ARM64_REGISTER_STRING(w18);
        ARM64_REGISTER_STRING(w19);
        ARM64_REGISTER_STRING(w20);
        ARM64_REGISTER_STRING(w21);
        ARM64_REGISTER_STRING(w22);
        ARM64_REGISTER_STRING(w23);
        ARM64_REGISTER_STRING(w24);
        ARM64_REGISTER_STRING(w25);
        ARM64_REGISTER_STRING(w26);
        ARM64_REGISTER_STRING(w27);
        ARM64_REGISTER_STRING(w28);
        ARM64_REGISTER_STRING(w29);
        ARM64_REGISTER_STRING(w30);
        ARM64_REGISTER_STRING(wsp);
        ARM64_REGISTER_STRING(wzr);
        ARM64_REGISTER_STRING(v0);
        ARM64_REGISTER_STRING(v1);
        ARM64_REGISTER_STRING(v2);
        ARM64_REGISTER_STRING(v3);
        ARM64_REGISTER_STRING(v4);
        ARM64_REGISTER_STRING(v5);
        ARM64_REGISTER_STRING(v6);
        ARM64_REGISTER_STRING(v7);
        ARM64_REGISTER_STRING(v8);
        ARM64_REGISTER_STRING(v9);
        ARM64_REGISTER_STRING(v10);
        ARM64_REGISTER_STRING(v11);
        ARM64_REGISTER_STRING(v12);
        ARM64_REGISTER_STRING(v13);
        ARM64_REGISTER_STRING(v14);
        ARM64_REGISTER_STRING(v15);
        ARM64_REGISTER_STRING(v16);
        ARM64_REGISTER_STRING(v17);
        ARM64_REGISTER_STRING(v18);
        ARM64_REGISTER_STRING(v19);
        ARM64_REGISTER_STRING(v20);
        ARM64_REGISTER_STRING(v21);
        ARM64_REGISTER_STRING(v22);
        ARM64_REGISTER_STRING(v23);
        ARM64_REGISTER_STRING(v24);
        ARM64_REGISTER_STRING(v25);
        ARM64_REGISTER_STRING(v26);
        ARM64_REGISTER_STRING(v27);
        ARM64_REGISTER_STRING(v28);
        ARM64_REGISTER_STRING(v29);
        ARM64_REGISTER_STRING(v30);
        ARM64_REGISTER_STRING(v31);

        ARM64_REGISTER_STRING(s0);
        ARM64_REGISTER_STRING(s1);
        ARM64_REGISTER_STRING(s2);
        ARM64_REGISTER_STRING(s3);
        ARM64_REGISTER_STRING(s4);
        ARM64_REGISTER_STRING(s5);
        ARM64_REGISTER_STRING(s6);
        ARM64_REGISTER_STRING(s7);
        ARM64_REGISTER_STRING(s8);
        ARM64_REGISTER_STRING(s9);
        ARM64_REGISTER_STRING(s10);
        ARM64_REGISTER_STRING(s11);
        ARM64_REGISTER_STRING(s12);
        ARM64_REGISTER_STRING(s13);
        ARM64_REGISTER_STRING(s14);
        ARM64_REGISTER_STRING(s15);
        ARM64_REGISTER_STRING(s16);
        ARM64_REGISTER_STRING(s17);
        ARM64_REGISTER_STRING(s18);
        ARM64_REGISTER_STRING(s19);
        ARM64_REGISTER_STRING(s20);
        ARM64_REGISTER_STRING(s21);
        ARM64_REGISTER_STRING(s22);
        ARM64_REGISTER_STRING(s23);
        ARM64_REGISTER_STRING(s24);
        ARM64_REGISTER_STRING(s25);
        ARM64_REGISTER_STRING(s26);
        ARM64_REGISTER_STRING(s27);
        ARM64_REGISTER_STRING(s28);
        ARM64_REGISTER_STRING(s29);
        ARM64_REGISTER_STRING(s30);
        ARM64_REGISTER_STRING(s31);

        ARM64_REGISTER_STRING(d0);
        ARM64_REGISTER_STRING(d1);
        ARM64_REGISTER_STRING(d2);
        ARM64_REGISTER_STRING(d3);
        ARM64_REGISTER_STRING(d4);
        ARM64_REGISTER_STRING(d5);
        ARM64_REGISTER_STRING(d6);
        ARM64_REGISTER_STRING(d7);
        ARM64_REGISTER_STRING(d8);
        ARM64_REGISTER_STRING(d9);
        ARM64_REGISTER_STRING(d10);
        ARM64_REGISTER_STRING(d11);
        ARM64_REGISTER_STRING(d12);
        ARM64_REGISTER_STRING(d13);
        ARM64_REGISTER_STRING(d14);
        ARM64_REGISTER_STRING(d15);
        ARM64_REGISTER_STRING(d16);
        ARM64_REGISTER_STRING(d17);
        ARM64_REGISTER_STRING(d18);
        ARM64_REGISTER_STRING(d19);
        ARM64_REGISTER_STRING(d20);
        ARM64_REGISTER_STRING(d21);
        ARM64_REGISTER_STRING(d22);
        ARM64_REGISTER_STRING(d23);
        ARM64_REGISTER_STRING(d24);
        ARM64_REGISTER_STRING(d25);
        ARM64_REGISTER_STRING(d26);
        ARM64_REGISTER_STRING(d27);
        ARM64_REGISTER_STRING(d28);
        ARM64_REGISTER_STRING(d29);
        ARM64_REGISTER_STRING(d30);
        ARM64_REGISTER_STRING(d31);
    }
    return "x0";
}

/**
 * @brief operator<< function for emission of mnemonics
 */
// cppcheck-suppress all
constexpr std::ostream& operator<<(std::ostream& os, Mnemonic mnemonic)
{
    switch (mnemonic) {
        ARM64_MNEMONIC_OSTREAM(add);
        ARM64_MNEMONIC_OSTREAM(adds);
        ARM64_MNEMONIC_OSTREAM(sub);
        ARM64_MNEMONIC_OSTREAM(subs);
        ARM64_MNEMONIC_OSTREAM(mul);
        ARM64_MNEMONIC_OSTREAM(sdiv);
        ARM64_MNEMONIC_OSTREAM(msub);
        ARM64_MNEMONIC_OSTREAM(udiv);
        ARM64_MNEMONIC_OSTREAM(and_);
        ARM64_MNEMONIC_OSTREAM(ands);
        ARM64_MNEMONIC_OSTREAM(orr);
        ARM64_MNEMONIC_OSTREAM(eor);
        ARM64_MNEMONIC_OSTREAM(mvn);
        ARM64_MNEMONIC_OSTREAM(movn);
        ARM64_MNEMONIC_OSTREAM(fmov);
        ARM64_MNEMONIC_OSTREAM(lsl);
        ARM64_MNEMONIC_OSTREAM(lsr);
        ARM64_MNEMONIC_OSTREAM(asr);
        ARM64_MNEMONIC_OSTREAM(ror);
        ARM64_MNEMONIC_OSTREAM(ldr);
        ARM64_MNEMONIC_OSTREAM(str);
        ARM64_MNEMONIC_OSTREAM(neg);
        ARM64_MNEMONIC_OSTREAM(ldp);
        ARM64_MNEMONIC_OSTREAM(stp);
        ARM64_MNEMONIC_OSTREAM(b);
        ARM64_MNEMONIC_OSTREAM(bl);
        ARM64_MNEMONIC_OSTREAM(ret);
        ARM64_MNEMONIC_OSTREAM(br);
        ARM64_MNEMONIC_OSTREAM(blr);
        ARM64_MNEMONIC_OSTREAM(cbz);
        ARM64_MNEMONIC_OSTREAM(cbnz);
        ARM64_MNEMONIC_OSTREAM(tbz);
        ARM64_MNEMONIC_OSTREAM(tbnz);
        ARM64_MNEMONIC_OSTREAM(b_eq);
        ARM64_MNEMONIC_OSTREAM(b_ne);
        ARM64_MNEMONIC_OSTREAM(b_lt);
        ARM64_MNEMONIC_OSTREAM(b_le);
        ARM64_MNEMONIC_OSTREAM(b_gt);
        ARM64_MNEMONIC_OSTREAM(b_ge);
        ARM64_MNEMONIC_OSTREAM(svc);
        ARM64_MNEMONIC_OSTREAM(adr);
        ARM64_MNEMONIC_OSTREAM(adrp);
        ARM64_MNEMONIC_OSTREAM(mov);
        ARM64_MNEMONIC_OSTREAM(cmp);
        ARM64_MNEMONIC_OSTREAM(cmn);
        ARM64_MNEMONIC_OSTREAM(tst);
        ARM64_MNEMONIC_OSTREAM(cset);
        ARM64_MNEMONIC_OSTREAM(nop);
    }
    return os;
}
/**
 * @brief Internal implementation type details
 *
 */

using Storage = common::Storage_T<Register>;
using Assignment_Operands = common::Binary_Operands_T<Register>;
using Ternary_Operands = common::Ternary_Operands_T<Register>;
using Instruction = common::Mnemonic_4ARY<Mnemonic, Register>;
using Literal_Type = std::variant<type::semantic::RValue, float, double>;
using Data_Pair = std::pair<Directive, Literal_Type>;
using Directives = std::deque<std::variant<Label, Data_Pair>>;
using Instructions = std::deque<common::Instruction_4ARY<Mnemonic, Register>>;
using Instruction_Pair = common::Instruction_Pair<Storage, Instructions>;
using Immediate = common::Immediate;
using Directive_Pair = std::pair<std::string, Directives>;

constexpr auto arm64_bitwise_binary_operators = { "<<",
    ">>",
    "~",
    "|",
    "^",
    "&" };

using ARM64_ASSEMBLY_INSERTER =
    target::common::assembly::Assembly_Inserter<arm64::assembly::Mnemonic,
        arm64::assembly::Register,
        arm64::assembly::Instructions>;

// Empty storage device
const Storage O_NUL = std::monostate{};

/**
 * @brief Instruction type constructor
 */

inline Instructions make_empty() noexcept
{
    return Instructions{};
}

/**
 * @brief Directive type constructor
 */
inline Directives make_directives() noexcept
{
    return Directives{};
}

constexpr std::string tabwidth(unsigned int t)
{
    std::string tab{};
    for (; t > 0; t--)
        tab += " ";
    return tab;
}

/**
 * @brief Checks if two storage devices of any type are equal
 */
constexpr bool is_equal_storage_devices(Storage const& lhs, Storage const& rhs)
{
    auto result{ false };
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](common::Stack_Offset const& s) {
                       if (is_variant(common::Stack_Offset, rhs))
                           result = s == std::get<common::Stack_Offset>(rhs);
                   },
                   [&](Register const& s) {
                       if (is_variant(Register, rhs))
                           result = s == std::get<Register>(rhs);
                   },
                   [&](Immediate const& s) {
                       if (is_variant(Immediate, rhs))
                           result = s == std::get<Immediate>(rhs);
                   },
               },
        lhs);

    return result;
}

constexpr std::string literal_type_to_string(Literal_Type const& literal)
{
    std::string as_str{};
    std::visit(util::overload{ [&](std::string const& i) { as_str = i; },
                   [&](float i) { as_str = std::to_string(i); },
                   [&](double i) { as_str = std::to_string(i); } },
        literal);
    return as_str;
}

constexpr Register get_register_from_integer_argument(int index)
{
    switch (index) {
        case 0:
            return Register::x0;
        case 1:
            return Register::x1;
        case 2:
            return Register::x2;
        case 3:
            return Register::x3;
        case 4:
            return Register::x4;
        case 5:
            return Register::x5;
        case 6:
            return Register::x6;
        case 7:
            return Register::x7;
        default:
            return Register::x0;
    }
}

constexpr std::string make_label(type::semantic::Label const& label)
{
    if (label == "main")
        return "_start";
    return label;
}

constexpr std::string make_label(type::semantic::Label const& label,
    type::semantic::Label const& scope)
{
    if (label == "main")
        return "_start";
    return fmt::format(".{}__{}", label, scope);
}

/**
 * @brief Get the data directive from rvalue data type
 */
constexpr Directive get_data_directive_from_rvalue_type(
    Immediate const& immediate)
{
    namespace m = matchit;
    using T = type::semantic::Type;
    T type = type::get_type_from_rvalue_data_type(immediate);
    return m::match(type)(
        m::pattern | T{ "double" } = [&] { return Directive::double_; },
        m::pattern |
            m::or_(T{ "int" }, T{ "long" }) = [&] { return Directive::long_; },
        m::pattern | T{ "float" } = [&] { return Directive::float_; },
        m::pattern | T{ "char" } = [&] { return Directive::string; },
        m::pattern | T{ "string" } = [&] { return Directive::xword; },
        m::pattern | m::_ = [&] { return Directive::xword; });
}

/**
 * @brief Instruction insertion helper
 */
inline void inserter(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}
/**
 * @brief Directive insertion helper
 */
inline void inserter(assembly::Directives& to, assembly::Directives const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

/**
 * @brief asciz directive insertion helper
 */
inline arm64::assembly::Immediate make_asciz_immediate(std::string_view address)
{
    return Immediate{ std::string(address), "string", 8UL };
}

/**
 * @brief Check if relative address offset (label reference)
 */
constexpr bool is_immediate_relative_address(Storage const& immediate)
{
    if (is_variant(Immediate, immediate))
        return util::contains(
            std::get<0>(std::get<Immediate>(immediate)), "._L_str");
    else
        return false;
}

/**
 * @brief Check if stack pointer or vector lvalue address offset
 */
constexpr bool is_immediate_pc_address_offset(Storage const& immediate)
{
    if (is_variant(Immediate, immediate))
        return util::contains(
                   std::get<0>(std::get<Immediate>(immediate)), "[sp") or
               util::contains(
                   std::get<0>(std::get<Immediate>(immediate)), "[x6");
    else
        return false;
}

/**
 * @brief Check if argv address offset
 */
constexpr bool is_immediate_x1_address_offset(Storage const& immediate)
{
    if (is_variant(Immediate, immediate))
        return util::contains(
            std::get<0>(std::get<Immediate>(immediate)), "[x1");
    else
        return false;
}

std::tuple<std::deque<Register>,
    std::deque<Register>,
    std::deque<Register>,
    std::deque<Register>>
get_available_argument_register();

/**
 * @brief
 *
 * Easy macro expansion of instruction and directive definitions from
 * templates
 *
 */

// directives
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(asciz,
    type::semantic::RValue);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(doublez,
    type::semantic::RValue);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(floatz,
    type::semantic::RValue);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(xword);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(word);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(hword);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(zero);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(align);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(p2align);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(float_);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(double_);
ARM64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(long_);

// arithmetic
ARM64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(inc);
ARM64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(dec);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mul);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(div);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(sub);
ARM64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(neg);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(neg);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(add);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mod);
// r (relational)
ARM64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(u_not);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_eq);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_neq);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_lt);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_gt);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_le);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_ge);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_or);
ARM64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_and);
// b (bitwise)
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
ARM64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);

ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
ARM64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);

// pointers
Instruction_Pair store(Storage const& ss0, common::Stack_Offset const& ss1);

/**
 * @brief Get the size in bytes from a register
 */
constexpr type::semantic::Size get_size_from_register(Register r)
{
    if (util::is_one_of(r, WORD_REGISTER)) {
        return 4;
    }
    if (util::is_one_of(r, DOUBLEWORD_REGISTER)) {
        return 8;
    }
    return 0;
}

/**
 * @brief Check that a common::Size is a valid Operand_Size
 */
constexpr bool is_valid_size(Size size)
{
    if (size == 1UL or size == 2UL or size == 4UL or size == 8UL)
        return true;
    return false;
}

/**
 * @brief Get the size in bytes from an rvalue data type
 */
constexpr common::Size get_size_from_type(Type const& type_)
{
    namespace m = matchit;
    return m::match(type_)(
        m::pattern | Type{ "byte" } = [] { return Size{ 1 }; },
        m::pattern | Type{ "int" } = [] { return Size{ 4 }; },
        m::pattern | Type{ "long" } = [] { return Size{ 8 }; },
        m::pattern | Type{ "float" } = [] { return Size{ 8 }; },
        m::pattern | Type{ "double" } = [] { return Size{ 8 }; },
        m::pattern | Type{ "string" } = [] { return Size{ 8 }; },
        m::pattern | m::_ = [] { return Size{ 0 }; });
}

/**
 * @brief Get the size in bytes from an rvalue data type
 */
constexpr type::semantic::Size get_size_from_rvalue_datatype(
    Immediate const& rvalue)
{
    using Size = common::Size;
    using Type = common::Type;
    Type type_ = type::get_type_from_rvalue_data_type(rvalue);
    if (type_ != "string")
        return get_size_from_type(type_);
    else {
        auto sv = type::get_value_from_rvalue_data_type(rvalue);
        return Size{ sv.length() + 1 };
    }
}

} // namespace arm64::detail