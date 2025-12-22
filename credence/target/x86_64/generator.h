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

#include "assembly.h"               // for Instructions, Operand_Size, Storage
#include "credence/ir/object.h"     // for Function, Object
#include "memory.h"                 // for Memory_Access, RValue, LValue
#include "stack.h"                  // for Stack
#include "syscall.h"                // for syscall_arguments_t
#include <credence/ir/ita.h>        // for Quadruple, Instructions
#include <credence/target/target.h> // for IR_Visitor
#include <credence/util.h>          // for AST_Node, CREDENCE_PRIVATE_UNLES...
#include <cstddef>                  // for size_t
#include <ostream>                  // for ostream
#include <string>                   // for basic_string, string
#include <string_view>              // for string_view
#include <utility>                  // for move

namespace credence::target::x86_64 {

using Instruction_Pair = assembly::Instruction_Pair;

void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast);

constexpr std::string emit_immediate_storage(
    assembly::Immediate const& immediate);

constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    assembly::Operand_Size size,
    flag::flags flags);

constexpr std::string emit_register_storage(assembly::Register device,
    assembly::Operand_Size size,
    flag::flags flags);

void emit_x86_64_assembly_intel_prologue(std::ostream& os);

assembly::Operand_Size get_operand_size_from_storage(
    assembly::Storage const& storage,
    memory::Stack_Pointer& stack);

class Assembly_Emitter;

/**
 * @brief Storage Emitter for destination and source storage devices
 */
class Storage_Emitter
{
  public:
    explicit Storage_Emitter(memory::Memory_Access& accessor,
        std::size_t index,
        Storage& source_storage)
        : accessor_(accessor)
        , instruction_index_(index)
        , source_storage_(source_storage)
    {
    }

    constexpr void set_address_size(Operand_Size address)
    {
        address_size = address;
    }

    constexpr void reset_address_size() { address_size = Operand_Size::Empty; }

    std::string get_storage_device_as_string(assembly::Storage const& storage,
        Operand_Size size);

    void emit(std::ostream& os,
        assembly::Storage const& storage,
        assembly::Mnemonic mnemonic,
        memory::Operand_Type type_);

  private:
    memory::Memory_Access accessor_;
    std::size_t instruction_index_;

  private:
    Operand_Size address_size = Operand_Size::Empty;
    Storage& source_storage_;
};

/**
 * @brief Text Emitter for the text section in a x86_64 application
 */
class Text_Emitter
{
  public:
    explicit Text_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
        instructions_ = accessor->instruction_accessor;
    }
    friend class Assembly_Emitter;

    void emit_stdlib_externs(std::ostream& os);
    void emit_text_directives(std::ostream& os);
    void emit_text_section(std::ostream& os);

  private:
    void emit_assembly_instruction(std::ostream& os,
        std::size_t index,
        assembly::Instruction const& s);
    void emit_assembly_label(std::ostream& os,
        Label const& s,
        bool set_label = true);
    void emit_text_instruction(std::ostream& os,
        std::variant<Label, assembly::Instruction> const& instruction,
        std::size_t index,
        bool set_label = true);
    void emit_function_epilogue(std::ostream& os);
    void emit_epilogue_jump(std::ostream& os);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    bool test_no_stdlib{ false };
    // clang-format on

  private:
    memory::Memory_Access accessor_;

  private:
    memory::Instruction_Pointer instructions_;
    assembly::Instructions return_instructions_;
    std::size_t label_size_{ 0 };
    Label frame_{};
    Label branch_{};
};

/**
 * @brief Data Emitter for the data section in a x86_64 application
 */

class Data_Emitter
{
  public:
    explicit Data_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    friend class Assembly_Emitter;

  public:
    void emit_data_section(std::ostream& os);

  private:
    void set_data_globals();
    void set_data_strings();
    void set_data_floats();
    void set_data_doubles();

  private:
    assembly::Directives get_instructions_from_directive_type(
        assembly::Directive directive,
        RValue const& rvalue);

  private:
    memory::Memory_Access accessor_;

  private:
    assembly::Directives instructions_;
};

/**
 * @brief Instruction Inserter used to map IR instructions to x64 assembly
 */
class Instruction_Inserter
{
  public:
    explicit Instruction_Inserter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    void insert(ir::Instructions const& ir_instructions);

  private:
    memory::Memory_Access accessor_;
};

/**
 * @brief Operand Inserter to translate operand types into x64 instructions
 */
class Operand_Inserter
{
  public:
    explicit Operand_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    Storage get_operand_storage_from_rvalue(RValue const& rvalue);
    void insert_from_immediate_rvalues(Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);
    void insert_from_operands(Storage_Operands& operands,
        std::string const& op);
    void insert_from_mnemonic_operand(LValue const& lhs, RValue const& rhs);

  private:
    void insert_from_string_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs);
    void insert_from_float_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs);
    void insert_from_double_address_operand(LValue const& lhs,
        Storage const& storage,
        RValue const& rhs);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Expression Inserter to translate rvalue types into x64 instructions
 */
