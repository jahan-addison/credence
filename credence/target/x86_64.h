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

#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for Backend
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <ostream>                  // for ostream
#include <stack>                    // for stack
#include <string>                   // for basic_string, string
#include <variant>                  // for variant
#include <vector>                   // for vector

// https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf

namespace credence::target::x86_64 {

// https://math.hws.edu/eck/cs220/f22/registers.html

namespace registers {

enum class r64
{
    rbp, // frame pointer
    rsp, // stack pointer
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
};

enum class r32
{
    ebp, // frame pointer
    esp, // stack pointer
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
    r15d

};

} // namespace registers

class Code_Generator final : public target::Backend_Target_Platform
{
  public:
    Code_Generator() = delete;
    explicit Code_Generator(ir::Table::Table_PTR table)
        : Backend_Target_Platform(std::move(table))
    {
    }

  public:
    using Immediate = std::string;
    using Register = std::variant<registers::r64, registers::r32>;

  private:
    struct Stack_Address
    {
        constexpr explicit Stack_Address(unsigned int offset)
            : offset(offset)
        {
        }
        Stack_Address() = delete;
        unsigned int offset;
    };
    using Instructions = std::vector<std::string>;
    using Storage =
        std::variant<std::monostate, Stack_Address, Register, Immediate>;
    using Data_Location = std::pair<Storage, Instructions>;

  private:
    void setup_table();
    void setup_registers();
    void setup_constants();
    void teardown_registers();

  public:
    enum class Operand_Type
    {
        Register,
        Label,
        Immediate,
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

  public:
    void build_from_ita();
    // void emit(std::ostream& os) override;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void emit_push(Operand_Size size);
    void emit_pop(Operand_Size size, Storage const& dest);
    void emit_mov(Operand_Size size, Storage const& src, Storage const& dest);

    CREDENCE_PRIVATE_UNLESS_TESTED:
    void emit_enter(Operand_Size size);
    void emit_leave(Operand_Size size);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Immediate from_immediate_value(ir::Table::RValue_Data_Type const& imm_value);
    Data_Location from_ita_expression(ir::Table::RValue const& expr);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Location from_ita_unary_expression(ir::ITA::Quadruple const& inst);
    Data_Location from_ita_bitwise_expression(ir::ITA::Quadruple const& inst);
    Data_Location from_ita_binary_expression(ir::ITA::Quadruple const& inst);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    // void from_func_start_ita() override;
    // void from_func_end_ita() override;
    // void from_label_ita() override;
    // void from_goto_ita() override;
    // void from_locl_ita() override;
    // void from_globl_ita() override;
    // void from_if_ita() override;
    // void from_jmp_e_ita() override;
    // void from_push_ita() override;
    // void from_pop_ita() override;
    // void from_call_ita() override;
    // void from_cmp_ita() override;
    // void from_mov_ita() override;
    // void from_return_ita() override;
    void from_leave_ita() override;
    void from_noop_ita() override {
    }
    // clang-format on

  private:
    unsigned int constant_index{ 0 };

  private:
    std::string current_frame{ "main" };
    Instructions instructions_;
    Instructions data_;
    std::stack<registers::r64> gpr_stack_64_{};
    std::stack<registers::r32> gpr_stack_32_{};
};

} // namespace x86_64
