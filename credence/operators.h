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
#include <array>
#include <credence/eternal.h>
#include <credence/util.h>
#include <map>
#include <ostream>
#include <sstream>
#include <utility>

namespace credence {

namespace type {

/**
 * @brief Operators
 *
 */
enum class Operator
{
    // relational operators
    R_EQUAL,
    R_NEQUAL,
    U_NOT,
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
    AND,
    OR,
    XOR,
    LSHIFT,
    U_ONES_COMPLEMENT,

    U_MINUS,
    B_TERNARY,
    B_ASSIGN,

    // pointer operators
    U_ADDR_OF,
    U_INDIRECTION,
    U_CALL,
    U_PUSH,
    U_SUBSCRIPT
};

enum class Associativity
{
    LEFT_TO_RIGHT,
    RIGHT_TO_LEFT
};

// B operator precedence is more or less the same as C. Where
// there were differences I made small changes for consistency
constexpr auto OPERATOR_PRECEDENCE =
    mapbox::eternal::map<Operator, std::pair<Associativity, unsigned int>>(
        { // Left-to-right
          { Operator::POST_INC, { Associativity::LEFT_TO_RIGHT, 1 } },
          { Operator::POST_DEC, { Associativity::LEFT_TO_RIGHT, 1 } },
          { Operator::U_SUBSCRIPT, { Associativity::LEFT_TO_RIGHT, 1 } },
          { Operator::U_CALL, { Associativity::LEFT_TO_RIGHT, 1 } },

          // Right-to-left
          { Operator::PRE_INC, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::PRE_DEC, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_PUSH, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_MINUS, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_NOT, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_ADDR_OF, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_INDIRECTION, { Associativity::RIGHT_TO_LEFT, 2 } },
          { Operator::U_ONES_COMPLEMENT, { Associativity::RIGHT_TO_LEFT, 2 } },
          // Left-to-right
          { Operator::B_MUL, { Associativity::LEFT_TO_RIGHT, 3 } },
          { Operator::B_DIV, { Associativity::LEFT_TO_RIGHT, 3 } },
          { Operator::B_MOD, { Associativity::LEFT_TO_RIGHT, 3 } },
          // Left-to-right
          { Operator::B_ADD, { Associativity::LEFT_TO_RIGHT, 4 } },
          { Operator::B_SUBTRACT, { Associativity::LEFT_TO_RIGHT, 4 } },
          // Left-to-right
          { Operator::LSHIFT, { Associativity::LEFT_TO_RIGHT, 5 } },
          { Operator::RSHIFT, { Associativity::LEFT_TO_RIGHT, 5 } },
          // Left-to-right
          { Operator::R_LT, { Associativity::LEFT_TO_RIGHT, 6 } },
          { Operator::R_LE, { Associativity::LEFT_TO_RIGHT, 6 } },
          { Operator::R_GT, { Associativity::LEFT_TO_RIGHT, 6 } },
          { Operator::R_GE, { Associativity::LEFT_TO_RIGHT, 6 } },
          // Left-to-right
          { Operator::R_EQUAL, { Associativity::LEFT_TO_RIGHT, 7 } },
          { Operator::R_NEQUAL, { Associativity::LEFT_TO_RIGHT, 7 } },
          // Left-to-right
          { Operator::AND, { Associativity::LEFT_TO_RIGHT, 8 } },
          // Left-to-right
          { Operator::XOR, { Associativity::LEFT_TO_RIGHT, 9 } },
          // Left-to-right
          { Operator::OR, { Associativity::LEFT_TO_RIGHT, 10 } },
          // Left-to-right
          { Operator::R_AND, { Associativity::LEFT_TO_RIGHT, 11 } },
          // Left-to-right
          { Operator::R_OR, { Associativity::LEFT_TO_RIGHT, 12 } },

          { Operator::B_TERNARY, { Associativity::RIGHT_TO_LEFT, 13 } },
          { Operator::B_ASSIGN, { Associativity::RIGHT_TO_LEFT, 14 } } });

constexpr auto BINARY_OPERATORS =
    mapbox::eternal::map<std::string_view, Operator>(
        { { "||", Operator::R_OR },
          { "&", Operator::AND },
          { "|", Operator::OR },
          { "&&", Operator::R_AND },
          { "==", Operator::R_EQUAL },
          { "!=", Operator::R_NEQUAL },
          { "<", Operator::R_LT },
          { "<=", Operator::R_LE },
          { ">", Operator::R_GT },
          { ">=", Operator::R_GE },

          { "^", Operator::XOR },
          { "<<", Operator::LSHIFT },
          { ">>", Operator::RSHIFT },

          { "-", Operator::B_SUBTRACT },
          { "+", Operator::B_ADD },
          { "%", Operator::B_MOD },
          { "*", Operator::B_MUL },
          { "/", Operator::B_DIV } });

constexpr bool is_left_associative(Operator op)
{
    return OPERATOR_PRECEDENCE.find(op)->second.first ==
           Associativity::LEFT_TO_RIGHT;
}

constexpr unsigned int get_precedence(Operator const op)
{
    return OPERATOR_PRECEDENCE.find(op)->second.second;
}

constexpr std::ostream& operator<<(std::ostream& os, Operator const& op)
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
            os << "||";
            break;
        case Operator::R_AND:
            os << "&&";
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
        case Operator::OR:
            os << "|";
            break;
        case Operator::AND:
            os << "&";
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
            break;
        case Operator::U_MINUS:
            os << "-";
            break;

