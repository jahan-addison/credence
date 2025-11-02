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

namespace credence::target::x86_64 {

// https://math.hws.edu/eck/cs220/f22/registers.html

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
    r15d
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
    call
};

enum class Operand_Size
{
    Byte,
    Word,
    Dword,
    Qword,
};

struct Stack_Address
{
    constexpr explicit Stack_Address(unsigned int offset)
        : offset(offset)
    {
    }
    Stack_Address() = delete;
    unsigned int offset;
};

inline constexpr auto O_NUL = std::monostate{};

using Immediate = ir::Table::RValue_Data_Type;
using Storage =
    std::variant<std::monostate, Stack_Address, Register, Immediate>;
using Instruction = std::tuple<Mnemonic, Operand_Size, Storage, Storage>;
using Instructions = std::deque<Instruction>;
using Operation_Pair = std::pair<Storage, Instructions>;

inline Instructions make_inst()
{
    return Instructions{};
}
inline void insert_inst(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue);

Operation_Pair imul(Operand_Size size, Storage& lhs, Storage& rhs);
Operation_Pair idiv(Operand_Size size, Storage& lhs, Storage& rhs);
Operation_Pair sub(Operand_Size size, Storage& lhs, Storage& rhs);
Operation_Pair add(Operand_Size size, Storage& lhs, Storage& rhs);
Operation_Pair mod(Operand_Size size, Storage& lhs, Storage& rhs);

} // namespace x86_64