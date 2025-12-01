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
#include <credence/ir/ita.h>        // for Instruction
#include <credence/ir/table.h>      // for Table
#include <credence/map.h>           // for Ordered_Map
#include <credence/target/target.h> // for Backend
#include <credence/types.h>         // for RValue, is_temporary, LValue
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>                  // for size_t
#include <deque>                    // for deque
#include <functional>               // for function
#include <map>                      // for map
#include <ostream>                  // for basic_ostream, endl
#include <ostream>                  // for ostream
#include <string>                   // for basic_string, string, char_traits
#include <tuple>                    // for get, tuple
#include <utility>                  // for pair, move
#include <variant>                  // for monostate

namespace credence::target::x86_64 {

namespace detail {

/**
 * @brief
 * Encapsulation of a push-down stack for the x86-64 architecture
 *
 * Provides a means to allocate, traverse, and verify offsets
 * on the stack by lvalues and vice-versa.
 *
 * Each instance should encompass a single stack frame in a function
 *
 */
class Stack
{
  public:
    explicit Stack() = default;
    Stack(Stack const&) = delete;
    Stack& operator=(Stack const&) = delete;

  public:
    using Type = type::semantic::Type;
    using Size = type::semantic::Size;
    using LValue = type::semantic::LValue;
    using RValue = type::semantic::RValue;
    using Offset = detail::Stack_Offset;
    using Entry = std::pair<Offset, detail::Operand_Size>;
    using Pair = std::pair<LValue, Entry>;
    using Local = Ordered_Map<LValue, Entry>;

  public:
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
    constexpr std::string get_lvalue_from_offset(Offset offset) const;
    constexpr Size get_stack_size_from_table_vector(
        ir::detail::Vector const& vector);
    constexpr Size get_stack_offset_from_table_vector_index(
        LValue const& lvalue,
        std::string const& key,
        ir::detail::Vector const& vector);
    constexpr detail::Operand_Size get_operand_size_from_offset(
        Offset offset) const;

    constexpr Offset allocate(Operand_Size operand);
    constexpr void allocate_aligned_lvalue(
        LValue const& lvalue,
        Size value_size,
        Operand_Size operand_size);
    constexpr void set_address_from_accumulator(
        LValue const& lvalue,
        Register acc);
    constexpr void set_address_from_address(LValue const& lvalue);
    constexpr void set_address_from_type(LValue const& lvalue, Type type);
    constexpr void set_address_from_size(
        LValue const& lvalue,
        Offset allocate,
        Operand_Size operand = Operand_Size::Dword);
    constexpr void set_address_from_immediate(
        LValue const& lvalue,
        Immediate const& rvalue);

  private:
    Offset size{ 0 };
    Local stack_address{};
};

/**
 * @brief Push a platform-dependent newline character to an ostream
 */
inline void newline(std::ostream& os, int amount = 1)
{
    for (; amount > 0; amount--)
        os << std::endl;
}

namespace flag {

using flags = unsigned int;

enum Instruction_Flag : flags
{
    None = 0,
    Address = 1 << 0,
    Indirect = 1 << 1,
    Indirect_Source = 1 << 2
};

} // namespace flag

constexpr std::string emit_immediate_storage(Immediate const& immediate);

constexpr std::string emit_stack_storage(
    Stack const& stack,
    Stack::Offset offset,
    flag::flags flags);

constexpr std::string emit_register_storage(
    Register device,
    Operand_Size size,
    flag::flags flags);

} // namespace detail

/**
 * @brief Code generator for the x86-64 architecture and ISA
 *
 * The Storage_Container is defined in instructions.h, and
 * each intermediate instruction is a quadruple defined in ita.h.
 *
 * Macros and helpers to compose mnemonics, registers, and immediate
 * values instructions are defined in instructions.h.
 *
 * Emits to an std::ostream.
 *
 */
class Code_Generator final
    : public target::Backend<detail::Storage, ir::Quadruple>
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
    using LValue = detail::Stack::LValue;
    using RValue = detail::Stack::RValue;

  private:
    using Storage_Operands = std::pair<Storage, Storage>;
    using Operand_Size = detail::Operand_Size;
    using Operator_Symbol = std::string;
    using Instruction_Pair = detail::Instruction_Pair;
    using Immediate = detail::Immediate;
    using Directives = detail::Directives;
    using Instructions = detail::Instructions;

  public:
    void emit(std::ostream& os) override;

  private:
    void emit_syntax_directive(std::ostream& os);
    void emit_text_section(std::ostream& os);
    void emit_data_section(std::ostream& os);

  private:
    void build_text_section_instructions();
    void build_data_section_instructions();

