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

#include "inserter.h"

#include "assembly.h"                           // for inserter, Mnemonic
#include "credence/error.h"                     // for credence_assert, thr...
#include "credence/target/arm64/visitor.h"      // for IR_Instruction_Visitor
#include "credence/target/common/accessor.h"    // for Buffer_Accessor
#include "credence/target/common/assembly.h"    // for direct_immediate
#include "credence/target/common/memory.h"      // for is_temporary, is_imm...
#include "credence/target/common/runtime.h"     // for is_stdlib_function
#include "credence/target/common/types.h"       // for Immediate, Table_Poi...
#include "credence/util.h"                      // for is_variant, sv, __so...
#include "flags.h"                              // for Align_Folded
#include "memory.h"                             // for Memory_Accessor, Ins...
#include "runtime.h"                            // for Library_Call_Inserter
#include "stack.h"                              // for Stack
#include "syscall.h"                            // for syscall_arguments_t
#include <credence/ir/object.h>                 // for RValue, Function
#include <credence/target/common/flags.h>       // for Instruction_Flag
#include <credence/target/common/stack_frame.h> // for Stack_Frame, Locals
#include <cstddef>                              // for size_t
#include <fmt/format.h>                         // for format
#include <matchit.h>                            // for App, pattern, app
#include <memory>                               // for shared_ptr
#include <tuple>                                // for get, tuple
#include <variant>                              // for variant, get, operat...

/****************************************************************************
 *
 * ARM64 Instruction Inserters
 *
 * Translates B language operations into ARM64 instruction sequences.
 * Includes arithmetic, bitwise, relational operators, and lvalue and rvalue
 * type assignments.
 *
 * Example - arithmetic operation:
 *
 *   B code:    z = x + y * 2;
 *
 * Inserter generates (locals in w9, w10, w11):
 *   mov w8, w10             ; load y from w10 into accumulator
 *   lsl w8, w8, #1          ; y * 2 (shift left)
 *   add w8, w9, w8          ; x + (y * 2), x in w9
 *   mov w11, w8             ; store to z in w11
 *
 * Example - comparison:
 *
 *   B code:    if (x > 10) { ... }
 *
 * Inserter generates (x in w9):
 *   cmp w9, #10
 *   b.gt ._L1__main
 *
 *****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x6  = intermediate scratch and data section register
 *      s6  = floating point
 *      d6  = double
 *      v6  = SIMD
 *   x15      = Second data section register
 *   x7       = multiplication scratch register
 *   x8       = The default "accumulator" register for expression expansion
 *   x10      = The stack move register; additional scratch register
 *   x9 - x18 = If there are no function calls in a stack frame, local scope
 *             variables are stored in x9-x18, after which the stack is used
 *
 *   Vectors and vector offsets will always be on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief Get a stack of rvalues by evaluating the lvalue of a ir temporary
 */
void Bitwise_Operator_Inserter::get_operand_stack_from_temporary_lvalue(
    LValue const& lvalue,
    Operand_Stack& stack)
{
    auto& table = accessor_->table_accessor.get_table();
    auto stack_frame = accessor_->get_frame_in_memory();
    auto frame = stack_frame.get_stack_frame();
    auto rvalue = table->lvalue_at_temporary_object_address(lvalue, frame);
    auto& locals = frame->get_locals();

    if (type::is_unary_expression(rvalue))
        get_operand_stack_from_temporary_lvalue(
            type::get_unary_rvalue_reference(rvalue), stack);
    if (type::is_binary_expression(rvalue)) {
        auto [left, right, op] = type::from_rvalue_binary_expression(rvalue);
        if (type::is_rvalue_data_type(left))
            get_operand_stack_from_temporary_lvalue(left, stack);
        if (type::is_rvalue_data_type(right))
            get_operand_stack_from_temporary_lvalue(right, stack);
        if (locals.is_defined(left))
            stack.emplace_back(
                accessor_->device_accessor.get_operand_rvalue_device(left));
        if (locals.is_defined(right))
            stack.emplace_back(
                accessor_->device_accessor.get_operand_rvalue_device(right));
    }
    if (type::is_rvalue_data_type(rvalue))
        stack.emplace_back(type::get_rvalue_datatype_from_string(rvalue));
    if (locals.is_defined(rvalue))
        stack.emplace_back(
            accessor_->device_accessor.get_operand_rvalue_device(rvalue));
}

/**
 * @brief Bitwise operator inserter of bitwise expression operands
 */
