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
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include "assembly.h"          // for Instructions, Storage
#include "credence/ir/table.h" // for Table
#include "credence/types.h"    // for Label
#include "memory.h"            // for Address_Accessor
#include "stack.h"             // for Stack
#include <array>               // for array
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
 *  A `printf' routine that takes a format string and up to 8 arguments
 *  %rdi is the variable size of arguments, up to 8
 *  %rsi is the format string
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

namespace detail {

void add_stdlib_function_to_table_symbols(std::string const& stdlib_function,
    util::AST_Node& symbols);

void add_syscall_functions_to_symbols(util::AST_Node& symbols);

} // namespace detail

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