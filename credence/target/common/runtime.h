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

#pragma once

#include "stack_frame.h"    // for Locals
#include "types.h"          // for Enum_T, Label, Stack_Pointer, Storage_T
#include <array>            // for array
#include <credence/error.h> // for compile_error_impl, throw_compiletim...
#include <credence/language/hir/binding.h> // for Object_Table
#include <credence/util.h>  // for AST_Node, __source__, range_contains
#include <cstddef>          // for size_t
#include <deque>            // for deque
#include <easyjson.h>       // for object
#include <fmt/format.h>     // for format
#include <initializer_list> // for initializer_list
#include <map>              // for map
#include <source_location>  // for source_location
#include <string>           // for basic_string, string, char_traits
#include <string_view>      // for basic_string_view, string_view
#include <utility>          // for pair
#include <vector>           // for vector

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

using library_t = std::array<std::size_t, 1>;
using library_list_t = std::map<std::string_view, library_t>;

/**
 * @brief The standard library functions and their register argument count
 *
 * The count of `print` includes the length of its buffer, which is added at
 * compile time, and the count of a variadic function is its maximum
 */
inline const library_list_t library_list = {
    { "printf",  { 10 } },
    { "print",   { 2 }  },
    { "putchar", { 1 }  },
    { "getchar", { 0 }  }
};

constexpr auto variadic_library_list = { "printf" };

using library_t = std::array<std::size_t, 1>;
template<Enum_T R>
using address_t = Storage_T<R>;
using library_list_t = std::map<std::string_view, library_t>;
template<Enum_T R>
using library_arguments_t = std::deque<address_t<R>>;

/**
 * @brief Throw an error during code generation
 *
 * An error in the code generator is a program the target cannot run, such
 * as a `main` with more than `argc` and `argv`
 */
inline void throw_runtime_error(std::string_view message,
    std::string_view symbol,
    std::source_location const& location = std::source_location::current(),
    std::string_view type_ = "symbol",
    std::string_view scope = "main",
    util::AST_Node const& symbols = util::AST::object())
{
    credence::detail::compile_error_impl(location,
        fmt::format("{} in function '{}' runtime-error", message, scope),
        symbol,
        symbols,
        type_);
}

namespace detail {

void add_stdlib_function_to_table_symbols(std::string const& stdlib_function,
    util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type);

void add_syscall_functions_to_symbols(util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type);

} // namespace detail

void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type,
    bool with_syscalls = true);

bool is_syscall_function(Label const& label,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type);
bool is_stdlib_function(Label const& label,
    assembly::OS_Type os_type,
    assembly::Arch_Type arch_type);
bool is_library_function(Label const& label);
bool is_returning_library_function(Label const& label,
    assembly::Arch_Type arch_type);

std::vector<std::string> get_library_symbols();

/**
 * @brief Check if a label is available as a variadic library function
 */
constexpr bool is_variadic_library_function(std::string_view const& label)
{
    return util::range_contains(label, variadic_library_list);
}

/**
 * @brief Translate a call to a standard library function
 *
 * Each target loads the arguments into the registers of its calling
 * convention, and checks the argument count of each call
 */
template<Enum_T Registers, Stack_T Stack, Deque_T Instructions>
struct Library_Call_Inserter
{
    virtual ~Library_Call_Inserter() = default;

    virtual void make_library_call(Instructions& instructions,
        std::string_view syscall_function,
        library_arguments_t<Registers> const& arguments) = 0;

    Registers get_available_standard_library_register(
        std::deque<Registers>& available_registers,
        memory::Locals& argument_stack,
        std::size_t index);

    virtual bool is_address_device_pointer_to_buffer(
        address_t<Registers>& address) = 0;

    /**
     * @brief Check the argument count of a call to a library function
     *
     * A variadic function takes at most its count, and any other function
     * takes exactly its count
     */
    void library_call_argument_check(std::string_view library_function,
        library_arguments_t<Registers> const& arguments,
        std::size_t arg_size)
    {

        if (is_variadic_library_function(library_function)) {
            if (arguments.size() > arg_size)
                throw_compiletime_error(
                    fmt::format("too many arguments '{}' passed to variadic, "
                                "expected at most '{}' arguments",
                        arguments.size(),
                        arg_size),
                    library_function,
                    __source__,
                    "function invocation");
        } else {
            if (arguments.size() != arg_size)
                throw_compiletime_error(
                    fmt::format("invalid argument size '{}' passed to "
                                "function, expected '{}' arguments",
                        arguments.size(),
                        arg_size),
                    library_function,
                    __source__,
                    "function invocation");
        }
    }
};

std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame);

} // namespace target::common::runtime
