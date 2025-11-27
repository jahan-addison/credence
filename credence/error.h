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
#include <exception>
#include <fmt/compile.h>
#include <fmt/format.h>
#include <simplejson.h>
#include <source_location>
#include <string_view>

#define credence_error(message) (                                            \
    credence::detail::assert_fail(std::source_location::current(), message)  \
)

#define credence_compile_error(location, message, symbol, symbols) (  \
    credence::detail::compile_error_impl(                             \
        location,                                                     \
        message,                                                      \
        symbol,                                                       \
        symbols)                                                      \
)

#define credence_assert(condition) (              \
    credence::detail::assert_impl(                \
        std::source_location::current(),          \
        condition)                                \
)

#define credence_assert_message(condition, message) (   \
    credence::detail::assert_impl(                      \
        std::source_location::current(),                \
        condition,                                      \
        message)                                        \
)

#define credence_assert_equal(actual, expected) (       \
    credence::detail::assert_equal_impl(                \
        std::source_location::current(),                \
        actual,                                         \
        expected)                                       \
)

#define credence_assert_nequal(actual, expected) (         \
    credence::detail::assert_nequal_impl(                  \
        std::source_location::current(),                   \
        actual,                                            \
        expected)                                          \
    )

namespace credence::detail {

class Credence_Exception : public std::exception
{
    std::string message_;

  public:
    template<typename... Args>
    explicit constexpr Credence_Exception(
        fmt::format_string<Args...> fmt_string,
        Args&&... args)
        : message_{ fmt::format(fmt_string, std::forward<Args>(args)...) }
    {
    }

    [[nodiscard]] const char* what() const noexcept override
    {
        return message_.c_str();
    }
};

inline void compile_error_impl(
    std::source_location const& location,
    std::string_view message,
    std::string_view symbol_name,
    json::JSON const& symbols)
{
    auto symbol = symbol_name.data();
#ifndef DEBUG
    throw Credence_Exception(
        "\n  Credence could not compile source:\n    on symbol '{}'\n    with: "
        "\"{}\"\n  > from line {} column {}:{}",
        symbol,
        message,
        symbols[symbol]["line"].to_int(),
        symbols[symbol]["column"].to_int(),
        symbols[symbol]["end_column"].to_int());
#else
    throw Credence_Exception(
        "\n  Credence could not compile source:\n    on symbol '{}'\n    with: "
        "\"{}\"\n  > from line {} column {}:{}\n\n\n >>> In file "
        "'{}'\n line {}\n   ::: '{}'\n",
        symbol,
        message,
        symbols[symbol]["line"].to_int(),
        symbols[symbol]["column"].to_int(),
        symbols[symbol]["end_column"].to_int(),
        location.file_name(),
        location.line(),
        location.function_name());
#endif
}

inline void assert_fail(
    std::source_location const& location,
    std::string_view message = "")
{
    if (message.empty()) {
        throw Credence_Exception(
            "\n    Assertion failed in '{}'\n    at line {}\n  ::: "
            "'{}'\n",
            location.file_name(),
            location.line(),
            location.function_name());
    } else {
        throw Credence_Exception(
            "\n    Assertion failed in '{}'\n    at line {}\n  ::: "
            "'{}':\n    "
            "with "
            "'{}'\n",
            location.file_name(),
            location.line(),
            location.function_name(),
            message);
    }
}

inline void assert_impl(
    std::source_location const& location,
    bool condition,
    std::string_view message = "")
{
    if (!condition) {
        if (message.empty()) {
            throw Credence_Exception(
                "\n    Assertion Failed in '{}'\n    at line {} "
                "\n  ::: '{}'\n",
                location.file_name(),
                location.line(),
                location.function_name());
        } else {
            throw Credence_Exception(
                "\n    Assertion failed in '{}'\n    at line {} "
                "\n  ::: '{}': \n   "
                " "
                "with "
                "'{}'\n",
                location.file_name(),
                location.line(),
                location.function_name(),
                message);
        }
    }
}

template<typename T1, typename T2>
constexpr inline void assert_equal_impl(
    std::source_location const& location,
    const T1& actual,
    const T2& expected)
{
    assert_impl(location, actual == expected);
}

template<typename T1, typename T2>
constexpr inline void assert_nequal_impl(
    std::source_location const& location,
    const T1& actual,
    const T2& expected)
{
    assert_impl(location, actual != expected);
}

} // namespace credence::detail