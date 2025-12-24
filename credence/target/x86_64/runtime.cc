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

#include <credence/ir/table.h> // for Table

#include "assembly.h"       // for Register, get_storage_as_string, is_qwor...
#include "stack.h"          // for Stack
#include "syscall.h"        // for get_platform_syscall_symbols
#include <algorithm>        // for __find, find
#include <array>            // for array, get
#include <credence/error.h> // for assert_equal_impl, credence_assert, cred...
#include <credence/util.h>  // for contains, AST_Node, AST, overload
#include <easyjson.h>       // for JSON, object
#include <fmt/format.h>     // for format
#include <variant>          // for monostate, visit

namespace credence::target::x86_64::runtime {

/**
 * @brief Check if a label is available as a syscall on the current
 * platform
 */
bool is_syscall_function(type::semantic::Label const& label)
{
    auto syscalls = syscall_ns::common::get_platform_syscall_symbols();
    return std::ranges::find(syscalls, label) != syscalls.end();
}

/**
 * @brief Check if a label is available as standard library method
 */

bool is_library_function(type::semantic::Label const& label)
{
    auto lib = get_library_symbols();
    return std::ranges::find(lib, label) != lib.end();
}

/**
 * @brief Check if a label is either a syscall or standard library
 * function on the current platform
 */
bool is_stdlib_function(type::semantic::Label const& label)
{
    return is_syscall_function(label) or is_library_function(label);
}

/**
 * @brief Get the list of all available standard library functions
 */
std::vector<std::string> get_library_symbols()
{
    std::vector<std::string> symbols{};
    // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
    for (auto const& libf : library_list) {
        // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
        symbols.emplace_back(libf.first);
    }
    return symbols;
}

/**
 * @brief Add a standard library function to the hoisted symbol table
 */
void detail::add_stdlib_function_to_table_symbols(
    std::string const& stdlib_function,
    util::AST_Node& symbols)
{
    if (!is_stdlib_function(stdlib_function))
        credence_error(
            fmt::format("Invalid stdlib function '{}'", stdlib_function));
    symbols[stdlib_function] = util::AST::object();
    auto& node = symbols[stdlib_function];
    node["type"] = "function_definition";
}

/**
 * @brief Add a syscall routine to the hoisted symbol table
 */
void detail::add_syscall_functions_to_symbols(util::AST_Node& symbols)
{
    auto syscalls = syscall_ns::common::get_platform_syscall_symbols();
    for (auto const& routine : syscalls)
        add_stdlib_function_to_table_symbols(routine, symbols);
}

/**
 * @brief Add the standard library and syscall routines to the hoisted
 * symbol table
 */
void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    bool with_syscalls)
{
    auto libcalls = get_library_symbols();
    for (auto const& f : libcalls)
        detail::add_stdlib_function_to_table_symbols(f, symbols);
    if (with_syscalls)
        detail::add_syscall_functions_to_symbols(symbols);
}

/**
 * @brief Compile-time check and stack frame argc argv existence flags
 */
std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame)
{
    credence_assert_equal(stack_frame.symbol, "main");
    auto entry_point = stack_frame.get_stack_frame("main");
    if (entry_point->parameters.size() > 2)
        throw_runtime_error("invalid argument count, expected at most two for "
                            "'argc' and 'argv'",
            "main",
            __source__,
            "program invocation");

    auto main_argc = entry_point->parameters.size() >= 1;
    auto main_argv = entry_point->parameters.size() == 2;

    return { main_argc, main_argv };
}

/**
 * @brief A compiletime check on a buffer allocation in a storage device
 */
bool is_address_device_pointer_to_buffer(Address& address,
    ir::object::Object_PTR& objects,
    std::shared_ptr<assembly::Stack>& stack)
{
    auto stack_frame = objects->get_stack_frame();
    bool is_buffer{ false };

    std::visit(
        util::overload{ [&](std::monostate) {},
            [&](assembly::Stack_Offset const& offset) {
                auto lvalue = stack->get_lvalue_from_offset(offset);
                auto type = type::get_type_from_rvalue_data_type(
                    ir::object::get_rvalue_at_lvalue_object_storage(
                        lvalue, stack_frame, objects->vectors, __source__));
                is_buffer = type == "null" or type == "string";
            },
            [&](assembly::Register& device) {
                is_buffer = assembly::is_qword_register(device);
            },
            [&](assembly::Immediate& immediate) {
                if (util::contains(
                        assembly::get_storage_as_string(immediate), "[rsp]"))
                    is_buffer = true;
                if (util::contains(
                        assembly::get_storage_as_string(immediate), "rsp +"))
                    is_buffer = true;
                if (util::contains(
                        assembly::get_storage_as_string(immediate), "rip +"))
                    is_buffer = true;
                if (type::is_rvalue_data_type_string(immediate))
                    is_buffer = true;
            } },
        address);
    return is_buffer;
}

