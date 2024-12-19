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

namespace ast {

class node;
struct definition;
class lvalue_node;
class expression_node;

class Abstract_Syntax_Tree
{
  public:
    Abstract_Syntax_Tree(Abstract_Syntax_Tree const&) = delete;
    Abstract_Syntax_Tree& operator=(Abstract_Syntax_Tree const&) = delete;

    using ast_type = std::vector<ast::definition>;

  public:
    explicit Abstract_Syntax_Tree(std::string_view parse_tree);
    ~Abstract_Syntax_Tree();

  private:
    ast_type ast_;
};

enum class literal : int
{
    Number,
    Constant,
    String,
    Unknown
};

struct definition
{
    enum class type : int
    {
        Function,
        Vector,
        Unknown
    };

    type type{ type::Unknown };
    std::vector<node> children{};
};

using literal_type = std::tuple<literal, std::string>;

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

class rvalue_node final : public node
{
  public:
    using expr_ptr = std::unique_ptr<expression_node>;
    explicit rvalue_node(std::string type = "unknown")
        : type_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string type_;
    expr_ptr rvalue_;
};

class lvalue_node final : public node
{
  public:
    explicit lvalue_node(std::string type = "unknown")
        : identifier_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string identifier_;
};

class expression_node final : public node
{
  public:
    using expr_datatype =
        std::vector<std::variant<std::monostate, lvalue_node, rvalue_node>>;
    explicit expression_node(std::string type)
        : type_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string type_;
    expr_datatype expr_;
};

class statement_node final : public node
{
  public:
    using statement_datatype = std::
        variant<std::monostate, std::string, expression_node, literal_type>;
    explicit statement_node(std::string type)
        : type_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string type_;
    statement_datatype data_{};
    std::vector<node> children_{};
};

} // namespace ast
} // namespace roxas
