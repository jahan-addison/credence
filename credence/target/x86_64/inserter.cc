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
#include "credence/target/common/accessor.h"    // for Buffer_Accessor
#include "credence/target/common/assembly.h"    // for direct_immediate
#include "credence/target/common/memory.h"      // for is_temporary, is_imm...
#include "credence/target/common/runtime.h"     // for is_stdlib_function
#include "credence/target/common/types.h"       // for Immediate, Table_Poi...
#include "credence/util.h"                      // for is_variant, sv, __so...
#include "memory.h"                             // for Memory_Accessor, Ins...
#include "runtime.h"                            // for Library_Call_Inserter
#include "stack.h"                              // for Stack
#include "syscall.h"                            // for syscall_arguments_t
#include "visitor.h"                            // for IR_Instruction_Visitor
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
 * x86-64 Instruction Inserters Implementation
 *
 * Translates B language operations into x86-64 instruction sequences.
 * Handles arithmetic, bitwise, relational operators, and assignments.
 *
 * Example - arithmetic operation:
 *
 *   B code:    z = x + y * 2;
 *
 * Inserter generates:
 *   mov eax, qword ptr [rbp - 8]  ; load y
 *   imul rax, 2                     ; y * 2
 *   mov ecx, dword ptr [rbp - 4]   ; load x
 *   add eax, ecx                    ; x + (y * 2)
 *   mov dword ptr [rbp - 12], eax  ; store to z
 *
 * Example - comparison:
 *
 *   B code:    if (x > 10) { ... }
 *
 * Inserter generates:
 *   mov eax, dword ptr [rbp - 4]
 *   cmp eax, 10
 *   jg ._L1__main
 *
 *****************************************************************************/

namespace credence::target::x86_64 {

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
 *
 * @brief Setup the stack frame for a function during instruction insertion
 *
 *  Note: %r15 is reserved for the argc address and argv offsets in memory
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
    if (name == "main") {
        // setup argc, argv
        auto argc_argv =
            common::runtime::argc_argv_kernel_runtime_access(stack_frame);
        if (argc_argv.first) {
            auto& instructions =
                accessor_->instruction_accessor->get_instructions();
            auto argc_address = direct_immediate("[rsp]");
            x8664_add__asm(instructions, lea, r15, argc_address);
        }
    }
    visitor.from_func_start_ita(name);
}

/**
 * @brief IR instruction visitor to map x64 instructions in memory
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
 * @brief Get the storage devices of arguments on the stack from ir::Table
 */
syscall_ns::syscall_arguments_t
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
                arguments.emplace_back(Register::rax);
            else
                arguments.emplace_back(Register::eax);
        } else {
            arguments.emplace_back(
                operands.get_operand_storage_from_rvalue(rvalue));
        }
    }
    return arguments;
}

/**
 * @brief Operand inserter for immediate rvalue operand types
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
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(assembly::x8664_is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    x8664_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
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
            return direct_immediate("[r15]");
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
                fmt::format("[r15 + 8 * {}]", offset_integer));
        }
    }
    if (frame->is_pointer_parameter(rvalue))
        return memory::registers::available_qword_register.at(index_of);
    else
        return memory::registers::available_dword_register.at(index_of);
}

/**
 * @brief Get the storage device of a stack operand
 */
inline Storage Operand_Inserter::get_operand_storage_from_stack(
    RValue const& rvalue)
{
    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
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
        return Register::rax;
    else
        return Register::eax;
}

/**
 * @brief Get the storage device of an immediate operand
 */
Storage Operand_Inserter::get_operand_storage_from_immediate(
    RValue const& rvalue)
{
    assembly::Storage storage{};
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
 * @brief Get the storage device of an rvalue operand
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto& stack = accessor_->stack;
    auto frame = stack_frame_.get_stack_frame();

    if (frame->is_parameter(rvalue))
        return get_operand_storage_from_parameter(rvalue);

    if (stack->is_allocated(rvalue))
        return get_operand_storage_from_stack(rvalue);

#if defined(CREDENCE_TEST) || defined(__linux__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();

#elif defined(__APPLE__) || defined(__bsdi__)
    if (!stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);

    return operand;
}

