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
#include <ostream> // for ostream

namespace roxas {
namespace ir {
/**
 * @brief Operators
 *
 */
enum class Operator
{
    FUNC_START,
    FUNC_END,
    LABEL,
    GOTO,
    EQUAL,
    MINUS,
    PLUS,
    LT,
    GT,
    LE,
    GE,
    XOR,
    LSHIFT,
    RSHIFT,
    SUBTRACT,
    ADD,
    MOD,
    MUL,
    DIV,
    INDIRECT,
    ADDR_OF,
    UMINUS,
    UNOT,
    UONE,
    PUSH,
    POP,
    CALL,
    VARIABLE,
    RETURN,
    EOL,
    NOOP
};

/**
 * @brief operator enum type << operator overload
 *
 * @param os
 * @param op
 * @return std::ostream&
 */
inline std::ostream& operator<<(std::ostream& os, Operator const& op)
{
    switch (op) {
        case Operator::LABEL:
        case Operator::VARIABLE:
        case Operator::NOOP:
            os << "";
            break;
        case Operator::FUNC_START:
            os << "BeginFunc";
            break;
        case Operator::FUNC_END:
            os << "EndFunc";
            break;

        case Operator::MINUS:
            os << "-";
            break;
        case Operator::EQUAL:
            os << "=";
            break;
        case Operator::PLUS:
            os << "+";
            break;
        case Operator::LT:
            os << "<";
            break;
        case Operator::GT:
            os << ">";
            break;
        case Operator::LE:
            os << "<=";
            break;
        case Operator::GE:
            os << ">=";
            break;
        case Operator::XOR:
            os << "^";
            break;
        case Operator::LSHIFT:
            os << "<<";
            break;
        case Operator::RSHIFT:
            os << ">>";
            break;
        case Operator::SUBTRACT:
            os << "-";
            break;
        case Operator::ADD:
            os << "+";
            break;
        case Operator::MOD:
            os << "%";
            break;
        case Operator::MUL:
            os << "*";
            break;
        case Operator::DIV:
            os << "/";
            break;
        case Operator::INDIRECT:
            os << "*";
            break;
        case Operator::ADDR_OF:
            os << "&";
            break;
        case Operator::UMINUS:
            os << "-";
            break;
        case Operator::UNOT:
            os << "!";
            break;
        case Operator::UONE:
            os << "~";
            break;
        case Operator::PUSH:
            os << "Push";
            break;
        case Operator::POP:
            os << "Pop";
            break;
        case Operator::CALL:
            os << "Call";
            break;
        case Operator::GOTO:
            os << "Goto";
            break;
        case Operator::EOL:
            os << ";";
            break;
        default:
            os << "null";
            break;
    }
    return os;
}

} // namespace ir
} // namespace roxas