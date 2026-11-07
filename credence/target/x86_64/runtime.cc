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

#include "runtime.h"
#include "assembly.h"                           // for Register, operator<<
#include "memory.h"                             // for Register, Address_Ac...
#include "stack.h"                              // for Stack
#include <array>                                // for get, array
#include <credence/error.h>                     // for credence_assert
#include <credence/language/hir/binding.h>      // for get_rvalue_at_lvalue...
#include <credence/target/common/assembly.h>    // for get_storage_as_string
#include <credence/target/common/runtime.h>     // for library_list
#include <credence/target/common/stack_frame.h> // for Locals
#include <credence/target/common/types.h>       // for Stack_Offset
#include <credence/types.h>                     // for get_type_from_rvalue...
#include <credence/util.h>                      // for contains, __source__
#include <deque>                                // for deque
#include <fmt/format.h>                         // for format
#include <stdexcept>                            // for out_of_range
#include <variant>                              // for get, monostate, visit

/****************************************************************************
 * x86-64 Runtime and Standard Library
 *
 * Calls to the standard library under the System V ABI calling convention.
 * See common/runtime.h for the functions. The arguments are in rdi, rsi,
 * rdx, rcx, r8, and r9, or xmm0 to xmm7 for a float or double, and the
 * return value is in rax.
 *
 *  Example:
 *
 *    printf("%d\n", add(counter, 2));
 *
 *  Becomes:
 *
 *    lea rdi, [rip + ._L_str1__]      // the format string
 *    mov esi, dword ptr [rbp - 8]     // the return value of add
 *    call printf
 *
 ****************************************************************************/

namespace credence::target::x86_64::runtime {

/**
 * @brief Check at compile time that a storage device holds a buffer address
 *
 * A quadword register, a stack or rip relative address, or a string is a
 * buffer. A local on the stack is a buffer when it is a string or `null`,
 * and an argument stored before its call when it is a quadword.
 *
 *  Examples:
 *
 *    is_address_device_pointer_to_buffer(rdi)                  // true
 *    is_address_device_pointer_to_buffer(edi)                  // false
 *    is_address_device_pointer_to_buffer([rip + ._L_str1__])   // true
 */
bool Library_Call_Inserter::is_address_device_pointer_to_buffer(
    address_t& address)
{
    auto& table = accessor_->table_accessor.get_table();
    auto& stack = accessor_->stack;
    auto& stack_frame = table->get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](common::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                // an argument stored before its call is a qword
                if (common::memory::is_parameter(lvalue) or
                    type::is_temporary(lvalue)) {
                    is_buffer = stack->get_operand_size_from_offset(offset) ==
                                assembly::Operand_Size::Qword;
                    return;
                }
                auto type = type::get_type_from_rvalue_data_type(
                    language::hir::binding::get_rvalue_at_lvalue_object_storage(
                        lvalue, stack_frame, table->vectors, __source__));
                is_buffer = type == "null" or type == "string";
            },
            [&](assembly::Register& device) {
                is_buffer = assembly::is_qword_register(device);
            },
            [&](assembly::Immediate& immediate) {
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "[rsp]"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "rsp +"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "rip +"))
                    is_buffer = true;
                if (type::is_rvalue_data_type_string(immediate))
                    is_buffer = true;
            } },
        address);
    return is_buffer;
}

/**
 * @brief Get registers for argument storage
 */
/**
 * @brief Get the argument register of an argument of a library call
 *
 * A float or double is the next xmm register, and any other argument the
 * next register of its size
 */
assembly::Register
Library_Call_Inserter::get_available_standard_library_register(
    std::deque<assembly::Register>& available_registers,
    common::memory::Locals& argument_stack,
    std::size_t index)
{
    Register storage = Register::eax;
    auto& address_accessor = accessor_->address_accessor;
    try {
        if (address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "float") or
            address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "double")) {
            storage = xmm_registers_.back();
            xmm_registers_.pop_back();
        } else {
            storage = available_registers.back();
        }
    } catch ([[maybe_unused]] std::out_of_range const& e) {
        storage = available_registers.back();
    }
    return storage;
}

/**
 * @brief Load an argument of a library call into its register
 *
 * A string is loaded with `lea`, a float with `movss`, a double with
 * `movsd`, and any other argument is moved
 */
