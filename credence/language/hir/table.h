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

#include "checker.h"         // for Frame, Value, Vectors
#include "types.h"           // for Label, LValue
#include <credence/object.h> // for Object_Table
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>           // for size_t
#include <map>               // for map
#include <memory>            // for shared_ptr, make_shared
#include <set>               // for set
#include <source_location>   // for source_location
#include <string_view>       // for string_view
#include <vector>            // for vector

/****************************************************************************
 * Table
 *
 * Semantic analysis on the HIR. A pass over each definition and statement
 * of a program that lowers its expressions into the HIR and type checks
 * them in their stack frame. The result is the definitions of the object
 * table - vectors, functions, and parameters - that the IR binds onto. A
 * global variable is a vector of one element.
 *
 *  Example:
 *
 *  main() {
 *    extrn unit;
 *    auto x, arr[3];
 *    x = unit;
 *  }
 *
 *  unit 10;
 *
 *  Becomes:
 *
 *    functions["main"] -> { parameters: {} }
 *    vectors["arr"]    -> { size: 3, data: {} }
 *    vectors["unit"]   -> { size: 1, data: {"0": (10:int:4)} }
 *
 ****************************************************************************/

namespace credence::language::hir {

using Object_Table_PTR = std::shared_ptr<object::Object_Table>;

/**
 * @brief Semantic analysis and object table definitions from the HIR
 */
class Table
{
  public:
    Table() = delete;
    Table(Table const&) = delete;
    Table& operator=(Table const&) = delete;
    Table(Table&&) = default;
    Table& operator=(Table&&) = default;
    explicit Table(util::AST_Node const& hoisted_symbols)
        : hoisted_symbols_(hoisted_symbols)
        , objects_(std::make_shared<object::Object_Table>())
    {
    }

  public:
    using Node = util::AST_Node;

  public:
    /**
     * @brief Object table factory from the symbols and program node of a
     * program
     */
    static inline Object_Table_PTR build(Node const& hoisted_symbols,
        Node const& program)
    {
        auto table = Table{ hoisted_symbols };
        table.build_from_definitions(program);
        return table.objects_;
    }

  public:
    void build_from_definitions(Node const& node);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_function_definition(Node const& node);
    void build_from_union_definition(Node const& node);
    void build_from_vector_definition(Node const& node);
    void build_global_vector(Label const& name,
        std::size_t size,
        Node const& values);
    void build_vector_definitions_from_symbols();
    void build_vector_definitions_from_globals();
    void set_pointer_parameters_from_call_arguments();
    void set_unset_global_values();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_declarations(Node const& node);
    void build_from_variable_declaration(Node const& node);
    void build_from_union_declaration(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void build_from_statement(Node const& node);
    void build_from_block_statement(Node const& node);
    void build_from_auto_statement(Node const& node);
    void build_from_union_statement(Node const& node);
    void build_from_extrn_statement(Node const& node);
    void build_from_if_statement(Node const& node);
    void build_from_while_statement(Node const& node);
    void build_from_switch_statement(Node const& node);
    void build_from_label_statement(Node const& node);
    void build_from_goto_statement(Node const& node);
    void build_from_return_statement(Node const& node);
    void build_from_rvalue_statement(Node const& node);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    checking::Value from_expression_node(Node const& node);
    std::vector<Node> get_expression_nodes(Node const& node);
    checking::Frame& get_stack_frame();
    void set_union_variable(LValue const& name, Label const& label);
    void set_union_tag_symbols();
    // clang-format on

  private:
    void semantic_error(std::string_view message,
        std::string_view symbol,
        std::source_location const& location = std::source_location::current());

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Node hoisted_symbols_;
    Symbol_Table<> symbols_{};
    Symbol_Table<> globals_{};
    checking::Vectors vectors_{};
    checking::Unions unions_{};
    // each global union variable and the label of its union definition
    std::map<LValue, Label> union_globals_{};
    // each global variable with no initial value
    std::set<LValue> unset_globals_{};
    std::map<Label, checking::Frame> frames_{};
    checking::Call_Arguments call_arguments_{};
    Label stack_frame_symbol_{};
    Object_Table_PTR objects_;
    // clang-format on
};

} // namespace credence::language::hir
