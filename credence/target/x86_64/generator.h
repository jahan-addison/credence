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
#include <credence/map.h>           // for Ordered_Map
#include <credence/target/target.h> // for Backend
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>                    // for deque
#include <initializer_list>         // for initializer_list
#include <map>                      // for map
#include <numeric>                  // for accumulate
#include <ostream>                  // for ostream
#include <set>                      // for set
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

  private:
    using Stack_Entry = std::pair<detail::Stack_Offset, Operand_Size>;
    using Stack_Pair = std::pair<ir::Table::LValue, Stack_Entry>;
    using Operator_Symbol = std::string;
    using Instruction_Pair = detail::Instruction_Pair;
    using Storage = detail::Storage;
    using Immediate = detail::Immediate;
    using Instructions = detail::Instructions;
    using Storage_Operands = std::pair<Storage, Storage>;
    using Local_Stack = Ordered_Map<ir::Table::LValue, Stack_Entry>;

  public:
    const Storage EMPTY_STORAGE = std::monostate{};
    // clang-format off
    const std::map<Operand_Size, std::string>
    suffix = { { Operand_Size::Byte, "b" },
              { Operand_Size::Word, "w" },
              { Operand_Size::Dword, "l" },
              { Operand_Size::Qword,
                "q" } };
    static constexpr const auto QWORD_REGISTER = {
        Register::rdi, Register::r8,
        Register::r9,  Register::rsi,
        Register::rdx, Register::rcx
    };
    static constexpr const auto DWORD_REGISTER = {
        Register::rdi, Register::r8,
        Register::r9,  Register::rsi,
        Register::rdx, Register::rcx
    };
  private:
    std::deque<Register> available_qword_register =
    {
        Register::rdi, Register::r8,
        Register::r9,  Register::rsi,
        Register::rdx, Register::rcx
    };
    std::deque<Register> available_dword_register =
    {
        Register::edi, Register::r8d,
        Register::r9d, Register::esi,
        Register::edx, Register::ecx
    };
    constexpr static auto math_binary_operators = {
        "*", "/", "-", "+", "%"
    };
    constexpr static auto unary_operators = {
        "++", "--", "*", "&", "-",
        "+",  "~",  "!", "~"
    };
    constexpr static auto relation_binary_operators = {
        "==", "!=", "<", "&&",
        "||", ">", "<=", ">="
    };
    // clang-format on
    constexpr inline bool is_binary_math_operator(
        ir::Table::RValue const& rvalue)
    {
        auto test = std::ranges::find_if(
            math_binary_operators.begin(),
            math_binary_operators.end(),
            [&](std::string_view s) {
                return rvalue.find(s) != std::string::npos;
            });
        return test != math_binary_operators.end();
    }

    constexpr inline bool is_relation_binary_operator(
        ir::Table::RValue const& rvalue)
    {
        auto test = std::ranges::find_if(
            relation_binary_operators.begin(),
            relation_binary_operators.end(),
            [&](std::string_view s) {
                return rvalue.find(s) != std::string::npos;
            });
        return test != relation_binary_operators.end();
    }

    constexpr inline bool is_binary_operator(ir::Table::RValue const& rvalue)
    {
        return is_binary_math_operator(rvalue) or
               is_relation_binary_operator(rvalue);
    }

    constexpr inline bool is_unary_operator(ir::Table::RValue const& rvalue)
    {
        return ir::Table::is_unary(rvalue);
    }

    constexpr inline Register get_accumulator_register_from_size(
        Operand_Size size = Operand_Size::Dword)
    {
        namespace m = matchit;
        if (special_register != Register::eax) {
            auto special = special_register;
            special_register = Register::eax;
            return special;
        }
        return m::match(size)(
            m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
            m::pattern | Operand_Size::Word = [&] { return Register::ax; },
            m::pattern | Operand_Size::Byte = [&] { return Register::al; },
            m::pattern | m::_ = [&] { return Register::eax; });
    }

    constexpr inline Operand_Size get_size_from_accumulator_register(
        Register acc)
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
    void set_stack_address_from_accumulator_size(
        ir::Table::LValue const& lvalue,
        Register acc);
    void set_stack_address_from_immediate(
        ir::Table::LValue const& lvalue,
        Immediate const& rvalue);

  public:
    void emit(std::ostream& os) override;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void insert_from_temporary_table_rvalue(ir::Table::RValue const& expr);
    void from_binary_operator_expression(ir::Table::RValue const& expr);
    void insert_from_temporary_immediate_rvalues(
        Storage& lhs,
        std::string const& op,
        Storage& rhs);
    Immediate get_result_from_trivial_integral_expression(
        Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);
    Immediate get_result_from_trivial_relational_expression(
        Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);
    Storage get_storage_from_temporary_lvalue(
        ir::Table::LValue const& lvalue,
        std::string const& op);
    Instruction_Pair from_ita_expression(ir::Table::RValue const& expr);
    Instruction_Pair from_ita_unary_expression(ir::ITA::Quadruple const& inst);
    Instruction_Pair from_ita_bitwise_expression(
        ir::ITA::Quadruple const& inst);
    Instruction_Pair from_arithmetic_expression_operands(
        Storage_Operands& operands,
        std::string const& binary_op);
    Instruction_Pair from_relational_expression_operands(
        Storage_Operands& operands,
        std::string const& binary_op);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::string emit_storage_device(Storage const& storage);
    void set_table_stack_frame(ir::Table::Label const& name);
    void setup_constants();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Storage get_storage_device(Operand_Size size = Operand_Size::Qword);
    // clang-format on
    inline std::size_t get_stack_offset_from_lvalue(
        ir::Table::LValue const& lvalue)
    {
        CREDENCE_ASSERT(stack_address[lvalue].second != Operand_Size::Empty);
        return stack_address[lvalue].first;
    }

    constexpr inline std::size_t get_stack_offset()
    {
        return std::accumulate(
            stack_address.begin(),
            stack_address.end(),
            0UL,
            [&](std::size_t offset, Stack_Pair const& entry) {
                return offset += entry.second.first;
            });
    }

    constexpr inline std::size_t get_size_of_stored_lvalues()
    {
        return std::accumulate(
            stack_address.begin(),
            stack_address.end(),
            0UL,
            [&](std::size_t offset, Stack_Pair const& entry) {
                if (entry.second.second != Operand_Size::Empty)
                    return ++offset;
                return offset;
            });
    }

    constexpr inline Operand_Size get_stack_operand_size_from_offset(
        std::size_t offset)
    {
        return std::accumulate(
            stack_address.begin(),
            stack_address.end(),
            Operand_Size::Empty,
            [&](Operand_Size size, Stack_Pair const& entry) {
                if (entry.second.first == offset)
                    return entry.second.second;
                return size;
            });
    }

    constexpr inline void reset_o_register()
    {
        // clang-format off
        available_qword_register = {
            Register::rdi, Register::r8,  Register::r9,
            Register::rsi, Register::rdx, Register::rcx
        };
        available_dword_register = {
            Register::edi, Register::esi, Register::r8d,
            Register::r9d, Register::edx, Register::ecx
        };
        // clang-format on
    }

    constexpr inline bool is_qword_register(Register r)
    {
        return std::ranges::find(
                   QWORD_REGISTER.begin(), QWORD_REGISTER.end(), r) !=
               QWORD_REGISTER.end();
    }

    constexpr inline bool is_dword_register(Register r)
    {
        return std::ranges::find(
                   DWORD_REGISTER.begin(), DWORD_REGISTER.end(), r) !=
               DWORD_REGISTER.end();
    }

    // clang-format off
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
    Local_Stack stack_address{};

  private:
    std::string current_frame{ "main" };
    std::size_t ita_index{ 0 };
    Register special_register = Register::eax;
    bool temporary_expansion{ false };
    detail::Instructions instructions_;
    detail::Instructions data_;
};

void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast);

} // namespace x86_64