/**
 * @brief Invocation inserter for kernel syscall function call
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    assembly::Instructions& instructions)
{
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);
    auto operands = get_operands_storage_from_argument_stack();
    syscall_ns::common::make_syscall(
        instructions, routine, operands, &stack_frame_, &accessor_);
}

/**
 * @brief Invocation inserter for user defined functions and arguments
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    for (auto const& operand : operands) {
        auto size = get_operand_size_from_storage(operand, accessor_->stack);
        auto storage = accessor_->register_accessor.get_available_register(
            size, accessor_->stack);
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(
                    std::get<Immediate>(operand))) {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Load,
                    accessor_->instruction_accessor->size());
            }
        }
        if (size == Operand_Size::Qword)
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Argument,
                accessor_->instruction_accessor->size());
        x8664_add__asm(instructions, mov, storage, operand);
    }
    auto immediate = direct_immediate(routine);
    x8664_add__asm(instructions, call, immediate);
}

/**
 * @brief Invocation inserter for standard library functions and arguments
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    assembly::Instructions& instructions)
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
    operands.emplace_back(u32_int_immediate(buffer_size));
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
 * @brief Insert into a storage device from the %rip offset address of a string
 */
void Operand_Inserter::insert_from_string_address_operand(LValue const& lhs,
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
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::rcx);
    x8664_add__asm(instructions, mov, storage, rcx);
}

/**
 * @brief Insert into a storage device from the %rip offset address of a float
 */
void Operand_Inserter::insert_from_float_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_float(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
}

/**
 * @brief Insert into a storage device from the %rip offset address of a double
 */
void Operand_Inserter::insert_from_double_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_double(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
}

/**
 * @brief Mnemonic operand default inserter via pattern matching
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

    auto is_address = [&](RValue const& rvalue) {
        return stack->is_allocated(rvalue);
    };

    m::match(rhs)(
        // Translate from an immediate value assignment
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
                            lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                if (type::get_type_from_rvalue_data_type(imm) == "string")
                    insert_from_string_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "float")
                    insert_from_float_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "double")
                    insert_from_double_address_operand(lhs, lhs_storage, rhs);
                else
                    x8664_add__asm(instructions, mov, lhs_storage, imm);
            },
        // the expanded temporary rvalue is in a accumulator register,
        // use it
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    Storage lhs_storage = stack->get(lhs).first;
                    x8664_add__asm(
                        instructions, mov, lhs_storage, Register::rcx);
                } else {
                    auto acc = accumulator.get_accumulator_register_from_size();
                    if (!type::is_unary_expression(lhs))
                        stack->set_address_from_accumulator(lhs, acc);
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_lvalue_address_and_insertion_instructions(
                                lhs, instruction_accessor->size());
                    assembly::inserter(instructions, storage_inst);
                    x8664_add__asm(instructions, mov, lhs_storage, acc);
                }
            },
        // Local-to-local variable assignment
        m::pattern | m::app(is_address, true) =
            [&] {
                credence_assert(stack->get(rhs).second != Operand_Size::Empty);
                Storage lhs_storage = stack->get(lhs).first;
                Storage rhs_storage = stack->get(rhs).first;
                auto acc = accumulator.get_accumulator_register_from_size(
                    stack->get(rhs).second);
                x8664_add__asm(instructions, mov, acc, rhs_storage);
                x8664_add__asm(instructions, mov, lhs_storage, acc);
            },
        // Unary expression assignment, including pointers to an address
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto [lhs_storage, storage_inst] =
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
                            lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    unary_op, lhs_storage);
            },
        // Translate from binary expressions in the IR
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { binary_inserter.from_binary_operator_expression(rhs); },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Unary temporary expression inserter
 *
 * See ir/temporary.h for details
 */
