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
#include <algorithm>         // for copy, max
#include <credence/json.h>   // for JSON
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for RValue, Type_, Value_Type
#include <deque>             // for deque
#include <iomanip>           // for operator<<, setw
#include <map>               // for map
#include <optional>          // for optional, nullopt
#include <sstream>           // for operator<<, basic_ostream, basic_ostrin...
#include <string>            // for allocator, char_traits, string, operator<<
#include <tuple>             // for get, make_tuple, tuple
#include <utility>           // for pair
#include <variant>           // for monostate
#include <vector>            // for vector
namespace credence {

namespace ir {

using Node = json::JSON;

enum class Instruction
{
    FUNC_START,
    FUNC_END,
    LABEL,
    GOTO,
    IF,
    PUSH,
    POP,
    CALL,
    CMP,
    VARIABLE,
    RETURN,
    LEAVE,
    EOL,
    NOOP
};
using Quadruple =
    std::tuple<Instruction, std::string, std::string, std::string>;

using Instructions = std::deque<Quadruple>;
using Branch_Instructions = std::pair<Instructions, Instructions>;

constexpr Quadruple make_quadruple(Instruction op,
                                   std::string const& s1,
                                   std::string const& s2,
                                   std::string const& s3 = "")
{
    return std::make_tuple(op, s1, s2, s3);
}

namespace detail {

using Branch_Comparator = std::pair<std::string, Instructions>;

void insert_rvalue_or_block_branch_instructions(
    Symbol_Table<>& symbols,
    Symbol_Table<>& globals,
    Node& block,
    Node& details,
    Quadruple const& tail_branch,
    int* temporary,
    Instructions& branch_instructions);

std::string build_from_branch_comparator_from_rvalue(Symbol_Table<>& symbols,
                                                     Node& details,
                                                     Node& block,
                                                     Instructions& instructions,
                                                     int* temporary);

} // namespace detail

void emit_quadruple(std::ostream& os, Quadruple qaud);

Instructions build_from_definitions(Symbol_Table<>& symbols,
                                    Symbol_Table<>& globals,
                                    Node& node,
                                    Node& details);

Instructions build_from_function_definition(Symbol_Table<>& symbols,
                                            Symbol_Table<>& globals,
                                            Node& node,
                                            Node& details);

void build_from_vector_definition(Symbol_Table<>& symbols,
                                  Node& node,
                                  Node& details);

Branch_Instructions build_from_switch_statement(Symbol_Table<>& symbols,
                                                Symbol_Table<>& globals,
                                                Quadruple const& tail_branch,
                                                Node& node,
                                                Node& details,
                                                int* temporary);

Branch_Instructions build_from_while_statement(Symbol_Table<>& symbols,
                                               Symbol_Table<>& globals,
                                               Quadruple const& tail_branch,
                                               Node& node,
                                               Node& details,
                                               int* temporary);

Branch_Instructions build_from_if_statement(Symbol_Table<>& symbols,
                                            Symbol_Table<>& globals,
                                            Quadruple const& tail_branch,
                                            Node& node,
                                            Node& details,
                                            int* temporary);

Instructions build_from_label_statement(Symbol_Table<>& symbols,
                                        Node& node,
                                        Node& details);
Instructions build_from_goto_statement(Symbol_Table<>& symbols,
                                       Node& node,
                                       Node& details);
Instructions build_from_return_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details,
                                         int* temporary);
Instructions build_from_block_statement(
    Symbol_Table<>& symbols,
    Symbol_Table<>& globals,
    Node& node,
    Node& details,
    bool ret = false,
    std::optional<Quadruple> tail_branch = std::nullopt,
    std::optional<int*> temporary = std::nullopt);

void build_from_auto_statement(Symbol_Table<>& symbols, Node& node);

void build_from_extrn_statement(Symbol_Table<>& symbols,
                                Symbol_Table<>& globals,
                                Node& node);

Instructions build_from_rvalue_statement(Symbol_Table<>& symbols,
                                         Node& node,
                                         Node& details,
                                         int* temporary);

std::vector<std::string> build_from_rvalue_expression(
    type::RValue::Type& rvalue);

/**
 * @brief Instruction_Operator enum type << operator overload
 */
constexpr std::ostream& operator<<(std::ostream& os, Instruction const& op)
{
    switch (op) {
        case Instruction::FUNC_START:
            os << "BeginFunc";
            break;
        case Instruction::FUNC_END:
            os << "EndFunc";
            break;
        case Instruction::LABEL:
            break;
        case Instruction::VARIABLE:
            os << "=";
            break;
        case Instruction::NOOP:
            os << "";
            break;
        case Instruction::CMP:
            os << "CMP";
            break;
        case Instruction::RETURN:
            os << "RET";
            break;
        case Instruction::LEAVE:
            os << "LEAVE";
            break;
        case Instruction::IF:
            os << "IF";
            break;
        case Instruction::PUSH:
            os << "PUSH";
            break;
        case Instruction::POP:
            os << "POP";
            break;
        case Instruction::CALL:
            os << "CALL";
            break;
        case Instruction::GOTO:
            os << "GOTO";
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

inline std::string instruction_to_string(
    Instruction op) // not constexpr until C++23
{
    std::ostringstream os;
    os << op;
    return os.str();
}

inline std::string quadruple_to_string(
    Quadruple qaud) // not constexpr until C++23
{
    std::ostringstream os;
    os << std::setw(2) << std::get<1>(qaud) << std::get<0>(qaud)
       << std::get<2>(qaud) << std::get<3>(qaud);

    return os.str();
}

} // namespace ir

} // namespace credence