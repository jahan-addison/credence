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

#include <fstream>
#include <roxas/json.h>
#include <roxas/symbol.h>
#include <roxas/util.h>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace roxas {

/**
 * @brief
 *
 *  Intermediate_Representation Class
 *
 * An implementation of the Three-address IR as Quintuple,
 * where there can be 4 or less arguments with an operator.
 * Notes:
 *  https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf
 *
 */
class Intermediate_Representation
{
  public:
    Intermediate_Representation(Intermediate_Representation const&) = delete;
    Intermediate_Representation& operator=(Intermediate_Representation const&) =
        delete;

  public:
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
        NOOP
    };
    using DataType = Default_Table_Type;
    using Quintuple = std::
        tuple<Operator, std::string, std::string, std::string, std::string>;

    friend std::ostream& operator<<(std::ostream& os, Operator const& op)
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
            default:
                os << "null";
                break;
        }
        return os;
    }

  public:
    /**
     * @brief Construct a new Intermediate_Representation object
     *
     */
    explicit Intermediate_Representation(json::JSON const& symbols)
        : internal_symbols_(symbols)
    {
    }
    /**
     * @brief Destroy the Intermediate_Representation object
     *
     */
    ~Intermediate_Representation() = default;

  public:
    using Node = json::JSON;
    void emit_to_stdout();
    /**
     * @brief
     * Throw Parsing or program flow error
     *
     * Use source data provided by internal symbol table
     *
     * @param message
     * @param object
     */
    void parsing_error(std::string_view message, std::string_view object);
    void emit_to(std::fstream const& fstream);

  public:
    /**
     * @brief
     *  Parse assignment expression
     *
     * @param node
     */
    void from_assignment_expression(Node& node);
    /**
     * @brief
     * Parse identifier lvalue
     *
     *  Parse and verify identifer is declared with auto or extern
     *
     * @param node
     */
    void from_identifier(Node& node);
    void from_indirect_identifier(Node& node);
    void from_vector_idenfitier(Node& node);
    /**
     * @brief
     * Parse number literal node into IR symbols
     *
     * @param node
     */
    DataType from_number_literal(Node& node);
    /**
     * @brief
     * Parse string literal node into IR symbols
     *
     * @param node
     */
    DataType from_string_literal(Node& node);
    /**
     * @brief
     * Parse constant literal node into IR symbols
     *
     * @param node
     */
    DataType from_constant_literal(Node& node);

    /* clang-format off */
  ROXAS_PRIVATE_UNLESS_TESTED:
    /* clang-format on*/
    json::JSON internal_symbols_;
    int temporary_{ 0 };
    Symbol_Table<> symbols_{};
    std::vector<Quintuple> quintuples_{};
    std::vector<std::string> labels_{};
};

} // namespace roxas