Storage Unary_Operator_Inserter::insert_from_unary_operator_rvalue(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_space = accessor_->address_accessor;
    auto& table_accessor = accessor_->table_accessor;
    auto& register_accessor = accessor_->register_accessor;

    credence_assert(type::is_unary_expression(expr));

    Storage storage{};

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto is_vector = [&](RValue const& rvalue) {
        return table_accessor.table_->get_vectors().contains(
            type::from_lvalue_offset(rvalue));
    };

    if (stack->contains(rvalue)) {
        // This is the address-of operator, use a qword size register
        if (op == "&") {
            address_space.address_ir_assignment = true;
            insert_from_unary_operator_operands(op, stack->get(rvalue).first);
            accessor_->set_signal_register(Register::rcx);
            return Register::rcx;
        }
        auto size = stack->get(rvalue).second;
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, stack->get(rvalue).first);
        insert_from_unary_operator_operands(op, storage);
    } else if (is_vector(rvalue)) {
        auto [address, address_inst] =
            address_space.get_lvalue_address_and_insertion_instructions(
                rvalue, false);
        assembly::inserter(instructions, address_inst);
        auto size = get_operand_size_from_storage(address, stack);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        address_space.address_ir_assignment = true;
        accessor_->set_signal_register(Register::rax);
        insert_from_unary_operator_operands(op, storage, address);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = assembly::get_operand_size_from_rvalue_datatype(immediate);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, immediate);
        insert_from_unary_operator_operands(op, storage);
    }
    return storage;
}

/**
 * @brief Expression inserter from string rvalue to an .asciz directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_string(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_string(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_string_address_offset(
            str));
    x8664_add__asm(instructions, lea, rcx, location);
}

/**
 * @brief Expression inserter from float rvalue to a .float directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_float(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_float(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_float_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
}

/**
 * @brief Expression inserter from double rvalue to a .double directive
 *
 * The buffer_accessor holds the %rip offset in the data section
 */
void Expression_Inserter::insert_from_double(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_double(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_double_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
}

/**
 * @brief Binary operator inserter of expression operands
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& table_accessor = accessor_->table_accessor;
    auto registers = accessor_->register_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    Storage lhs_s{};
    Storage rhs_s{};

    Operand_Inserter operand_inserter{ accessor_ };
    auto expression = type::from_rvalue_binary_expression(rvalue);
    auto [lhs, rhs, op] = expression;
    auto immediate = false;
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->stack->is_allocated(rvalue);
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
                    lhs_s = registers.get_available_register(
                        stack->get(lhs).second, stack);
                    x8664_add__asm(
                        instructions, mov, lhs_s, stack->get(lhs).first);
                    rhs_s = stack->get(rhs).first;
                } else {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    x8664_add__asm(
                        instructions, mov, lhs_s, stack->get(lhs).first);
                    rhs_s = stack->get(rhs).first;
                }
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, true)) =
            [&] {
                auto size = assembly::get_operand_size_from_register(
                    accumulator.get_accumulator_register_from_size());
                auto acc = accumulator.get_accumulator_register_from_size();
                lhs_s = acc;
                auto& immediate_stack =
                    accessor_->address_accessor.immediate_stack;
                if (!immediate_stack.empty()) {
                    rhs_s = immediate_stack.back();
                    immediate_stack.pop_back();
                    if (!immediate_stack.empty()) {
                        x8664_add__asm(
                            instructions, mov, acc, immediate_stack.back());
                        immediate_stack.pop_back();
                    }
                } else {
                    auto intermediate =
                        registers.get_second_register_from_size(size);
                    rhs_s = intermediate;
                }
            },
        m::pattern |
            m::ds(m::app(is_address, true), m::app(is_address, false)) =
            [&] {
                lhs_s = stack->get(lhs).first;
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    x8664_add__asm(
                        instructions, mov, acc, stack->get(lhs).first);
                }
                if (is_temporary(rhs)) {
                    lhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(lhs).second);
                    rhs_s = stack->get(lhs).first;
                }

                if (table_accessor.is_ir_instruction_temporary()) {
                    if (type::is_bitwise_binary_operator(op)) {
                        auto storage = registers.get_available_register(
                            stack->get(lhs).second, stack);
                        x8664_add__asm(
                            instructions, mov, storage, stack->get(lhs).first);
                        lhs_s = storage;
                    } else {
                        if (!type::is_relation_binary_operator(op))
                            lhs_s = accumulator
                                        .get_accumulator_register_from_storage(
                                            lhs_s, stack);
                    }
                }
            },
        m::pattern |
            m::ds(m::app(is_address, false), m::app(is_address, true)) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = stack->get(rhs).first;
                if (table_accessor.last_ir_instruction_is_assignment()) {
                    auto acc = accumulator.get_accumulator_register_from_size(
                        stack->get(rhs).second);
                    x8664_add__asm(
                        instructions, mov, acc, stack->get(rhs).first);
                }
                if (is_temporary(lhs) or
                    table_accessor.is_ir_instruction_temporary())
                    rhs_s = accumulator.get_accumulator_register_from_size(
                        stack->get(rhs).second);
            },
        m::pattern |
            m::ds(m::app(is_temporary, true), m::app(is_temporary, false)) =
            [&] {
                lhs_s = accumulator.get_accumulator_register_from_size();
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
            },
        m::pattern |
            m::ds(m::app(is_temporary, false), m::app(is_temporary, true)) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = accumulator.get_accumulator_register_from_size();
            },
        m::pattern | m::ds(m::_, m::_) =
            [&] {
                lhs_s = registers.get_register_for_binary_operator(lhs, stack);
                rhs_s = registers.get_register_for_binary_operator(rhs, stack);
            });
    if (!immediate) {
        auto operand_inserter = Operand_Inserter{ accessor_ };
        assembly::Binary_Operands operands = { lhs_s, rhs_s };
        operand_inserter.insert_from_binary_operands(operands, op);
    }
}

/**
 * @brief Operand Inserter mediator for expression mnemonics operands
 */
void Operand_Inserter::insert_from_binary_operands(
    assembly::Binary_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_r15_address_offset(operands.first) and
        not assembly::is_immediate_rip_address_offset(operands.first))
        std::swap(operands.first, operands.second);
    if (type::is_binary_arithmetic_operator(op)) {
        auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            arithmetic.from_arithmetic_expression_operands(operands, op)
                .second);
    } else if (type::is_relation_binary_operator(op)) {
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
    } else if (type::is_bitwise_binary_operator(op)) {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            bitwise.from_bitwise_expression_operands(operands, op).second);
    } else if (type::is_unary_expression(op)) {
        auto unary = Unary_Operator_Inserter{ accessor_ };
        unary.insert_from_unary_operator_operands(op, operands.first);
    } else {
        credence_error(fmt::format("unreachable: operator '{}'", op));
    }
}

