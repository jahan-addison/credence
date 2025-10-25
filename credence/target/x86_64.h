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

#include <credence/target/target.h>

// https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf

namespace credence {
namespace target {
namespace x86_64 {

// https://math.hws.edu/eck/cs220/f22/registers.html

enum class Registers_64bit
{
    rax,
    rbx, // save before using
    rcx,
    rsi,
    rdi,
    rbp, // frame pointer
    rsp, // stack pointer
    r8,
    r9,
    r10,
    r11,
    r12, // save before using
    r13, // save before using
    r14, // save before using
    r15  // save before using

};

enum class Registers_32bit
{
    eax,
    ebx, // save before using
    ecx,
    esi,
    edi,
    ebp, // frame pointer
    esp, // stack pointer
    r8d,
    r9d,
    r10d,
    r11d,
    r12d, // save before using
    r13d, // save before using
    r14d, // save before using
    r15d  // save before using

};

enum class Operand_Type
{
    Register,
    Address,
    Label,
    Immediate,
    Count
};

enum class Operand_Size
{
    Byte = 1,
    Word = 2,
    Dword = 4,
    Qword = 8,
    Oword = 16,
    Yword = 32,
};

class Code_Generator : target::Backend
{};

} // namespace x86_64
} // namespace target
} // namespace credence