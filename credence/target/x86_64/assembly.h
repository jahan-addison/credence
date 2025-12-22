/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <algorithm>        // for __find, find
#include <concepts>         // for integral
#include <credence/types.h> // for integral_from_type, Type, Data_...
#include <credence/util.h>  // for contains, is_variant
#include <cstddef>          // for size_t
#include <deque>            // for deque
#include <fmt/format.h>     // for format
#include <initializer_list> // for initializer_list
#include <map>              // for map
#include <matchit.h>        // for pattern, PatternHelper, Pattern...
#include <ostream>          // for basic_ostream, operator<<
#include <sstream>          // for basic_ostringstream, ostream
#include <string>           // for basic_string, char_traits, string
#include <string_view>      // for basic_string_view, string_view
#include <tuple>            // for tuple
#include <type_traits>      // for underlying_type_t
#include <utility>          // for pair
#include <variant>          // for variant, monostate

/**
 * @brief
 *
 *  Macros and helpers to construct x86-64 instructions
 *
 */

#define mn(n) x86_64::assembly::Mnemonic::n
#define rr(n) x86_64::assembly::Register::n
#define dd(n) x86_64::assembly::Directive::n

#define DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                \
        x86_64::assembly::Storage const& dest,              \
        x86_64::assembly::Storage const& src)
#define DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(name)                           \
    x86_64::assembly::Instructions name(x86_64::assembly::Storage const& dest, \
        x86_64::assembly::Storage const& src,                                  \
        x86_64::assembly::Label const& to)
#define DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                \
        x86_64::assembly::Storage const& dest,              \
        x86_64::assembly::Storage const& s1,                \
        x86_64::assembly::Storage const& s2)
#define DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    x86_64::assembly::Instruction_Pair name(                \
        x86_64::assembly::Storage const& src)

#define DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_TEMPLATE(name) \
    x86_64::assembly::Directive_Pair name(                     \
        std::size_t* index, type::semantic::RValue const& rvalue)

#define DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(name) \
    x86_64::assembly::Directives name(type::semantic::RValue const& rvalue)

/**
 * @brief
 *
 *  Instrction insertion helpers
 *
 */
#define add_asm__(inst, op, lhs, rhs) \
    inst.emplace_back(x86_64::assembly::Instruction{ op, lhs, rhs })

// Add an instruction with a mnemonic shorthand
#define add_asm__as(inst, op, lhs, rhs)              \
    inst.emplace_back(x86_64::assembly::Instruction{ \
        x86_64::assembly::Mnemonic::op, lhs, rhs })

// Add an instruction with a mnemonic and destination shorthand
#define asm__dest_rs(inst, op, lhs, rhs)                               \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::Register::lhs,                           \
            rhs })

// Add an instruction with a mnemonic and source operand shorthand
#define asm__src_rs(inst, op, lhs, rhs)                                \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            lhs,                                                       \
            x86_64::assembly::Register::rhs })

// Add an instruction with a mnemonic, destination, and source operand
// shorthand
#define asm__short(inst, op, lhs, rhs)                                 \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::Register::lhs,                           \
            x86_64::assembly::Register::rhs })

// Add an instruction with a mnemonic shorthand, no operands (.e.g 'ret')
#define asm__zero_o(inst, op)                                          \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::O_NUL,                                   \
            x86_64::assembly::O_NUL })

// Add an instruction with a mnemonic with no operand (e.g. idiv)
#define asm__dest(inst, op, dest)                    \
    inst.emplace_back(x86_64::assembly::Instruction{ \
        Mnemonic::op, dest, x86_64::assembly::O_NUL })

// Add an instruction with a mnemonic with 1 operand and shorthand
#define asm__dest_s(inst, op, dest)                                    \
    inst.emplace_back(                                                 \
        x86_64::assembly::Instruction{ x86_64::assembly::Mnemonic::op, \
            x86_64::assembly::Register::dest,                          \
            x86_64::assembly::O_NUL })

// Add an instruction with a mnemonic with 1 operand (e.g. idiv)
#define asm__src(inst, op, dest)                     \
    inst.emplace_back(x86_64::assembly::Instruction{ \
        x86_64::assembly::Mnemonic::op, dest, x86_64::assembly::O_NUL })

/**
 * @brief
 *
 *  Macros and helpers to compute trivial immediate rvalues
 *
 */

