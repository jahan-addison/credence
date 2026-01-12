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
 * Handles arithmetic, bitwise, relational operators, and assignments.
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

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief Get the operand size (word size) of a storage device
 */
assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<Immediate> i;
    m::Id<Register> r;
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] { return stack->get_operand_size_from_offset(*s); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return assembly::get_operand_size_from_rvalue_datatype(*i); },
        m::pattern | m::as<Register>(r) =
            [&] { return assembly::get_operand_size_from_register(*r); },
        m::pattern | m::_ = [&] { return Operand_Size::Empty; });
}

/**
 * @brief Get a stack of rvalues by evaluating the lvalue of a ir temporary
 */
void Bitwise_Operator_Inserter::get_operand_stack_from_temporary_lvalue(
    LValue const& lvalue,
    Operand_Stack& stack)
{
    auto& table = accessor_->table_accessor.table_;
    auto stack_frame = accessor_->stack_frame;
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
    auto frame = accessor_->stack_frame.get_stack_frame();
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
                lhs_s = devices.get_device_by_lvalue(lhs);
                rhs_s = devices.get_device_by_lvalue(rhs);
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = assembly::get_operand_size_from_size(
                    accessor_->table_accessor.table_
                        ->lvalue_size_at_temporary_object_address(lhs, frame));
                if (!immediate_stack.empty()) {
                    lhs_s = devices.get_operand_rvalue_device(lhs);
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
                rhs_s = devices.get_operand_rvalue_device(rhs);
            });
    if (!immediate) {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        auto acc = accessor_->get_accumulator_with_rvalue_context(lhs_s);
        registers.stack.emplace_back(acc);
        if (acc == Register::x23 or acc == Register::w23 or
            is_variant(Immediate, lhs_s)) {
            frame->get_tokens().insert("x23");
        }
        if (is_variant(Immediate, lhs_s)) {
            auto intermediate = devices.get_second_register_for_binary_operand(
                devices.get_word_size_from_storage(lhs_s));
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
    auto stack_frame = accessor_->stack_frame;
    auto symbol = std::get<1>(ir_instructions.at(index - 1));
    auto name = type::get_label_as_human_readable(symbol);
    stack_frame.set_stack_frame(name);
    accessor_->device_accessor.set_current_frame_symbol(name);
    if (name == "main") {
        // setup argc, argv
        auto argc_argv =
            common::runtime::argc_argv_kernel_runtime_access(stack_frame);
        if (argc_argv.first) {
            auto& instructions =
                accessor_->instruction_accessor->get_instructions();
            auto argc_address = direct_immediate("[sp]");
            arm64_add__asm(instructions, ldr, x27, argc_address);
        }
    }
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
    auto& table = accessor_->table_accessor.table_;
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };
    if (argument_stack.front() != "RET" and
        not argument_stack.front().starts_with("&"))
        if (!address_storage.is_lvalue_storage_type(
                argument_stack.front(), "string") and
            not library_caller.is_address_device_pointer_to_buffer(
                operands.front(), table, accessor_->stack))
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
    auto& table = accessor_->table_accessor.table_;
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    if (argument_stack.front() == "RET" or
        type::is_rvalue_data_type_string(argument_stack.front()))
        return;
    if (!address_storage.is_lvalue_storage_type(
            argument_stack.front(), "string") and
        not library_caller.is_address_device_pointer_to_buffer(
            operands.front(), table, accessor_->stack))
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
    auto& instruction_accessor = accessor_->instruction_accessor;
    auto& address_space = accessor_->address_accessor;

    credence_assert(type::is_unary_expression(expr));

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto offset =
        accessor_->device_accessor.get_device_by_lvalue_reference(rvalue);
    address_space.address_ir_assignment = true;
    accessor_->stack->allocate_pointer_on_stack();
    accessor_->stack_frame.get_stack_frame()->get_pointers().emplace_back(
        rvalue);
    accessor_->stack->add_address_location_to_stack(rvalue);

    accessor_->flag_accessor.set_instruction_flag(
        detail::flags::Align_Folded, instruction_accessor->size());

    insert_from_unary_operator_operands(
        op, offset, accessor_->stack->get(rvalue).first);

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
                    lhs_s = devices.get_device_by_lvalue(lhs);
                    rhs_s = devices.get_device_by_lvalue(rhs);
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        devices.get_word_size_from_lvalue(lhs));
                    rhs_s = devices.get_device_by_lvalue(rhs);
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
                    rhs_s = devices.get_device_by_lvalue(rhs);
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                lhs_s = devices.get_device_by_lvalue(lhs);
                rhs_s = devices.get_operand_rvalue_device(rhs);

                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto size = devices.get_word_size_from_storage(lhs_s);
                    auto acc =
                        accumulator.get_accumulator_register_from_size(size);
                    arm64_add__asm(instructions, mov, acc, lhs_s);
                }

                if (is_temporary(rhs)) {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        assembly::get_operand_size_from_size(
                            devices.get_size_from_rvalue_reference(lhs)));
                    rhs_s = devices.get_device_by_lvalue(lhs);
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
                rhs_s = devices.get_device_by_lvalue(rhs);

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
                lhs_s = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_storage(rhs_s));
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                rhs_s = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_storage(lhs_s));
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = devices.get_operand_rvalue_device(lhs);
                rhs_s = devices.get_operand_rvalue_device(rhs);
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
        return accessor_->table_accessor.table_->get_vectors().contains(
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
                            lvalue, device_accessor);
                assembly::inserter(instructions, vector_i);
                return get_operand_size_from_storage(
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
    auto frame = accessor_->table_accessor.table_->get_stack_frame();
    auto acc = accessor_->get_accumulator_with_rvalue_context(size);
    if (acc == Register::x23 or acc == Register::w23)
        frame->get_tokens().insert("x23");
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
        return accessor_->table_accessor.table_->get_vectors().contains(
            type::from_lvalue_offset(rvalue));
    };
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(rvalue)(
        m::pattern | m::app(is_address, true) =
            [&] {
                storage = devices.get_device_by_lvalue_reference(rvalue);
                insert_from_unary_operator_operands(op, storage, storage);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto [address, address_inst] =
                    address_space.get_arm64_lvalue_and_insertion_instructions(
                        rvalue, devices);
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
    auto frame = accessor_->table_accessor.table_->get_stack_frame();
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
                auto size =
                    accessor_->device_accessor.get_word_size_from_storage(dest);
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
                auto size =
                    accessor_->device_accessor.get_word_size_from_storage(dest);
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
    auto& table = accessor_->table_accessor.table_;
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
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_rvalue(ret->second);
    if (accessor_->device_accessor.is_doubleword_storage_size(immediate))
        arm64_add__asm(instructions, mov, x0, immediate);
    else
        arm64_add__asm(instructions, mov, w0, immediate);
}

/**
 * @brief Expression inserter from rvalue
 */
void Expression_Inserter::insert_from_temporary_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
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
                    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
                    stack_frame_.get_stack_frame()->get_tokens().insert("x26");
                    auto rhs_s = u32_int_immediate(
                        unary_inserter.from_lvalue_address_of_expression(
                            rvalue));
                    arm64_add__asm(instructions, add, x26, sp, rhs_s);
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
                if (get_operand_size_from_storage(immediate,
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
            lhs, accessor_->device_accessor);
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rhs, accessor_->device_accessor);
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    arm64_add__asm(instructions, ldr, acc, rhs_storage);
    arm64_add__asm(instructions, str, acc, lhs_storage);
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
            lhs, frame, accessor_->table_accessor.table_->get_vectors());
        auto size = assembly::get_operand_size_from_size(
            devices.get_size_from_rvalue_data_type(lhs, rvalue));
        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                size);
        Storage lhs_storage = devices.get_device_by_lvalue(lhs_lvalue);
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
    // swap operands for "dest, src" order"
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_x28_address_offset(operands.first) and
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
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto relational = Relational_Operator_Inserter{ accessor_ };
                auto& ir_instructions =
                    accessor_->table_accessor.table_->get_ir_instructions();
                auto ir_index = accessor_->table_accessor.index;
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
    auto is_stack = [&](Storage const& st) {
        return is_variant(assembly::Stack::Offset, st);
    };

    m::match(rhs)(
        // Translate from an immediate value assignment
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lhs, devices);
                assembly::inserter(instructions, storage_inst);
                if (is_stack(lhs_storage)) {
                    auto size = get_operand_size_from_storage(
                        lhs_storage, accessor_->stack);
                    auto work = size == Operand_Size::Doubleword
                                    ? assembly::Register::x8
                                    : assembly::Register::w8;
                    arm64_add__asm(instructions, mov, work, imm);
                    arm64_add__asm(instructions, str, work, lhs_storage);
                } else {
                    arm64_add__asm(instructions, mov, lhs_storage, imm);
                }
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
                        lhs, devices);
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    unary_op, lhs_storage);
            },
        // The expanded temporary rvalue is in a accumulator register,
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, devices);
                    arm64_add__asm(instructions, mov, lhs_storage, x26);
                } else {
                    auto frame = stack_frame_.get_stack_frame();
                    auto size = assembly::get_operand_size_from_size(
                        accessor_->table_accessor.table_
                            ->lvalue_size_at_temporary_object_address(
                                rhs, frame));
                    auto acc =
                        accumulator.get_accumulator_register_from_size(size);
                    if (!type::is_unary_expression(lhs))
                        accessor_->device_accessor.insert_lvalue_to_device(lhs);
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, devices);
                    assembly::inserter(instructions, storage_inst);
                    arm64_add__asm(instructions, mov, lhs_storage, acc);
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs, devices);
                assembly::inserter(instructions, lhs_inst);
                auto [rhs_storage, rhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        rhs, devices);
                assembly::inserter(instructions, rhs_inst);
                auto acc = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_lvalue(rhs));
                arm64_add__asm(instructions, mov, acc, rhs_storage);
                arm64_add__asm(instructions, mov, lhs_storage, acc);
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Get the storage device of a stack operand
 */
