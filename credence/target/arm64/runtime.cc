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
#include "credence/ir/object.h"                 // for get_rvalue_at_lvalue...
#include "credence/target/common/runtime.h"     // for library_list
#include "credence/target/common/stack_frame.h" // for Locals
#include "matchit.h"                            // for Or, Wildcard, pattern
#include "stack.h"                              // for Stack
#include <array>                                // for get, array
#include <credence/error.h>                     // for credence_assert
#include <credence/target/arm64/memory.h>       // for Register, Address_Ac...
#include <credence/target/common/assembly.h>    // for get_storage_as_string
#include <credence/target/common/types.h>       // for Immediate, Stack_Offset
#include <credence/types.h>                     // for get_type_from_rvalue...
#include <credence/util.h>                      // for contains, sv, __sour...
#include <deque>                                // for deque
#include <fmt/format.h>                         // for format
#include <stdexcept>                            // for out_of_range
#include <variant>                              // for get, monostate, visit

/****************************************************************************
 *
 * ARM64 Runtime and Standard Library Integration Implementation
 *
 * Handles function calls to the standard library and manages the ARM64 PCS
 * calling convention. Arguments passed in registers: x0-x7, then stack.
 * Return value in x0. x30 (lr) holds return address.
 *
 * Example - calling printf:
 *
 *   B code:    printf("Value: %d\n", x);
 *
 * Generates (x is local in x9):
 *   adrp x0, ._L_str1__@PAGE       ; format string in x0
 *   add x0, x0, ._L_str1__@PAGEOFF
 *   mov x1, x9                      ; x from register x9
 *   bl printf                       ; from stdlib
 *
 * Example - main with argc/argv:
 *
 *   B code:    main(argc, argv) { ... }
 *
 * Setup:
 *   x0 contains argc, x1 contains argv pointer
 *
 *****************************************************************************/

namespace credence::target::arm64::runtime {

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
                        lvalue, stack_frame, table->get_vectors(), __source__));
                is_buffer = type == "null" or type == "string";
            },
            [&](assembly::Register& device) {
                is_buffer = assembly::is_doubleword_register(device);
            },
            [&](common::Immediate& immediate) {
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "[sp]"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "sp,"))
                    is_buffer = true;
                if (util::contains(common::assembly::get_storage_as_string<
                                       assembly::Register>(immediate),
                        "._L"))
                    is_buffer = true;
                if (type::is_rvalue_data_type_string(immediate))
                    is_buffer = true;
            } },
        address);
    return is_buffer;
}

using library_register = std::deque<assembly::Register>;

/**
 * @brief Get the operand storage devices from the argument stack
 */
assembly::Register
Library_Call_Inserter::get_available_standard_library_register(
    std::deque<assembly::Register>& available_registers,
    common::memory::Locals& argument_stack,
    std::size_t index)
{
    auto address_accessor = accessor_->address_accessor;
    Register storage = Register::w0;
    try {
        if (address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "float") or
            address_accessor.is_lvalue_storage_type(
                argument_stack.at(index), "double")) {
            storage = vector_registers_.back();
            vector_registers_.pop_back();
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
    namespace m = matchit;
    m::match(arg_type)(
        m::pattern | m::or_(sv("string"), sv("float"), sv("double")) =
            [&] {
                auto immediate = type::get_value_from_rvalue_data_type(
                    std::get<Immediate>(argument));
                auto imm_1 =
                    direct_immediate(fmt::format("{}@PAGE", immediate));
                arm64_add__asm(instructions, adrp, storage, imm_1);
                auto imm_2 =
                    direct_immediate(fmt::format("{}@PAGEOFF", immediate));
                arm64_add__asm(instructions, add, storage, storage, imm_2);
            },
        m::pattern | m::_ =
            [&] {
                if (storage == assembly::Register::x26 and
                    *signal_register == assembly::Register::x26) {
                    *signal_register = assembly::Register::w0;
                    arm64_add__asm(instructions, mov, storage, x26);
                    return;
                }
                if (is_variant(Register, argument) and
                    assembly::is_word_register(std::get<Register>(argument))) {
                    auto storage_dword =
                        assembly::get_word_register_from_doubleword(storage);
                    arm64_add__asm(instructions, mov, storage_dword, argument);
                } else
                    arm64_add__asm(instructions, mov, storage, argument);
            });
}

std::pair<memory::registers::general_purpose,
    memory::registers::general_purpose>
get_argument_general_purpose_registers()
{
    std::deque<assembly::Register> d = { assembly::Register::x7,
        assembly::Register::x6,
        assembly::Register::x5,
        assembly::Register::x4,
        assembly::Register::x3,
        assembly::Register::x2,
        assembly::Register::x1,
        assembly::Register::x0 };

    std::deque<assembly::Register> w = { assembly::Register::w7,
        assembly::Register::w6,
        assembly::Register::w5,
        assembly::Register::w4,
        assembly::Register::w3,
        assembly::Register::w2,
        assembly::Register::w1,
        assembly::Register::w0 };

    return std::make_pair(d, w);
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

    library_call_argument_check(syscall_function, arguments, arg_size);

    auto [doubleword_storage, word_storage] =
        get_argument_general_purpose_registers();

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);
        Register storage{};
        std::string arg_type =
            locals.size() > i
                ? type::get_type_from_rvalue_data_type(locals.at(i))
                : "";

        auto float_size = vector_registers_.size();

        if (memory::is_doubleword_storage_size(
                arg, accessor_->stack, stack_frame_))
            storage = get_available_standard_library_register(
                doubleword_storage, locals, i);
        else
            storage = get_available_standard_library_register(
                word_storage, locals, i);

        insert_argument_instructions_standard_library_function(
            storage, instructions, arg_type, arg);
        if (float_size == vector_registers_.size()) {
            doubleword_storage.pop_back();
            word_storage.pop_back();
        }
    }
#if defined(__linux__)
    auto call_immediate =
        common::assembly::make_array_immediate(syscall_function);
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    auto call_immediate = common::assembly::make_array_immediate(
        fmt::format("_{}", syscall_function));
#endif
    arm64_add__asm(instructions, bl, call_immediate);
}

} // namespace credence::target::arm64::runtime