void Bitwise_Operator_Inserter::from_bitwise_temporary_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto frame = accessor_->get_frame_in_memory().get_stack_frame();
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& immediate_stack = accessor_->address_accessor.immediate_stack;
    auto& devices = accessor_->device_accessor;
    auto& registers = accessor_->register_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    Storage lhs_s{};
    Storage rhs_s{};

    Operand_Inserter operand_inserter{ accessor_ };
    auto expression = type::from_rvalue_binary_expression(rvalue);
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    auto is_address = [&](RValue const& rvalue) {
        return devices.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(lhs, rhs)(
        m::pattern |
            m::ds(m::app(is_immediate, true), m::app(is_immediate, true)) =
            [&] {
                auto [lhs_i, rhs_i] = get_rvalue_pair_as_immediate(lhs, rhs);
                lhs_s = lhs_i;
                rhs_s = rhs_i;
                operand_inserter.insert_from_immediate_rvalues(
                    lhs_i, op, rhs_i);
                immediate = true;
            },
        m::pattern | m::ds(m::app(is_address, true), m::app(is_address, true)) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lhs, instructions.size(), devices);
                assembly::inserter(instructions, lhs_inst);
                lhs_s = lhs_storage;
                rhs_s = devices.get_device_by_lvalue(rhs);
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = assembly::get_operand_size_from_size(
                    accessor_->table_accessor.get_table()
                        ->lvalue_size_at_temporary_object_address(lhs, frame));
                if (!immediate_stack.empty()) {
                    lhs_s = devices.get_operand_rvalue_device(lhs);
                    if (is_variant(common::Stack_Offset, lhs_s)) {
                        auto [lhs_storage, lhs_inst] =
                            accessor_->address_accessor
                                .get_arm64_lvalue_and_insertion_instructions(
                                    lhs, instructions.size(), devices);
                        assembly::inserter(instructions, lhs_inst);
                        lhs_s = lhs_storage;
                    }
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        auto acc =
                            accessor_->get_accumulator_with_rvalue_context(
                                immediate_stack.back());
                        arm64_add__asm(
                            instructions, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }

                } else {
                    auto& registers_stack = registers.stack;
                    if (!registers_stack.empty()) {
                        if (registers_stack.size() >= 2) {
                            rhs_s = registers_stack.back();
                            registers_stack.pop_back();
                            lhs_s = registers_stack.back();
                            registers_stack.pop_back();
                            return;
                        }
                        if (registers_stack.size() == 1) {
                            rhs_s = registers_stack.back();
                            registers_stack.pop_back();
                            lhs_s =
                                accumulator.get_accumulator_register_from_size(
                                    size);
                            return;
                        }
                    } else {
                        auto operand_stack = Operand_Stack{};
                        get_operand_stack_from_temporary_lvalue(
                            lhs, operand_stack);
                        if (operand_stack.size() == 1)
                            lhs_s = operand_stack.back();
                        operand_stack.clear();
                        get_operand_stack_from_temporary_lvalue(
                            rhs, operand_stack);
                        if (operand_stack.size() == 1)
                            rhs_s = operand_stack.back();
                    }
                }
            },
        m::pattern | m::_ =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                if (is_variant(common::Stack_Offset, lhs_s)) {
                    auto [lhs_storage, lhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, lhs_inst);
                    lhs_s = lhs_storage;
                }
                rhs_s = devices.get_operand_rvalue_device(rhs);
            });
    if (!immediate) {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        auto acc = accessor_->get_accumulator_with_rvalue_context(lhs_s);
        registers.stack.emplace_back(acc);
        if (is_variant(Immediate, lhs_s)) {
            auto intermediate = memory::get_second_register_for_binary_operand(
                memory::get_word_size_from_storage(
                    lhs_s, accessor_->stack, accessor_->get_frame_in_memory()));
            arm64_add__asm(instructions, mov, intermediate, lhs_s);
            lhs_s = intermediate;
        }
        assembly::Ternary_Operands operands = { acc, lhs_s, rhs_s };
        assembly::inserter(instructions,
            bitwise.from_bitwise_expression_operands(operands, op).second);
    }
}

/**
 * @brief Setup the stack frame for a function during instruction insertion
 */
void Instruction_Inserter::setup_stack_frame_in_function(
    ir::Instructions const& ir_instructions,
    IR_Instruction_Visitor& visitor,
    int index)
{
    auto stack_frame = accessor_->get_frame_in_memory();
    auto symbol = std::get<1>(ir_instructions.at(index - 1));
    auto name = type::get_label_as_human_readable(symbol);
    stack_frame.set_stack_frame(name);
    accessor_->device_accessor.set_current_frame_symbol(name);
    visitor.from_func_start_ita(name);
}

/**
 * @brief Insert and type check the argument instructions for the print
 * function
 */
void Invocation_Inserter::insert_type_check_stdlib_print_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };
    if (argument_stack.front() != "RET" and
        not argument_stack.front().starts_with("&"))
        if (!address_storage.is_lvalue_storage_type(
                argument_stack.front(), "string") and
            not library_caller.is_address_device_pointer_to_buffer(
                operands.front()))
            throw_compiletime_error(
                fmt::format("argument '{}' is not a valid buffer address",
                    argument_stack.front()),
                "print",
                __source__,
                "function invocation");
    auto buffer = operands.back();
    auto buffer_size = address_storage.buffer_accessor.has_bytes()
                           ? address_storage.buffer_accessor
                                 .get_size_of_string_lvalue_buffer_address(
                                     argument_stack.back(), stack_frame_)
                           : address_storage.buffer_accessor.read_bytes();
    operands.emplace_back(
        common::assembly::make_u32_int_immediate(buffer_size));
}

/**
 * @brief Insert and type check the argument instructions for the printf
 * function
 */
void Invocation_Inserter::insert_type_check_stdlib_printf_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    if (argument_stack.front() == "RET" or
        type::is_rvalue_data_type_string(argument_stack.front()))
        return;
    if (!address_storage.is_lvalue_storage_type(
            argument_stack.front(), "string") and
        not library_caller.is_address_device_pointer_to_buffer(
            operands.front()))
        throw_compiletime_error(
            fmt::format("invalid format string '{}'", argument_stack.front()),
            "printf",
            __source__,
            "function invocation");
}

/**
 * @brief Unary address-of expression inserter
 */
common::Stack_Offset Unary_Operator_Inserter::from_lvalue_address_of_expression(
    RValue const& expr)
{
    auto& table = accessor_->table_accessor.get_table();
    auto& instruction_accessor = accessor_->instruction_accessor;
    auto& address_space = accessor_->address_accessor;

    credence_assert(type::is_unary_expression(expr));

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto offset =
        accessor_->device_accessor.get_device_by_lvalue_reference(rvalue);
    address_space.address_ir_assignment = true;
    accessor_->stack->allocate_pointer_on_stack();
    accessor_->get_frame_in_memory()
        .get_stack_frame()
        ->get_pointers()
        .emplace_back(rvalue);

    accessor_->stack->add_address_location_to_stack(rvalue);

    auto stack_frame_has_jump = table->stack_frame_contains_call_instruction(
        stack_frame_.symbol, *table->get_ir_instructions());

    if (!stack_frame_has_jump)
        accessor_->flag_accessor.set_instruction_flag(
            detail::flags::Align_Folded, instruction_accessor->size());

    insert_from_unary_operator_operands(
        op, offset, accessor_->stack->get(rvalue).first);

    if (!stack_frame_has_jump)
        accessor_->flag_accessor.set_instruction_flag(
            detail::flags::Align_SP_Folded, instruction_accessor->size());

    return accessor_->stack->get(rvalue).first;
}

/**
 * @brief IR instruction visitor inserter to arm64 instructions
 */
