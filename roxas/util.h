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
#include <roxas/types.h>  // for RValue
#include <roxas/queue.h>  // for RValue
#include <filesystem>     // for path
#include <sstream>        // for basic_stringstream, basic_ostream, stringst...
#include <string>         // for allocator, char_traits, string
#include <string_view>    // for operator<<, string_view
#include <tuple>          // for apply
#include <variant>        // for tuple
namespace json { class JSON; }
// clang-format on

// access specifier macros for Doctest unit tests
#define ROXAS_PUBLIC public
#if defined(DOCTEST_LIBRARY_INCLUDED)
#define ROXAS_PRIVATE_UNLESS_TESTED public
#else
#define ROXAS_PRIVATE_UNLESS_TESTED private
#endif

namespace roxas {

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

/**
 * @brief log function
 *
 * @param level LogLevel log level
 * @param message log message
 */
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

} // namespace roxas