#define make_integral_relational_entry(T, op)         \
    m::pattern | std::string{ STRINGIFY(T) } = [&] {  \
        result = type::integral_from_type<T>(lhs_imm) \
            op type::integral_from_type<T>(rhs_imm);  \
    }

#define make_string_relational_entry(op)                                      \
    m::pattern | std::string{ "string" } = [&] {                              \
        result =                                                              \
            std::string_view{ lhs_imm }.compare(std::string_view{ rhs_imm }); \
    }

#define make_char_relational_entry(op)                                    \
    m::pattern | std::string{ "char" } = [&] {                            \
        result = static_cast<int>(static_cast<unsigned char>(lhs_imm[1])) \
            op static_cast<int>(static_cast<unsigned char>(rhs_imm[1]));  \
    }

#define make_trivial_immediate_binary_result(op)                    \
    m::pattern | std::string{ STRINGIFY(op) } = [&] {               \
        m::match(lhs_type)(make_integral_relational_entry(int, op), \
            make_integral_relational_entry(long, op),               \
            make_integral_relational_entry(float, op),              \
            make_integral_relational_entry(double, op),             \
            make_string_relational_entry(op),                       \
            make_char_relational_entry(op));                        \
    }

/**
 * @brief
 *
 *  Register and directive string std::ostream macros
 *
 */

#define REGISTER_OSTREAM(reg) \
    case rr(reg):             \
        os << STRINGIFY(reg); \
        break

#define REGISTER_STRING(reg)   \
    case rr(reg):              \
        return STRINGIFY(reg); \
        break

#define DIRECTIVE_OSTREAM(d)         \
    case dd(d): {                    \
        auto di = sv(STRINGIFY(d));  \
        if (util::contains(di, "_")) \
            di.remove_suffix(1);     \
        os << "." << di;             \
    }; break

#define DIRECTIVE_OSTREAM_2ARY(d, g)                                       \
    case dd(d): {                                                          \
        auto di =                                                          \
            sv(STRINGIFY(d)) == sv("start") ? "global" : sv(STRINGIFY(d)); \
        if (util::contains(di, "_"))                                       \
            di.remove_suffix(1);                                           \
        os << fmt::format(".{} {}", di, STRINGIFY(g));                     \
    }; break

#define MNEMONIC_OSTREAM(mnem)                               \
    case mn(mnem): {                                         \
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

constexpr const auto QWORD_REGISTER = { Register::rdi,
    Register::r8,
    Register::r9,
    Register::rsi,
    Register::rdx,
    Register::rcx,
    Register::rax };
constexpr const auto DWORD_REGISTER = { Register::edi,
    Register::r8d,
    Register::r9d,
    Register::esi,
    Register::edx,
    Register::ecx,
    Register::eax };

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
 *  Template function to compute type-safe trivial arithmetic binary
 * expression
 */

template<util::Numeric T>
T trivial_arithmetic_from_numeric_table_type(std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T result{ 0 };
    T imm_l = type::integral_from_type<T>(lhs);
    T imm_r = type::integral_from_type<T>(rhs);
    switch (op[0]) {
        case '+':
            return imm_l + imm_r;
        case '-':
            return imm_l - imm_r;
        case '*':
            return imm_l * imm_r;
        case '/':
            return imm_l / imm_r;
    }
    return result;
}

/**
 * @brief
 *  Template function to compute type-safe trivial bitwise binary
 * expression
 */
template<util::Numeric T>
T trivial_bitwise_from_numeric_table_type(std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T imm_l = type::integral_from_type<T>(lhs);
    T imm_r = type::integral_from_type<T>(rhs);
    if (op == ">>")
        return imm_l >> imm_r;
    else if (op == "<<")
        return imm_l << imm_r;
    else
        switch (op[0]) {
            case '^':
                return imm_l ^ imm_r;
            case '&':
                return imm_l & imm_r;
            case '|':
                return imm_l | imm_r;
        }
    return 0;
}

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
        DIRECTIVE_OSTREAM_2ARY(start, _start);

        DIRECTIVE_OSTREAM(asciz);
        DIRECTIVE_OSTREAM(global);
        DIRECTIVE_OSTREAM(data);
        DIRECTIVE_OSTREAM(text);
        DIRECTIVE_OSTREAM(quad);
        DIRECTIVE_OSTREAM(long_);
        DIRECTIVE_OSTREAM(float_);
        DIRECTIVE_OSTREAM(double_);
        DIRECTIVE_OSTREAM(byte_);
        DIRECTIVE_OSTREAM(extern_);
    }
    return os;
}