void Instruction_Inserter::from_ir_instructions(
    ir::Instructions const& ir_instructions)
{
    auto ir_visitor = IR_Instruction_Visitor{ accessor_ };
    for (std::size_t index = 0; index < ir_instructions.size(); index++) {
        auto inst = ir_instructions[index];
        ir_visitor.set_iterator_index(index);
        accessor_->table_accessor.set_ir_iterator_index(index);
        ir::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir::Instruction::FUNC_START =
                [&] {
                    setup_stack_frame_in_function(
                        ir_instructions, ir_visitor, index);
                },
            m::pattern | ir::Instruction::FUNC_END =
                [&] { ir_visitor.from_func_end_ita(); },
            m::pattern |
                ir::Instruction::MOV = [&] { ir_visitor.from_mov_ita(inst); },
            m::pattern |
                ir::Instruction::PUSH = [&] { ir_visitor.from_push_ita(inst); },
            m::pattern |
                ir::Instruction::POP = [&] { ir_visitor.from_pop_ita(); },
            m::pattern |
                ir::Instruction::CALL = [&] { ir_visitor.from_call_ita(inst); },
            m::pattern | ir::Instruction::JMP_E =
                [&] { ir_visitor.from_jmp_e_ita(inst); },
            m::pattern |
                ir::Instruction::LOCL = [&] { ir_visitor.from_locl_ita(inst); },
            m::pattern |
                ir::Instruction::GOTO = [&] { ir_visitor.from_goto_ita(inst); },
            m::pattern |
                ir::Instruction::RETURN = [&] { ir_visitor.from_return_ita(); },
            m::pattern |
                ir::Instruction::LEAVE = [&] { ir_visitor.from_leave_ita(); },
            m::pattern | ir::Instruction::LABEL =
                [&] { ir_visitor.from_label_ita(inst); }

        );
    }
}

/**
 * @brief Binary operator inserter of expression operands
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table_accessor = accessor_->table_accessor;
    auto& addresses = accessor_->address_accessor;
    auto& devices = accessor_->device_accessor;
    auto registers = accessor_->register_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    Storage lhs_s{};
    Storage rhs_s{};

    Operand_Inserter operand_inserter{ accessor_ };
    auto expression = type::from_rvalue_binary_expression(rvalue);
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    auto is_address = [&](RValue const& rvalue) {
        return devices.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(lhs, rhs)(
        m::pattern |
            m::ds(m::app(is_immediate, true), m::app(is_immediate, true)) =
            [&] {
                auto [lhs_i, rhs_i] = get_rvalue_pair_as_immediate(lhs, rhs);
                lhs_s = lhs_i;
                rhs_s = rhs_i;
                operand_inserter.insert_from_immediate_rvalues(
                    lhs_i, op, rhs_i);
                immediate = true;
            },
        m::pattern | m::ds(m::app(is_address, true), m::app(is_address, true)) =
            [&] {
                if (!table_accessor.last_ir_instruction_is_assignment()) {
                    auto [lhs_storage, lhs_inst] =
                        addresses.get_arm64_lvalue_and_insertion_instructions(
                            lhs, instructions.size(), devices);
                    assembly::inserter(instructions, lhs_inst);
                    lhs_s = lhs_storage;
                    auto [rhs_storage, rhs_inst] =
                        addresses.get_arm64_lvalue_and_insertion_instructions(
                            rhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;

                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(lhs));

                    auto [rhs_storage, rhs_inst] =
                        addresses.get_arm64_lvalue_and_insertion_instructions(
                            rhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto acc = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_lvalue(lhs));
                lhs_s = acc;
                auto& immediate_stack = addresses.immediate_stack;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        arm64_add__asm(
                            instructions, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(lhs));
                    auto [rhs_storage, rhs_inst] =
                        addresses.get_arm64_lvalue_and_insertion_instructions(
                            rhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    addresses.get_arm64_lvalue_and_insertion_instructions(
                        lhs, instructions.size(), devices);
                assembly::inserter(instructions, lhs_inst);
                lhs_s = lhs_storage;
                rhs_s = devices.get_operand_rvalue_device(rhs);
                if (is_variant(common::Stack_Offset, rhs_s)) {
                    auto [rhs_storage, rhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                rhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto size = memory::get_word_size_from_storage(lhs_s,
                        accessor_->stack,
                        accessor_->get_frame_in_memory());
                    auto acc =
                        accumulator.get_accumulator_register_from_size(size);
                    arm64_add__asm(instructions, mov, acc, lhs_s);
                }

                if (is_temporary(rhs)) {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        assembly::get_operand_size_from_size(
                            devices.get_size_from_rvalue_reference(lhs)));
                    auto [rhs_storage, rhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }

                if (table_accessor.is_ir_instruction_temporary()) {
                    if (!type::is_relation_binary_operator(op))
                        lhs_s = accumulator.get_accumulator_register_from_size(
                            devices.get_word_size_from_lvalue(lhs));
                }
            },
        m::pattern |
            m::ds(m::app(is_address, false), m::app(is_address, true)) =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                if (is_variant(common::Stack_Offset, lhs_s)) {
                    auto [lhs_storage, lhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, lhs_inst);
                    lhs_s = lhs_storage;
                }
                auto [rhs_storage, rhs_inst] =
                    addresses.get_arm64_lvalue_and_insertion_instructions(
                        rhs, instructions.size(), devices);
                assembly::inserter(instructions, rhs_inst);
                rhs_s = rhs_storage;
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        assembly::get_operand_size_from_size(
                            devices.get_size_from_rvalue_reference(rhs)));
                    arm64_add__asm(instructions, mov, acc, rhs_s);
                }
                if (is_temporary(lhs))
                    rhs_s = accumulator.get_accumulator_register_from_size(
                        assembly::get_operand_size_from_size(
                            devices.get_size_from_rvalue_reference(rhs)));
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, false)) =
            [&] {
                rhs_s = devices.get_operand_rvalue_device(rhs);
                if (is_variant(common::Stack_Offset, rhs_s)) {
                    auto [rhs_storage, rhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                rhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }
                lhs_s = accumulator.get_accumulator_register_from_size(
                    memory::get_word_size_from_storage(rhs_s,
                        accessor_->stack,
                        accessor_->get_frame_in_memory()));
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                if (is_variant(common::Stack_Offset, lhs_s)) {
                    auto [lhs_storage, lhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, lhs_inst);
                    lhs_s = lhs_storage;
                }
                rhs_s = accumulator.get_accumulator_register_from_size(
                    memory::get_word_size_from_storage(lhs_s,
                        accessor_->stack,
                        accessor_->get_frame_in_memory()));
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                if (is_variant(common::Stack_Offset, lhs_s)) {
                    auto [lhs_storage, lhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, lhs_inst);
                    lhs_s = lhs_storage;
                }
                rhs_s = devices.get_operand_rvalue_device(rhs);
                if (is_variant(common::Stack_Offset, rhs_s)) {
                    auto [rhs_storage, rhs_inst] =
                        accessor_->address_accessor
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    assembly::inserter(instructions, rhs_inst);
                    rhs_s = rhs_storage;
                }
            });
    if (!immediate) {
        auto operand_inserter = Operand_Inserter{ accessor_ };
        assembly::Assignment_Operands operands = { lhs_s, rhs_s };
        operand_inserter.insert_from_binary_operands(operands, op);
    }
}

Operand_Size Unary_Operator_Inserter::get_operand_size_from_lvalue_reference(
    LValue const& lvalue)
{
    auto& device_accessor = accessor_->device_accessor;
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto is_vector = [&](RValue const& rvalue) {
        return accessor_->table_accessor.get_table()->get_vectors().contains(
            type::from_lvalue_offset(rvalue));
    };
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };
    return m::match(lvalue)(
        m::pattern | m::app(is_address, true) =
            [&] { return device_accessor.get_word_size_from_lvalue(lvalue); },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto [vector_s, vector_i] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lvalue, instructions.size(), device_accessor);
                assembly::inserter(instructions, vector_i);
                return memory::get_operand_size_from_storage(
                    vector_s, accessor_->stack);
            },
        m::pattern | m::_ =
            [&] {
                auto immediate = type::get_rvalue_datatype_from_string(lvalue);
                return assembly::get_operand_size_from_rvalue_datatype(
                    immediate);
            });
}

Storage Unary_Operator_Inserter::get_temporary_storage_from_temporary_expansion(
    RValue const& rvalue)
{
    auto size = get_operand_size_from_lvalue_reference(rvalue);
    auto frame = accessor_->table_accessor.get_table()->get_stack_frame();
    auto acc = accessor_->get_accumulator_with_rvalue_context(size);
    return acc;
}

/**
 * @brief Inserter from ir unary expression types
 */
