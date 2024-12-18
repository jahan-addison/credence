#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace roxas {

namespace ast {

class node;
struct definition;

} // namespace ast

using AST = std::vector<ast::definition>;

AST parse_tree_to_ast(std::string const& parse_tree);

namespace ast {

enum class literal : int
{
    NUMBER,
    CONSTANT,
    STRING,
    UNKNOWN
};

using literal_datatype = std::tuple<literal, std::string>;

struct definition
{
    enum class type : int
    {
        FUNCTION,
        VECTOR
    };

    type type{ type::FUNCTION };
    std::vector<node> children{};
};

class node
{
  public:
    node(node const&) = delete;
    node& operator=(node const&) = delete;

    explicit node(std::string const& type);
    void print();

  private:
    std::string type_;
    std::optional<literal_datatype> literal_;
    std::optional<std::vector<node>> children_;
};

} // namespace ast
} // namespace roxas
