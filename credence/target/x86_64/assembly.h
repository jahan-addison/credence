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

#include <credence/ir/object.h>              // for Label
#include <credence/target/common/assembly.h> // for COMMON_REGISTER_OSTREAM
#include <credence/target/common/types.h>    // for Label, Binary_Operands_T
#include <credence/types.h>                  // for RValue, Type, get_type_...
#include <credence/util.h>                   // for contains, STRINGIFY
#include <cstddef>                           // for size_t
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <initializer_list>                  // for initializer_list
#include <map>                               // for map
#include <matchit.h>                         // for pattern, Or, PatternHelper
#include <ostream>                           // for basic_ostream, operator<<
#include <sstream>                           // for ostream
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view, oper...
#include <tuple>                             // for get
#include <type_traits>                       // for underlying_type_t
#include <utility>                           // for pair
#include <variant>                           // for variant, monostate, get
namespace credence {
namespace target {
namespace x86_64 {
namespace assembly {
enum class Directive;
}
}
}
}
namespace credence {
namespace target {
namespace x86_64 {
namespace assembly {
enum class Mnemonic;
}
}
}
}
namespace credence {
namespace target {
namespace x86_64 {
namespace assembly {
enum class Register;
}
}
}
}

/**
 * @brief
 *
 *  Macros and helpers to construct x86-64 instructions
 *
 */

#define x64_mn(n) x86_64::assembly::Mnemonic::n
#define x64_rr(n) x86_64::assembly::Register::n
#define x64_dd(n) x86_64::assembly::Directive::n

#define X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                    \
        x86_64::assembly::Storage const& dest,                  \
        x86_64::assembly::Storage const& src)
#define X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(name)                       \
    x86_64::assembly::Instructions name(x86_64::assembly::Storage const& dest, \
        x86_64::assembly::Storage const& src,                                  \
        target::common::Label const& to,                                       \
        x86_64::assembly::Register const& with)
#define X64_DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                    \
        x86_64::assembly::Storage const& dest,                  \
        x86_64::assembly::Storage const& s1,                    \
        x86_64::assembly::Storage const& s2)
#define X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                    \
        x86_64::assembly::Storage const& src)

#define X64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_TEMPLATE(name) \
    x86_64::assembly::Directive_Pair name(                         \
        std::size_t* index, type::semantic::RValue const& rvalue)

#define X64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE( \
    name, type)                                                       \
    x86_64::assembly::Directive_Pair name(                            \
        std::size_t* index, type const& rvalue)

#define X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(name) \
    x86_64::assembly::Directives name(type::semantic::RValue const& rvalue)

/**
 * @brief
 *
 *  Instrction insertion helpers
 *
 */
#define x64_add_asm__(inst, op, lhs, rhs) \
    inst.emplace_back(x86_64::assembly::Instruction{ op, lhs, rhs })

// Add an instruction with a mnemonic shorthand
#define x64_add_asm__as(inst, op, lhs, rhs)          \
    inst.emplace_back(x86_64::assembly::Instruction{ \
        x86_64::assembly::Mnemonic::op, lhs, rhs })

// Add an instruction with a mnemonic and destination shorthand
#define x64_asm__dest_rs(inst, op, lhs, rhs)                           \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::Register::lhs,                           \
            rhs })

// Add an instruction with a mnemonic and source operand shorthand
#define x64_asm__src_rs(inst, op, lhs, rhs)                            \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            lhs,                                                       \
            x86_64::assembly::Register::rhs })

// Add an instruction with a mnemonic, destination, and source operand
// shorthand
#define x64_asm__short(inst, op, lhs, rhs)                             \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::Register::lhs,                           \
            x86_64::assembly::Register::rhs })

// Add an instruction with a mnemonic shorthand, no operands (.e.g 'ret')
#define x64_asm__zero_o(inst, op)                      \
    COMMON_ASM_ZERO_O_2(x86_64::assembly::Instruction, \
        x86_64::assembly::Mnemonic,                    \
        x86_64::assembly::O_NUL,                       \
        inst,                                          \
        op)

// Add an instruction with a mnemonic with no operand (e.g. idiv)
#define x64_asm__dest(inst, op, dest)                \
    COMMON_ASM_DEST_2(x86_64::assembly::Instruction, \
        x86_64::assembly::Mnemonic,                  \
        x86_64::assembly::O_NUL,                     \
        inst,                                        \
        op,                                          \
        dest)