Storage Unary_Operator_Inserter::insert_from_unary_operator_rvalue(
    RValue const& expr)
{
    credence_assert(type::is_unary_expression(expr));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& devices = accessor_->device_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_space = accessor_->address_accessor;

    Storage storage{};

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto is_vector = [&](RValue const& rvalue) {
        return accessor_->table_accessor.get_table()->get_vectors().contains(
            type::from_lvalue_offset(rvalue));
    };
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(rvalue)(
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [e_storage, e_inst] =
                    address_space.get_arm64_lvalue_and_insertion_instructions(
                        rvalue, instructions.size(), devices);
                assembly::inserter(instructions, e_inst);
                storage = e_storage;
                insert_from_unary_operator_operands(op, storage, storage);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto [address, address_inst] =
                    address_space.get_arm64_lvalue_and_insertion_instructions(
                        rvalue, instructions.size(), devices);
                assembly::inserter(instructions, address_inst);
                storage =
                    get_temporary_storage_from_temporary_expansion(rvalue);
                address_space.address_ir_assignment = true;
                accessor_->set_signal_register(Register::x8);
                insert_from_unary_operator_operands(op, storage, address);
            },

        m::pattern | m::_ =
            [&] {
                auto immediate = type::get_rvalue_datatype_from_string(rvalue);
                storage =
                    get_temporary_storage_from_temporary_expansion(rvalue);
                insert_from_unary_operator_operands(op, storage, immediate);
            });

    return storage;
}

/**
 * @brief Inserter from unary expression
 */
