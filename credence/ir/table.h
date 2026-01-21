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

#include <credence/ir/ita.h>    // for Quadruple, Instructions, ITA
#include <credence/ir/object.h> // for Object, LValue, Label, Object_PTR
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for Data_Type, Address, RValue_Reference
#include <credence/util.h>      // for CREDENCE_PRIVATE_UNLESS_TESTED, AST_...
#include <easyjson.h>           // for JSON
#include <iosfwd>               // for ostream
#include <memory>               // for make_shared
#include <string>               // for basic_string
#include <utility>              // for move

/****************************************************************************
 * Table
 *
 * Table constructor for language objects. A visitor pattern on ITA instructions
 * to construct the object table of a program, including function frames,
 * vectors, locals, and globals and performs type checking on all assignments.
 * The result is stored in an ir::object::Object for backend passes.
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
        objects_ = std::make_shared<object::Object>(object::Object{});
        objects_->get_hoisted_symbols() = std::move(hoisted_symbols);
        instructions_ = std::make_shared<ir::Instructions>();
        objects_->get_ir_instructions() = instructions_;
        objects_->get_globals() = Symbol_Table<>{};
    }
#endif
    explicit Table(ITA::Node hoisted_symbols,
        Instructions& instructions,
        Symbol_Table<> globals)
    {
        objects_ = std::make_shared<object::Object>(object::Object{});
        instructions_ =
            std::make_shared<ir::Instructions>(std::move(instructions));
        objects_->get_ir_instructions() = instructions_;
        objects_->get_hoisted_symbols() = std::move(hoisted_symbols);
        objects_->get_globals() = std::move(globals);
    }

  public:
    object::Object_PTR& get_table_object() { return objects_; }
    object::Instruction_PTR& get_table_instructions() { return instructions_; }

  public:
    void build_from_ir_instructions();
    void build_vector_definitions_from_symbols();
    void build_vector_definitions_from_globals();

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
        LValue const& rhs,
        bool indirection = false);
  private:
    void from_trivial_vector_assignment(LValue const& lhs,
        type::Data_Type const& rvalue);
    void insert_address_storage_rvalue(RValue const& rvalue);
    void insert_address_storage_rvalue(type::Data_Type const& rvalue);
  private:
    void throw_object_type_error(std::string_view message,
        std::string_view symbol,
        std::string_view type_ = "symbol",
        std::source_location const& location = __source__)
    {
        throw_compiletime_error(message,
            symbol,
            location,
            type_,
            objects_->get_stack_frame()->get_symbol(),
            objects_->get_hoisted_symbols());
    }

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::Parameters temporary_parameter_stack{};
    type::semantic::Address instruction_index{ 0 };
    object::Instruction_PTR instructions_{};
    object::Object_PTR objects_;
};
// clang-format on
} // namespace credence::ir