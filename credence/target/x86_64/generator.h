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
#include <credence/types.h>         // for LValue, RValue, Data_Type, ...
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <deque>                    // for deque
#include <functional>               // for std::function
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

namespace detail {

class Stack
{
  public:
    explicit Stack() = default;
    Stack(Stack const&) = default;
    Stack& operator=(Stack const&) = default;

  public:
    using LValue = type::semantic::LValue;
    using RValue = type::semantic::RValue;
    using Offset = detail::Stack_Offset;
    using Entry = std::pair<Offset, detail::Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

  public:
    constexpr void make(LValue const& lvalue);
    constexpr Entry get(LValue const& lvalue);
    constexpr Entry get(Offset offset);
    constexpr inline void clear() { stack_address.clear(); }

  public:
    constexpr inline bool empty_at(LValue const& lvalue)
    {
        return stack_address[lvalue].second == detail::Operand_Size::Empty;
    }
    constexpr inline bool contains(LValue const& lvalue)
    {
        return stack_address.contains(lvalue);
    }
    constexpr inline bool is_allocated(LValue const& lvalue)
    {
        return stack_address.contains(lvalue) and not empty_at(lvalue);
    }

  public:
    constexpr inline std::string get_lvalue_from_offset(Offset offset) const;
    constexpr detail::Operand_Size get_operand_size_from_offset(
        Offset offset) const;

    constexpr Offset allocate(Operand_Size operand);
    constexpr void set_address_from_accumulator(
        LValue const& lvalue,
        Register acc);
    constexpr void set_address_from_address(LValue const& lvalue);
    constexpr void set_address_from_stack(LValue const& lhs, LValue const& rhs);
    constexpr void set_address_from_immediate(
        LValue const& lvalue,
        Immediate const& rvalue);

  private:
    Offset size{ 0 };
    Local stack_address{};
};

constexpr std::string emit_immediate_storage(Immediate const& immediate);
constexpr std::string emit_stack_storage(
    Stack const& stack,
    Stack::Offset offset,
    bool with_size = true);
std::string emit_register_storage(Register device, bool with_size = false);

namespace flag {

enum Instruction_Flag : unsigned int
{
    None = 0,
    Address = 1 << 0,
    Indirect = 1 << 1
};

} // namespace flag

} // namespace detail

class Code_Generator final : public target::Backend<detail::Storage>
{
  public:
    Code_Generator() = delete;
    explicit Code_Generator(ir::Table::Table_PTR table)
        : Backend(std::move(table))
    {
    }

  private:
    using Register = detail::Register;
    using Mnemonic = detail::Mnemonic;

  private:
    using Operand_Size = detail::Operand_Size;
    using Operator_Symbol = std::string;
    using Instruction_Pair = detail::Instruction_Pair;
    using Storage = detail::Storage;
    using Immediate = detail::Immediate;
    using Instructions = detail::Instructions;
    using Storage_Operands = std::pair<Storage, Storage>;

  public:
    void emit(std::ostream& os) override;

  public:
    const Storage EMPTY_STORAGE = std::monostate{};

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_instructions();
    void from_func_start_ita(type::semantic::Label const& name) override;
    void from_func_end_ita() override;
    void from_locl_ita(IR_Instruction const& inst) override;
    void from_cmp_ita(IR_Instruction const& inst) override;
    void from_mov_ita(IR_Instruction const& inst) override;
    void from_return_ita(Storage const& dest) override;
    void from_leave_ita() override;
    void from_label_ita(IR_Instruction const& inst) override;
    void from_push_ita(IR_Instruction const& inst) override;
    // void from_goto_ita() override;
    // void from_globl_ita() override;
    // void from_if_ita() override;
    // void from_jmp_e_ita() override;
    // void from_pop_ita() override;
    // void from_call_ita() override;

  private:
    std::deque<Register> available_qword_register = {
        Register::rdi, Register::r8,  Register::r9,
        Register::rsi, Register::rdx, Register::rcx
    };
    std::deque<Register> available_dword_register = {
        Register::edi, Register::r8d, Register::r9d,
        Register::esi, Register::edx, Register::ecx
    };

  private:
    detail::Stack stack{};
    constexpr Register get_accumulator_register_from_size(
        Operand_Size size = Operand_Size::Dword);
    constexpr Register get_second_register_from_size(
        Operand_Size size = Operand_Size::Dword);
    constexpr Register get_accumulator_register_from_storage(
       Storage const& storage);
    Storage get_storage_for_binary_operator(
        type::semantic::RValue const& rvalue);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    std::string emit_storage_device(
        Storage const& storage,
        bool with_size);
    Instruction_Pair from_ita_expression(type::semantic::RValue const& expr);
    void from_ita_unary_expression(
        std::string const& op,
        Storage const& dest,
        Storage const& src = detail::O_NUL);
    Instruction_Pair from_bitwise_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op);
    Instruction_Pair from_arithmetic_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op);
    Instruction_Pair from_relational_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void insert_from_temporary_lvalue(type::semantic::LValue const& lvalue);
    void insert_from_op_operands(
        Storage_Operands& operands,
        std::string const& op);
    void from_binary_operator_expression(
        type::semantic::RValue const& expr);
    void from_bitwise_operator_expression(
        type::semantic::RValue const& expr);
    void from_temporary_unary_operator_expression(
        type::semantic::RValue const& expr);
    void insert_from_immediate_rvalues(
        Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);
    Immediate get_from_immediate_rvalues(
        Storage& lhs,
        std::string const& op,
        Storage& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void set_table_stack_frame(type::semantic::Label const& name);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Storage get_storage_device(
        detail::Operand_Size size = detail::Operand_Size::Qword);
    inline void reset_avail_registers()
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

  private:
    using Operand_Lambda = std::function<bool(type::semantic::RValue)>;
    Operand_Lambda is_immediate = [&](type::semantic::RValue const& rvalue) {
        return type::is_rvalue_data_type(rvalue);
    };
    Operand_Lambda is_address = [&](type::semantic::RValue const& rvalue) {
        return stack.is_allocated(rvalue);
    };
    Operand_Lambda is_temporary = [&](type::semantic::RValue const& rvalue) {
        return type::is_temporary(rvalue);
    };

  private:
    inline bool is_ir_instruction_temporary()
    {
        return type::is_temporary(std::get<1>(table_->instructions[ita_index]));
    }
    std::string get_ir_instruction_lvalue()
    {
        return std::get<1>(table_->instructions[ita_index]);
    }
    inline bool last_ir_instruction_is_assignment()
    {
        if (ita_index < 1)
            return false;
        auto last = table_->instructions[ita_index - 1];
        return std::get<0>(last) == ir::Instruction::MOV and
               not type::is_temporary(std::get<1>(last));
    }
    inline bool next_ir_instruction_is_temporary()
    {
        if (table_->instructions.size() < ita_index + 1)
            return false;
        auto next = table_->instructions[ita_index + 1];
        return std::get<0>(next) == ir::Instruction::MOV and
               type::is_temporary(std::get<1>(next));
    }

  private:
    std::string current_frame{};
    std::size_t ita_index{ 0 };
    std::size_t constant_index{ 0 };

    Register special_register{ Register::eax };

  private:
    std::deque<Immediate> immediate_stack{};
    bool address_ir_assignment{ false };

  private:
    void set_instruction_flag(detail::flag::Instruction_Flag set_flag);
    Ordered_Map<unsigned int, unsigned int> instruction_flag{};
    Instructions instructions_{};
    Instructions data_{};
};

void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast);

} // namespace x86_64
