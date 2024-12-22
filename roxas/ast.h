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

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace roxas {

// Link to the grammar can be found here:
// https://github.com/jahan-addison/xion/blob/master/xion/grammar.lark

namespace ast {

struct definition;
class node;
class expression_node;
class statement_node;
class lvalue_node;
class rvalue_node;

enum class literal : int
{
    Number,
    Constant,
    String,
    Unknown
};

using literal_node = std::tuple<literal, std::string>;

} // namespace ast

/**
 * @brief
 *
 * The AST object that holds a representation of a parse tree from
 * the ParseTreeModuleLoader object suitable to construct a B program.
 *
 * Constructs child nodes with recursive descent on ast data types,
 * prepares data for future compiler passes.
 *
 */
class Abstract_Syntax_Tree
{
  public:
    Abstract_Syntax_Tree(Abstract_Syntax_Tree const&) = delete;
    Abstract_Syntax_Tree& operator=(Abstract_Syntax_Tree const&) = delete;

    using ast_type = std::vector<ast::definition>;

  public:
    /**
     * @brief Construct a new Abstract_Syntax_Tree object
     *
     * @param parse_tree A parse tree from the ParseTreeModuleLoader
     */
    explicit Abstract_Syntax_Tree(std::string_view parse_tree);
    ~Abstract_Syntax_Tree() = default;

  public:
    /**
     * @brief Get the AST
     *
     * @return ast_type the AST data structure
     */
    const ast_type& get_ast_definitions() const;

  private:
    ast::definition construct_definition_ast_();
    ast::statement_node construct_statement_node_();
    ast::expression_node construct_expression_node_();

  private:
    ast::lvalue_node construct_lvalue_node_();
    ast::rvalue_node construct_rvalue_node_();

  private:
    ast::literal_node construct_constant_ast_();

  private:
    std::string_view parse_tree_;
    ast_type ast_;
};

namespace ast {

// The overload pattern
template<class... Ts>
struct overload : Ts...
{
    using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

/**
 * @brief A representation of a Definition (vector or function) in the B
 * language
 *
 */
struct definition
{
    using ptr = std::unique_ptr<node>;
    enum class type : int
    {
        Function,
        Vector,
        Unknown
    };

    type type{ type::Unknown };
    std::vector<ptr> children{};
};

/**
 * @brief
 *
 * The AST node abstract class
 *
 */
class node
{
  public:
    node(node const&) = delete;
    node& operator=(node const&) = delete;
    virtual ~node() = default;

  protected:
    node() = default;

  public:
    virtual void print() const = 0;
};

/**
 * @brief The lvalue node
 *
 * Provides a string to its identifer
 *
 */
class lvalue_node final : public node
{
  public:
    /**
     * @brief Construct a new lvalue node
     *
     * @param type lvalue type, per the grammar
     */
    explicit lvalue_node(std::string type)
        : identifier_(std::move(type))
    {
    }
    /**
     * @brief The lvalue node print function
     *
     */
    void print() const override;

  private:
    std::string identifier_;
};

/**
 * @brief The rvalue node
 *
 * Holds a pointer to its data expression
 *
 */
class rvalue_node final : public node
{
  public:
    /**
     * @brief Construct a new rvalue node
     *
     * @param type rvalue type, per te grammar
     * @param node the rvalue expression
     */
    explicit rvalue_node(std::string type, std::string rvalue)
        : type_(std::move(type))
        , rvalue_(std::move(rvalue))

    {
    }

    /**
     * @brief The rvalue node print function
     *
     */
    void print() const override;

  private:
    std::string type_;
    std::string rvalue_;
};

/**
 * @brief The expression node
 *
 * An expression can be one or many of an lvalue, or rvalue
 *
 */
class expression_node final : public node
{
  public:
    using datatype =
        std::vector<std::variant<std::monostate, lvalue_node, rvalue_node>>;
    /**
     * @brief Construct a new expression node
     *
     * @param type expression type, per the grammar
     * @param expr the lvalue or rvalue of the expression
     */
    explicit expression_node(std::string type, datatype expr)
        : type_(std::move(type))
        , expr_(std::move(expr))
    {
    }

    /**
     * @brief The expression node print function
     *
     */
    void print() const override;

  private:
    std::string type_;
    datatype expr_;
};

/**
 * @brief The statement node
 *
 * A statement may hold construct one of a string, expression, or literal.
 * Note that statements may have branches (such as if statements or switch/case)
 *
 */
class statement_node final : public node
{
  public:
    using ptr = std::unique_ptr<statement_node>;
    using datatype = std::
        variant<std::monostate, std::string, expression_node, literal_node>;
    /**
     * @brief Construct a new statement node
     *
     * @param type statement type, per the grammar
     * @param branches a vector of the statement branches
     */
    explicit statement_node(std::string type,
                            std::vector<statement_node::ptr> branches)
        : type_(std::move(type))
        , branches_(std::move(branches))

    {
    }

    /**
     * @brief The statement node print function
     *
     */
    void print() const override;

  private:
    std::string type_;
    std::vector<statement_node::ptr> branches_;
    datatype data_;
};

} // namespace ast
} // namespace roxas