void Unary_Operator_Inserter::insert_from_unary_operator_operands(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table_accessor = accessor_->table_accessor;
    auto frame = accessor_->table_accessor.get_table()->get_stack_frame();
    auto index = accessor_->instruction_accessor->size();

    m::match(op)(
        m::pattern | std::string{ "++" } =
            [&] {
                assembly::inserter(instructions, assembly::inc(dest).second);
            },
        m::pattern | std::string{ "--" } =
            [&] {
                assembly::inserter(instructions, assembly::dec(dest).second);
            },
        m::pattern | std::string{ "~" } =
            [&] {
                auto size = memory::get_word_size_from_storage(
                    dest, accessor_->stack, accessor_->get_frame_in_memory());
                if (table_accessor.is_ir_instruction_temporary() and
                    table_accessor.next_ir_instruction_is_temporary()) {
                    auto acc =
                        accessor_->get_accumulator_with_rvalue_context(size);
                    assembly::inserter(
                        instructions, assembly::b_not(acc, src).second);
                    accessor_->register_accessor.stack.emplace_back(acc);
                } else
                    assembly::inserter(
                        instructions, assembly::b_not(dest, src).second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Address, index);
                assembly::inserter(instructions,
                    assembly::store(dest, std::get<common::Stack_Offset>(src))
                        .second);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto size = memory::get_word_size_from_storage(
                    dest, accessor_->stack, accessor_->get_frame_in_memory());
                auto acc = accessor_->get_accumulator_with_rvalue_context(size);
                if (!assembly::is_equal_storage_devices(acc, dest)) {
                    arm64_add__asm(instructions, mov, acc, dest);
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Indirect_Source, index + 1);
                } else {
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Indirect_Source, index);
                }

                arm64_add__asm(instructions, str, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(
                    instructions, assembly::neg(dest, src).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Expression inserter for lvalue at temporary object address
 */
void Expression_Inserter::insert_lvalue_at_temporary_object_address(
    LValue const& lvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.get_table();
    auto temporary = table->lvalue_at_temporary_object_address(lvalue, frame);

    insert_from_temporary_rvalue(temporary);
}

/**
 * @brief Inserter of a return value from a function body in the stack frame
 *
 *  test(*y) {
 *   return(y); <---
 *  }
 */
void Expression_Inserter::insert_from_return_rvalue(
    ir::object::Function::Return_RValue const& ret)
{
    auto& table = accessor_->table_accessor.get_table();
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_rvalue(ret->second);
    auto tail_frame = table->get_functions().at(stack_frame_.tail);
    auto return_rvalue = tail_frame->get_ret()->second;
    if (memory::is_doubleword_storage_size(
            immediate, accessor_->stack, accessor_->get_frame_in_memory()))
        arm64_add__asm(instructions, mov, x0, immediate);
    else
        arm64_add__asm(instructions, mov, w0, immediate);
}

/**
 * @brief Insert from an address-of assignment expression
 */
void Expression_Inserter::insert_from_address_of_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.get_table();
    auto& instructions = instruction_accessor->get_instructions();
    if (accessor_->table_accessor.next_ir_instruction_is_assignment()) {
        auto lvalue = std::get<1>(table->get_ir_instructions()->at(
            accessor_->table_accessor.get_index() + 1));
        auto lhs_s = accessor_->device_accessor.get_device_by_lvalue(lvalue);
        accessor_->address_accessor.address_ir_assignment = true;
        accessor_->stack->allocate_pointer_on_stack();
        accessor_->get_frame_in_memory()
            .get_stack_frame()
            ->get_pointers()
            .emplace_back(rvalue);
        accessor_->stack->add_address_location_to_stack(rvalue);
        if (is_variant(common::Stack_Offset, lhs_s)) {
            lhs_s = u32_int_immediate(std::get<common::Stack_Offset>(lhs_s));
            auto rhs_s = assembly::O_NUL;
            auto last_arm64_inst = std::get<1>(instructions.back());
            Storage last_src_one = std::get<1>(last_arm64_inst);
            if (is_variant(Register, last_src_one) and
                (std::get<Register>(last_src_one) == Register::x10 or
                    std::get<Register>(last_src_one) == Register::w10)) {
                rhs_s = std::get<common::Stack_Offset>(
                    std::get<2>(last_arm64_inst));
            } else
                rhs_s = accessor_->stack->get(rvalue).first;
            arm64_add__asm(instructions, add, x6, sp, lhs_s);
            arm64_add__asm(instructions, str, x6, rhs_s);
        } else {
            auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
            auto rhs_s = u32_int_immediate(
                unary_inserter.from_lvalue_address_of_expression(rvalue));
            arm64_add__asm(instructions, add, x6, sp, rhs_s);
        }
    } else {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        auto rhs_s = u32_int_immediate(
            unary_inserter.from_lvalue_address_of_expression(rvalue));
        arm64_add__asm(instructions, add, x6, sp, rhs_s);
    }
    return;
}

/**
 * @brief Expression inserter from rvalue
 */
void Expression_Inserter::insert_from_temporary_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.get_table();
    auto& instructions = instruction_accessor->get_instructions();

    auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
    auto bitwise_inserter = Bitwise_Operator_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };

    auto is_comparator = [](RValue const& rvalue) {
        return rvalue.starts_with("CMP");
    };

    m::match(rvalue)(
        m::pattern | m::app(type::is_bitwise_binary_expression, true) =
            [&] { bitwise_inserter.from_bitwise_temporary_expression(rvalue); },
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { binary_inserter.from_binary_operator_expression(rvalue); },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                if (type::is_address_of_expression(rvalue)) {
                    insert_from_address_of_rvalue(rvalue);
                    return;
                }
                unary_inserter.insert_from_unary_operator_rvalue(rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                Storage immediate =
                    operand_inserter.get_operand_storage_from_rvalue(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                arm64_add__asm(instructions, mov, acc, immediate);
                auto type = type::get_type_from_rvalue_data_type(rvalue);
                if (type == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Address, instruction_accessor->size());
            },
        m::pattern | m::app(is_comparator, true) = [&] {},
        m::pattern | RValue{ "RET" } =
            [&] {

#if defined(__linux__)
                if (common::runtime::is_stdlib_function(stack_frame_.tail,
                        common::assembly::OS_Type::Linux,
                        common::assembly::Arch_Type::ARM64))
                    return;
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
                if (common::runtime::is_stdlib_function(stack_frame_.tail,
                        common::assembly::OS_Type::BSD,
                        common::assembly::Arch_Type::ARM64))
                    return;
#endif
                credence_assert(
                    table->get_functions().contains(stack_frame_.tail));
                auto frame = table->get_functions()[stack_frame_.tail];
                credence_assert(frame->get_ret().has_value());
                auto immediate =
                    operand_inserter.get_operand_storage_from_rvalue(
                        frame->get_ret()->first);
                if (memory::get_operand_size_from_storage(immediate,
                        accessor_->stack) == Operand_Size::Doubleword) {
                    accessor_->set_signal_register(Register::x0);
                }
            },
        m::pattern | m::_ =
            [&] {
                auto symbols = table->get_stack_frame_symbols();
                Storage immediate = symbols.get_symbol_by_name(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                arm64_add__asm(instructions, mov, acc, immediate);
            });
}

/**
 * @brief Expression inserter for global vector assignment
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lhs, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rhs, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, rhs_storage_inst);
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::Indirect_Source, instructions.size());
    arm64_add__asm(instructions, ldr, lhs_storage, rhs_storage);
}

/**
 * @brief Inserter from unary-to-unary rvalue expressions
 *
 * The only supported type is dereferenced pointers
 */
void Unary_Operator_Inserter::insert_from_unary_to_unary_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto lhs_lvalue = type::get_unary_rvalue_reference(lhs);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rhs);
    auto devices = accessor_->device_accessor;

    m::match(type::get_unary_operator(lhs),
        type::get_unary_operator(rhs))(m::pattern | m::ds("*", "*") = [&] {
        auto frame = stack_frame_.get_stack_frame();
        auto rvalue = ir::object::get_rvalue_at_lvalue_object_storage(
            lhs, frame, accessor_->table_accessor.get_table()->get_vectors());
        auto size = assembly::get_operand_size_from_size(
            devices.get_size_from_rvalue_data_type(lhs, rvalue));
        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                size);
        auto [lhs_storage, lhs_inst] =
            accessor_->address_accessor
                .get_arm64_lvalue_and_insertion_instructions(
                    lhs_lvalue, instructions.size(), devices);
        assembly::inserter(instructions, lhs_inst);
        Storage rhs_storage = devices.get_device_by_lvalue(rhs_lvalue);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Indirect_Source, instruction_accessor->size());
        arm64_add__asm(instructions, ldr, acc, rhs_storage);
        if (is_variant(Register, lhs_storage))
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Indirect_Source, instruction_accessor->size());
        else
            accessor_->flag_accessor.set_instruction_flag(
                detail::flags::Align_Folded, instruction_accessor->size());

        arm64_add__asm(instructions, str, acc, lhs_storage);
    });
}

/**
 * @brief Operand Inserter mediator for expression mnemonics operands
 */