        // relational operators
        case Operator::U_CALL:
            os << "CALL";
            break;
        case Operator::U_PUSH:
            os << "PUSH";
            break;
        case Operator::B_ASSIGN:
            os << "=";
            break;
        case Operator::B_TERNARY:
            os << "?:";
            break;

        default:
            os << "null";
            break;
    }
    return os;
}

constexpr std::string operator_to_string(Operator const& op)
{
    switch (op) {
        // relational operators
        case Operator::R_EQUAL:
            return "==";
            break;
        case Operator::R_NEQUAL:
            return "!=";
            break;
        case Operator::R_LT:
            return "<";
            break;
        case Operator::R_GT:
            return ">";
            break;
        case Operator::R_LE:
            return "<=";
            break;
        case Operator::R_GE:
            return ">=";
            break;
        case Operator::R_OR:
            return "||";
            break;
        case Operator::R_AND:
            return "&&";
            break;

        // math binary operators
        case Operator::B_SUBTRACT:
            return "-";
            break;
        case Operator::B_ADD:
            return "+";
            break;
        case Operator::B_MOD:
            return "%";
            break;
        case Operator::B_MUL:
            return "*";
            break;
        case Operator::B_DIV:
            return "/";
            break;

        // unary increment/decrement
        case Operator::PRE_INC:
        case Operator::POST_INC:
            return "++";
            break;

        case Operator::PRE_DEC:
        case Operator::POST_DEC:
            return "--";
            break;

        // bit ops
        case Operator::RSHIFT:
            return ">>";
            break;
        case Operator::OR:
            return "|";
            break;
        case Operator::AND:
            return "&";
            break;
        case Operator::LSHIFT:
            return "<<";
            break;
        case Operator::XOR:
            return "^";
            break;

        case Operator::U_NOT:
            return "!";
            break;
        case Operator::U_ONES_COMPLEMENT:
            return "~";
            break;

        // pointer operators
        case Operator::U_INDIRECTION:
            return "*";
            break;
        case Operator::U_ADDR_OF:
            return "&";
            break;
        case Operator::U_MINUS:
            return "-";
            break;
        // relational operators
        case Operator::U_CALL:
            return "CALL";
            break;
        case Operator::U_PUSH:
            return "PUSH";
            break;
        case Operator::B_ASSIGN:
            return "=";
            break;
        case Operator::B_TERNARY:
            return "?:";
            break;

        default:
            return "null";
            break;
    }
}

} // namespace type

} // namespace credence