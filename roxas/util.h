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
#include <filesystem>  // for path
#include <sstream>     // for basic_stringstream, basic_ostream, stringstream
#include <string>      // for allocator, char_traits, string
#include <string_view> // for operator<<, string_view
#include <tuple>       // for apply
#include <variant>     // for tuple

// access specifier macros for Doctest
#define ROXAS_PUBLIC public
#if defined(DOCTEST_LIBRARY_INCLUDED)
#define ROXAS_PRIVATE_UNLESS_TESTED public
#else
#define ROXAS_PRIVATE_UNLESS_TESTED private
#endif

namespace roxas {

namespace util {

enum class Logging
{
    INFO,
    WARNING,
    ERROR
};

/**
 * @brief Recursively converts tuple elements
 *  to string if << operator is defined
 *
 * @tparam Types
 * @param t
 * @return std::string
 */
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
 *
 * @param path path to file
 * @return std::string
 */
std::string read_file_from_path(fs::path path);

} // namespace util

} // namespace roxas