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

#include <algorithm>           // for find
#include <credence/ir/table.h> // for Table
#include <credence/rvalue.h>   // for LValue, RValue, Data_Type, ...
#include <deque>               // for deque
#include <string>              // for basic_string, string
#include <tuple>               // for tuple
#include <utility>             // for pair
#include <variant>             // for variant

#define mn(n) Mnemonic::n
#define rr(n) Register::n

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name) \
    detail::Instruction_Pair name(detail::Storage& dest, detail::Storage& src)

#define DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name)  \
    detail::Instruction_Pair name(detail::Storage& dest, detail::Storage& s1, detail::Storage& s2)

#define DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name)  \
    detail::Instruction_Pair name(detail::Storage& src)

#define addiis(inst, op, lhs, rhs)      \
    inst.emplace_back(detail::Instruction{Mnemonic::op,lhs, rhs})

#define addiill(inst, op, lhs, rhs)     \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::lhs, rhs})

#define addiilr(inst, op, lhs, rhs)     \
    inst.emplace_back(detail::Instruction{Mnemonic::op,lhs, Register::rhs})

#define addiilrs(inst, op, lhs, rhs)    \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::lhs, Register::rhs})

#define addiie(inst, op)                \
    inst.emplace_back(detail::Instruction{Mnemonic::op,detail::O_NUL, detail::O_NUL})

#define addiid(inst, op, dest)          \
    inst.emplace_back(detail::Instruction{Mnemonic::op,dest, detail::O_NUL})

#define adiild(inst, op, dest)          \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::dest, detail::O_NUL})

#define addiils(inst, op, dest)         \
    inst.emplace_back(detail::Instruction{Mnemonic::op, dest, detail::O_NUL})

#define addii(inst, op, lhs, rhs)       \
    inst.emplace_back(detail::Instruction{op, lhs, rhs})

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define make_integral_relational_entry(T, op)                                                  \
    m::pattern | std::string{STRINGIFY(T)} = [&] {                                             \
        result = type::integral_from_type<T>(lhs_imm) op type::integral_from_type<T>(rhs_imm); \
    }

#define make_string_relational_entry(op)                                       \
    m::pattern | std::string{"string"} = [&] {                                 \
        result = std::string_view{lhs_imm}.compare(std::string_view{rhs_imm}); \
    }

#define make_char_relational_entry(op)                                    \
    m::pattern | std::string{"char"} = [&] {                              \
        result = static_cast<int>(static_cast<unsigned char>(lhs_imm[1])) \
        op                                                                \
        static_cast<int>(static_cast<unsigned char>(rhs_imm[1]));         \
    }

#define make_trivial_immediate_binary_result(op)         \
    m::pattern | std::string{STRINGIFY(op)} = [&] {      \
        m::match(lhs_type) (                             \
            make_integral_relational_entry(int, op),     \
            make_integral_relational_entry(long, op),    \
            make_integral_relational_entry(float, op),   \
            make_integral_relational_entry(double, op),  \
            make_string_relational_entry(op),            \
            make_char_relational_entry(op)               \
        );                                               \
    }

#define REGISTER_OSTREAM(reg) \
    case rr(reg):             \
        os << STRINGIFY(reg); \
    break

#define MNEMONIC_OSTREAM(mnem)                             \
    case mn(mnem): {                                       \
        auto mnem_str = std::string_view{STRINGIFY(mnem)}; \
        if (util::contains(mnem_str, "_"))                 \
            mnem_str.remove_suffix(1);                     \
        os << mnem_str;                                    \
        };                                                 \
    break