/**
 * @brief Check the argument size by the standard library table in runtime.h
 */
void library_call_argument_check(std::string_view library_function,
    library_arguments_t const& arguments,
    std::size_t arg_size)
{

    if (is_variadic_library_function(library_function)) {
        if (arguments.size() > arg_size)
            throw_compiletime_error(
                fmt::format("too many arguments '{}' passed to varadic, "
                            "expected '{}' arguments",
                    arg_size,
                    arguments.size()),
                library_function,
                __source__,
                "function invocation");
    } else {
        if (arguments.size() != arg_size)
            throw_compiletime_error(
                fmt::format("invaloud argument size '{}' passed to function"
                            "expected '{}' arguments",
                    arg_size,
                    arguments.size()),
                library_function,
                __source__,
                "function invocation");
    }
}

using library_register = std::deque<assembly::Register>;

/**
 * @brief Get registers for argument storage
 */
assembly::Register get_available_standard_library_register(
    memory::detail::Address_Accessor& accessor,
    library_register& argument_registers,
    library_register& float_registers,
    memory::Stack_Frame::IR_Stack& argument_stack,
    std::size_t index)
{
    Register storage = Register::eax;
    try {
        if (accessor.is_lvalue_storage_type(
                argument_stack.at(index), "float") or
            accessor.is_lvalue_storage_type(
                argument_stack.at(index), "double")) {
            storage = float_registers.back();
            float_registers.pop_back();
        } else {
            storage = argument_registers.back();
            argument_registers.pop_back();
        }
    } catch ([[maybe_unused]] std::out_of_range const& e) {
        storage = argument_registers.back();
        argument_registers.pop_back();
    }
    return storage;
}

/**
 * @brief Prepare registers for argument operand storage
 */
void insert_argument_instructions_standard_library_function(
    Instructions& instructions,
    Register storage,
    std::string_view arg_type,
    Address const& argument,
    Register* signal_register)
{
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
            auto storage_as_string = assembly::get_storage_as_string(storage);
            auto storage_dword_offset = assembly::make_direct_immediate(
                fmt::format("dword ptr [{}]", storage_as_string));
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::movq_, storage_dword_offset, argument });
        } else
            instructions.emplace_back(assembly::Instruction{
                assembly::Mnemonic::movq_, storage, argument });
    }
}

/**
 * @brief Create the instructions for a standard library call
 */
void make_library_call(Instructions& instructions,
    std::string_view library_function,
    library_arguments_t const& arguments,
    memory::Stack_Frame::IR_Stack& argument_stack,
    memory::detail::Address_Accessor& address_space,
    assembly::Register* address_of)
{
    credence_assert(library_list.contains(library_function));
    auto [arg_size] = library_list.at(library_function);

    library_call_argument_check(library_function, arguments, arg_size);

    std::deque<assembly::Register> argument_storage = { assembly::Register::r9,
        assembly::Register::r8,
        assembly::Register::rcx,
        assembly::Register::rdx,
        assembly::Register::rsi,
        assembly::Register::rdi };

    std::deque<assembly::Register> float_storage = assembly::FLOAT_REGISTER;

    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments.at(i);

        std::string arg_type =
            argument_stack.size() > i
                ? type::get_type_from_rvalue_data_type(argument_stack.at(i))
                : "";

        auto storage = get_available_standard_library_register(
            address_space, argument_storage, float_storage, argument_stack, i);

        insert_argument_instructions_standard_library_function(
            instructions, storage, arg_type, arg, address_of);
    }

    auto call_immediate = assembly::make_array_immediate(library_function);
    instructions.emplace_back(assembly::Instruction{
        assembly::Mnemonic::call, call_immediate, assembly::O_NUL });
}

} // namespace library