/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#include "lib.h"
#include "instructions.h"   // for Register, get_storage_as_string, is_qwor...
#include "stack.h"          // for Stack
#include "syscall.h"        // for get_platform_syscall_symbols
#include <algorithm>        // for __find, find
#include <array>            // for array, get
#include <credence/error.h> // for assert_equal_impl, credence_assert, cred...
#include <credence/util.h>  // for contains, AST_Node, AST, overload
#include <easyjson.h>       // for JSON, object
#include <fmt/format.h>     // for format
#include <variant>          // for monostate, visit

namespace credence::target::x86_64::library {

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
 * @brief A compiletime check on a storage buffer
 */
bool is_address_device_pointer_to_buffer(Address& address,
    ir::Table::Table_PTR& table,
    x86_64::detail::Stack const& stack)
{
    bool is_buffer{ false };
    // clang-format off
    std::visit(util::overload{
        [&](std::monostate) {},
        [&](x86_64::detail::Stack_Offset const& offset) {
            auto lvalue = stack.get_lvalue_from_offset(offset);
            is_buffer = type::is_rvalue_data_type_string(
                table->get_rvalue_data_type_at_pointer(lvalue));
        },
        [&](x86_64::detail::Register& device) {
            is_buffer = x86_64::detail::is_qword_register(device);
        },
        [&](x86_64::detail::Immediate& immediate) {
            if (util::contains(
                    x86_64::detail::get_storage_as_string(immediate),
                    "rip +"))
                is_buffer = true;
            if (type::is_rvalue_data_type_string(immediate))
                is_buffer = true;
        }
    }, address);
    // clang-format on
    return is_buffer;
}

/**
 * @brief Create the instructions for a standard library call
 * In each case, %rdi is ommitted as it holds the first syscall number
 */
void make_library_call(Instructions& instructions,
    std::string_view library_function,
    library_arguments_t const& arguments)
{
    using namespace x86_64::detail;
    credence_assert(library_list.contains(library_function));
    auto [arg_size] = library_list.at(library_function);
    credence_assert_equal(arg_size, arguments.size());
    std::deque<Register> argument_storage = {
        Register::r9, Register::r8, Register::r10, Register::rdx, Register::rsi
    };
    for (auto const& arg : arguments) {
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg))
            instructions.emplace_back(
                x86_64::detail::Instruction{ Mnemonic::lea, storage, arg });
        else
            instructions.emplace_back(
                x86_64::detail::Instruction{ Mnemonic::movq_, storage, arg });
    }
    auto call_immediate =
        x86_64::detail::make_array_immediate(library_function);
    instructions.emplace_back(x86_64::detail::Instruction{
        Mnemonic::call, call_immediate, x86_64::detail::O_NUL });
}

} // namespace library