void Library_Call_Inserter::
    insert_argument_instructions_standard_library_function(Register storage,
        Instructions& instructions,
        std::string_view arg_type,
        address_t const& argument)
{
    auto* signal_register = accessor_->register_accessor.signal_register;
    if (arg_type == "string") {
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::lea, storage, argument });
    } else if (arg_type == "float") {
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::movss, storage, argument });
    } else if (arg_type == "double") {
        instructions.emplace_back(assembly::Instruction{
            assembly::Mnemonic::movsd, storage, argument });
    } else {
        if (storage == assembly::Register::rdi and
            *signal_register == assembly::Register::rcx) {
            *signal_register = assembly::Register::eax;
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::movq_, storage, assembly::Register::rcx });
            return;
        }
        // a char on the stack is zero extended into its register
        if (is_variant(common::Stack_Offset, argument) and
            accessor_->stack->get_operand_size_from_offset(
                std::get<common::Stack_Offset>(argument)) ==
                assembly::Operand_Size::Byte) {
            instructions.emplace_back(
                assembly::Instruction{ assembly::Mnemonic::movzx,
                    assembly::get_dword_register_from_qword(storage),
                    argument });
            return;
        }
        if (is_variant(Register, argument) and
            assembly::is_dword_register(std::get<Register>(argument))) {
            auto storage_as_string =
                common::assembly::get_storage_as_string<Register>(storage);
            auto storage_dword_offset = common::assembly::make_direct_immediate(
                fmt::format("dword ptr [{}]", storage_as_string));
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::movq_, storage_dword_offset, argument });
        } else
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::movq_, storage, argument });
    }
}

/**
 * @brief Get the System V argument registers, quadword and doubleword
 *
 * Each list is in reverse, so the next argument register is at its back
 */
std::pair<memory::registers::general_purpose,
    memory::registers::general_purpose>
get_argument_general_purpose_registers()
{
    std::deque<assembly::Register> qword = { assembly::Register::r9,
        assembly::Register::r8,
        assembly::Register::rcx,
        assembly::Register::rdx,
        assembly::Register::rsi,
        assembly::Register::rdi };

    std::deque<assembly::Register> dword = { assembly::Register::r9d,
        assembly::Register::r8d,
        assembly::Register::ecx,
        assembly::Register::edx,
        assembly::Register::esi,
        assembly::Register::edi };

    return std::make_pair(qword, dword);
}

/**
 * @brief Insert a call to a standard library function
 *
 * The argument count is checked, and each argument is loaded into the next
 * System V argument register of its size, or the next xmm register for a
 * float or double
 *
 *  Example:
 *
 *    printf("%d\n", x);
 *
 *  Becomes:
 *
 *    lea rdi, [rip + ._L_str1__]
 *    mov esi, dword ptr [rbp - 4]
 *    call printf
 */
void Library_Call_Inserter::make_library_call(Instructions& instructions,
    std::string_view syscall_function,
    library_arguments_t const& arguments)
{
    auto locals = stack_frame_.argument_stack;
    credence_assert(common::runtime::library_list.contains(syscall_function));
    auto [arg_size] = common::runtime::library_list.at(syscall_function);

    auto& address_space = accessor_->address_accessor;

    library_call_argument_check(syscall_function, arguments, arg_size);

    auto [qword_storage, dword_storage] =
        get_argument_general_purpose_registers();

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);
        Register storage{};
        // the type of a literal is only its storage while it is a label
        std::string arg_type =
            locals.size() > i and is_variant(Immediate, arg)
                ? type::get_type_from_rvalue_data_type(locals.at(i))
                : "";

        auto float_size = xmm_registers_.size();
        if (address_space.is_qword_storage_size(arg))
            storage = get_available_standard_library_register(
                qword_storage, locals, i);
        else
            storage = get_available_standard_library_register(
                dword_storage, locals, i);
        insert_argument_instructions_standard_library_function(
            storage, instructions, arg_type, arg);
        if (float_size == xmm_registers_.size()) {
            qword_storage.pop_back();
            dword_storage.pop_back();
        }
    }

    auto call_immediate =
        common::assembly::make_array_immediate(syscall_function);
    instructions.emplace_back(assembly::Instruction{
        assembly::Mnemonic::call, call_immediate, assembly::O_NUL });
}

} // namespace library