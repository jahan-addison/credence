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
class rvalue_node;

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
    NUMBER,
    CONSTANT,
    STRING,
    UNKNOWN
};

struct definition
{
    enum class type : int
    {
        FUNCTION,
        VECTOR,
        UNKNOWN
    };

    type type{ type::UNKNOWN };
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

class expression_node final : public node
{
  public:
    using expression_datatype =
        std::vector<std::variant<std::monostate, lvalue_node, rvalue_node>>;
    explicit expression_node(std::string type)
        : type_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string type_;
    expression_datatype expr_;
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
    statement_datatype data_;
    std::vector<node> children_;
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

class rvalue_node final : public node
{
  public:
    using rvalue_datatype = expression_node;
    explicit rvalue_node(std::string type = "unknown")
        : type_(std::move(type))
    {
    }

    void print() const override;

  private:
    std::string type_;
    rvalue_datatype rvalue_{ type_ };
};

} // namespace ast
} // namespace roxas
