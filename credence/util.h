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

#include <filesystem> // for path
#include <fstream>    // for basic_ifstream
#include <iostream>
#include <ostream>      // for operator<<
#include <simplejson.h> // for JSON
#include <source_location>
#include <sstream>     // for basic_stringstream, stri...
#include <string>      // for char_traits, string, all...
#include <string_view> // for basic_string_view, strin...
#include <tuple>       // for apply, tuple
namespace json {
class JSON;
} // lines 31-31

// access specifier macros for Doctest unit tests
#define CREDENCE_PUBLIC public
#ifdef DOCTEST_LIBRARY_INCLUDED
#define CREDENCE_TEST
#define CREDENCE_PRIVATE_UNLESS_TESTED public
#else
#define CREDENCE_PRIVATE_UNLESS_TESTED private
#endif

namespace credence {

namespace util {

namespace AST = json;
using AST_Node = AST::JSON;

namespace fs = std::filesystem;

namespace detail {

inline void make_assertion(
    bool condition,
    std::string_view message,
    // clang-format off
    std::source_location const& location =
        std::source_location::current())
{
    // clang-format on
    if (!condition) {
        std::cerr << "Credence assertion: " << message << std::endl
                  << "  File: " << location.file_name() << std::endl
                  << "  Line: " << location.line() << std::endl
                  << "  Function: " << location.function_name() << std::endl;
        std::abort();
    }
}

template<typename T>
constexpr std::string to_constexpr_string(T const& val)
{
    if constexpr (std::is_same_v<T, int>) {
        std::string s;
        if (val == 0)
            return "0";
        int temp = val;
        bool negative = false;
        if (temp < 0) {
            negative = true;
            temp = -temp;
        }
        while (temp > 0) {
            s.insert(0, 1, static_cast<char>('0' + (temp % 10)));
            temp /= 10;
        }
        if (negative)
            s.insert(0, 1, '-');
        return s;
    } else if constexpr (std::is_same_v<T, double>) {
        return "double_val";
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(val);
    } else {
        return "unsupported_type";
    }
}

} // namespace detail

template<typename... Args>
constexpr std::string tuple_to_string(
    std::tuple<Args...> const& t,
    std::string_view separator = ", ")
{
    std::string result{};
    std::apply(
        [&](const auto&... elements) {
            bool first = true;
            (([&] {
                 if (!first) {
                     result += separator;
                 }
                 result += detail::to_constexpr_string(elements);
                 first = false;
             })(),
             ...);
        },
        t);
    result += ")";
    return result;
}

// The overload pattern
template<class... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

constexpr std::string unescape_string(std::string_view escaped_str)
{
    std::string unescaped_str;
    for (size_t i = 0; i < escaped_str.length(); ++i) {
        if (escaped_str[i] == '\\') {
            if (i + 1 < escaped_str.length()) {
                char next_char = escaped_str[i + 1];
                switch (next_char) {
                    case 'n':
                        unescaped_str += '\n';
                        break;
                    case 't':
                        unescaped_str += '\t';
                        break;
                    case '\\':
                        unescaped_str += '\\';
                        break;
                    case '"':
                        unescaped_str += '"';
                        break;
                    // Add more cases for other escape sequences like \r, \f,
                    // \b, \v, \a, etc. For Unicode escape sequences like
                    // \uXXXX, more complex parsing is needed.
                    default:
                        unescaped_str += escaped_str[i]; // If not a recognized
                                                         // escape, keep as is
                        unescaped_str += next_char;
                        break;
                }
                i++; // Skip the escaped character
            } else {
                unescaped_str += escaped_str[i]; // Backslash at end of string
            }
        } else {
            unescaped_str += escaped_str[i];
        }
    }
    return unescaped_str;
}

/**
 * @brief read a file from a fs::path
 */
inline std::string read_file_from_path(std::string_view path)
{
    std::ifstream f(path.data(), std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

} // namespace util

} // namespace credence