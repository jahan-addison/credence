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

#include "assembly.h"           // for Arch_Type
#include "stack_frame.h"        // for Locals, Stack_Frame_T
#include "types.h"              // for Enum_T, Label, Stack_Pointer, Storage_T
#include <array>                // for array
#include <credence/error.h>     // for compile_error_impl, throw_compiletim...
#include <credence/ir/object.h> // for Object_PTR
#include <credence/util.h>      // for AST_Node, __source__, range_contains
#include <cstddef>              // for size_t
#include <deque>                // for deque
#include <easyjson.h>           // for object
#include <fmt/format.h>         // for format
#include <initializer_list>     // for initializer_list
#include <map>                  // for map
#include <source_location>      // for source_location
#include <string>               // for basic_string, string, char_traits
#include <string_view>          // for basic_string_view, string_view
#include <utility>              // for pair
#include <vector>               // for vector

namespace credence::target::common::runtime {

using library_t = std::array<std::size_t, 1>;
using library_list_t = std::map<std::string_view, library_t>;

/**
 * @brief
 *
 *  The Standard library
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
 *   Warning:
 *      Double and float specifiers "work" but not perfectly
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
 * ------------------------------------------------------------------------
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
 * @brief Throw a runtime error during code generation
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

std::vector<std::string> get_library_symbols();

/**
 * @brief Check if a label is available as a variadic library function
 */
constexpr bool is_variadic_library_function(std::string_view const& label)
{
    return util::range_contains(label, variadic_library_list);
}

template<Enum_T Registers, Stack_T Stack, Deque_T Instructions>
struct Library_Call_Inserter
{
    virtual ~Library_Call_Inserter() = default;

    virtual void make_library_call(Instructions& instructions,
        std::string_view syscall_function,
        memory::Locals& locals,
        library_arguments_t<Registers> const& arguments) = 0;

    virtual Registers get_available_standard_library_register(
        std::deque<Registers>& available_registers,
        memory::Locals& argument_stack,
        std::size_t index) = 0;

    virtual bool is_address_device_pointer_to_buffer(
        address_t<Registers>& address,
        ir::object::Object_PTR& table,
        Stack_Pointer<Stack>& stack) = 0;

    void library_call_argument_check(std::string_view library_function,
        library_arguments_t<Registers> const& arguments,
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
};

std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame);

} // namespace target::common::runtime