/**
 * @brief Inserter from rvalue at a temporary lvalue location
 */
void Expression_Inserter::insert_lvalue_at_temporary_object_address(
    LValue const& lvalue)
{
    auto frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.table_;
    auto temporary = table->lvalue_at_temporary_object_address(lvalue, frame);
    insert_from_rvalue(temporary);
}

/**
 * @brief Inserter from ir unary expression types
 */
void Unary_Operator_Inserter::insert_from_unary_operator_operands(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
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
                assembly::inserter(instructions, assembly::b_not(dest).second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Address, index);
                auto acc = Register::rcx;
                if (src != assembly::O_NUL)
                    assembly::inserter(
                        instructions, assembly::lea(dest, src).second);
                else
                    assembly::inserter(
                        instructions, assembly::lea(acc, dest).second);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   dest, accessor_->stack);
                x8664_add__asm(instructions, mov, acc, dest);
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Indirect, index);
                x8664_add__asm(instructions, mov, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(instructions, assembly::neg(dest).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Expression inserter of rvalue expressions and rvalue references
 */
void Expression_Inserter::insert_from_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.table_;
    auto& instructions = instruction_accessor->get_instructions();

    auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };

    auto is_comparator = [](RValue const& rvalue) {
        return rvalue.starts_with("CMP");
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

    m::match(rvalue)(
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] { binary_inserter.from_binary_operator_expression(rvalue); },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] { unary_inserter.insert_from_unary_operator_rvalue(rvalue); },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                Storage immediate =
                    operand_inserter.get_operand_storage_from_rvalue(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                x8664_add__asm(instructions, mov, acc, immediate);
                auto type = type::get_type_from_rvalue_data_type(rvalue);
                if (type == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Address, instruction_accessor->size());
            },
        m::pattern | m::app(is_comparator, true) =
            [&] {
                // @TODO nice-to-have, but not necessary
            },
        m::pattern | RValue{ "RET" } =
            [&] {
                if (is_stdlib_function(stack_frame_.tail))
                    return;
                credence_assert(
                    table->get_functions().contains(stack_frame_.tail));
                auto frame = table->get_functions()[stack_frame_.tail];
                credence_assert(frame->get_ret().has_value());
                auto immediate =
                    operand_inserter.get_operand_storage_from_rvalue(
                        frame->get_ret()->first);
                if (get_operand_size_from_storage(
                        immediate, accessor_->stack) == Operand_Size::Qword) {
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::QWord_Dest, instruction_accessor->size());
                    accessor_->set_signal_register(Register::rax);
                }
            },
        m::pattern | m::_ =
            [&] {
                auto symbols = table->get_stack_frame_symbols();
                Storage immediate = symbols.get_symbol_by_name(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                x8664_add__asm(instructions, mov, acc, immediate);
            });
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
    if (get_operand_size_from_storage(immediate, accessor_->stack) ==
        Operand_Size::Qword)
        x8664_add__asm(instructions, mov, rax, immediate);
    else
        x8664_add__asm(instructions, mov, eax, immediate);
}