void Operand_Inserter::insert_from_binary_operands(
    assembly::Assignment_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_x1_address_offset(operands.first) and
        not assembly::is_immediate_relative_address(operands.first)) {
        std::swap(operands.first, operands.second);
    }

    m::match(op)(
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
                assembly::inserter(instructions,
                    arithmetic.from_arithmetic_expression_operands(operands, op)
                        .second);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto relational = Relational_Operator_Inserter{ accessor_ };
                auto& ir_instructions = accessor_->table_accessor.get_table()
                                            ->get_ir_instructions();
                auto ir_index = accessor_->table_accessor.get_index();
                if (ir_instructions->size() > ir_index and
                    std::get<0>(ir_instructions->at(ir_index + 1)) ==
                        ir::Instruction::IF) {
                    auto label = assembly::make_label(
                        std::get<3>(ir_instructions->at(ir_index + 1)),
                        stack_frame_.symbol);
                    assembly::inserter(instructions,
                        relational.from_relational_expression_operands(
                            operands, op, label));
                }
            },
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto relational = Relational_Operator_Inserter{ accessor_ };
                auto& ir_instructions = accessor_->table_accessor.get_table()
                                            ->get_ir_instructions();
                auto ir_index = accessor_->table_accessor.get_index();
                if (ir_instructions->size() > ir_index and
                    std::get<0>(ir_instructions->at(ir_index + 1)) ==
                        ir::Instruction::IF) {
                    auto label = assembly::make_label(
                        std::get<3>(ir_instructions->at(ir_index + 1)),
                        stack_frame_.symbol);
                    assembly::inserter(instructions,
                        relational.from_relational_expression_operands(
                            operands, op, label));
                }
            },
        m::pattern | m::_ =
            [&, op] {
                credence_error(fmt::format("unreachable: operator '{}'", op));
            });
}

/**
 * @brief Operand inserter for immediate rvalues
 */
