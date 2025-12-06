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
#include <fmt/format.h>     // for format
#include <vector>           // for vector

namespace credence::target::x86_64::library {

bool is_syscall_function(type::semantic::Label const& label)
{
    auto syscalls = syscall::common::get_platform_syscall_symbols();
    return std::ranges::find(syscalls, label) != syscalls.end();
}

bool is_library_function(type::semantic::Label const& label)
{
    auto lib = get_library_symbols();
    return std::ranges::find(lib, label) != lib.end();
}

bool is_stdlib_function(type::semantic::Label const& label)
{
    return is_syscall_function(label) or is_library_function(label);
}

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

void detail::add_syscall_functions_to_symbols(util::AST_Node& symbols)
{
    auto syscalls = syscall::common::get_platform_syscall_symbols();
    for (auto const& routine : syscalls)
        add_stdlib_function_to_table_symbols(routine, symbols);
}

void add_stdlib_functions_to_symbols(util::AST_Node& symbols)
{
    auto libcalls = get_library_symbols();
    for (auto const& f : libcalls)
        detail::add_stdlib_function_to_table_symbols(f, symbols);
    detail::add_syscall_functions_to_symbols(symbols);
}

} // namespace library