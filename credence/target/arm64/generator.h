/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include "assembly.h"                           // for Instructions, Binary...
#include "memory.h"                             // for Memory_Access, Stack...
#include "stack.h"                              // for Stack
#include "syscall.h"                            // for syscall_arguments_t
#include <credence/ir/ita.h>                    // for Quadruple, Instructions
#include <credence/ir/object.h>                 // for RValue, LValue, Label
#include <credence/target/common/flags.h>       // for flags
#include <credence/target/common/stack_frame.h> // for Locals
#include <credence/target/common/visitor.h>     // for IR_Visitor
#include <credence/types.h>                     // for Size
#include <credence/util.h>                      // for AST_Node, CREDENCE_P...
#include <cstddef>                              // for size_t
#include <ostream>                              // for ostream
#include <string>                               // for basic_string, string
#include <string_view>                          // for string_view
#include <utility>                              // for move
#include <variant>                              // for variant
namespace credence {
namespace target {
namespace arm64 {
class Visitor;
}
}
} // lines 51-51

namespace credence::target::arm64 {

namespace flag = common::flag;

using Instruction_Pair = assembly::Instruction_Pair;

void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast);

constexpr std::string emit_immediate_storage(
    assembly::Immediate const& immediate);

constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    common::flag::flags flags);

constexpr std::string emit_register_storage(assembly::Register device,
    common::flag::flags flags);

void emit_arm64_assembly_prologue(std::ostream& os);

class Assembly_Emitter;
class Visitor;

/**
 * @brief Storage Emitter for destination and source storage devices
 */
class Storage_Emitter
{
  public:
    explicit Storage_Emitter(memory::Memory_Access& accessor, std::size_t index)
        : accessor_(accessor)
        , instruction_index_(index)
    {
    }

    enum class Source
    {
        s_0,
        s_1,
        s_2,
        s_3,
    };

    constexpr bool is_alignment_mnemonic(Mnemonic mnemonic)
    {
        return mnemonic == arm_mn(sub) or mnemonic == arm_mn(add) or
               mnemonic == arm_mn(stp) or mnemonic == arm_mn(ldp) or
               mnemonic == arm_mn(ldr) or mnemonic == arm_mn(str);
    }

    std::string get_storage_device_as_string(assembly::Storage const& storage);

    void emit(std::ostream& os,
        assembly::Storage const& storage,
        assembly::Mnemonic mnemonic,
        Source source);

  private:
    void apply_stack_alignment(assembly::Storage& operand,
        assembly::Mnemonic mnemonic,
        Source source,
        common::flag::flags flags);
    void emit_mnemonic_operand(std::ostream& os,
        assembly::Storage const& operand,
        assembly::Mnemonic mnemonic,
        Source source,
        common::flag::flags flags);

  private:
    memory::Memory_Access accessor_;
    std::size_t instruction_index_;
};

/**
 * @brief Text Emitter for the text section in a arm64 application
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

    void emit_callee_saved_registers_stp(std::size_t index);

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
    int callee_saved{ 0 };
    Label frame_{};
    Label branch_{};
};

/**
 * @brief Data Emitter for the data section in a arm64 application
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

    inline void set_data_section()
    {
        set_data_globals();
        set_data_strings();
        set_data_floats();
        set_data_doubles();
    }

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
 * @brief Instruction Inserter used to map IR instructions to arm64 assembly
 */
class IR_Inserter
{
  public:
    explicit IR_Inserter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    friend class Visitor;
    void insert(ir::Instructions const& ir_instructions,
        memory::Stack_Frame&& initial_frame);
    void setup_stack_frame_in_function(ir::Instructions const& ir_instructions,
        Visitor& visitor,
        int index);

  private:
    memory::Memory_Access accessor_;
};

/**
 * @brief Operand Inserter to translate operand types into arm64 instructions
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

  public:
    Storage get_operand_storage_from_rvalue(RValue const& rvalue);

  public:
    void insert_from_immediate_rvalues(Immediate const& lhs,
        std::string const& op,
        Immediate const& rhs);
    void insert_from_operands(assembly::Binary_Operands& operands,
        std::string const& op);
    void insert_from_mnemonic_operand(LValue const& lhs, RValue const& rhs);

  private:
    Storage get_operand_storage_from_parameter(RValue const& rvalue);
    Storage get_operand_storage_from_stack(RValue const& rvalue);
    Storage get_operand_storage_from_return();
    Storage get_operand_storage_from_immediate(RValue const& rvalue);

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
 * @brief Expression Inserter to translate rvalue types into arm64 instructions
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
    void insert_from_mnemonic_operand(const Mnemonic& mnemonic,
        const Storage& dest,
        const Storage& source,
        type::semantic::Size size);

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
    using Size = assembly::Operand_Size;

    Size get_operand_size_from_lvalue_reference(LValue const& lvalue);
    void insert_from_unary_expression(std::string const& op,
        Storage const& dest,
        Storage const& src = assembly::O_NUL);

    Storage from_temporary_unary_operator_expression(RValue const& expr);

  private:
    Storage get_temporary_storage_from_temporary_expansion(
        RValue const& rvalue);
    void from_lvalue_address_of_expression(RValue const& expr);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Relational Operator Inserter to translate relational operands
 */
struct Relational_Operator_Inserter
{
    explicit Relational_Operator_Inserter(memory::Memory_Access accessor,
        memory::Stack_Frame& stack_frame)
        : accessor_(accessor)
        , stack_frame_(stack_frame)
    {
    }
    assembly::Instructions from_relational_expression_operands(
        assembly::Binary_Operands const& operands,
        std::string const& binary_op,
        Label const& jump_label);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
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
        assembly::Binary_Operands const& operands,
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
        assembly::Binary_Operands const& operands,
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
        common::memory::Locals const& argument_stack,
        syscall_ns::syscall_arguments_t& operands);
    void insert_type_check_stdlib_printf_arguments(
        common::memory::Locals const& argument_stack,
        syscall_ns::syscall_arguments_t& operands);

  private:
    memory::Memory_Access accessor_;
    memory::Stack_Frame& stack_frame_;
};

/**
 * @brief Assembly Emitter that emits the data and text section of an arm64
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
 * @brief IR Visitor for the arm64 architecture and ISA
 *
 * The Storage_Container is defined in assembly.h, and
 * each intermediate instruction is a quadruple defined in ita.h.
 *
 * Macros and helpers to compose mnemonics, registers, and immediate
 * values instructions are defined in assembly.h.
 *
 */
class Visitor final
    : public target::common::IR_Visitor<ir::Quadruple,
          arm64::assembly::Instructions>
{
  public:
    explicit Visitor(memory::Memory_Access& accessor,
        memory::Stack_Frame stack_frame)
        : accessor_(accessor)
        , stack_frame_(std::move(stack_frame))
    {
    }

    using Instructions = arm64::assembly::Instructions;
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
} // namespace credence::target::arm64