/**
 * @brief Inserter for vector assignment between global vectors
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                lhs, instruction_accessor->size());
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rhs, instruction_accessor->size());
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    x8664_add__asm(instructions, mov, acc, rhs_storage);
    x8664_add__asm(instructions, mov, lhs_storage, acc);
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

    auto& stack = accessor_->stack;
    auto registers = accessor_->register_accessor;

    auto& table = accessor_->table_accessor.table_;
    auto lhs_op = type::get_unary_operator(lhs);
    auto rhs_op = type::get_unary_operator(rhs);

    auto frame = table->get_functions()[stack_frame_.symbol];
    auto& locals = table->get_stack_frame_symbols();

    m::match(lhs_op, rhs_op)(m::pattern | m::ds("*", "*") = [&] {
        credence_assert_nequal(
            stack->get(lhs_lvalue).second, Operand_Size::Empty);
        credence_assert_nequal(
            stack->get(rhs_lvalue).second, Operand_Size::Empty);

        auto acc =
            accessor_->accumulator_accessor.get_accumulator_register_from_size(
                Operand_Size::Qword);
        Storage lhs_storage = stack->get(lhs_lvalue).first;
        Storage rhs_storage = stack->get(rhs_lvalue).first;
        auto size = assembly::get_operand_size_from_type(
            type::get_type_from_rvalue_data_type(locals.get_symbol_by_name(
                locals.get_pointer_by_name(lhs_lvalue))));
        auto temp = registers.get_second_register_from_size(size);

        x8664_add__asm(instructions, mov, acc, rhs_storage);
        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Indirect_Source | common::flag::Address,
            accessor_->instruction_accessor->size());
        x8664_add__asm(instructions, mov, temp, acc);
        x8664_add__asm(instructions, mov, acc, lhs_storage);

        accessor_->flag_accessor.set_instruction_flag(
            common::flag::Indirect, instruction_accessor->size());

        x8664_add__asm(instructions, mov, acc, temp);
    });
}

/**
 * @brief Inserter of arithmetic expressions and their storage device
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                auto storage =
                    accessor_->register_accessor.get_available_register(
                        Operand_Size::Dword, accessor_->stack);
                x8664_add__asm(
                    instructions.second, mov, storage, operands.first);
                instructions = assembly::div(storage, operands.second);
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
                auto storage =
                    accessor_->register_accessor.get_available_register(
                        Operand_Size::Dword, accessor_->stack);
                accessor_->set_signal_register(Register::edx);
                x8664_add__asm(
                    instructions.second, mov, storage, operands.first);
                instructions = assembly::mod(storage, operands.second);
            });
    return instructions;
}

/**
 * @brief Inserter of bitwise expressions and their storage device
 */
Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "<<" } =
            [&] {
                instructions =
                    assembly::lshift(operands.first, operands.second);
            },
        m::pattern | std::string{ ">>" } =
            [&] {
                instructions =
                    assembly::rshift(operands.first, operands.second);
            },
        m::pattern | std::string{ "^" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   operands.first, accessor_->stack);
                instructions = assembly::b_xor(acc, operands.second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                instructions = assembly::b_and(operands.first, operands.second);
            },
        m::pattern | std::string{ "|" } =
            [&] {
                instructions = assembly::b_or(operands.first, operands.second);
            });
    return instructions;
}

/**
 * @brief Inserter of relational expressions and their storage device
 */
assembly::Instructions
Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::eax;
    if (accessor_->address_accessor.is_qword_storage_size(
            operands.first, stack_frame_) or
        accessor_->address_accessor.is_qword_storage_size(
            operands.second, stack_frame_)) {
        register_storage = Register::rax;
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
        m::pattern | m::_ = [&] { return assembly::Instructions{}; });
}

}