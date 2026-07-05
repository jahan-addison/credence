/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include <credence/util.h> // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <easyjson.h>      // for JSON

/****************************************************************************
 *
 * B language Symbol Table
 *
 * Rebuilds the flat "name -> {type, size?, void?}"" symbol map that augur's
 * transformer.py used to build as a side effect of its own AST transform, by
 * walking the finished AST_Node tree parser.cc produces instead.
 *
 * https://github.com/jahan-addison/augur/blob/master/augur/transformer.py
 *
 *****************************************************************************/

namespace credence::language {

class Symbol_Table_Builder
{
  public:
    Symbol_Table_Builder(Symbol_Table_Builder const&) = delete;
    Symbol_Table_Builder& operator=(Symbol_Table_Builder const&) = delete;

  public:
    static util::AST_Node build(util::AST_Node const& program)
    {
        Symbol_Table_Builder builder{};
        builder.visit(program);
        return builder.table_;
    }

  private:
    Symbol_Table_Builder() = default;

    // clang-format off

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void visit(util::AST_Node const& node);

    void visit_function_definition(util::AST_Node const& node);
    void visit_vector_definition(util::AST_Node const& node);
    void visit_label_statement(util::AST_Node const& node);
    void visit_lvalue(util::AST_Node const& node);
    void visit_indirect_lvalue(util::AST_Node const& node);
    void visit_vector_lvalue(util::AST_Node const& node);

  private:
    // clang-format on
    util::AST_Node table_;
};

} // namespace credence::language