void Operand_Inserter::insert_from_immediate_rvalues(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    auto& immediate_stack = accessor_->address_accessor.immediate_stack;
    auto accumulator = accessor_->accumulator_accessor;
    m::match(op)(
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_integral_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accessor_->get_accumulator_with_rvalue_context(imm);
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    arm64_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief
 *
 * Resolve the return rvalue to store in an lvalue, we take special care with
 * `getchar' which is the only standard library function that may return a
 * value:
 *
 *  auto x = getchar();
 *  putchar(x);
 *
 */
void Expression_Inserter::insert_lvalue_from_return_rvalue(LValue const& lvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table = accessor_->table_accessor.get_table();
    auto operand_inserter = Operand_Inserter{ accessor_ };
#if defined(__linux__)
    if (common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64) and
        stack_frame_.tail != "getchar")
        return;
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    if (common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64) and
        stack_frame_.tail != "getchar")
        return;
#endif
    if (stack_frame_.tail != "getchar" and
        not table->get_functions().contains(stack_frame_.tail))
        return;
    auto frame = table->get_functions()[stack_frame_.tail];
    auto [lhs_s, lhs_i] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lvalue, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, lhs_i);
    if (is_variant(common::Stack_Offset, lhs_s)) {
        auto lhs_r = accessor_->register_accessor.get_available_register(
            accessor_->device_accessor.get_word_size_from_lvalue(lvalue));
        arm64_add__asm(instructions, str, lhs_r, lhs_s);
        lhs_s = lhs_r;
    }
    if (stack_frame_.tail == "getchar")
        arm64_add__asm(instructions, mov, lhs_s, w0);
    else {
        auto immediate = operand_inserter.get_operand_storage_from_rvalue(
            frame->get_ret()->first);
        arm64_add__asm(instructions, mov, lhs_s, immediate);
    }
}

/**
 * @brief Operand inserter for mnemonic operand
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& devices = accessor_->device_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lhs, instructions.size(), devices);
                if (is_variant(common::Stack_Offset, lhs_storage)) {
                    auto lhs_r =
                        accessor_->register_accessor.get_available_register(
                            devices.get_word_size_from_lvalue(rhs));
                    arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                    lhs_storage = lhs_r;
                }
                assembly::inserter(instructions, storage_inst);
                auto expression_inserter = Expression_Inserter{ accessor_ };
                if (type::get_type_from_rvalue_data_type(imm) == "string")
                    insert_from_string_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "float")
                    insert_from_float_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "double")
                    insert_from_double_address_operand(lhs, lhs_storage, rhs);
                else
                    arm64_add__asm(instructions, mov, lhs_storage, imm);
            },
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] {
                auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
                binary_inserter.from_binary_operator_expression(rhs);
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

                auto [lhs_storage, storage_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs, instructions.size(), devices);
                if (is_variant(common::Stack_Offset, lhs_storage)) {
                    auto lhs_r =
                        accessor_->register_accessor.get_available_register(
                            devices.get_word_size_from_lvalue(rhs));
                    arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                    lhs_storage = lhs_r;
                }
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    unary_op, lhs_storage);
            },
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    if (is_variant(common::Stack_Offset, lhs_storage)) {
                        auto lhs_r =
                            accessor_->register_accessor.get_available_register(
                                devices.get_word_size_from_lvalue(rhs));
                        arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                        lhs_storage = lhs_r;
                    }
                    arm64_add__asm(instructions, mov, lhs_storage, x6);
                } else {
                    auto frame = stack_frame_.get_stack_frame();
                    auto rvalue =
                        accessor_->table_accessor.get_table()
                            ->lvalue_at_temporary_object_address(rhs, frame);
                    auto size = assembly::get_operand_size_from_size(
                        accessor_->table_accessor.get_table()
                            ->lvalue_size_at_temporary_object_address(
                                rhs, frame));
                    auto expression_inserter = Expression_Inserter{ accessor_ };
                    expression_inserter.insert_lvalue_from_return_rvalue(lhs);
                    if (rvalue == "RET") {
                        expression_inserter.insert_lvalue_from_return_rvalue(
                            lhs);
                        return;
                    } else {
                        auto acc =
                            accumulator.get_accumulator_register_from_size(
                                size);
                        if (!type::is_unary_expression(lhs))
                            accessor_->device_accessor.insert_lvalue_to_device(
                                lhs);
                        auto [lhs_storage, storage_inst] =
                            address_storage
                                .get_arm64_lvalue_and_insertion_instructions(
                                    lhs, instructions.size(), devices);
                        if (is_variant(common::Stack_Offset, lhs_storage)) {
                            auto lhs_r =
                                accessor_->register_accessor
                                    .get_available_register(
                                        devices.get_word_size_from_lvalue(rhs));
                            arm64_add__asm(
                                instructions, str, lhs_r, lhs_storage);
                            lhs_storage = lhs_r;
                        }
                        assembly::inserter(instructions, storage_inst);
                        arm64_add__asm(instructions, mov, lhs_storage, acc);
                    }
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs, instructions.size(), devices);
                assembly::inserter(instructions, lhs_inst);
                auto [rhs_storage, rhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        rhs, instructions.size(), devices);
                assembly::inserter(instructions, rhs_inst);
                auto acc = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_lvalue(rhs));

                arm64_add__asm(instructions, mov, acc, rhs_storage);
                arm64_add__asm(instructions, mov, lhs_storage, acc);
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Get the storage device of a return rvalue in the stack frame
 */
inline Storage Operand_Inserter::get_operand_storage_from_return()
{
    auto& tail_call = accessor_->table_accessor.get_table()
                          ->get_functions()[stack_frame_.tail];
    if (tail_call->get_locals().is_pointer(tail_call->get_ret()->first) or
        type::is_rvalue_data_type_string(tail_call->get_ret()->first))
        return Register::x0;
    else
        return Register::w0;
}

/**
 * @brief Get the storage device of an immediate operand
 */
Storage Operand_Inserter::get_operand_storage_from_immediate(
    RValue const& rvalue)
{
    Storage storage{};
    auto immediate = type::get_rvalue_datatype_from_string(rvalue);
    auto type = type::get_type_from_rvalue_data_type(immediate);
    if (type == "string") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_string_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "float") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_float_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "double") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_double_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }

    storage = type::get_rvalue_datatype_from_string(rvalue);
    return storage;
}

/**
 * @brief Get the storage device of an parameter rvalue in the stack frame
 */
Storage Operand_Inserter::get_operand_storage_from_parameter(
    RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    auto index_of = frame->get_index_of_parameter(rvalue);
    credence_assert_nequal(index_of, -1);
    // the argc and argv special cases
    if (frame->get_symbol() == "main") {
        if (index_of == 0)
            return accessor_->stack->get("argc").first;
        if (index_of == 1) {
            if (!is_vector_offset(rvalue))
                common::runtime::throw_runtime_error(
                    "invalid argv access, argv is a vector to strings", rvalue);
            auto offset = type::from_decay_offset(rvalue);
            if (!util::is_numeric(offset) and
                not accessor_->address_accessor.is_lvalue_storage_type(
                    offset, "int"))
                common::runtime::throw_runtime_error(
                    fmt::format(
                        "invalid argv access, argv has malformed offset '{}'",
                        offset),
                    rvalue);
        }
    }
    if (frame->is_pointer_parameter(rvalue))
        return memory::registers::available_doubleword_argument.at(index_of);
    else
        return memory::registers::available_word_argument.at(index_of);
}

/**
 * @brief Get the storage device of an rvalue operand
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue)) {
        return get_operand_storage_from_parameter(rvalue);
    }

#if defined(__linux__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, operand_inst);
    return operand;
}

Storage Operand_Inserter::get_operand_storage_from_rvalue_no_instructions(
    RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue)) {
        return get_operand_storage_from_parameter(rvalue);
    }
#if defined(__linux__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, instructions.size(), accessor_->device_accessor);
    return operand;
}

/**
 * @brief Get operands storage from argument stack
 */
Invocation_Inserter::arguments_t
Invocation_Inserter::get_operands_storage_from_argument_stack()
{
    Operand_Inserter operands{ accessor_ };
    syscall_ns::syscall_arguments_t arguments{};
    auto caller_frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.get_table();
    for (auto const& rvalue : stack_frame_.argument_stack) {
        if (rvalue == "RET") {
            credence_assert(table->get_functions().contains(stack_frame_.tail));
            auto tail_frame = table->get_functions().at(stack_frame_.tail);
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    tail_frame->get_ret()->first, "string") or
                caller_frame->is_pointer_in_stack_frame(
                    tail_frame->get_ret()->first))
                arguments.emplace_back(Register::x0);
            else
                arguments.emplace_back(Register::w0);
        } else {
            auto operand = assembly::O_NUL;
            if (accessor_->stack->is_allocated(rvalue)) {
                if (accessor_->stack->is_allocated(rvalue)) {
                    arguments.emplace_back(accessor_->stack->get(rvalue).first);
                    continue;
                }
            }
            if (is_vector_offset(rvalue))
                operand = operands.get_operand_storage_from_rvalue(rvalue);
            else
                operand =
                    operands.get_operand_storage_from_rvalue_no_instructions(
                        rvalue);
            if (is_variant(Register, operand) and
                std::get<Register>(operand) == Register::x15) {
                auto lhs = type::from_lvalue_offset(rvalue);
                auto offset = type::from_decay_offset(rvalue);
                auto vector = table->get_vectors().at(lhs);
                auto vector_s =
                    accessor_->stack->get_stack_offset_from_table_vector_index(
                        lhs, offset, *vector);
                arguments.emplace_back(vector_s);
            } else
                arguments.emplace_back(operand);
        }
    }
    return arguments;
}

/**
 * @brief Invocation inserter for syscall function
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    Instructions& instructions)
{
    auto syscall_inserter =
        syscall_ns::Syscall_Invocation_Inserter{ accessor_, stack_frame_ };
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);

    auto operands = this->get_operands_storage_from_argument_stack();

    syscall_inserter.make_syscall(
        instructions, routine, operands, stack_frame_.argument_stack);
}

/**
 * @brief Invocation inserter for user defined functions
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    for (std::size_t i = 0; i < operands.size(); i++) {
        auto const& operand = operands[i];
        auto argument = stack_frame_.argument_stack.at(i);
        auto arg_type = type::get_type_from_rvalue_data_type(argument);
        auto arg_register = assembly::get_register_from_integer_argument(i);
        if (type::from_lvalue_offset(argument) == "argv") {
            auto offset = type::from_lvalue_offset(argument);
            auto argv_address = accessor_->stack->get("argv").first;
            auto offset_integer = type::integral_from_type_ulint(offset);
            auto argv_offset =
                direct_immediate(fmt::format("[x10, #{}]", 8 * offset_integer));
            arm64_add__asm(instructions, ldr, x10, argv_address);
            arm64_add__asm(instructions, ldr, operand, argv_offset);
        } else
            m::match(arg_type)(
                m::pattern | sv("string") =
                    [&] {
                        auto immediate = type::get_value_from_rvalue_data_type(
                            std::get<Immediate>(operand));
                        auto imm_1 =
                            direct_immediate(fmt::format("{}@PAGE", immediate));
                        arm64_add__asm(instructions, adrp, arg_register, imm_1);
                        auto imm_2 = direct_immediate(
                            fmt::format("{}@PAGEOFF", immediate));
                        arm64_add__asm(instructions,
                            add,
                            arg_register,
                            arg_register,
                            imm_2);
                    },
                m::pattern | m::or_(sv("float"), sv("double")) =
                    [&] {
                        auto immediate = type::get_value_from_rvalue_data_type(
                            std::get<Immediate>(operand));
                        auto imm =
                            direct_immediate(fmt::format("={}", immediate));
                        arm64_add__asm(instructions, ldr, arg_register, imm);
                    },
                m::pattern | m::_ =
                    [&] {
                        accessor_->flag_accessor.set_instruction_flag(
                            common::flag::Argument, instructions.size());
                        if (is_variant(common::Stack_Offset, operand) or
                            assembly::is_immediate_pc_address_offset(operand))
                            arm64_add__asm(
                                instructions, ldr, arg_register, operand);
                        else if (is_variant(Register, operand) and
                                 assembly::is_word_register(
                                     std::get<Register>(operand))) {
                            auto storage_dword =
                                assembly::get_word_register_from_doubleword(
                                    arg_register);
                            arm64_add__asm(
                                instructions, mov, storage_dword, operand);
                        } else
                            arm64_add__asm(
                                instructions, mov, arg_register, operand);
                    });
    }
    arm64_add__asm(instructions, bl, direct_immediate(routine));
}

/**
 * @brief Invocation inserter for standard library function
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    auto& argument_stack = stack_frame_.argument_stack;
    m::match(routine)(
        m::pattern | sv("putchar") = [&] {},
        m::pattern | sv("getchar") = [&] {},
        m::pattern | sv("print") =
            [&] {
                insert_type_check_stdlib_print_arguments(
                    argument_stack, operands);
            },
        m::pattern | sv("printf") =
            [&] {
                insert_type_check_stdlib_printf_arguments(
                    argument_stack, operands);
            });
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    library_caller.make_library_call(instructions, routine, operands);
}

/**
 * @brief Arithmetic operator inserter for expression operands
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Assignment_Operands const& operands,
    std::string const& binary_op)
{
    auto frame = accessor_->table_accessor.get_table()->get_stack_frame();
    Instruction_Pair instructions{ Register::w8, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                instructions = assembly::div(operands.first, operands.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                instructions = assembly::sub(operands.first, operands.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                instructions = assembly::add(operands.first, operands.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                instructions = assembly::mod(operands.first, operands.second);
            });
    return instructions;
}

Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Ternary_Operands const& operands,
    std::string const& binary_op)
{
    auto [s0, s1, s2] = operands;
    Instruction_Pair instructions{ Register::wzr, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "<<" } =
            [&] { instructions = assembly::lshift(s0, s1, s2); },
        m::pattern | std::string{ ">>" } =
            [&] { instructions = assembly::rshift(s0, s1, s2); },
        m::pattern | std::string{ "^" } =
            [&] { instructions = assembly::b_xor(s0, s1, s2); },
        m::pattern | std::string{ "~" } =
            [&] { instructions = assembly::b_not(s0, s1, s1); },
        m::pattern | std::string{ "&" } =
            [&] { instructions = assembly::b_and(s0, s1, s2); },
        m::pattern | std::string{ "|" } =
            [&] { instructions = assembly::b_or(s0, s1, s2); });
    return instructions;
}

/**
 * @brief Relational operator inserter for expression operands
 */
Instructions Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Assignment_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::w8;
    if (memory::is_doubleword_storage_size(operands.first,
            accessor_->stack,
            accessor_->get_frame_in_memory()) or
        memory::is_doubleword_storage_size(operands.second,
            accessor_->stack,
            accessor_->get_frame_in_memory())) {
        register_storage = Register::x8;
    }

    return m::match(binary_op)(
        m::pattern | std::string{ "==" } =
            [&] {
                return assembly::r_eq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                return assembly::r_neq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                return assembly::r_lt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                return assembly::r_gt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                return assembly::r_le(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                return assembly::r_ge(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | m::_ = [&] { return Instructions{}; });
}

void Operand_Inserter::insert_from_float_address_operand(
    [[maybe_unused]] LValue const& lhs,
    [[maybe_unused]] Storage const& storage,
    [[maybe_unused]] RValue const& rhs)
{
}

void Operand_Inserter::insert_from_double_address_operand(
    [[maybe_unused]] LValue const& lhs,
    [[maybe_unused]] Storage const& storage,
    [[maybe_unused]] RValue const& rhs)
{
}

/**
 * @brief Insert into a device from the page address of a string
 */
void Operand_Inserter::insert_from_string_address_operand(
    [[maybe_unused]] LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_string(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Doubleword);
    }
    arm64_add__asm(instructions, mov, storage, x6);
}

/**
 * @brief Expression inserter of a string in the data section
 */
void Expression_Inserter::insert_from_string(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_string(str));
    auto immediate =
        accessor_->address_accessor.buffer_accessor.get_string_address_offset(
            str);
    auto imm_1 = direct_immediate(fmt::format("{}@PAGE", immediate));
    arm64_add__asm(instructions, adrp, x6, imm_1);
    auto imm_2 = direct_immediate(fmt::format("{}@PAGEOFF", immediate));
    arm64_add__asm(instructions, add, x6, x6, imm_2);
}

/**
 * @brief Expression inserter from a string in the data section
 */
void Expression_Inserter::insert_from_float(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto immediate = direct_immediate(fmt::format("={}", str));
    arm64_add__asm(instructions, ldr, s6, immediate);
}

void Expression_Inserter::insert_from_double(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto immediate = direct_immediate(fmt::format("={}", str));
    arm64_add__asm(instructions, ldr, d6, immediate);
}

} // namespace credence::target::arm64