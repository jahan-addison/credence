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

#define BINARY_OP_INST_DEFINITION(name)                \
    detail::Instruction_Pair name(Operand_Size size, detail::Storage& dest, detail::Storage& src)

#define THREE_OP_INST_DEFINITION(name)                \
    detail::Instruction_Pair name(Operand_Size size, detail::Storage& dest, detail::Storage& s1, detail::Storage& s2)

#define UNARY_OP_INST_DEFINITION(name)                 \
    detail::Instruction_Pair name(Operand_Size size, detail::Storage& dest)

#define add_inst_s(inst, op, size, lhs, rhs)    \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, lhs, rhs})

#define add_inst_ll(inst, op, size, lhs, rhs)   \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, Register::lhs, rhs})

#define add_inst_lr(inst, op, size, lhs, rhs)   \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, lhs, Register::rhs})

#define add_inst_lrs(inst, op, size, lhs, rhs)  \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, Register::lhs, Register::rhs})

#define add_inst_e(inst, op, size)              \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, detail::O_NUL, detail::O_NUL})

#define add_inst_d(inst, op, size, dest)        \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, dest, detail::O_NUL})

#define add_inst_ld(inst, op, size, dest)        \
    inst.emplace_back(detail::Instruction{Mnemonic::op, size, Register::dest, detail::O_NUL})

#define add_inst(inst, op, size, lhs, rhs)      \
    inst.emplace_back(detail::Instruction{op, size, lhs, rhs})

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

    al
};

enum class Mnemonic
{
    imul,
    lea,
    ret,
    sub,
    add,
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
    xor_
};

enum class Operand_Size
{
    Byte = 1,
    Word = 2,
    Dword = 4,
    Qword = 8,
};

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
    }
    return os;
}

constexpr std::ostream& operator<<(std::ostream& os, Mnemonic mnemonic)
{
    switch (mnemonic) {
        case mn(imul):
            os << "imul";
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

        case mn(xor_):
            os << "xor";
            break;
    }
    return os;
}

namespace detail {

using Stack_Offset = unsigned int;

inline constexpr auto O_NUL = std::monostate{};

using Immediate = ir::Table::RValue_Data_Type;
using Storage = std::variant<std::monostate, Stack_Offset, Register, Immediate>;

// Intel syntax, Storage = Storage
using Instruction = std::tuple<Mnemonic, Operand_Size, Storage, Storage>;
using Three_Operand_Instruction =
    std::tuple<Mnemonic, Operand_Size, Storage, Storage, Storage>;
using Label = ir::Table::Label;
using Instructions = std::deque<std::variant<Label, Instruction>>;

using Instruction_Pair = std::pair<Storage, detail::Instructions>;

inline Instructions make_inst()
{
    return Instructions{};
}

inline void insert_inst(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

inline Immediate make_integer_immediate(int imm)
{
    return Immediate{ std::to_string(imm), "int", 4UL };
}

inline Immediate make_u32_integer_immediate(unsigned int imm)
{
    return Immediate{ std::to_string(imm), "int", 4UL };
}

} // namespace detail

Register get_accumulator_register_from_size(
    Operand_Size size = Operand_Size::Dword);
Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue);

UNARY_OP_INST_DEFINITION(inc);
UNARY_OP_INST_DEFINITION(dec);
UNARY_OP_INST_DEFINITION(u_not);
BINARY_OP_INST_DEFINITION(mul);
BINARY_OP_INST_DEFINITION(div);
BINARY_OP_INST_DEFINITION(sub);
BINARY_OP_INST_DEFINITION(add);
BINARY_OP_INST_DEFINITION(mod);
BINARY_OP_INST_DEFINITION(r_eq);
BINARY_OP_INST_DEFINITION(r_neq);
BINARY_OP_INST_DEFINITION(r_lt);
BINARY_OP_INST_DEFINITION(r_gt);
BINARY_OP_INST_DEFINITION(r_le);
BINARY_OP_INST_DEFINITION(r_ge);
BINARY_OP_INST_DEFINITION(r_or);
BINARY_OP_INST_DEFINITION(r_and);
BINARY_OP_INST_DEFINITION(r_lt);
THREE_OP_INST_DEFINITION(imul);

} // namespace x86_64