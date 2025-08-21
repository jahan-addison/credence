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
#include <list>
#include <map>
#include <memory> // For std::unique_ptr
#include <roxas/operators.h>
#include <sstream> // for basic_ostringstream, ostringstream
#include <string>
#include <variant>
#include <vector>

namespace roxas {

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
using RValue_Pointer_PTR = std::shared_ptr<RValue>;
} // namespace detail

struct RValue
{
    using RValue_Pointer = detail::RValue_Pointer_PTR;

    using Value = Value_Type;

    using LValue = std::pair<std::string, Value>;

    using Symbol = std::pair<LValue, RValue_Pointer>;

    using Unary = std::pair<Operator, RValue_Pointer>;

    using Relation = std::pair<Operator, std::vector<RValue_Pointer>>;

    // name, arguments
    using Function = std::pair<LValue, std::vector<RValue_Pointer>>;

    using Type = std::variant<std::monostate,
                              RValue_Pointer,
                              Symbol,
                              Unary,
                              Relation,
                              Function,
                              LValue,
                              Value>;

    using Type_Pointer = std::shared_ptr<type::RValue::Type>;

    Type value;
};

inline RValue::Type_Pointer rvalue_type_pointer_from_rvalue(
    RValue::Type rvalue_type)
{
    return std::make_shared<type::RValue::Type>(rvalue_type);
}

} // namespace type

} // namespace roxas