// Add an instruction with a mnemonic with 1 operand and shorthand
#define x64_asm__dest_s(inst, op, dest)             \
    COMMON_ASM_DEST_S_2(x86_64::assembly::Register, \
        x86_64::assembly::Instruction,              \
        x86_64::assembly::Mnemonic,                 \
        x86_64::assembly::O_NUL,                    \
        inst,                                       \
        op,                                         \
        dest)

// Add an instruction with a mnemonic with 1 operand (e.g. idiv)
#define x64_asm__src(inst, op, dest)                \
    COMMON_ASM_SRC_2(x86_64::assembly::Instruction, \
        x86_64::assembly::Mnemonic,                 \
        x86_64::assembly::O_NUL,                    \
        inst,                                       \
        op,                                         \
        dest)

/**
 * @brief
 *
 *  Macros and helpers to compute trivial immediate rvalues
 *
 */

// Immediate relational macros moved to common header

/**
 * @brief
 *
 *  Register and directive string std::ostream macros
 *
 */

#define X64_REGISTER_OSTREAM(reg) COMMON_REGISTER_OSTREAM(x64_rr, reg)

#define X64_REGISTER_STRING(reg) COMMON_REGISTER_STRING(x64_rr, reg)

#define X64_DIRECTIVE_OSTREAM(d) COMMON_DIRECTIVE_OSTREAM(x64_dd, d)

#define X64_DIRECTIVE_OSTREAM_2ARY(d, g) \
    COMMON_DIRECTIVE_OSTREAM_2ARY(x64_dd, d, g)

#define X64_MNEMONIC_OSTREAM(mnem)                           \
    case x64_mn(mnem): {                                     \
        auto mnem_str = std::string_view{ STRINGIFY(mnem) }; \
        if (mnem_str == "goto_") {                           \
            os << "jmp";                                     \
            break;                                           \
        }                                                    \
        if (mnem_str.ends_with("q_"))                        \
            mnem_str.remove_suffix(2);                       \
        if (util::contains(mnem_str, "_"))                   \
            mnem_str.remove_suffix(1);                       \
        os << mnem_str;                                      \
    }; break

namespace credence::target::x86_64::assembly {

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
 * x86-64 architecture implementation details.
 *
 */

enum class Register
{
    rbp,
    rsp,
    rax,
    rbx,
    rcx,
    rdx,
    rsi,
    rdi,
    r8,
    r9,
    r10,
    r11,
    r12,
    r13,
    r14,
    r15,

    ebp,
    esp,
    eax,
    ebx,
    edx,
    ecx,
    esi,
    edi,
    r8d,
    r9d,
    r10d,
    r11d,
    r12d,
    r13d,
    r14d,
    r15d,

    di,
    ax,
    al,
    dil,

