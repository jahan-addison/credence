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

#include <iostream>
#include <roxas/ast.h>

namespace roxas {

/**
 * @brief Construct a new Abstract_Syntax_Tree object
 *
 * @param parse_tree A parse tree from the ParseTreeModuleLoader
 */
Abstract_Syntax_Tree::Abstract_Syntax_Tree(std::string_view parse_tree)
    : parse_tree_(parse_tree)
{
}

/**
 * @brief Get the AST
 *
 * @return ast_type the AST data structure
 */

const Abstract_Syntax_Tree::ast_type& Abstract_Syntax_Tree::get_ast()
{
    return ast_;
}

/**
 * @brief The rvalue node print function
 *
 */
void ast::rvalue_node::print() const
{
    rvalue_->print();
}

/**
 * @brief The lvalue node print function
 *
 */
void ast::lvalue_node::print() const
{
    std::cout << "lvalue: " << identifier_ << std::endl;
}

/**
 * @brief The expression node print function
 *
 */
void ast::expression_node::print() const
{
    for (auto const& item : expr_) {
        std::visit(
            ast::overload{
                [](std::monostate) {},
                [&](ast::lvalue_node const&) { std::get<1>(item).print(); },
                [&](ast::rvalue_node const&) { std::get<2>(item).print(); } },
            item);
    }
}

/**
 * @brief The statement node print function
 *
 */
void ast::statement_node::print() const
{
    // @TODO
}

} // namespace roxas