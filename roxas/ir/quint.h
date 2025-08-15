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
#include <ostream>          // for basic_ostream, operator<<
#include <ostream>          // for ostream
#include <roxas/ir/types.h> // for Type_, Value_Type
#include <roxas/json.h>     // for JSON
#include <roxas/symbol.h>   // for Symbol_Table
#include <string>           // for basic_string, string
#include <tuple>            // for tuple
#include <utility>          // for pair
#include <variant>          // for monostate
#include <vector>           // for vector

namespace roxas {

namespace ir {

namespace quint {

using Node = json::JSON;

// https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf

enum class Instruction
{
    FUNC_START,
    FUNC_END,
    LABEL,
    GOTO,
    PUSH,
    POP,
    CALL,
    VARIABLE,
    RETURN,
    EOL,
    NOOP
};
using Operand = type::Value_Type;
using Quadruple = std::tuple<Instruction, Operand, Operand, Operand>;

static Operand NULL_DATA_TYPE = { std::monostate(), type::Type_["null"] };

// Label in front
using Quintuple = std::pair<std::string, Quadruple>;

using Instructions = std::vector<Quintuple>;

void build_from_auto_statement(Symbol_Table<>& symbols, Node& node);
Instructions build_from_rvalue_statement(Node& node);

/**
 * @brief Instruction_Operator enum type << operator overload
 *
 * @param os
 * @param op
 * @return std::ostream&
 */
inline std::ostream& operator<<(std::ostream& os, Instruction const& op)
{
    switch (op) {
        case Instruction::FUNC_START:
            os << "BeginFunc";
            break;
        case Instruction::FUNC_END:
            os << "EndFunc";
            break;
        case Instruction::LABEL:
        case Instruction::VARIABLE:
        case Instruction::NOOP:
            os << "";
            break;
        case Instruction::RETURN:
            os << "Ret";
            break;
        case Instruction::PUSH:
            os << "Push";
            break;
        case Instruction::POP:
            os << "Pop";
            break;
        case Instruction::CALL:
            os << "Call";
            break;
        case Instruction::GOTO:
            os << "Goto";
            break;
        case Instruction::EOL:
            os << ";";
            break;
        default:
            os << "null";
            break;
    }
    return os;
}

} // namespace quint

} // namespace ir

} // namespace roxas