  public:
    const Storage EMPTY_STORAGE = std::monostate{};

  private:
    void from_func_start_ita(type::semantic::Label const& name) override;
    void from_func_end_ita() override;
    void from_locl_ita(ir::Quadruple const& inst) override;
    void from_cmp_ita(ir::Quadruple const& inst) override;
    void from_mov_ita(ir::Quadruple const& inst) override;
    void from_return_ita(Storage const& dest) override;
    void from_leave_ita() override;
    void from_label_ita(ir::Quadruple const& inst) override;
    void from_push_ita(ir::Quadruple const& inst) override;
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
    Storage get_storage_for_binary_operator(RValue const& rvalue);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    constexpr std::string emit_storage_device(
        Storage const& storage,
        Operand_Size size,
        detail::flag::flags flag);
    void from_ita_string(RValue const& str);
    Instruction_Pair from_ita_expression(RValue const& expr);
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
    void insert_from_unary_to_unary_assignment(
        LValue const& lhs,
        LValue const& rhs);
    void insert_from_global_vector_assignment(
        LValue const& lhs,
        LValue const& rhs);
    void insert_from_mnemonic_operand(
        LValue const& lhs,
        RValue const& rhs);
    void insert_from_temporary_lvalue(LValue const& lvalue);
    void insert_from_rvalue(RValue const& rvalue);
    void insert_from_op_operands(
        Storage_Operands& operands,
        std::string const& op);
    void from_binary_operator_expression(
        RValue const& expr);
    void from_bitwise_operator_expression(
        RValue const& expr);
    void from_temporary_unary_operator_expression(
        RValue const& expr);
    void insert_from_immediate_rvalues(
        Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void set_stack_frame_from_table(type::semantic::Label const& name);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Storage get_storage_device(
        detail::Operand_Size size = detail::Operand_Size::Qword);

    inline void reset_avail_registers()
    {
        available_qword_register = {
            Register::rdi, Register::r8,  Register::r9,
            Register::rsi, Register::rdx, Register::rcx
        };
        available_dword_register = {
            Register::edi, Register::esi, Register::r8d,
            Register::r9d, Register::edx, Register::ecx
        };
    }
    // clang-format on
  private:
    Storage get_lvalue_address(LValue const& lvalue);

  private:
    /**
     * Short-form Lambda helpers for fast pattern matching
     */
    using Operand_Lambda = std::function<bool(RValue)>;
    Operand_Lambda is_immediate = [&](RValue const& rvalue) {
        return type::is_rvalue_data_type(rvalue);
    };
    Operand_Lambda is_address = [&](RValue const& rvalue) {
        return stack.is_allocated(rvalue);
    };
    Operand_Lambda is_temporary = [&](RValue const& rvalue) {
        return type::is_temporary(rvalue);
    };
    Operand_Lambda is_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue)) and
               table->globals.is_defined(rvalue);
    };
    Operand_Lambda is_vector_offset = [&](RValue const& rvalue) {
        return util::contains(rvalue, "[") and util::contains(rvalue, "]");
    };

  private:
    Operand_Size get_operand_size_from_storage(Storage const& storage);

  private:
    /**
     * @brief Check if the current ir instruction
     * is a temporary lvalue assignment
     */
    inline bool is_ir_instruction_temporary()
    {
        return type::is_temporary(std::get<1>(table->instructions[ita_index]));
    }
    /**
     * @brief Get the lvalue of the current ir instruction
     */
    inline std::string get_ir_instruction_lvalue()
    {
        return std::get<1>(table->instructions[ita_index]);
    }
    /**
     * @brief Check if last ir instruction was Instruction::MOV
     *
     * Primarily used to determine if we need a second register for an
     * expression
     */
    inline bool last_ir_instruction_is_assignment()
    {
        if (ita_index < 1)
            return false;
        auto last = table->instructions[ita_index - 1];
        return std::get<0>(last) == ir::Instruction::MOV and
               not type::is_temporary(std::get<1>(last));
    }
    /**
     * @brief Check if next ir instruction is a temporary assignment
     *
     * Primarily used to determine if we need a second register for an
     * expression
     */
    inline bool next_ir_instruction_is_temporary()
    {
        if (table->instructions.size() < ita_index + 1)
            return false;
        auto next = table->instructions[ita_index + 1];
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
    void set_instruction_flag(detail::flag::flags flags);
    Ordered_Map<unsigned int, detail::flag::flags> instruction_flag{};
    std::map<std::string, RValue> string_storage{};
    Instructions instructions_{};
    Directives data_{};
};

void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast);

} // namespace x86_64
