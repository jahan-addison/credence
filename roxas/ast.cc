#include <iostream>
#include <roxas/ast.h>

namespace roxas {

namespace ast {

node::node(std::string const& type)
    : type_(std::move(type))
    , literal_({ literal::UNKNOWN, "unknown" })
    , children_({})
{
}

void node::print()
{
    std::cout << "node type: " << type_;
}

} // namespace ast

} // namespace roxas