/**
 * @brief operator<< function for emission of registers
 */
constexpr std::ostream& operator<<(std::ostream& os, Register reg)
{
    switch (reg) {
        REGISTER_OSTREAM(rbp);
        REGISTER_OSTREAM(rsp);
        REGISTER_OSTREAM(rax);
        REGISTER_OSTREAM(rbx);
        REGISTER_OSTREAM(rcx);
        REGISTER_OSTREAM(rdx);
        REGISTER_OSTREAM(rsi);
        REGISTER_OSTREAM(rdi);
        REGISTER_OSTREAM(r8);
        REGISTER_OSTREAM(r9);
        REGISTER_OSTREAM(r10);
        REGISTER_OSTREAM(r11);
        REGISTER_OSTREAM(r12);
        REGISTER_OSTREAM(r13);
        REGISTER_OSTREAM(r14);
        REGISTER_OSTREAM(ebp);
        REGISTER_OSTREAM(esp);
        REGISTER_OSTREAM(eax);
        REGISTER_OSTREAM(ebx);
        REGISTER_OSTREAM(edx);
        REGISTER_OSTREAM(ecx);
        REGISTER_OSTREAM(esi);
        REGISTER_OSTREAM(edi);
        REGISTER_OSTREAM(r8d);
        REGISTER_OSTREAM(r9d);
        REGISTER_OSTREAM(r10d);
        REGISTER_OSTREAM(r11d);
        REGISTER_OSTREAM(r12d);
        REGISTER_OSTREAM(r13d);
        REGISTER_OSTREAM(r14d);
        REGISTER_OSTREAM(r15d);
        REGISTER_OSTREAM(di);
        REGISTER_OSTREAM(dil);
        REGISTER_OSTREAM(al);
        REGISTER_OSTREAM(ax);
        REGISTER_OSTREAM(xmm7);
        REGISTER_OSTREAM(xmm6);
        REGISTER_OSTREAM(xmm5);
        REGISTER_OSTREAM(xmm4);
        REGISTER_OSTREAM(xmm3);
        REGISTER_OSTREAM(xmm2);
        REGISTER_OSTREAM(xmm1);
        REGISTER_OSTREAM(xmm0);
    }
    return os;
}

/**
 * @brief Get register as an std::string
 */
