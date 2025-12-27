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
#include "assembly.h"                        // for Register, operator<<
#include "stack.h"                           // for Stack
#include <array>                             // for get, array
#include <credence/error.h>                  // for credence_assert
#include <credence/target/common/assembly.h> // for get_storage_as_string
#include <credence/target/common/types.h>    // for Stack_Offset
#include <credence/target/x86_64/memory.h>   // for Register, Address_Accessor
#include <credence/types.h>                  // for get_type_from_rvalue_da...
#include <credence/util.h>                   // for contains, __source__
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <initializer_list>                  // for initializer_list
#include <stdexcept>                         // for out_of_range
#include <variant>                           // for get, monostate, visit

namespace credence::target::x86_64::runtime {

/**
 * @brief A compiletime check on a buffer allocation in a storage device
 */
bool Library_Call_Inserter::is_address_device_pointer_to_buffer(
    address_t& address,
    ir::object::Object_PTR& table,
    memory::Stack_Pointer& stack)
{
    auto stack_frame = table->get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](common::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                auto type = type::get_type_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
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
assembly::Register
Library_Call_Inserter::get_available_standard_library_register(
    std::deque<assembly::Register>& available_registers,
    common::memory::Locals& argument_stack,
    std::size_t index)
{
    Register storage = Register::eax;
    auto address_accessor = accessor_->address_accessor;
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
 * @brief Prepare registers for argument operand storage
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
            assembly::Mnemonic::movsd, storage, argument });
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
 * @brief General purpsoe register stack in ABI system V order
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
 * @brief Create the instructions for a standard library call
 */
void Library_Call_Inserter::make_library_call(Instructions& instructions,
    std::string_view syscall_function,
    common::memory::Locals& locals,
    library_arguments_t const& arguments)
{

    credence_assert(common::runtime::library_list.contains(syscall_function));
    auto [arg_size] = common::runtime::library_list.at(syscall_function);

    auto address_space = accessor_->address_accessor;

    library_call_argument_check(syscall_function, arguments, arg_size);

    auto [qword_storage, dword_storage] =
        get_argument_general_purpose_registers();

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);
        Register storage{};
        std::string arg_type =
            locals.size() > i
                ? type::get_type_from_rvalue_data_type(locals.at(i))
                : "";

        auto float_size = xmm_registers_.size();
        if (address_space.is_qword_storage_size(arg, stack_frame_))
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