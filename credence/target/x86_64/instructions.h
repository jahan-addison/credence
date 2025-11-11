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

#include <credence/ir/table.h> // for Table
#include <deque>               // for deque
#include <string>              // for basic_string, string
#include <tuple>               // for tuple
#include <utility>             // for pair
#include <variant>             // for variant

#define mn(n) Mnemonic::n
#define rr(n) Register::n

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

#define addii(inst, op, lhs, rhs)    \
    inst.emplace_back(detail::Instruction{op, lhs, rhs})

namespace credence::target::x86_64 {

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

    al,
    ax
};

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
    idiv,
    inc,
    dec,
    cqo,
    cdq,
    leave,
    mov,
    push,
    pop,
    call,
    cmp,
    sete,
    setne,
    setl,
    setg,
    setle,
    setge,
    and_,
    mov_,
    xor_,
    not_
};

enum class Operand_Size : std::size_t
{
    Byte = 1,
    Word = 2,
    Dword = 4,
    Qword = 8,
    Empty = 0
};

constexpr inline std::size_t get_size_from_operand_size(Operand_Size size)
{
    return static_cast<std::underlying_type_t<Operand_Size>>(size);
}

constexpr inline std::ostream& operator<<(std::ostream& os, Register r)
{
    switch (r) {
        case rr(rbp):
            os << "rbp";
            break;

        case rr(rsp):
            os << "rsp";
            break;

        case rr(rax):
            os << "rax";
            break;

        case rr(rbx):
            os << "rbx";
            break;

        case rr(rcx):
            os << "rcx";
            break;

        case rr(rdx):
            os << "rdx";
            break;

        case rr(rsi):
            os << "rsi";
            break;

        case rr(rdi):
            os << "rdi";
            break;

        case rr(r8):
            os << "r8";
            break;

        case rr(r9):
            os << "r9";
            break;

        case rr(r10):
            os << "r10";
            break;

        case rr(r11):
            os << "r11";
            break;

        case rr(r12):
            os << "r12";
            break;

        case rr(r13):
            os << "r13";
            break;

        case rr(r14):
            os << "r14";
            break;

        case rr(ebp):
            os << "ebp";
            break;

        case rr(esp):
            os << "esp";
            break;

        case rr(eax):
            os << "eax";
            break;

        case rr(ebx):
            os << "ebx";
            break;

        case rr(edx):
            os << "edx";
            break;

        case rr(ecx):
            os << "ecx";
            break;

        case rr(esi):
            os << "esi";
            break;

        case rr(edi):
            os << "edi";
            break;

        case rr(r8d):
            os << "r8d";
            break;

        case rr(r9d):
            os << "r9d";
            break;

        case rr(r10d):
            os << "r10d";
            break;

        case rr(r11d):
            os << "r11d";
            break;

        case rr(r12d):
            os << "r12d";
            break;

        case rr(r13d):
            os << "r13d";
            break;

        case rr(r14d):
            os << "r14d";
            break;

        case rr(r15d):
            os << "r15d";
            break;

        case rr(al):
            os << "al";
            break;

        case rr(ax):
            os << "ax";
            break;
    }
    return os;
}

constexpr std::ostream& operator<<(std::ostream& os, Mnemonic mnemonic)
{
    switch (mnemonic) {
        case mn(imul):
            os << "imul";
            break;
        case mn(neg):
            os << "neg";
            break;

        case mn(lea):
            os << "lea";
            break;

        case mn(ret):
            os << "ret";
            break;

        case mn(sub):
            os << "sub";
            break;

        case mn(add):
            os << "add";
            break;

        case mn(je):
            os << "je";
            break;

        case mn(jne):
            os << "jne";
            break;

        case mn(jle):
            os << "jle";
            break;

        case mn(jl):
            os << "jl";
            break;

        case mn(idiv):
            os << "idiv";
            break;

        case mn(inc):
            os << "inc";
            break;

        case mn(dec):
            os << "dec";
            break;

        case mn(cqo):
            os << "cqo";
            break;

        case mn(cdq):
            os << "cdq";
            break;

        case mn(leave):
            os << "leave";
            break;

        case mn(mov):
            os << "mov";
            break;

        case mn(mov_):
            os << "mov";
            break;

        case mn(push):
            os << "push";
            break;

        case mn(pop):
            os << "pop";
            break;

        case mn(call):
            os << "call";
            break;

        case mn(cmp):
            os << "cmp";
            break;

        case mn(sete):
            os << "sete";
            break;

        case mn(setne):
            os << "setne";
            break;

        case mn(setl):
            os << "setl";
            break;

        case mn(setg):
            os << "setg";
            break;

        case mn(setle):
            os << "setle";
            break;

        case mn(setge):
            os << "setge";
            break;

        case mn(and_):
            os << "and";
            break;

        case mn(not_):
            os << "not";
            break;

        case mn(xor_):
            os << "xor";
            break;
    }
    return os;
}

Register get_accumulator_register_from_size(
    Operand_Size size = Operand_Size::Dword);

constexpr Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue)
{
    namespace m = matchit;
    using T = ir::Table::Type;
    ir::Table::Type type = ir::Table::get_type_from_rvalue_data_type(rvalue);
    return m::match(type)(
        m::pattern | m::or_(T{ "int" }, T{ "string" }) =
            [&] { return Operand_Size::Dword; },
        m::pattern | m::or_(T{ "double" }, T{ "long" }) =
            [&] { return Operand_Size::Qword; },
        m::pattern | T{ "float" } = [&] { return Operand_Size::Dword; },
        m::pattern | T{ "char" } = [&] { return Operand_Size::Byte; },
        m::pattern | m::_ = [&] { return Operand_Size::Dword; });
}

namespace detail {

using Stack_Offset = std::size_t;

inline constexpr auto O_NUL = std::monostate{};
// clang-format off
using Immediate = ir::Table::RValue_Data_Type;
using Storage = std::variant<std::monostate, Stack_Offset, Register, Immediate>;
// Intel syntax, Storage = Storage
using Instruction = std::tuple<Mnemonic, Storage, Storage>;
using Three_Operand_Instruction = std::tuple<Mnemonic, Storage, Storage, Storage>;
using Label = ir::Table::Label;
using Instructions = std::deque<std::variant<Label, Instruction>>;
// clang-format on
using Instruction_Pair = std::pair<Storage, detail::Instructions>;

inline Instructions make_inst()
{
    return Instructions{};
}

inline void insert_inst(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}
template<typename T = int>
inline Immediate make_integer_immediate(T imm, std::string const& type = "int")
{
    return Immediate{ std::to_string(imm), type, 4UL };
}

inline Immediate make_u32_integer_immediate(unsigned int imm)
{
    return Immediate{ std::to_string(imm), "int", 4UL };
}

} // namespace detail

// Disambiguation:
// arithmetic
// b (bitwise)
// r (relational)
// u (unary)

DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(inc);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(dec);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mul);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(div);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(sub);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(neg);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(add);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(mod);

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

DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(rshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(lshift);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_and);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_or);
DEFINE_1ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_not);
DEFINE_2ARY_OPERAND_INSTRUCTION_FROM_TEMPLATE(b_xor);

} // namespace x86_64