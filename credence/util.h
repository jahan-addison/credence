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

// clang-format off
#if defined(DEBUG)
#include <cpptrace/from_current.hpp>
#endif
#include <ostream>                           // for operator<<
#include <credence/queue.h>                  // for RValue_Queue
#include <credence/types.h>                  // for RValue
#include <cpptrace/from_current_macros.hpp>  // for CPPTRACE_TRY
#include <filesystem>                        // for path
#include <sstream>                           // for basic_stringstream, stri...
#include <string>                            // for char_traits, string, all...
#include <string_view>                       // for basic_string_view, strin...
#include <tuple>                             // for apply, tuple
namespace json { class JSON; }  // lines 31-31
// clang-format on

// access specifier macros for Doctest unit tests
#define CREDENCE_PUBLIC public
#ifdef DOCTEST_LIBRARY_INCLUDED
#define CREDENCE_PRIVATE_UNLESS_TESTED public
#else
#define CREDENCE_PRIVATE_UNLESS_TESTED private
#endif

#if defined(DEBUG)
#define CREDENCE_TRY CPPTRACE_TRY
#define CREDENCE_CATCH CPPTRACE_CATCH
#else
#define CREDENCE_TRY try
#define CREDENCE_CATCH catch
#endif

namespace credence {

namespace util {

json::JSON* unravel_nested_node_array(json::JSON* node);

std::string dump_value_type(type::RValue::Value,
                            std::string_view separator = ":");

std::string rvalue_to_string(type::RValue::Type const& rvalue,
                             bool separate = true);

std::string queue_of_rvalues_to_string(RValue_Queue* rvalues_queue);

enum class Logging
{
    INFO,
    WARNING,
    ERROR
};

std::string unescape_string(std::string const& escaped_str);

template<typename... Types>
std::string tuple_to_string(std::tuple<Types...> const& t,
                            std::string_view separator = ", ")
{
    std::stringstream ss;
    auto first = true;
    std::apply(
        [&](const auto&... args) {
            ((ss << (first ? "" : separator) << args, first = false), ...);
        },
        t);
    return ss.str();
}

void log(Logging level, std::string_view message);

// The overload pattern
template<class... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

namespace fs = std::filesystem;

/**
 * @brief read a file from a fs::path
 */
std::string read_file_from_path(fs::path path);

} // namespace util

} // namespace credence