namespace credence::target::x86_64::detail {

// clang-format off
enum class Register
{
    rbp, rsp, rax, rbx, rcx, rdx, rsi, rdi,
    r8, r9, r10, r11, r12, r13, r14, ebp,
    esp, eax, ebx, edx, ecx, esi, edi, r8d, ax,
    r9d, r10d, r11d, r12d, r13d, r14d, r15d, al
};

constexpr auto math_binary_operators =
    { "*", "/", "-", "+", "%" };
constexpr auto bitwise_operators =
    { "<<", ">>", "^", "&", "%", "~" };
constexpr auto unary_operators =
    { "++", "--", "*", "&", "-", "+", "~", "!", "~" };
constexpr auto relation_binary_operators =
    { "==", "!=", "<",  "&&",
    "||", ">",  "<=", ">=" };

constexpr const auto QWORD_REGISTER = {
    Register::rdi, Register::r8,  Register::r9,
    Register::rsi, Register::rdx, Register::rcx
};
constexpr const auto DWORD_REGISTER = {
    Register::rdi, Register::r8,  Register::r9,
    Register::rsi, Register::rdx, Register::rcx
};

enum class Mnemonic
{
    imul, lea, ret, sub, add,
    neg, je, jne, jle, jl,
    idiv, inc, dec, cqo, cdq,
    leave, mov, push, pop, call,
    cmp, sete, setne, setl, setg,
    setle, setge, mov_, and_, or_,
    xor_, not_, shl, shr
};

// clang-format on

template<typename T>
T trivial_arithmetic_from_numeric_table_type(
    std::string const& lhs,
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

template<typename T>
T trivial_bitwise_from_numeric_table_type(
    std::string const& lhs,
    std::string const& op,
    std::string const& rhs)
{
    T result{ 0 };
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
    return result;
}

constexpr inline bool is_binary_math_operator(symbol::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        math_binary_operators.begin(),
        math_binary_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != math_binary_operators.end();
}

constexpr inline bool is_bitwise_operator(symbol::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        bitwise_operators.begin(),
        bitwise_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != bitwise_operators.end();
}

constexpr inline bool is_relation_binary_operator(symbol::RValue const& rvalue)
{
    if (util::substring_count_of(rvalue, " ") != 2)
        return false;
    auto test = std::ranges::find_if(
        relation_binary_operators.begin(),
        relation_binary_operators.end(),
        [&](std::string_view s) {
            return rvalue.find(s) != std::string::npos;
        });
    return test != relation_binary_operators.end();
}

constexpr inline bool is_binary_operator(symbol::RValue const& rvalue)
{
    return is_binary_math_operator(rvalue) or
           is_relation_binary_operator(rvalue) or is_bitwise_operator(rvalue);
}

constexpr inline bool is_unary_operator(symbol::RValue const& rvalue)
{
    return symbol::is_unary(rvalue);
}

enum class Operand_Size : std::size_t
{
    Byte = 1,
    Word = 2,
    Dword = 4,
    Qword = 8,
    Empty = 0
};

const std::map<Operand_Size, std::string> suffix = {
    { Operand_Size::Byte, "b" },
    { Operand_Size::Word, "w" },
    { Operand_Size::Dword, "l" },
    { Operand_Size::Qword, "q" }
};

constexpr inline bool is_qword_register(Register r)
{
    return std::ranges::find(QWORD_REGISTER.begin(), QWORD_REGISTER.end(), r) !=
           QWORD_REGISTER.end();
}
constexpr inline bool is_dword_register(Register r)
{
    return std::ranges::find(DWORD_REGISTER.begin(), DWORD_REGISTER.end(), r) !=
           DWORD_REGISTER.end();
}

constexpr inline Operand_Size get_size_from_accumulator_register(Register acc)
{
    if (acc == Register::al) {
        return Operand_Size::Byte;
    } else if (acc == Register::ax) {
        return Operand_Size::Word;
    } else if (is_qword_register(acc)) {
        return Operand_Size::Qword;
    } else {
        return Operand_Size::Dword;
    }
}

constexpr inline std::size_t get_size_from_operand_size(Operand_Size size)
{
    return static_cast<std::underlying_type_t<Operand_Size>>(size);
}

constexpr inline std::ostream& operator<<(std::ostream& os, Register reg)
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
        REGISTER_OSTREAM(al);
        REGISTER_OSTREAM(ax);
    }
    return os;
}

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
        MNEMONIC_OSTREAM(idiv);
        MNEMONIC_OSTREAM(inc);
        MNEMONIC_OSTREAM(dec);
        MNEMONIC_OSTREAM(cqo);
        MNEMONIC_OSTREAM(cdq);
        MNEMONIC_OSTREAM(leave);
        MNEMONIC_OSTREAM(mov);
        MNEMONIC_OSTREAM(mov_);
        MNEMONIC_OSTREAM(push);
        MNEMONIC_OSTREAM(pop);
        MNEMONIC_OSTREAM(call);
        MNEMONIC_OSTREAM(cmp);
        MNEMONIC_OSTREAM(sete);
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
    }
    return os;
}

Register get_accumulator_register_from_size(
    Operand_Size size = Operand_Size::Dword);

constexpr Operand_Size get_size_from_table_rvalue(
    symbol::Data_Type const& rvalue)
{
    namespace m = matchit;
    using T = symbol::Type;
    symbol::Type type = symbol::get_type_from_rvalue_data_type(rvalue);
    return m::match(type)(
        m::pattern | m::or_(T{ "int" }, T{ "string" }) =
            [&] { return Operand_Size::Dword; },
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

using Stack_Offset = std::size_t;

inline constexpr auto O_NUL = std::monostate{};

using Immediate = symbol::Data_Type;
using Storage = std::variant<std::monostate, Stack_Offset, Register, Immediate>;
using Instruction = std::tuple<Mnemonic, Storage, Storage>;
using Label = symbol::Label;
using Instructions = std::deque<std::variant<Label, Instruction>>;
using Instruction_Pair = std::pair<Storage, detail::Instructions>;

Immediate get_result_from_trivial_integral_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_relational_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);
Immediate get_result_from_trivial_bitwise_expression(
    Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs);

inline Instructions make()
{
    return Instructions{};
}

inline void insert(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}
template<typename T = int>
inline Immediate make_int_immediate(T imm, std::string const& type = "int")
{
    return Immediate{ std::to_string(imm), type, 4UL };
}

inline Immediate make_u32_int_immediate(unsigned int imm)
{
    return Immediate{ std::to_string(imm), "int", 4UL };
}

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
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_eq);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_neq);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_lt);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_gt);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_le);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_ge);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_or);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_and);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(r_lt);
// b (bitwise)
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);

} // namespace x86_64::detail