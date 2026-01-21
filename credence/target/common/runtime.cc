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

#include "credence/ir/object.h" // for Function
#include "easyjson.h"           // for JSON, object
#include "stack_frame.h"        // for Stack_Frame
#include "syscall.h"            // for get_platform_syscall_symbols
#include "types.h"              // for Label
#include <algorithm>            // for __find, find
#include <array>                // for array
#include <credence/error.h>     // for assert_equal_impl, credence_assert_e...
#include <credence/types.h>     // for Label
#include <credence/util.h>      // for AST_Node, AST, __source__
#include <string>               // for basic_string, string, operator==
#include <string_view>          // for basic_string_view

/****************************************************************************
 *
 *  | The Standard library |
 *
 *  The object file may be found in <root>/stdlib/<platform>/<os>/stdlib.o
 *
 * ------------------------------------------------------------------------
 *
 * printf(9):
 *
 *  A `printf' routine that takes a format string and up to 8 variadic arguments
 *   Formatting:
 *     "int=%d, float=%f, double=%g, string=%s, bool=%b, char=%c"
 *
 * print(1):
 *
 *  A `print' routine that is type safe for buffer addresses and strings
 *
 *  The length of the buffer is resolved at compiletime
 *
 * putchar(1):
 *
 *  A `putchar' routine that writes to stdout for single byte characters
 *
 * getchar(1):
 *
 *  A `getchar' routine that reads from stdin for single byte characters
 *
 * Example:
 *
 *   main(argc, argv) {
 *     if (argc > 1) {
 *       auto x;
 *       x = getchar();
 *       printf("Hello, %s! Your character is: %c", argv[1], x);
 *
 *     }
 *   }
 *
 ****************************************************************************/

namespace credence::target::common::assembly {
enum class Arch_Type;
enum class OS_Type;
}

namespace credence::target::common::runtime {

/**
 * @brief Check if a label is available as a syscall on the current
 * platform
 */
bool is_syscall_function(Label const& label,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type)
{
    auto syscalls =
        syscall_ns::get_platform_syscall_symbols(os_type, arch_type);
    return std::ranges::find(syscalls, label) != syscalls.end();
}

/**
 * @brief Check if a label is available as standard library method
 */

bool is_library_function(Label const& label)
{
    auto lib = get_library_symbols();
    return std::ranges::find(lib, label) != lib.end();
}

std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame)
{
    credence_assert_equal(stack_frame.symbol, "main");
    auto entry_point = stack_frame.get_stack_frame("main");
    if (entry_point->get_parameters().size() > 2)
        throw_runtime_error("invalid argument count, expected at most two for "
                            "'argc' and 'argv'",
            "main",
            __source__,
            "program invocation");

    auto main_argc = entry_point->get_parameters().size() >= 1;
    auto main_argv = entry_point->get_parameters().size() == 2;

    return { main_argc, main_argv };
}

/**
 * @brief Check if a label is either a syscall or standard library
 * function on the current platform
 */
bool is_stdlib_function(type::semantic::Label const& label,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type)
{
    return is_syscall_function(label, os_type, arch_type) or
           is_library_function(label);
}

/**
 * @brief Get the list of all available standard library functions
 */
std::vector<std::string> get_library_symbols()
{
    std::vector<std::string> symbols{};
    // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
    for (auto const& libf : runtime::library_list) {
        // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
        symbols.emplace_back(libf.first);
    }
    return symbols;
}

/**
 * @brief Add the standard library and syscall routines to the hoisted
 * symbol table
 */
void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type,
    bool with_syscalls)
{
    auto libcalls = get_library_symbols();
    for (auto const& f : libcalls)
        detail::add_stdlib_function_to_table_symbols(
            f, symbols, os_type, arch_type);
    if (with_syscalls)
        detail::add_syscall_functions_to_symbols(symbols, os_type, arch_type);
}

/**
 * @brief Add a standard library function to the hoisted symbol table
 */
void detail::add_stdlib_function_to_table_symbols(
    std::string const& stdlib_function,
    util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type)
{
    if (!is_stdlib_function(stdlib_function, os_type, arch_type))
        credence_error(
            fmt::format("Invalid stdlib function '{}'", stdlib_function));
    symbols[stdlib_function] = util::AST::object();
    auto& node = symbols[stdlib_function];
    node["type"] = "function_definition";
}

/**
 * @brief Add a syscall routine to the hoisted symbol table
 */
void detail::add_syscall_functions_to_symbols(util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type)
{
    auto syscalls =
        syscall_ns::get_platform_syscall_symbols(os_type, arch_type);
    for (auto const& routine : syscalls)
        add_stdlib_function_to_table_symbols(
            routine, symbols, os_type, arch_type);
}

}
