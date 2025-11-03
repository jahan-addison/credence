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

#include "instructions.h"           // for Register, Operand_Size, Immediate
#include <algorithm>                // for __find_if, find_if
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for Backend
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>                    // for deque
#include <initializer_list>         // for initializer_list
#include <map>                      // for map
#include <ostream>                  // for ostream
#include <string>                   // for basic_string, string
#include <string_view>              // for basic_string_view, string_view
#include <utility>                  // for pair, move
#include <variant>                  // for variant

namespace credence::target::x86_64 {

class Code_Generator final : public target::Backend<detail::Storage>
{
  public:
    Code_Generator() = delete;
    explicit Code_Generator(ir::Table::Table_PTR table)
        : Backend(std::move(table))
    {
    }

  public:
    using Instruction_Pair = detail::Instruction_Pair;
    using Storage = detail::Storage;
    using Instructions = detail::Instructions;
    using Operands = std::pair<Storage, Storage>;
    using Binary_Operands = std::pair<std::string, Operands>;
    using Immediate_Operands = std::variant<
        ir::Table::Binary_Expression,
        ir::Table::LValue,
        detail::Immediate>;
    using RValue_Operands = std::variant<
        std::pair<detail::Immediate, detail::Immediate>,
        detail::Immediate>;
    using Local_Stack = std::map<ir::Table::LValue, unsigned int>;

  public:
    std::map<Operand_Size, std::string> suffix = { { Operand_Size::Byte, "b" },
                                                   { Operand_Size::Word, "w" },
                                                   { Operand_Size::Dword, "l" },
                                                   { Operand_Size::Qword,
                                                     "q" } };
    constexpr static auto math_binary_operators = { "*", "/", "-", "+", "%" };
    constexpr static auto relation_binary_operators = { "==", "!=", "<",
                                                        ">",  "<=", ">=" };
    constexpr inline bool is_binary_math_operator(ir::Table::RValue rvalue)
    {
        auto test = std::ranges::find_if(
            math_binary_operators.begin(),
            math_binary_operators.end(),
            [&](std::string_view s) {
                return rvalue.find(s) != std::string::npos;
            });
        return test != math_binary_operators.end();
    }

    constexpr inline bool is_relation_binary_operators(ir::Table::RValue rvalue)
    {
        auto test = std::ranges::find_if(
            relation_binary_operators.begin(),
            relation_binary_operators.end(),
            [&](std::string_view s) {
                return rvalue.find(s) != std::string::npos;
            });
        return test != relation_binary_operators.end();
    }

  public:
    void emit(std::ostream& os) override;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    RValue_Operands resolve_immediate_operands_from_table(
        Immediate_Operands const& imm_value);
    Binary_Operands operands_from_binary_ita_operands(
        ir::ITA::Quadruple const& inst);
    Instruction_Pair from_ita_expression(ir::Table::RValue const& expr);
    Instruction_Pair from_ita_unary_expression(ir::ITA::Quadruple const& inst);
    Instruction_Pair from_ita_bitwise_expression(ir::ITA::Quadruple const& inst);
    Instruction_Pair from_ita_trivial_relational_expression(
        ir::ITA::Quadruple const& inst);
    Instruction_Pair from_ita_binary_arithmetic_expression(
        ir::ITA::Quadruple const& inst);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::string emit_storage_device(Storage const& storage);
    void set_table_stack_frame(ir::Table::Label const& name);
    void setup_constants();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Storage get_storage_device(Operand_Size size = Operand_Size::Qword);
    Storage get_stack_address(Operand_Size size = Operand_Size::Qword);

    std::deque<Register> o_qword_register = { Register::rdi, Register::rsi,
                                        Register::rdx, Register::rcx,
                                        Register::r8,  Register::r9 };
    std::deque<Register> o_dword_register = { Register::edi, Register::esi,
                                        Register::edx, Register::ecx,
                                        Register::r8d,  Register::r9d };

    inline void reset_o_register()
    {
        o_qword_register = { Register::rdi, Register::rsi, Register::rdx,
                             Register::rcx, Register::r8,  Register::r9 };
        o_dword_register = { Register::edi, Register::esi, Register::edx,
                             Register::ecx, Register::r8d, Register::r9d };
    }

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_ita_table();
    void from_func_start_ita(ir::Table::Label const& name) override;
    void from_func_end_ita() override;
    void from_locl_ita(ITA_Inst const& inst) override;
    void from_cmp_ita(ITA_Inst const& inst) override;
    void from_mov_ita(ITA_Inst const& inst) override;
    void from_return_ita(Storage const& dest) override;
    void from_leave_ita() override;
    void from_label_ita(ITA_Inst const& inst) override;
    void from_push_ita(ITA_Inst const& inst) override;
    // void from_goto_ita() override;
    // void from_globl_ita() override;
    // void from_if_ita() override;
    // void from_jmp_e_ita() override;
    // void from_pop_ita() override;
    // void from_call_ita() override;
    void from_noop_ita() override {
    }
    // clang-format on

  private:
    unsigned int constant_index{ 0 };
    unsigned int stack_offset{ 0 };
    Local_Stack stack{};

  private:
    std::string current_frame{ "main" };

    detail::Instructions instructions_;
    detail::Instructions data_;
};

} // namespace x86_64
