/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include <algorithm>        // for all_of, find, any_of
#include <cctype>           // for isdigit, toupper
#include <charconv>         // for from_chars, from_chars_result
#include <concepts>         // for floating_point, integral
#include <cstddef>          // for size_t, ptrdiff_t
#include <cstdint>          // for uint32_t, uint_least32_t
#include <filesystem>       // for filesystem
#include <fstream>          // for ostringstream
#include <initializer_list> // for begin, end
#include <iterator>         // for distance
#include <optional>         // for optional, nullopt, nullopt_t
#include <ranges>           // for end, begin
#include <string>           // for basic_string, string, to_string
#include <string_view>      // for basic_string_view, string_view
#include <system_error>     // for errc
#include <tuple>            // for apply, tuple
#include <type_traits>      // for is_same_v, is_convertible_v
namespace easyjson {
class JSON;
} // lines 41-41

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

#define WIDE_AND_STRINGIFY(x) L##x
#define WIDE_STRINGIFY(x) WIDE_AND_STRINGIFY(x)

#define __source__ std::source_location::current()
#define SET_INLINE_DEBUG std::source_location const& source = __source__
#define INLINE_DEBUG std::source_location const& source
#define DEBUG_SOURCE source

namespace easyjson {
class JSON;
} // lines 30-30
namespace easyjson {
class JSON;
} // lines 31-31

#define CREDENCE_PUBLIC public
#ifdef CREDENCE_TEST
#define CREDENCE_PRIVATE_UNLESS_TESTED public
#define CREDENCE_PROTECTED_UNLESS_TESTED public

#else
#define CREDENCE_PRIVATE_UNLESS_TESTED private
#define CREDENCE_PROTECTED_UNLESS_TESTED protected
#endif

#define sv(m)        \
    std::string_view \
    {                \
        m            \
    }

namespace credence {

namespace util {

namespace fs = std::filesystem;

namespace AST = easyjson;
using AST_Node = AST::JSON;

////////////////////
// Visitor Pattern
////////////////////

#define is_variant(S, V) std::holds_alternative<S>(V)

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

namespace detail {
// FNV-1a constants for 32-bit hash
const uint32_t FNV_PRIME_32 = 16777619;
const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
} // namepace detail

using fnv1a_hash = uint32_t;

constexpr fnv1a_hash fnv1a_32_hash(std::string const& data)
{
    uint32_t hash = detail::FNV_OFFSET_BASIS_32;
    for (char byte : data) {
        hash ^= static_cast<uint32_t>(byte);
        hash *= detail::FNV_PRIME_32;
    }
    return hash;
}

inline std::optional<uint32_t> to_u32_int_safe(std::string_view s)
{
    uint32_t value{};
    if (std::from_chars(s.data(), s.data() + s.size(), value).ec == std::errc{})
        return value;
    else
        return std::nullopt;
}

constexpr std::string strip_char(std::string_view str, char char_to_strip)
{
    std::string result;
    result.reserve(str.length());
    for (char c : str) {
        if (c != char_to_strip) {
            result += c;
        }
    }
    return result;
}

inline std::string capitalize(const char* str)
{
    auto s = std::string{ str };
    if (!s.empty())
        s[0] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}

constexpr inline std::size_t substring_count_of(std::string_view text,
    std::string_view sub)
{
    std::size_t count = 0UL;
    std::size_t index = 0UL;
    while ((index = text.find(sub, index)) != std::string_view::npos) {
        count++;
        index += sub.size();
    }
    return count;
}

constexpr bool is_numeric(std::string_view s)
{
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c);
    });
}

constexpr bool contains(std::string_view lhs, std::string_view rhs)
{
    return lhs.find(rhs) != std::string_view::npos;
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
    } else if constexpr (std::is_same_v<T, uint_least32_t>) {
        return std::to_string(val);
    } else if constexpr (std::is_same_v<T, unsigned int>) {
        return std::to_string(val);
    } else if constexpr (std::is_same_v<T, std::size_t>) {
        return std::to_string(val);
    } else if constexpr (std::is_same_v<T, double>) {
        return "double_val";
    } else if constexpr (std::is_convertible_v<T, std::string_view>) {
        return std::string(val);
    } else {
        return "unsupported_type";
    }
}

constexpr std::string_view WHITESPACE = " \t\n\r\f\v";

constexpr std::string str_trim_ws(std::string const& ss)
{
    size_t start = ss.find_first_not_of(WHITESPACE);
    if (start == std::string_view::npos) {
        return "";
    }
    size_t end = ss.find_last_not_of(WHITESPACE);
    size_t length = end - start + 1;
    return ss.substr(start, length);
}

constexpr std::string get_numbers_from_string(std::string_view str)
{
    std::string result;
    result.reserve(str.length()); // Pre-allocate memory for efficiency
    for (char c : str) {
        if (c >= '0' && c <= '9') {
            result += c;
        }
    }
    return result;
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
                    // Add more cases for other escape sequences like \r,
                    // \f,
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
constexpr std::string tuple_to_string(std::tuple<Args...> const& t,
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

///////////////////
// Numeric helpers
///////////////////

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

constexpr unsigned int align_up_to_16(unsigned int n)
{
    const unsigned int ALIGNMENT = 16;
    const unsigned int MASK = ALIGNMENT - 1; // MASK = 15 (0xF)
    // The logic is: (n + 15) & ~15
    return (n + MASK) & (~MASK);
}

constexpr unsigned int align_up_to_8(unsigned int n)
{
    const unsigned int ALIGNMENT = 8;
    const unsigned int MASK = ALIGNMENT - 1;
    return (n + MASK) & (~MASK);
}

////////////////
// File helpers
////////////////

void write_to_file_from_string_stream(std::string_view file_name,
    std::ostringstream const& oss,
    std::string_view ext = "bo");

std::string read_file_from_path(std::string_view path);

//////////
// Other
//////////

template<typename T, typename C>
constexpr bool is_one_of(const T& value, const C& container)
{
    return std::any_of(std::begin(container),
        std::end(container),
        [&](auto const& item) { return item == value; });
}

template<typename Range, typename Value>
constexpr bool range_contains(Value const& needle, Range const& haystack)
{
    auto it = std::ranges::find(haystack, needle);
    return it != std::ranges::end(haystack);
}

template<typename Range, typename Value>
constexpr std::ptrdiff_t find_index(const Range& range, const Value& value)
{
    auto it = std::ranges::find(range, value);
    if (it != std::ranges::end(range)) {
        return std::distance(std::ranges::begin(range), it);
    } else {
        return -1;
    }
}

} // namespace util

} // namespace credence