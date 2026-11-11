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

#include "assembly.h"        // for get_os_type, Arch_Type
#include "easyjson.h"        // for JSON, object
#include "stack_frame.h"     // for Stack_Frame
#include "syscall.h"         // for get_platform_syscall_symbols
#include "types.h"           // for Label
#include <algorithm>         // for __find, find
#include <array>             // for array
#include <credence/error.h>  // for assert_equal_impl, credence_assert_e...
#include <credence/object.h> // for Function
#include <credence/types.h>  // for Label
#include <credence/util.h>   // for AST_Node, AST, __source__
#include <string>            // for basic_string, string, operator==
#include <string_view>       // for basic_string_view

/****************************************************************************
 * Standard Library
 *
 * The standard library functions and syscalls a program may call, and the
 * checks of their arguments. The functions do not depend on libc, and the
 * object file is in <root>/stdlib/<platform>/<os>/stdlib.o.
 *
 *  * printf - a format string and up to 8 arguments,
 *    "int=%d, float=%f, double=%g, string=%s, bool=%b, char=%c"
 *
 *  * print - a buffer address or string, with its length resolved at
 *    compile time
 *
 *  * putchar - writes a single byte character to stdout
 *
 *  * getchar - reads a single byte character from stdin
 *
 *  Example:
 *
 *    main() {
 *      auto *s;
 *      s = "hello";
 *      print(s);
 *    }
 *
 *  Becomes, on arm64:
 *
 *    ldr x0, [sp, #16]   // the buffer, on the stack in a frame with a call
 *    mov w1, #5          // its length, from the object table
 *    bl _print
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
 * @brief Check if a label is a function of the standard library object
 */
bool is_library_function(Label const& label)
{
    auto lib = get_library_symbols();
    return std::ranges::find(lib, label) != lib.end();
}

/**
 * @brief Check if `main` takes `argc`, and `argv`
 *
 * The kernel passes `argc` and `argv` to the entry point, in x0 and x1 on
 * arm64 Darwin and on the stack otherwise, so `main` takes at most two
 * parameters
 *
 *  Examples:
 *
 *    main(argc, argv)   // { true, true }
 *    main()             // { false, false }
 */
std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame)
{
    credence_assert_equal(stack_frame.symbol, "main");
    auto& entry_point = stack_frame.get_stack_frame("main");
    if (entry_point.parameters.size() > 2)
        throw_runtime_error("invalid argument count, expected at most two for "
                            "'argc' and 'argv'",
            "main",
            __source__,
            "program invocation");

    auto main_argc = entry_point.parameters.size() >= 1;
    auto main_argv = entry_point.parameters.size() == 2;

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
 * @brief Check if a call to the standard library or a syscall has a return
 * value, on the host platform
 *
 * `getchar` returns its character, and a syscall returns the result of the
 * kernel, each in the return register
 *
 *  Examples:
 *
 *    is_returning_library_function("getpid", Arch_Type::ARM64)   // true
 *    is_returning_library_function("printf", Arch_Type::ARM64)   // false
 */
bool is_returning_library_function(Label const& label,
    assembly::Arch_Type arch_type)
{
    return label == "getchar" or
           is_syscall_function(label, assembly::get_os_type(), arch_type);
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
    symbols[stdlib_function]["type"] = "function_definition";
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
    // a syscall returns the result of the kernel
    for (auto const& routine : syscalls) {
        add_stdlib_function_to_table_symbols(
            routine, symbols, os_type, arch_type);
        symbols[routine]["void"] = false;
    }
}

}
