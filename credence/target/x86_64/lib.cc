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

#include "lib.h"
#include "syscall.h"        // for syscall_list
#include <algorithm>        // for __find, find
#include <array>            // for array
#include <credence/error.h> // for assert_nequal_impl, credence_assert_nequal
#include <credence/util.h>  // for AST_Node, AST

namespace credence::target::x86_64::library {

void detail::add_stdlib_function_to_table_symbols(
    std::string const& stdlib_function,
    util::AST_Node& symbols)
{
    auto syscalls = syscall::common::get_platform_syscall_symbols();
    if (!util::initializer_list_contains<std::string_view>(
            sv(stdlib_function), STDLIB_FUNCTIONS) and
        std::ranges::find(syscalls, stdlib_function) != syscalls.end())
        credence_error("Invalid stdlib function");
    symbols[stdlib_function] = util::AST::object();
    auto& node = symbols[stdlib_function];
    node["type"] = "function_definition";
}

void detail::add_syscall_functions_to_symbols(util::AST_Node& symbols)
{
    auto syscalls = syscall::common::get_platform_syscall_symbols();
    for (auto const& routine : syscalls)
        add_stdlib_function_to_table_symbols(routine, symbols);
}

void add_stdlib_functions_to_symbols(util::AST_Node& symbols)
{
    for (auto const& routine : STDLIB_FUNCTIONS)
        detail::add_stdlib_function_to_table_symbols(routine.data(), symbols);
    detail::add_syscall_functions_to_symbols(symbols);
}

} // namespace library