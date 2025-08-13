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
#include <map>
#include <memory> // For std::unique_ptr
#include <roxas/ir/operators.h>
#include <sstream> // for basic_ostringstream, ostringstream
#include <string>
#include <variant>
#include <vector>

namespace roxas {

namespace ir {

namespace type {

using Byte = unsigned char;

using Value = std::variant<std::monostate,
                           int,
                           long,
                           Byte,
                           float,
                           double,
                           bool,
                           std::string,
                           char>;

using Type_Size = std::pair<std::string, std::size_t>;

static std::map<std::string, Type_Size> Type_ = {
    { "word", { "word", sizeof(void*) } },
    { "byte", { "byte", sizeof(unsigned char) } },
    { "int", { "int", sizeof(int) } },
    { "long", { "long", sizeof(long) } },
    { "float", { "float", sizeof(float) } },
    { "double", { "double", sizeof(double) } },
    { "bool", { "bool", sizeof(bool) } },
    { "null", { "null", 0 } },
    { "char", { "char", sizeof(char) } }
};

using Value_Type = std::pair<Value, Type_Size>;

// Recursive RValue expression type
struct RValue;
namespace detail {
// @TODO This needs to be an address table
using _RValue_PTR = std::unique_ptr<RValue>;
} // namespace detail

struct RValue
{
    using _RValue = detail::_RValue_PTR;

    using Value = Value_Type;

    using LValue = std::pair<std::string, Value_Type>;

    using Symbol = std::pair<LValue, _RValue>;

    using Unary = std::pair<Operator, _RValue>;

    using Relation = std::tuple<Operator, _RValue, _RValue>;

    using Function = std::pair<std::string, _RValue>;

    using Type = std::variant<std::monostate,
                              _RValue,
                              Symbol,
                              Unary,
                              Relation,
                              Function,
                              LValue,
                              Value_Type>;
    Type value;
};

using Quintuple =
    std::tuple<Operator, std::string, std::string, std::string, std::string>;

using Instruction = Quintuple;

using Instructions = std::vector<Quintuple>;

} // namespace type

} // namespace ir

} // namespace roxas