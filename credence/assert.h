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

#include <iostream>
#include <source_location>
#include <string_view>

namespace {

void assert_impl(
    bool condition,
    std::string_view message,
    std::source_location const& location = std::source_location::current())
{
    if (!condition) {
        std::cerr << "Credence Assertion: " << message << std::endl
                  << "  File: " << location.file_name() << std::endl
                  << "  Line: " << location.line() << std::endl
                  << "  Function: " << location.function_name() << std::endl;
        std::abort();
    }
}

template<typename T1, typename T2>
void assert_equal_impl(
    [[maybe_unused]] const T1& actual,
    [[maybe_unused]] const T2& expected,
    [[maybe_unused]] const std::source_location location =
        std::source_location::current())
{
#ifndef DEBUG
    if (actual != expected) {
        std::cerr << std::format(
            "Credence Assertion: `actual == expected`\n"
            "  File: {}:{}\n"
            "  Function: {}\n"
            "  Actual:   {}\n"
            "  Expected: {}\n",
            location.file_name(),
            location.line(),
            location.function_name(),
            actual,
            expected);
        std::abort();
    }
#endif
}

} // namespace

#ifndef DEBUG
#define CREDENCE_ASSERT(condition) ((void)0)
#define CREDENCE_ASSERT_MESSAGE(condition, message) ((void)0)
#define CREDENCE_ASSERT_EQUAL(lhs, rhs) ((void)0)
#define CREDENCE_ASSERT_NODE(lhs, rhs) ((void)0)
#else
#define CREDENCE_ASSERT(condition)                                              \
        ::assert_impl(condition, "", std::source_location::current())
#define CREDENCE_ASSERT_MESSAGE(condition, message)                             \
        ::assert_impl(condition, message, std::source_location::current());
#define CREDENCE_ASSERT_EQUAL(actual, expected)  \
        ::assert_equal_impl(actual, expected)
#define CREDENCE_ASSERT_NODE(actual, expected)   \
        ::assert_equal_impl(actual, expected)

#endif