inline Storage Operand_Inserter::get_operand_storage_from_stack(
    RValue const& rvalue)
{
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, accessor_->device_accessor);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);
    return operand;
}

/**
 * @brief Get the storage device of a return rvalue in the stack frame
 */
inline Storage Operand_Inserter::get_operand_storage_from_return()
{
    auto& tail_call =
        accessor_->table_accessor.table_->get_functions()[stack_frame_.tail];
    if (tail_call->get_locals().is_pointer(tail_call->get_ret()->first) or
        type::is_rvalue_data_type_string(tail_call->get_ret()->first))
        return Register::x8;
    else
        return Register::w8;
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
            return direct_immediate("[x28]");
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
            auto offset_integer = type::integral_from_type_ulint(offset) + 1;
            return direct_immediate(
                fmt::format("[x28, #{}]", 8 * offset_integer));
        }
    }
    if (frame->is_pointer_parameter(rvalue))
        return memory::registers::available_doubleword.at(index_of);
    else
        return memory::registers::available_word.at(index_of);
}

/**
 * @brief Get the storage device of an rvalue operand
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    if (frame->is_parameter(rvalue))
        return get_operand_storage_from_parameter(rvalue);

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

    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, accessor_->device_accessor);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);
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
    auto& table = accessor_->table_accessor.table_;
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
            arguments.emplace_back(
                operands.get_operand_storage_from_rvalue(rvalue));
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
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);
    auto operands = this->get_operands_storage_from_argument_stack();

    syscall_ns::make_syscall(instructions, routine, operands, &accessor_);
}

/**
 * @brief Invocation inserter for user defined functions
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    for (std::size_t i = 0; i < operands.size(); i++) {
        auto const& operand = operands[i];
        auto size = get_operand_size_from_storage(operand, accessor_->stack);
        auto arg_register = assembly::get_register_from_integer_argument(i);
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(
                    std::get<Immediate>(operand))) {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Load,
                    accessor_->instruction_accessor->size());
            }
        }
        if (size == Operand_Size::Doubleword)
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Argument,
                accessor_->instruction_accessor->size());
        arm64_add__asm(instructions, mov, arg_register, operand);
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

    library_caller.make_library_call(
        instructions, routine, argument_stack, operands);
}

/**
 * @brief Arithmetic operator inserter for expression operands
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Assignment_Operands const& operands,
    std::string const& binary_op)
{
    auto frame = accessor_->table_accessor.table_->get_stack_frame();
    Instruction_Pair instructions{ Register::w8, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                if (is_variant(Immediate, operands.second))
                    frame->get_tokens().insert("x23");
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                if (is_variant(Immediate, operands.second))
                    frame->get_tokens().insert("x23");
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
                if (is_variant(Immediate, operands.second))
                    frame->get_tokens().insert("x23");
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
    if (accessor_->device_accessor.is_doubleword_storage_size(operands.first) or
        accessor_->device_accessor.is_doubleword_storage_size(
            operands.second)) {
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

// @TODO:

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

void Operand_Inserter::insert_from_string_address_operand(
    [[maybe_unused]] LValue const& lhs,
    [[maybe_unused]] Storage const& storage,
    [[maybe_unused]] RValue const& rhs)
{
}

void Expression_Inserter::insert_from_string([[maybe_unused]] RValue const& str)
{
}

void Expression_Inserter::insert_from_float([[maybe_unused]] RValue const& str)
{
}

void Expression_Inserter::insert_from_double([[maybe_unused]] RValue const& str)
{
}

} // namespace credence::target::arm64