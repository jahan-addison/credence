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

#include <cctype>      // for toupper
#include <filesystem>  // for filesystem
#include <fstream>     // for ostream
#include <stddef.h>    // for size_t
#include <string>      // for basic_string, string
#include <string_view> // for basic_string_view, string_view
#include <tuple>       // for apply, tuple
#include <type_traits> // for is_same_v, is_convertible_v
namespace json {
class JSON;
}

#define CREDENCE_PUBLIC public
#ifdef CREDENCE_TEST
#define CREDENCE_PRIVATE_UNLESS_TESTED public
#else
#define CREDENCE_PRIVATE_UNLESS_TESTED private
#endif

namespace credence {

namespace util {

namespace fs = std::filesystem;

////////////////////
// Visitor Pattern
////////////////////

namespace AST = json;
using AST_Node = AST::JSON;

// The overload pattern
// (std::variant visitor)

template<class... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

//////////////////
// String Helpers
//////////////////

inline std::string capitalize(const char* str)
{
    auto s = std::string{ str };
    if (!s.empty())
        s[0] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

template<typename T>
constexpr inline std::string to_constexpr_string(T const& val)
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
                 result += to_constexpr_string(elements);
                 first = false;
             })(),
             ...);
        },
        t);
    result += ")";
    return result;
}

////////////////
// File helpers
////////////////

void write_file_to_path_from_stringstream(
    std::string_view file_name,
    std::ostringstream const& oss,
    std::string_view ext = "bo");

std::string read_file_from_path(std::string_view path);

} // namespace util

} // namespace credence