    xmm7,
    xmm6,
    xmm5,
    xmm4,
    xmm3,
    xmm2,
    xmm1,
    xmm0
};

constexpr const auto QWORD_REGISTER = { Register::rbp,
    Register::rsp,
    Register::rax,
    Register::rbx,
    Register::rcx,
    Register::rdx,
    Register::rsi,
    Register::rdi,
    Register::r8,
    Register::r9,
    Register::r10,
    Register::r11,
    Register::r12,
    Register::r13,
    Register::r14,
    Register::r15 };

constexpr const auto DWORD_REGISTER = {
    Register::ebp,
    Register::esp,
    Register::eax,
    Register::ebx,
    Register::edx,
    Register::ecx,
    Register::esi,
    Register::edi,
    Register::r8d,
    Register::r9d,
    Register::r10d,
    Register::r11d,
    Register::r12d,
    Register::r13d,
    Register::r14d,
    Register::r15d,
};

constexpr const auto FLOAT_REGISTER = { Register::xmm7,
    Register::xmm6,
    Register::xmm5,
    Register::xmm4,
    Register::xmm3,
    Register::xmm2,
    Register::xmm1,
    Register::xmm1,
    Register::xmm0 };

enum class Mnemonic
{
    imul,
    lea,
    ret,
    sub,
    add,
    neg,
    je,
    jne,
    jle,
    jl,
    jg,
    jge,
    idiv,
    inc,
    dec,
    cqo,
    cdq,
    leave,
    push,
    pop,
    call,
    cmp,
    sete,
    setne,
    goto_,
    setl,
    setg,
    setle,
    setge,
    mov,
    movq_,
    movzx,
    movss,
    movups,
    movsd,
    mov_,
    and_,
    or_,
    xor_,
    not_,
    shl,
    shr,
    syscall
};

enum class Directive
{
    asciz,
    data,
    text,
    start,
    global,
    long_,
    quad,
    float_,
    double_,
    byte_,
    extern_
};

// clang-format on

/**
 * @brief
 *
 * Word and operand size implementation helpers
 *
 */

enum class Operand_Size : std::size_t
{
    Byte = 1,
    Word = 2,
    Dword = 4,
    Qword = 8,
    Empty = 0
};

const std::map<Operand_Size, std::string> suffix = {
    { Operand_Size::Byte,  "b" },
    { Operand_Size::Word,  "w" },
    { Operand_Size::Dword, "l" },
    { Operand_Size::Qword, "q" }
};

constexpr bool is_qword_register(Register r)
{
    return util::range_contains(r, QWORD_REGISTER);
}
constexpr bool is_dword_register(Register r)
{
    return util::range_contains(r, DWORD_REGISTER);
}

constexpr Operand_Size get_operand_size_from_register(Register acc)
{
    if (acc == Register::al) {
        return Operand_Size::Byte;
    } else if (acc == Register::ax) {
        return Operand_Size::Word;
    } else if (is_qword_register(acc)) {
        return Operand_Size::Qword;
    } else if (is_dword_register(acc)) {
        return Operand_Size::Dword;
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
        m::pattern | m::or_(T{ "double" },
                         T{ "long" }) = [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | T{ "string" } = [&] { return Operand_Size::Qword; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

constexpr Operand_Size get_operand_size_from_type(type::semantic::Type type)
{
    namespace m = matchit;
    using T = type::semantic::Type;
    return m::match(type)(
        m::pattern | m::or_(T{ "double" },
                         T{ "long" }) = [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | T{ "string" } = [&] { return Operand_Size::Qword; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

constexpr std::size_t get_size_from_operand_size(Operand_Size size)
{
    return static_cast<std::underlying_type_t<Operand_Size>>(size);
}

/**
 * @brief operator<< function for emission of directives
 */
constexpr std::ostream& operator<<(std::ostream& os, Directive d)
{
    switch (d) {
        // the special ".global _start" directive
        X64_DIRECTIVE_OSTREAM_2ARY(start, _start);

        X64_DIRECTIVE_OSTREAM(asciz);
        X64_DIRECTIVE_OSTREAM(global);
        X64_DIRECTIVE_OSTREAM(data);
        X64_DIRECTIVE_OSTREAM(text);
        X64_DIRECTIVE_OSTREAM(quad);
        X64_DIRECTIVE_OSTREAM(long_);
        X64_DIRECTIVE_OSTREAM(float_);
        X64_DIRECTIVE_OSTREAM(double_);
        X64_DIRECTIVE_OSTREAM(byte_);
        X64_DIRECTIVE_OSTREAM(extern_);
    }
    return os;
}

/**
 * @brief operator<< function for emission of registers
 */
constexpr std::ostream& operator<<(std::ostream& os, Register reg)
{
    switch (reg) {
        X64_REGISTER_OSTREAM(rbp);
        X64_REGISTER_OSTREAM(rsp);
        X64_REGISTER_OSTREAM(rax);
        X64_REGISTER_OSTREAM(rbx);
        X64_REGISTER_OSTREAM(rcx);
        X64_REGISTER_OSTREAM(rdx);
        X64_REGISTER_OSTREAM(rsi);
        X64_REGISTER_OSTREAM(rdi);
        X64_REGISTER_OSTREAM(r8);
        X64_REGISTER_OSTREAM(r9);
        X64_REGISTER_OSTREAM(r10);
        X64_REGISTER_OSTREAM(r11);
        X64_REGISTER_OSTREAM(r12);
        X64_REGISTER_OSTREAM(r13);
        X64_REGISTER_OSTREAM(r14);
        X64_REGISTER_OSTREAM(r15);
        X64_REGISTER_OSTREAM(ebp);
        X64_REGISTER_OSTREAM(esp);
        X64_REGISTER_OSTREAM(eax);
        X64_REGISTER_OSTREAM(ebx);
        X64_REGISTER_OSTREAM(edx);
        X64_REGISTER_OSTREAM(ecx);
        X64_REGISTER_OSTREAM(esi);
        X64_REGISTER_OSTREAM(edi);
        X64_REGISTER_OSTREAM(r8d);
        X64_REGISTER_OSTREAM(r9d);
        X64_REGISTER_OSTREAM(r10d);
        X64_REGISTER_OSTREAM(r11d);
        X64_REGISTER_OSTREAM(r12d);
        X64_REGISTER_OSTREAM(r13d);
        X64_REGISTER_OSTREAM(r14d);
        X64_REGISTER_OSTREAM(r15d);
        X64_REGISTER_OSTREAM(di);
        X64_REGISTER_OSTREAM(dil);
        X64_REGISTER_OSTREAM(al);
        X64_REGISTER_OSTREAM(ax);
        X64_REGISTER_OSTREAM(xmm7);
        X64_REGISTER_OSTREAM(xmm6);
        X64_REGISTER_OSTREAM(xmm5);
        X64_REGISTER_OSTREAM(xmm4);
        X64_REGISTER_OSTREAM(xmm3);
        X64_REGISTER_OSTREAM(xmm2);
        X64_REGISTER_OSTREAM(xmm1);
        X64_REGISTER_OSTREAM(xmm0);
    }
    return os;
}

/**
 * @brief Get register as an std::string
 */
constexpr std::string register_as_string(Register reg)
{
    switch (reg) {
        X64_REGISTER_STRING(rbp);
        X64_REGISTER_STRING(rsp);
        X64_REGISTER_STRING(rax);
        X64_REGISTER_STRING(rbx);
        X64_REGISTER_STRING(rcx);
        X64_REGISTER_STRING(rdx);
        X64_REGISTER_STRING(rsi);
        X64_REGISTER_STRING(rdi);
        X64_REGISTER_STRING(r8);
        X64_REGISTER_STRING(r9);
        X64_REGISTER_STRING(r10);
        X64_REGISTER_STRING(r11);
        X64_REGISTER_STRING(r12);
        X64_REGISTER_STRING(r13);
        X64_REGISTER_STRING(r14);
        X64_REGISTER_STRING(r15);
        X64_REGISTER_STRING(ebp);
        X64_REGISTER_STRING(esp);
        X64_REGISTER_STRING(eax);
        X64_REGISTER_STRING(ebx);
        X64_REGISTER_STRING(edx);
        X64_REGISTER_STRING(ecx);
        X64_REGISTER_STRING(esi);
        X64_REGISTER_STRING(edi);
        X64_REGISTER_STRING(r8d);
        X64_REGISTER_STRING(r9d);
        X64_REGISTER_STRING(r10d);
        X64_REGISTER_STRING(r11d);
        X64_REGISTER_STRING(r12d);
        X64_REGISTER_STRING(r13d);
        X64_REGISTER_STRING(r14d);
        X64_REGISTER_STRING(r15d);
        X64_REGISTER_STRING(di);
        X64_REGISTER_STRING(dil);
        X64_REGISTER_STRING(al);
        X64_REGISTER_STRING(ax);
        X64_REGISTER_STRING(xmm7);
        X64_REGISTER_STRING(xmm6);
        X64_REGISTER_STRING(xmm5);
        X64_REGISTER_STRING(xmm4);
        X64_REGISTER_STRING(xmm3);
        X64_REGISTER_STRING(xmm2);
        X64_REGISTER_STRING(xmm1);
        X64_REGISTER_STRING(xmm0);
    }
    return "rax";
}

/**
 * @brief operator<< function for emission of mnemonics
 */
// cppcheck-suppress all
constexpr std::ostream& operator<<(std::ostream& os, Mnemonic mnemonic)
{
    switch (mnemonic) {
        X64_MNEMONIC_OSTREAM(imul);
        X64_MNEMONIC_OSTREAM(neg);
        X64_MNEMONIC_OSTREAM(lea);
        X64_MNEMONIC_OSTREAM(ret);
        X64_MNEMONIC_OSTREAM(sub);
        X64_MNEMONIC_OSTREAM(add);
        X64_MNEMONIC_OSTREAM(je);
        X64_MNEMONIC_OSTREAM(jne);
        X64_MNEMONIC_OSTREAM(jle);
        X64_MNEMONIC_OSTREAM(jl);
        X64_MNEMONIC_OSTREAM(jg);
        X64_MNEMONIC_OSTREAM(jge);
        X64_MNEMONIC_OSTREAM(idiv);
        X64_MNEMONIC_OSTREAM(inc);
        X64_MNEMONIC_OSTREAM(dec);
        X64_MNEMONIC_OSTREAM(cqo);
        X64_MNEMONIC_OSTREAM(cdq);
        X64_MNEMONIC_OSTREAM(leave);
        X64_MNEMONIC_OSTREAM(mov);
        X64_MNEMONIC_OSTREAM(movzx);
        X64_MNEMONIC_OSTREAM(movss);
        X64_MNEMONIC_OSTREAM(movups);
        X64_MNEMONIC_OSTREAM(movsd);
        X64_MNEMONIC_OSTREAM(movq_);
        X64_MNEMONIC_OSTREAM(mov_);
        X64_MNEMONIC_OSTREAM(push);
        X64_MNEMONIC_OSTREAM(pop);
        X64_MNEMONIC_OSTREAM(call);
        X64_MNEMONIC_OSTREAM(cmp);
        X64_MNEMONIC_OSTREAM(sete);
        X64_MNEMONIC_OSTREAM(goto_);
        X64_MNEMONIC_OSTREAM(setne);
        X64_MNEMONIC_OSTREAM(setl);
        X64_MNEMONIC_OSTREAM(setg);
        X64_MNEMONIC_OSTREAM(setle);
        X64_MNEMONIC_OSTREAM(setge);
        X64_MNEMONIC_OSTREAM(and_);
        X64_MNEMONIC_OSTREAM(not_);
        X64_MNEMONIC_OSTREAM(xor_);
        X64_MNEMONIC_OSTREAM(or_);
        X64_MNEMONIC_OSTREAM(shl);
        X64_MNEMONIC_OSTREAM(shr);
        X64_MNEMONIC_OSTREAM(syscall);
    }
    return os;
}

/**
 * @brief Internal implementation type details
 *
 */

using Storage = common::Storage_T<Register>;
using Binary_Operands = common::Binary_Operands_T<Register>;
using Instruction = common::Mnemonic_2ARY<Mnemonic, Register>;
using Literal_Type = std::variant<type::semantic::RValue, float, double>;
using Data_Pair = std::pair<Directive, Literal_Type>;
using Directives = std::deque<std::variant<Label, Data_Pair>>;
using Instructions = std::deque<common::Instruction_2ARY<Mnemonic, Register>>;
using Instruction_Pair = common::Instruction_Pair<Storage, Instructions>;
using Immediate = common::Immediate;
using Directive_Pair = std::pair<std::string, Directives>;

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

constexpr std::string literal_type_to_string(Literal_Type const& literal)
{
    std::string as_str{};
    std::visit(util::overload{ [&](std::string const& i) { as_str = i; },
                   [&](float i) { as_str = std::to_string(i); },
                   [&](double i) { as_str = std::to_string(i); } },
        literal);
    return as_str;
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

Immediate get_result_from_trivial_integral_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_relational_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_bitwise_expression(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);

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
        m::pattern | T{ "char" } = [&] { return Directive::byte_; },
        m::pattern | T{ "string" } = [&] { return Directive::quad; },
        m::pattern | m::_ = [&] { return Directive::quad; });
}

std::string get_storage_as_string(Storage const& storage);

/**
 * @brief Instruction insertion helper
 */
inline void inserter(assembly::Instructions& to,
    assembly::Instructions const& from)
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
inline x86_64::assembly::Immediate make_asciz_immediate(
    std::string_view address)
{
    return Immediate{ fmt::format("[rip + {}]", address), "string", 8UL };
}

/**
 * @brief Check if rip address offset
 */
constexpr bool is_immediate_rip_address_offset(Storage const& immediate)
{
    if (is_variant(Immediate, immediate))
        return util::contains(
            std::get<0>(std::get<Immediate>(immediate)), "rip + ._L");
    else
        return false;
}

/**
 * @brief Check if r15 (argc, argv) address offset
 */
constexpr bool is_immediate_r15_address_offset(Storage const& immediate)
{
    if (is_variant(Immediate, immediate))
        return util::contains(
            std::get<0>(std::get<Immediate>(immediate)), "[r15");
    else
        return false;
}

/**
 * @brief
 *
 * Easy macro expansion of instruction and directive definitions from
 * templates
 *
 */

// directives
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(asciz,
    type::semantic::RValue);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(doublez,
    type::semantic::RValue);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_LITERAL_TEMPLATE(floatz,
    type::semantic::RValue);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(quad);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(long_);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(float_);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(double_);
X64_DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(byte_);

// arithmetic
X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(inc);
X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(dec);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mul);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(div);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(sub);
X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(neg);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(add);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mod);
// r (relational)
X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(u_not);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_eq);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_neq);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_lt);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_gt);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_le);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_ge);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_or);
X64_DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_and);
// b (bitwise)
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
X64_DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);
// pointers
X64_DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lea);

} // namespace x86_64::detail