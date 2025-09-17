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

#include <credence/util.h>
#include <source_location>
#include <string_view>

#ifndef DEBUG
#define CREDENCE_ASSERT(condition) ((void)0)
#define CREDENCE_ASSERT_MESSAGE(condition, message) ((void)0)
#define CREDENCE_ASSERT_EQUAL(lhs, rhs) ((void)0)
#define CREDENCE_ASSERT_NODE(lhs, rhs) ((void)0)
#else
#define CREDENCE_ASSERT(condition)                            \
    do {                                                      \
        credence::util::detail::make_assertion(               \
            condition, "", std::source_location::current());  \
    } while (false)
#define CREDENCE_ASSERT_MESSAGE(condition, message)   \
    do {                                              \
        credence::util::detail::make_assertion(       \
            condition,                                \
            message,                                  \
            std::source_location::current());         \
    } while (false)
#define CREDENCE_ASSERT_EQUAL(str_lhs, str_rhs)                 \
    CREDENCE_ASSERT_MESSAGE(                                    \
        std::string_view{str_lhs} == std::string_view{str_rhs}, \
        "EQUALITY")

#define CREDENCE_ASSERT_NODE(str_lhs, str_rhs)                  \
    CREDENCE_ASSERT_MESSAGE(                                    \
        std::string_view{str_lhs} == std::string_view{str_rhs}, \
        "AST NODE")

#endif
