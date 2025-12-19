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

namespace credence::ir {

void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast);

/**
 * @brief
 * IR Instruction visitor to build the table object for type and storage data
 */
class Table
{

  public:
    Table() = delete;
    ~Table() = default;

#ifdef CREDENCE_TEST
    explicit Table(ITA::Node const& hoisted_symbols)
    {
        objects_ = std::make_shared<object::Object>(object::Object{});
        objects_->hoisted_symbols = std::move(hoisted_symbols);
        instructions_ = std::make_shared<ir::Instructions>();
        objects_->ir_instructions = instructions_;
        objects_->globals = Symbol_Table<>{};
    }
#endif
    explicit Table(ITA::Node hoisted_symbols,
        Instructions& instructions,
        Symbol_Table<> globals)
    {
        objects_ = std::make_shared<object::Object>(object::Object{});
        instructions_ =
            std::make_shared<ir::Instructions>(std::move(instructions));
        objects_->ir_instructions = instructions_;
        objects_->hoisted_symbols = std::move(hoisted_symbols);
        objects_->globals = std::move(globals);
    }

  public:
    object::Object_PTR& get_table_object() { return objects_; }
    object::Instruction_PTR& get_table_instructions() { return instructions_; }

  public:
    void build_from_ir_instructions();

  public:
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
  private:
    void throw_object_type_error(std::string_view message,
        std::string_view symbol,
        std::string_view type_ = "symbol")
    {
        throw_compiletime_error(message,
            symbol,
            __source__,
            type_,
            objects_->get_stack_frame()->symbol,
            objects_->hoisted_symbols);
    }

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::semantic::Address instruction_index{ 0 };
    object::Instruction_PTR instructions_{};
    object::Object_PTR objects_;
};
// clang-format on
} // namespace credence::ir