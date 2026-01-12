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

#include "visitor.h"

#include "assembly.h"                        // for Register, Mnemonic, arm...
#include "credence/error.h"                  // for credence_assert
#include "credence/map.h"                    // for Ordered_Map
#include "credence/symbol.h"                 // for Symbol_Table
#include "credence/target/common/assembly.h" // for Arch_Type, OS_Type, ali...
#include "credence/target/common/memory.h"   // for is_parameter
#include "credence/target/common/types.h"    // for Table_Pointer
#include "credence/types.h"                  // for from_lvalue_offset, get...
#include "flags.h"                           // for set_alignment_flag
#include "inserter.h"                        // for Expression_Inserter
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "stack.h"                           // for Stack
#include "syscall.h"                         // for exit_syscall
#include <credence/ir/object.h>              // for Object, Function, Label
#include <credence/target/common/runtime.h>  // for is_stdlib_function, is_...
#include <deque>                             // for deque
#include <matchit.h>                         // for Wildcard, App, Ds, app
#include <memory>                            // for shared_ptr
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get, tuple
#include <utility>                           // for pair
#include <variant>                           // for variant

/****************************************************************************
 *
 * ARM64 IR Visitor
 *
 * Visits ITA intermediate representation instructions and emits ARM64
 * assembly. Implements the IR_Visitor interface for ARM64 ISA.
 *
 * Example - visiting assignment:
 *
 *   ITA:    x = 42;  (x is first local variable)
 *
 * Visitor generates:
 *   mov w9, #42              ; x in register w9
 *
 * Example - visiting function call:
 *
 *   ITA:    CALL add
 *
 * Visitor generates:
 *   bl add
 *
 *****************************************************************************/

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief IR Instruction Instruction::FUNC_START
 */
void IR_Instruction_Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& table = accessor_->table_accessor.table_;
    credence_assert(table->get_functions().contains(name));
    accessor_->device_accessor.reset_storage_devices();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto frame = table->get_functions()[name];
    accessor_->stack->allocate(16);
    set_alignment_flag(Align_SP);
    arm64_add__asm(instructions, stp, x29, x30, alignment__integer());
    arm64_add__asm(instructions, mov, x29, sp);
    set_alignment_flag(Callee_Saved);
    arm64_add__asm(instructions, stp, x26, x23, alignment__sp_integer(16));
}

/**
 * @brief Reset available general purpose registers at end of frame
 */
void IR_Instruction_Visitor::from_func_end_ita()
{
    accessor_->register_accessor.reset_available_registers();
}

// Unused
void IR_Instruction_Visitor::from_cmp_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief IR Instruction Instruction::MOV
 */
void IR_Instruction_Visitor::from_mov_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->get_vectors().contains(rvalue_reference) and
               table->get_globals().is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
            },
        m::pattern | m::ds(m::app(type::is_dereference_expression, true),
                         m::app(type::is_dereference_expression, true)) =
            [&] {
                auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
                unary_inserter.insert_from_unary_to_unary_assignment(lhs, rhs);
            },
        m::pattern | m::ds(m::app(type::is_dereference_expression, true),
                         m::app(type::is_dereference_expression, false)) =
            [&] {
                auto lhs_s = accessor_->device_accessor.get_device_by_lvalue(
                    type::get_unary_rvalue_reference(lhs));
                auto rhs_s =
                    accessor_->device_accessor.get_operand_rvalue_device(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    "*", rhs_s, lhs_s);
            },
        m::pattern | m::ds(m::app(is_global_vector, true), m::_) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::ds(m::_, m::app(is_global_vector, true)) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });

    if (accessor_->stack->is_allocated(lhs))
        set_pointer_address_of_lvalue(lhs);

    if (!type::is_temporary(lhs) and
        not accessor_->register_accessor.stack.empty())
        accessor_->register_accessor.stack.clear();
}

/**
 * @brief Update the address in the stack that holds the pointer of an lvalue
 */
void IR_Instruction_Visitor::set_pointer_address_of_lvalue(LValue const& lvalue)
{
    auto device_storage =
        accessor_->device_accessor.get_device_by_lvalue(lvalue);
    auto address_storage = accessor_->stack->get(lvalue).first;
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::Indirect_Source, instructions.size());
    arm64_add__asm(instructions, str, device_storage, address_storage);
}

/**
 * @brief IR Instruction Instruction::PUSH
 */
void IR_Instruction_Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(std::get<1>(inst), frame));
}

/**
 * @brief IR Instruction Instruction::RETURN
 */
void IR_Instruction_Visitor::from_return_ita()
{
    auto inserter = Expression_Inserter{ accessor_ };
    auto frame =
        accessor_->table_accessor.table_->get_functions()[stack_frame_.symbol];
    if (frame->get_ret().has_value())
        inserter.insert_from_return_rvalue(frame->get_ret());
}

/**
 * @brief IR Instruction Instruction::LEAVE
 */
void IR_Instruction_Visitor::from_leave_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();

    set_alignment_flag(Callee_Saved);
    arm64_add__asm(instructions, ldp, x26, x23, alignment__sp_integer(16));

    auto sp_imm = direct_immediate("[sp]");
    set_alignment_flag(Align_S3_Folded);
    arm64_add__asm(instructions, ldp, x29, x30, sp_imm, alignment__integer());

    if (stack_frame_.symbol == "main")
        syscall_ns::exit_syscall(instructions, 0);
    else
        arm64_add__asm(instructions, ret);
}

/**
 * @brief IR Instruction Instruction::LABEL
 */
void IR_Instruction_Visitor::from_label_ita(ir::Quadruple const& inst)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    instructions.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

/**
 * @brief Clean up argument stack after function call
 */
void IR_Instruction_Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    if (!stack_frame_.call_stack.empty())
        stack_frame_.call_stack.pop_back();
}

/**
 * @brief IR Instruction Instruction::CALL
 */
void IR_Instruction_Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_ };
    auto& instructions = instruction_accessor->get_instructions();
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));

    auto is_syscall_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);

#endif
    };
    auto is_stdlib_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);

#endif
    };
    accessor_->device_accessor.save_and_allocate_before_instruction_jump(
        instructions);
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instructions);
            },
        m::pattern | m::app(is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instructions);
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instructions);
            });
    stack_frame_.call_stack.emplace_back(function_name);
    stack_frame_.tail = function_name;
    accessor_->device_accessor.restore_and_deallocate_after_instruction_jump(
        instructions);
}

/**
 * @brief IR Instruction Instruction::GOTO
 */
void IR_Instruction_Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = common::assembly::make_direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    arm64_add__asm(instructions, b, label);
}

/**
 * @brief IR Instruction Instruction::LOCL
 */
void IR_Instruction_Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto& table = accessor_->table_accessor.table_;
    auto& stack = accessor_->stack;
    auto is_vector = [&](RValue const& rvalue) {
        return table->get_vectors().contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                accessor_->device_accessor.insert_lvalue_to_device(
                    lvalue, Operand_Size::Doubleword);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto vector = table->get_vectors().at(locl_lvalue);
                auto size = stack->get_stack_size_from_table_vector(*vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        m::pattern | m::_ =
            [&] {
                accessor_->device_accessor.insert_lvalue_to_device(locl_lvalue);
            }

    );
}

/**
 * @brief IR Instruction Instruction::JMP_E
 */
void IR_Instruction_Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame->get_temporary().at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage = accessor_->address_accessor
                                 .get_arm64_lvalue_and_insertion_instructions(
                                     of_comparator, accessor_->device_accessor)
                                 .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::w8);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief Unused - read-ahead during relational jumps
 */
void IR_Instruction_Visitor::from_if_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

}