constexpr std::string register_as_string(Register reg)
{
    switch (reg) {
        REGISTER_STRING(rbp);
        REGISTER_STRING(rsp);
        REGISTER_STRING(rax);
        REGISTER_STRING(rbx);
        REGISTER_STRING(rcx);
        REGISTER_STRING(rdx);
        REGISTER_STRING(rsi);
        REGISTER_STRING(rdi);
        REGISTER_STRING(r8);
        REGISTER_STRING(r9);
        REGISTER_STRING(r10);
        REGISTER_STRING(r11);
        REGISTER_STRING(r12);
        REGISTER_STRING(r13);
        REGISTER_STRING(r14);
        REGISTER_STRING(ebp);
        REGISTER_STRING(esp);
        REGISTER_STRING(eax);
        REGISTER_STRING(ebx);
        REGISTER_STRING(edx);
        REGISTER_STRING(ecx);
        REGISTER_STRING(esi);
        REGISTER_STRING(edi);
        REGISTER_STRING(r8d);
        REGISTER_STRING(r9d);
        REGISTER_STRING(r10d);
        REGISTER_STRING(r11d);
        REGISTER_STRING(r12d);
        REGISTER_STRING(r13d);
        REGISTER_STRING(r14d);
        REGISTER_STRING(r15d);
        REGISTER_STRING(di);
        REGISTER_STRING(dil);
        REGISTER_STRING(al);
        REGISTER_STRING(ax);
        REGISTER_STRING(xmm7);
        REGISTER_STRING(xmm6);
        REGISTER_STRING(xmm5);
        REGISTER_STRING(xmm4);
        REGISTER_STRING(xmm3);
        REGISTER_STRING(xmm2);
        REGISTER_STRING(xmm1);
        REGISTER_STRING(xmm0);
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
        MNEMONIC_OSTREAM(imul);
        MNEMONIC_OSTREAM(neg);
        MNEMONIC_OSTREAM(lea);
        MNEMONIC_OSTREAM(ret);
        MNEMONIC_OSTREAM(sub);
        MNEMONIC_OSTREAM(add);
        MNEMONIC_OSTREAM(je);
        MNEMONIC_OSTREAM(jne);
        MNEMONIC_OSTREAM(jle);
        MNEMONIC_OSTREAM(jl);
        MNEMONIC_OSTREAM(jg);
        MNEMONIC_OSTREAM(jge);
        MNEMONIC_OSTREAM(idiv);
        MNEMONIC_OSTREAM(inc);
        MNEMONIC_OSTREAM(dec);
        MNEMONIC_OSTREAM(cqo);
        MNEMONIC_OSTREAM(cdq);
        MNEMONIC_OSTREAM(leave);
        MNEMONIC_OSTREAM(mov);
        MNEMONIC_OSTREAM(movzx);
        MNEMONIC_OSTREAM(movq_);
        MNEMONIC_OSTREAM(mov_);
        MNEMONIC_OSTREAM(push);
        MNEMONIC_OSTREAM(pop);
        MNEMONIC_OSTREAM(call);
        MNEMONIC_OSTREAM(cmp);
        MNEMONIC_OSTREAM(sete);
        MNEMONIC_OSTREAM(goto_);
        MNEMONIC_OSTREAM(setne);
        MNEMONIC_OSTREAM(setl);
        MNEMONIC_OSTREAM(setg);
        MNEMONIC_OSTREAM(setle);
        MNEMONIC_OSTREAM(setge);
        MNEMONIC_OSTREAM(and_);
        MNEMONIC_OSTREAM(not_);
        MNEMONIC_OSTREAM(xor_);
        MNEMONIC_OSTREAM(or_);
        MNEMONIC_OSTREAM(shl);
        MNEMONIC_OSTREAM(shr);
        MNEMONIC_OSTREAM(syscall);
    }
    return os;
}

/**
 * @brief Internal implementation type details
 *
 */

using Immediate = type::Data_Type;
using Label = type::semantic::Label;
using Stack_Offset = std::size_t;
using Storage = std::variant<std::monostate, Stack_Offset, Register, Immediate>;
using Instruction = std::tuple<Mnemonic, Storage, Storage>;
using Data_Pair = std::pair<Directive, type::semantic::RValue>;
using Directives = std::deque<std::variant<Label, Data_Pair>>;
using Instructions = std::deque<std::variant<Label, Instruction>>;
using Instruction_Pair = std::pair<Storage, Instructions>;
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
 * @brief directive insertion helper for arrays
 */
inline x86_64::assembly::Immediate make_array_immediate(
    std::string_view address)
{
    return Immediate{ address, "string", 8UL };
}

/**
 * @brief directive insertion for arbitrary immediate devices
 */
inline x86_64::assembly::Immediate make_direct_immediate(std::string_view str)
{
    return Immediate{ str, "string", 8UL };
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
 * @brief Type-safe numeric type::Data_Type immediate constructor
 */
template<util::Numeric T>
constexpr Immediate make_numeric_immediate(T imm,
    std::string const& type = "int")
{
    return Immediate{ util::to_constexpr_string(imm), type, 4UL };
}

/**
 * @brief Type-safe numeric u32 type::Data_Type immediate constructor
 */
constexpr Immediate make_u32_int_immediate(unsigned int imm)
{
    return Immediate{ util::to_constexpr_string(imm), "int", 4UL };
}

/**
 * @brief
 *
 * Easy macro expansion of instruction and directive definitions from
 * templates
 *
 */

// directives
DEFINE_1ARY_OPERAND_DIRECTIVE_PAIR_FROM_TEMPLATE(asciz);
DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(quad);
DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(long_);
DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(float_);
DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(double_);
DEFINE_1ARY_OPERAND_DIRECTIVE_FROM_TEMPLATE(byte_);

// arithmetic
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(inc);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(dec);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mul);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(div);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(sub);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(neg);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(add);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mod);
// r (relational)
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(u_not);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_eq);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_neq);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_lt);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_gt);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_le);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_ge);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_or);
DEFINE_2ARY_OPERAND_JUMP_FROM_TEMPLATE(r_and);
// b (bitwise)
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);
// pointers
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lea);

} // namespace x86_64::detail