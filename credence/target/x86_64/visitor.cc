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

#include "assembly.h"                        // for Register, Mnemonic, x86...
#include "credence/error.h"                  // for credence_assert
#include "credence/map.h"                    // for Ordered_Map
#include "credence/symbol.h"                 // for Symbol_Table
#include "credence/target/common/assembly.h" // for u32_int_immediate, Arch...
#include "credence/target/common/flags.h"    // for Instruction_Flag
#include "credence/target/common/memory.h"   // for is_parameter
#include "credence/target/common/types.h"    // for Table_Pointer
#include "credence/types.h"                  // for from_lvalue_offset, get...
#include "inserter.h"                        // for Expression_Inserter
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "stack.h"                           // for Stack
#include "syscall.h"
#include <credence/ir/checker.h>            // for Type_Checker
#include <credence/ir/object.h>             // for Object, Function, Label
#include <credence/target/common/runtime.h> // for is_stdlib_function, is_...
#include <deque>                            // for deque
#include <matchit.h>                        // for App, Wildcard, Ds, app
#include <memory>                           // for shared_ptr
#include <string>                           // for basic_string, char_traits
#include <string_view>                      // for basic_string_view
#include <tuple>                            // for get, tuple
#include <utility>                          // for pair
#include <variant>                          // for variant

/****************************************************************************
 *
 * x86-64 IR Visitor
 *
 * Visits ITA intermediate representation instructions and emits x86_64
 * assembly. Implements the IR_Visitor interface for x86_64 ISA.
 *
 * Example - visiting assignment:
 *
 *   ITA:    x = 42;
 *
 * Visitor generates:
 *   mov dword ptr [rbp - 4], 42
 *
 * Example - visiting function call:
 *
 *   ITA:    CALL add
 *
 * Visitor generates:
 *   call add
 *
 *****************************************************************************/

namespace credence::target::x86_64 {

namespace m = matchit;

/**
 * @brief IR Instruction Instruction::FUNC_START
 */
void IR_Instruction_Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    credence_assert(table->get_functions().contains(name));
    accessor_->stack->clear();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    auto frame = table->get_functions()[name];
    auto& inst = instruction_accessor->get_instructions();
    // function prologue
    x8664_add__asm(inst, push, rbp);
    x8664_add__asm(inst, mov_, rbp, rsp);
    // align %rbp if there's a CALL in this stack frame
    if (table->stack_frame_contains_call_instruction(
            name, *accessor_->table_accessor.table_->get_ir_instructions())) {
        auto imm = u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Align, instruction_accessor->size());
        x8664_add__asm(inst, sub, rsp, imm);
    }
}

/**
 * @brief IR Instruction IR Instruction::FUNC_END
 */
void IR_Instruction_Visitor::from_func_end_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    if (table->stack_frame_contains_call_instruction(frame->get_symbol(),
            *accessor_->table_accessor.table_->get_ir_instructions())) {
        auto imm = u32_int_immediate(0);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Align, instruction_accessor->size());
    }
    accessor_->register_accessor.reset_available_registers();
}

/**
 * @brief IR Instruction Instruction::LOCL
 *
 *   Note that at this point we allocate local variables on the stack
 */
void IR_Instruction_Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto& stack = accessor_->stack;
    auto& locals = accessor_->table_accessor.table_->get_stack_frame_symbols();

    auto type_checker =
        ir::Type_Checker{ accessor_->table_accessor.table_, frame };

    // The storage of an immediate (and, really, all) relational
    // expressions will be the `al` register, 1 for true, 0 for false
    auto is_immediate_relational_expression = [&](RValue const& rvalue) {
        return type::is_relation_binary_expression(
            type::get_value_from_rvalue_data_type(
                locals.get_symbol_by_name(rvalue)));
    };
    auto is_vector = [&](RValue const& rvalue) {
        return table->get_vectors().contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        // Allocate a pointer on the stack
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                stack->set_address_from_address(lvalue);
            },
        // Allocate a vector (array), including all of its elements on
        // the stack
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto vector = table->get_vectors().at(locl_lvalue);
                auto size = stack->get_stack_size_from_table_vector(*vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        // Allocate 1 byte on the stack for the al register
        m::pattern | m::app(is_immediate_relational_expression, true) =
            [&] {
                stack->set_address_from_accumulator(locl_lvalue, Register::al);
            },
        // Allocate on the stack from the size of the lvalue type
        m::pattern | m::_ =
            [&] {
                auto type =
                    type_checker.get_type_from_rvalue_data_type(locl_lvalue);
                stack->set_address_from_type(locl_lvalue, type);
            }

    );
}

/**
 * @brief IR Instruction Instruction::PUSH
 */
void IR_Instruction_Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.table_;
    auto frame = stack_frame_.get_stack_frame();
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(std::get<1>(inst), frame));
}

/**
 * @brief IR Instruction Instruction::POP
 */
void IR_Instruction_Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    stack_frame_.call_stack.pop_back();
}

/**
 * @brief IR Instruction Instruction::CALL
 */
void IR_Instruction_Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_ };
    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));
    auto is_syscall_function = [&](Label const& label) {
#if defined(CREDENCE_TEST) || defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);
#endif
    };
    auto is_stdlib_function = [&](Label const& label) {
#if defined(CREDENCE_TEST) || defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);

#endif
    };
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::app(is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instruction_accessor->get_instructions());
            });
    stack_frame_.call_stack.emplace_back(function_name);
    stack_frame_.tail = function_name;
    accessor_->register_accessor.reset_available_registers();
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
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->get_vectors().contains(rvalue_reference) and
               table->get_globals().is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        // Parameters are prepared in the table, so skip parameter
        // lvalues
        m::pattern | m::ds(m::app(is_parameter, true), m::_) = [] {},
        // Translate an rvalue from a mutual-recursive temporary lvalue
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
            },
        // Translate a unary-to-unary rvalue reference
        m::pattern | m::ds(m::app(type::is_unary_expression, true),
                         m::app(type::is_unary_expression, true)) =
            [&] {
                unary_inserter.insert_from_unary_to_unary_assignment(lhs, rhs);
            },
        // Translate from a vector in global scope
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
        // Direct operand to mnemonic translation
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });
}

// Unused, used as rvalues
void IR_Instruction_Visitor::from_cmp_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}
// Unused, read-ahead during relational jumps
void IR_Instruction_Visitor::from_if_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief IR Instruction Instruction::JNP_E
 */
void IR_Instruction_Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame->get_temporary().at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage = accessor_->address_accessor
                                 .get_lvalue_address_and_insertion_instructions(
                                     of_comparator, instructions.size())
                                 .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::eax);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief IR Instruction Instruction::GOTO
 */
void IR_Instruction_Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    x8664_add__asm(instructions, goto_, label);
}

/**
 * @brief IR Instruction Instruction::RET
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
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    // care must be taken in the main function during function epilogue
    if (stack_frame_.symbol == "main") {
        if (accessor_->table_accessor.table_
                ->stack_frame_contains_call_instruction(stack_frame_.symbol,
                    *accessor_->table_accessor.table_->get_ir_instructions())) {
            auto size = u32_int_immediate(
                accessor_->stack->get_stack_frame_allocation_size());
            x8664_add__asm(accessor_->instruction_accessor->get_instructions(),
                add,
                rsp,
                size);
        }
        syscall_ns::common::exit_syscall(instructions, 0);
    } else {
        x8664_add__asm(instructions, pop, rbp);
        x8664_add__asm(instructions, ret);
    }
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

}