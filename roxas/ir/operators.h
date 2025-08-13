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
#include <sstream>
namespace roxas {
namespace ir {
/**
 * @brief Operators
 *
 */
enum class Operator
{
    // relational operators
    R_EQUAL,
    R_NEQUAL,
    R_LT,
    R_GT,
    R_LE,
    R_GE,
    R_OR,
    R_AND,

    // math binary operators
    B_SUBTRACT,
    B_ADD,
    B_MOD,
    B_MUL,
    B_DIV,

    // unary increment/decrement
    PRE_INC,
    PRE_DEC,
    POST_INC,
    POST_DEC,

    // bit ops
    RSHIFT,
    XOR,
    LSHIFT,
    U_NOT,
    U_ONES_COMPLEMENT,

    U_MINUS,

    // pointer operators
    U_ADDR_OF,
    U_INDIRECTION
};

namespace {
enum class Instruction_Operator
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
} // namespace detail

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
        // relational operators
        case Operator::R_EQUAL:
            os << "==";
            break;
        case Operator::R_NEQUAL:
            os << "!=";
            break;
        case Operator::R_LT:
            os << "<";
            break;
        case Operator::R_GT:
            os << ">";
            break;
        case Operator::R_LE:
            os << "<=";
            break;
        case Operator::R_GE:
            os << ">=";
            break;
        case Operator::R_OR:
            os << "|";
            break;
        case Operator::R_AND:
            os << "&";
            break;

        // math binary operators
        case Operator::B_SUBTRACT:
            os << "-";
            break;
        case Operator::B_ADD:
            os << "+";
            break;
        case Operator::B_MOD:
            os << "%";
            break;
        case Operator::B_MUL:
            os << "*";
            break;
        case Operator::B_DIV:
            os << "/";
            break;

        // unary increment/decrement
        case Operator::PRE_INC:
        case Operator::POST_INC:
            os << "++";
            break;

        case Operator::PRE_DEC:
        case Operator::POST_DEC:
            os << "--";
            break;

        // bit ops
        case Operator::RSHIFT:
            os << ">>";
            break;
        case Operator::LSHIFT:
            os << "<<";
            break;
        case Operator::XOR:
            os << "^";
            break;

        case Operator::U_NOT:
            os << "!";
            break;
        case Operator::U_ONES_COMPLEMENT:
            os << "~";
            break;

        // pointer operators
        case Operator::U_INDIRECTION:
            os << "*";
            break;
        case Operator::U_ADDR_OF:
            os << "&";

        case Operator::U_MINUS:
            os << "-";
            break;

        default:
            os << "null";
            break;
    }
    return os;
}

inline std::string operator_to_string(Operator op)
{
    std::ostringstream os;
    os << op;
    return os.str();
}

/**
 * @brief Instruction_Operator enum type << operator overload
 *
 * @param os
 * @param op
 * @return std::ostream&
 */
inline std::ostream& operator<<(std::ostream& os,
                                Instruction_Operator const& op)
{
    switch (op) {
        case Instruction_Operator::FUNC_START:
            os << "BeginFunc";
            break;
        case Instruction_Operator::FUNC_END:
            os << "EndFunc";
            break;
        case Instruction_Operator::LABEL:
        case Instruction_Operator::VARIABLE:
        case Instruction_Operator::NOOP:
            os << "";
            break;
        case Instruction_Operator::RETURN:
            os << "Ret";
            break;
        case Instruction_Operator::PUSH:
            os << "Push";
            break;
        case Instruction_Operator::POP:
            os << "Pop";
            break;
        case Instruction_Operator::CALL:
            os << "Call";
            break;
        case Instruction_Operator::GOTO:
            os << "Goto";
            break;
        case Instruction_Operator::EOL:
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