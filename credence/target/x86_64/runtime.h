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

#include "assembly.h"          // for Instructions, Storage
#include "credence/ir/table.h" // for Table
#include "credence/types.h"    // for Label
#include "memory.h"            // for Address_Accessor
#include "stack.h"             // for Stack
#include <array>               // for array
#include <credence/error.h>    // for compile_error_impl
#include <credence/util.h>     // for AST_Node
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <map>                 // for map
#include <source_location>     // for source_location
#include <string>              // for string
#include <string_view>         // for basic_string_view, string_view
#include <vector>              // for vector

namespace credence::target::x86_64::runtime {

using Instructions = x86_64::assembly::Instructions;

using library_t = std::array<std::size_t, 1>;
using Address = x86_64::assembly::Storage;
using library_list_t = std::map<std::string_view, library_t>;
using library_arguments_t = std::deque<Address>;

std::vector<std::string> get_library_symbols();

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
 *  %rdi is the format string
 *  Float and double arguments are in xmm0-xmm7
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

const auto variadic_library_list = { "printf" };

const library_list_t library_list = {
    { "printf",  { 10 } },
    { "print",   { 2 }  },
    { "putchar", { 1 }  },
    { "getchar", { 0 }  }
};

bool is_syscall_function(type::semantic::Label const& label);
bool is_library_function(type::semantic::Label const& label);
bool is_stdlib_function(type::semantic::Label const& label);

/**
 * @brief Check if a label is available as a variadic library function
 */
constexpr bool is_variadic_library_function(std::string_view const& label)
{
    return util::range_contains(label, variadic_library_list);
}

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
    util::AST_Node& symbols);

void add_syscall_functions_to_symbols(util::AST_Node& symbols);

} // namespace detail

std::pair<bool, bool> argc_argv_kernel_runtime_access(
    memory::Stack_Frame& stack_frame);

bool is_address_device_pointer_to_buffer(Address& address,
    ir::object::Object_PTR& table,
    std::shared_ptr<assembly::Stack>& stack);

void make_library_call(Instructions& instructions,
    std::string_view libary_function,
    library_arguments_t const& arguments,
    memory::Stack_Frame::IR_Stack& argument_stack,
    memory::detail::Address_Accessor& address_space,
    assembly::Register* address_of);

void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    bool with_syscalls = true);

} // namespace library