struct Expression_Inserter
{
    explicit Expression_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    void insert_from_string(RValue const& str);
    void insert_from_float(RValue const& str);
    void insert_from_double(RValue const& str);
    Instruction_Pair insert_from_expression(RValue const& expr);
    void insert_from_global_vector_assignment(LValue const& lhs,
        LValue const& rhs);
    void insert_lvalue_at_temporary_object_address(LValue const& lvalue);
    void insert_from_rvalue(RValue const& rvalue);
    void insert_from_return_rvalue(
        ir::object::Function::Return_RValue const& ret);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Binary Operator Inserter to translate binary expressions
 */
struct Binary_Operator_Inserter
{
    explicit Binary_Operator_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    void from_binary_operator_expression(RValue const& rvalue);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Unary Operator to translate unary operator expressions
 */
struct Unary_Operator_Inserter
{
    explicit Unary_Operator_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    void insert_from_unary_expression(std::string const& op,
        Storage const& dest,
        Storage const& src = assembly::O_NUL);
    void insert_from_unary_to_unary_assignment(LValue const& lhs,
        LValue const& rhs);
    void from_temporary_unary_operator_expression(RValue const& expr);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Relational Operator Inserter to translate relational operands
 */
struct Relational_Operator_Inserter
{
    explicit Relational_Operator_Inserter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    assembly::Instructions from_relational_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op,
        Label const& jump_label);

  private:
    memory::Memory_Access accessor_;
};

/**
 * @brief Arithemtic Operator Inserter to translate arithemtic expressions
 */
struct Arithemtic_Operator_Inserter
{
    explicit Arithemtic_Operator_Inserter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    Instruction_Pair from_arithmetic_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op);

  private:
    memory::Memory_Access accessor_;
};

/**
 * @brief Bitwise Operator Inserter to translate bitwise expressions
 */
struct Bitwise_Operator_Inserter
{
    explicit Bitwise_Operator_Inserter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    Instruction_Pair from_bitwise_expression_operands(
        Storage_Operands const& operands,
        std::string const& binary_op);
    void from_bitwise_operator_expression(RValue const& expr);

  private:
    memory::Memory_Access accessor_;
};

/**
 * @brief Invocation Inserter to translate function invocation and arguments
 */
struct Invocation_Inserter
{
    explicit Invocation_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    syscall_ns::syscall_arguments_t get_operands_storage_from_argument_stack();
    void insert_from_standard_library_function(std::string_view routine,
        assembly::Instructions& instructions);
    void insert_from_user_defined_function(std::string_view routine,
        assembly::Instructions& instructions);
    void insert_from_syscall_function(std::string_view routine,
        assembly::Instructions& instructions);
    void insert_type_check_stdlib_print_arguments(
        memory::Stack_Frame::IR_Stack const& argument_stack,
        syscall_ns::syscall_arguments_t& operands);
    void insert_type_check_stdlib_printf_arguments(
        memory::Stack_Frame::IR_Stack const& argument_stack,
        syscall_ns::syscall_arguments_t& operands);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Assembly Emitter that emits the data and text section of an x64
 * application
 */
class Assembly_Emitter
{
  public:
    explicit Assembly_Emitter(memory::Memory_Access accessor)
        : accessor_(std::move(accessor))
    {
        ir_instructions_ = *accessor_->table_accessor.table_->ir_instructions;
    }

  public:
    void emit(std::ostream& os);

  private:
    memory::Memory_Access accessor_;

  private:
    ir::Instructions ir_instructions_;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Emitter data_{ accessor_ };
    Text_Emitter text_{ accessor_ };
    // clang-format on
};

/**
 * @brief IR Visitor for the x86-64 architecture and ISA
 *
 * The Storage_Container is defined in assembly.h, and
 * each intermediate instruction is a quadruple defined in ita.h.
 *
 * Macros and helpers to compose mnemonics, registers, and immediate
 * values instructions are defined in assembly.h.
 *
 */

class IR_Instruction_Visitor final
    : public target::IR_Visitor<ir::Quadruple, x86_64::assembly::Instructions>
{
  public:
    explicit IR_Instruction_Visitor(memory::Memory_Access& accessor,
        memory::Stack_Frame stack_frame)
        : accessor_(accessor)
        , stack_frame_(std::move(stack_frame))
    {
    }

    using Instructions = x86_64::assembly::Instructions;
    void set_stack_frame_from_table(Label const& function_name);

  public:
    constexpr void set_iterator_index(std::size_t index)
    {
        iterator_index_ = index;
    }

  public:
    void from_locl_ita(ir::Quadruple const& inst) override;
    void from_pop_ita() override;
    void from_push_ita(ir::Quadruple const& inst) override;
    void from_func_start_ita(Label const& name) override;
    void from_func_end_ita() override;
    void from_cmp_ita(ir::Quadruple const& inst) override;
    void from_mov_ita(ir::Quadruple const& inst) override;
    void from_return_ita() override;
    void from_leave_ita() override;
    void from_label_ita(ir::Quadruple const& inst) override;
    void from_call_ita(ir::Quadruple const& inst) override;
    void from_if_ita(ir::Quadruple const& inst) override;
    void from_jmp_e_ita(ir::Quadruple const& inst) override;
    void from_goto_ita(ir::Quadruple const& inst) override;

  private:
    std::size_t iterator_index_{ 0 };

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame stack_frame_;
};

#ifdef CREDENCE_TEST
void emit(std::ostream& os,
    util::AST_Node& symbols,
    util::AST_Node const& ast,
    bool no_stdlib = true);
#endif
} // namespace x86_64
