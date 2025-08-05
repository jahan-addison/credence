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

// Notes:
// https://github.com/shinh/elvm/blob/master/ir/ir.h
// https://github.com/arnlaugsson/project-3/blob/master/code.py
// https://web.stanford.edu/class/archive/cs/cs143/cs143.1128/lectures/13/Slides13.pdf
//  slide 156

namespace roxas {

/**
 * @brief
 *
 *  Intermediate_Representation Class
 *
 * An implementation of the Three-address IR as Quintuple,
 * where there can be 4 or less arguments with an operator.
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

    using Quintuple = std::tuple<Operator,
                                 std::string_view,
                                 std::string_view,
                                 std::string_view,
                                 std::string_view>;

    /**
     * @brief
     * Operator to string
     *
     * @param op
     * @return constexpr std::string_view
     */
    constexpr std::string_view operator_to_string(Operator op) noexcept;

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
    void emit_to(std::fstream const& fstream);

  public:
    void from_assignment_expression(Node node);
    // inline lvalues
    /**
     * @brief
     * Parse identifier lvalue
     *
     *  Parse and verify identifer is declared with auto or extern
     *
     * @param node
     */
    void from_identifier(Node node);
    void from_indirect_identifier(Node node);
    void from_vector_idenfitier(Node node);
    /**
     * @brief
     * Parse number literal node into IR symbols
     *
     * @param node
     */
    void from_number_literal(Node node);
    /**
     * @brief
     * Parse string literal node into IR symbols
     *
     * @param node
     */
    void from_string_literal(Node node);
    /**
     * @brief
     * Parse constant literal node into IR symbols
     *
     * @param node
     */
    void from_constant_literal(Node node);

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
