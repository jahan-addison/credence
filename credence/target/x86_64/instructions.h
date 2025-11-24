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
#include <credence/types.h>    // for LValue, RValue, Data_Type, ...
#include <deque>               // for deque
#include <ostream>             // for ostream
#include <string>              // for basic_string, string
#include <tuple>               // for tuple
#include <utility>             // for pair
#include <variant>             // for variant

#define mn(n) Mnemonic::n
#define rr(n) Register::n

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define is_empty_storage(storage) is_variant(std::monostate, storage)

#define DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name)    \
    detail::Instruction_Pair name(detail::Storage const& dest, \
        detail::Storage const& src)

#define DEFINE_3ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name)    \
    detail::Instruction_Pair name(detail::Storage const& dest, \
        detail::Storage const& s1,                             \
        detail::Storage const& s2)

#define DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(name)  \
    detail::Instruction_Pair name(detail::Storage const& src)

#define add_i_s(inst, op, lhs, rhs)      \
    inst.emplace_back(detail::Instruction{Mnemonic::op,lhs, rhs})

#define add_i_ll(inst, op, lhs, rhs)     \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::lhs, rhs})

#define add_i_llr(inst, op, lhs, rhs)     \
    inst.emplace_back(detail::Instruction{Mnemonic::op,lhs, Register::rhs})

#define add_i_llrs(inst, op, lhs, rhs)    \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::lhs, Register::rhs})

#define add_i_e(inst, op)                \
    inst.emplace_back(detail::Instruction{Mnemonic::op,detail::O_NUL, detail::O_NUL})

#define add_i_d(inst, op, dest)          \
    inst.emplace_back(detail::Instruction{Mnemonic::op,dest, detail::O_NUL})

#define add_i_ld(inst, op, dest)          \
    inst.emplace_back(detail::Instruction{Mnemonic::op,Register::dest, detail::O_NUL})

#define add_i_ls(inst, op, dest)         \
    inst.emplace_back(detail::Instruction{Mnemonic::op, dest, detail::O_NUL})

#define add_i(inst, op, lhs, rhs)       \
    inst.emplace_back(detail::Instruction{op, lhs, rhs})

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define make_integral_relational_entry(T, op)               \
    m::pattern | std::string{STRINGIFY(T)} = [&] {          \
        result = type::integral_from_type<T>(lhs_imm) op    \
        type::integral_from_type<T>(rhs_imm);               \
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
    rbp, rsp, rax, rbx, rcx,
    rdx, rsi, rdi, r8, r9,
    r10, r11, r12, r13, r14,
    ebp, di, esp, eax, ebx,
    edx, ecx, esi, edi, r8d,
    ax, r9d, r10d, r11d, r12d,
    r13d, r14d, r15d, al, dil
};

constexpr const auto QWORD_REGISTER = {
    Register::rdi, Register::r8,  Register::r9,
    Register::rsi, Register::rdx, Register::rcx,
    Register::rax
};
constexpr const auto DWORD_REGISTER = {
    Register::edi, Register::r8d,  Register::r9d,
    Register::esi, Register::edx, Register::ecx,
    Register::eax
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

constexpr const auto QWORD_STORAGE_MNEMONIC = {
    mn(lea)
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

constexpr bool is_qword_register(Register r)
{
    return std::ranges::find(QWORD_REGISTER.begin(), QWORD_REGISTER.end(), r) !=
           QWORD_REGISTER.end();
}
constexpr bool is_dword_register(Register r)
{
    return std::ranges::find(DWORD_REGISTER.begin(), DWORD_REGISTER.end(), r) !=
           DWORD_REGISTER.end();
}

constexpr Operand_Size get_size_from_accumulator_register(Register acc)
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

constexpr std::size_t get_size_from_operand_size(Operand_Size size)
{
    return static_cast<std::underlying_type_t<Operand_Size>>(size);
}

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
    }
    return os;
}

inline std::string register_as_string(Register device)
{
    std::ostringstream buf{};
    buf << device;
    return buf.str();
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

constexpr Operand_Size get_operand_size_from_rvalue_datatype(
    type::Data_Type const& rvalue)
{
    namespace m = matchit;
    using T = type::semantic::Type;
    T type = type::get_type_from_rvalue_data_type(rvalue);
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

using Immediate = type::Data_Type;
using Storage = std::variant<std::monostate, Stack_Offset, Register, Immediate>;
using Instruction = std::tuple<Mnemonic, Storage, Storage>;
using Instructions =
    std::deque<std::variant<type::semantic::Label, Instruction>>;
using Instruction_Pair = std::pair<Storage, detail::Instructions>;

inline Instructions make() noexcept
{
    return Instructions{};
}

constexpr std::string tabwidth(unsigned int t)
{
    std::string tab{};
    for (; t > 0; t--)
        tab += " ";
    return tab;
}

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

constexpr bool is_qword_storage_mnemonic(Mnemonic inst)
{
    return std::ranges::find(
               QWORD_STORAGE_MNEMONIC.begin(),
               QWORD_STORAGE_MNEMONIC.end(),
               inst) != QWORD_STORAGE_MNEMONIC.end();
}

std::string get_storage_as_string(Storage const& storage);

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
// pointers
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lea);

} // namespace x86_64::detail