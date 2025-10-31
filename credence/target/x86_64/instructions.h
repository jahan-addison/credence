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

namespace detail {

struct Stack_Address
{
    constexpr explicit Stack_Address(unsigned int offset)
        : offset(offset)
    {
    }
    Stack_Address() = delete;
    unsigned int offset;
};

} // namespace detail

enum class Register
{
    rbp,
    rsp,

    rax,
    rbx,
    rcx,
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
    noop
};

enum class Mnemonic
{
    imul,
    lea,
    ret,
    leave,
    mov,
    push,
    pop,
    call
};

enum class Operand_Type
{
    Register,
    Label,
    Immediate,
};

enum class Operand_Size
{
    Byte = 1,
    Word,
    Dword,
    Qword,
};

namespace detail {

using Immediate = ir::Table::RValue_Data_Type;
using Storage = std::variant<Stack_Address, Register, Immediate>;
using Instruction = std::tuple<Mnemonic, Operand_Size, Storage, Storage>;
using Instructions = std::deque<Instruction>;
using Operation_Pair = std::pair<Storage, Instructions>;

inline Instructions make_instructions()
{
    return Instructions{};
}

inline void insert_instructions(Instructions& to, Instructions const& from)
{
    to.insert(to.end(), from.begin(), from.end());
}

Operand_Size get_size_from_table_rvalue(
    ir::Table::RValue_Data_Type const& rvalue);

Operation_Pair push(Operand_Size size);
Operation_Pair pop(Operand_Size size, Storage const& dest);
Operation_Pair mov(Operand_Size size, Storage const& src, Storage const& dest);
Operation_Pair enter(Operand_Size size);
Operation_Pair leave(Operand_Size size);

Operation_Pair imul(Operand_Size size, Storage const& lhs, Storage const& rhs);

} // namespace detail

} // namespace x86_64