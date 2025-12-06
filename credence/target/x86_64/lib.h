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

#include "instructions.h"   // for Storage
#include <credence/util.h>  // for AST_Node
#include <initializer_list> // for initializer_list
#include <string>           // for string
#include <string_view>      // for basic_string_view, string_view
#include <vector>           // for vector

namespace credence::target::x86_64::library {

using library_t = std::array<std::size_t, 1>;
using Address = x86_64::detail::Storage;
using library_list_t = std::map<std::string_view, library_t>;
using library_arguments_t = std::deque<Address>;

std::vector<std::string> get_library_symbols();

const library_list_t library_list = { { "print", { 1 } } };

bool is_syscall_function(type::semantic::Label const& label);
bool is_library_function(type::semantic::Label const& label);
bool is_stdlib_function(type::semantic::Label const& label);

namespace detail {

void add_stdlib_function_to_table_symbols(
    std::string const& stdlib_function,
    util::AST_Node& symbols);

void add_syscall_functions_to_symbols(util::AST_Node& symbols);

} // namespace detail

void add_stdlib_functions_to_symbols(util::AST_Node& symbols);

x86_64::detail::Instructions make_print_lib_function(Address const& buffer);

} // namespace library