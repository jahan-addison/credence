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

#include "credence/ir/table.h" // for Table
#include "credence/types.h"    // for Label
#include "instructions.h"      // for Instructions, Storage
#include <array>               // for array
#include <credence/util.h>     // for AST_Node
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <map>                 // for map
#include <string>              // for string
#include <string_view>         // for basic_string_view, string_view
#include <vector>              // for vector
namespace credence::target::x86_64::detail {
class Stack;
}

namespace credence::target::x86_64::library {

using Instructions = x86_64::detail::Instructions;

using library_t = std::array<std::size_t, 1>;
using Address = x86_64::detail::Storage;
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
 * print(1):
 *
 *  A `print' routine that is type safe for all strings
 *
 *  The length of the buffer is provided at compiletime
 *
 * putchar(1):
 *
 *  A `putchar' routine that is type safe for all byte characters
 *
 * ------------------------------------------------------------------------
 */
const library_list_t library_list = {
    { "print",   { 2 } },
    { "putchar", { 1 } }
};

bool is_syscall_function(type::semantic::Label const& label);
bool is_library_function(type::semantic::Label const& label);
bool is_stdlib_function(type::semantic::Label const& label);

namespace detail {

void add_stdlib_function_to_table_symbols(std::string const& stdlib_function,
    util::AST_Node& symbols);

void add_syscall_functions_to_symbols(util::AST_Node& symbols);

} // namespace detail

bool is_address_device_pointer_to_buffer(Address& address,
    ir::Table::Table_PTR& table,
    x86_64::detail::Stack const& stack);

void make_library_call(Instructions& instructions,
    std::string_view libary_function,
    library_arguments_t const& arguments);

void add_stdlib_functions_to_symbols(util::AST_Node& symbols,
    bool with_syscalls = true);

} // namespace library