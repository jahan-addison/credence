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

#include <credence/ir/ita.h>             // for Quadruple, Instructions, ITA
#include <credence/language/hir/table.h> // for Object_Table_PTR, Table
#include <credence/object.h>             // for Object_Table, LValue, Label
#include <credence/symbol.h>             // for Symbol_Table
#include <credence/types.h> // for Data_Type, Address, Parameters, RVal...
#include <credence/util.h>  // for CREDENCE_PRIVATE_UNLESS_TESTED, AST_...
#include <easyjson.h>       // for JSON
#include <iosfwd>           // for ostream
#include <memory>           // for make_shared
#include <string_view>      // for basic_string_view, string_view
#include <utility>          // for move

/****************************************************************************
 * Table
 *
 * Binds the ITA instructions of a program onto the object table built by
 * the semantic analysis pass in hir/table.h. A visitor pattern on ITA
 * instructions that stores function frame addresses, label addresses,
 * temporaries, call parameters, and the storage of each local for backend
 * passes. Type checking is done on the HIR, before the IR is constructed.
 *
 *  Example table construction:
 *
 *  main() {
 *    auto x,
 *    auto arr[3];
 *    x = 10;
 *    arr[0] = x;
 *  }
 *
 *  Builds Object table:
 *    functions["main"] -> { locals: {x: (10:int:4)} }
 *    vectors["arr"]    -> { size: 3, data: {"0": (10:int:4)} }
 *
 ****************************************************************************/

namespace credence::ir {

using Object_Table_PTR = language::hir::Object_Table_PTR;
using Instructions_PTR = std::shared_ptr<Instructions>;

void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast);

class Table
{

  public:
    Table() = delete;
    ~Table() = default;

#ifdef CREDENCE_TEST
    explicit Table(ITA::Node const& hoisted_symbols)
    {
        objects_ = std::make_shared<object::Object_Table>();
        objects_->hoisted_symbols = hoisted_symbols;
        instructions_ = std::make_shared<ir::Instructions>();
        objects_->globals = Symbol_Table<>{};
    }
#endif
    explicit Table(Object_Table_PTR objects, Instructions& instructions)
        : instructions_(
              std::make_shared<ir::Instructions>(std::move(instructions)))
        , objects_(std::move(objects))
    {
    }

  public:
    Object_Table_PTR& get_table_object() { return objects_; }
    Instructions_PTR& get_table_instructions() { return instructions_; }

  public:
    void build_from_ir_instructions();

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction(Label const& label);
    void from_func_end_ita_instruction();
    void from_call_ita_instruction(Label const& label);
    void from_globl_ita_instruction(Label const& label);
    void from_locl_ita_instruction(Quadruple const& instruction);
    void from_push_instruction(Quadruple const& instruction);
    void from_pop_instruction(Quadruple const& instruction);
    void from_return_instruction(Quadruple const& instruction);
    void from_label_ita_instruction(Quadruple const& instruction);
    void from_mov_ita_instruction(Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::Data_Type from_integral_unary_expression(RValue const& lvalue);
    void from_temporary_reassignment(LValue const& lhs, LValue const& rhs);
    void from_temporary_assignment(LValue const& lhs, LValue const& rhs);
    type::Data_Type from_rvalue_unary_expression(LValue const& lvalue,
        RValue const& rvalue,
        type::RValue_Reference unary_operator);
    void from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs);
    void from_pointer_or_vector_assignment(LValue const& lhs,
        LValue const& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void bind_pointer_or_vector_lvalue(LValue const& lvalue,
        RValue const& rvalue);
    void bind_pointer(LValue const& lvalue, RValue const& rvalue);
    void bind_trivial_vector(LValue const& lvalue, RValue const& rvalue);
    void bind_vector(LValue const& lvalue, RValue const& rvalue);
    void bind_dereference(LValue const& lvalue, RValue const& rvalue);
    bool is_trivial_vector_assignment(LValue const& lhs, LValue const& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::Parameters temporary_parameter_stack{};
    type::semantic::Address instruction_index{ 0 };
    Instructions_PTR instructions_{};
    Object_Table_PTR objects_;
};
// clang-format on

bool stack_frame_contains_call_instruction(object::Function const& frame,
    Instructions const& instructions);

/**
 * @brief Table factory from the symbols and definitions of a program
 *
 * Runs semantic analysis on the HIR, constructs the ITA instructions, then
 * binds the instructions onto the object table
 */
inline std::shared_ptr<Table> make_table(util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    auto objects = language::hir::Table::build(symbols, ast);
    auto instructions = ITA::make_ita_instructions(symbols, ast);
    auto table = std::make_shared<Table>(objects, instructions);
    table->build_from_ir_instructions();
    return table;
}

} // namespace credence::ir
