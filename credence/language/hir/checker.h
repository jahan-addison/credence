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

#include "../literal.h"    // for Literal
#include "hir.h"           // for Data_Kind
#include "types.h"         // for LValue, RValue, Type, Label
#include <credence/util.h> // for AST_Node, CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>         // for size_t
#include <map>             // for map
#include <optional>        // for optional
#include <set>             // for set
#include <source_location> // for source_location
#include <string_view>     // for string_view
#include <vector>          // for vector

/****************************************************************************
 *  Type Checker
 *
 *  Type checker on the HIR. Each expression evaluates to the storage and
 *  type of its value in a stack frame, and an assignment between storage
 *  or types that do not agree is a compile-time error.
 *
 *  Example type checking:
 *
 *  main() {
 *    auto x, *p;
 *    x = 5;
 *    p = &x;
 *    p = x;
 *  }
 *
 *  Becomes:
 *
 *    x  -> scalar int
 *    p  -> pointer to int
 *    error: invalid pointer assignment, right-hand-side 'x' is not a pointer
 *
 ****************************************************************************/

namespace credence::language::hir::checking {

/**
 * @brief The storage of an object or expression value
 */
enum class Storage
{
    SCALAR,
    POINTER
};

/**
 * @brief The storage and type of an object or expression value
 *
 * A pointer's type is the type of the object it points to, and "null" until
 * it's assigned. A pointer from an address-of expression also keeps the
 * lvalue it points to, so a dereference resolves to that object's current
 * type. A scalar from a literal keeps its constant, so a vector offset of
 * that scalar is bounds checked. A "word" is a machine word with no narrower
 * type, such as a parameter, a call result, or a binary expression.
 */
struct Value
{
    Storage storage{ Storage::SCALAR };
    Type type{ "null" };
    std::optional<LValue> address{};
    std::optional<RValue> constant{};
};

/**
 * @brief A vector definition and the types of its assigned elements
 */
struct Vector
{
    std::size_t size{ 0 };
    std::map<RValue, Value> data{};
    std::optional<Value> unknown_offset{};
};

using Vectors = std::map<LValue, Vector>;

/**
 * @brief The objects, vectors, labels, and return type of a function
 */
struct Frame
{
    Label symbol{};
    std::map<LValue, Value> locals{};
    std::set<LValue> parameters{};
    Vectors vectors{};
    Labels labels{};
    std::optional<Type> ret{};
};

/**
 * @brief An argument of a call to a function of the program
 *
 * A parameter of the callee is a pointer when any call passes a pointer to
 * it, or passes a parameter of the caller that is a pointer, so each
 * argument is recorded for the table to resolve after every function is
 * built
 *
 *  Example:
 *
 *    show(s) { printf("%s", s); }
 *    main(argc, argv) { show(argv[1]); }
 *
 *  Becomes:
 *
 *    Call_Argument{ "main", "show", 0, true, "" }   // s is a pointer
 */
struct Call_Argument
{
    Label caller{};
    Label callee{};
    std::size_t index{};
    bool pointer{ false };
    LValue parameter{};
};

using Call_Arguments = std::vector<Call_Argument>;

/**
 * @brief Type checker of HIR expressions in a stack frame
 */
class Type_Checker
{
  public:
    Type_Checker() = delete;
    Type_Checker(Type_Checker const&) = delete;
    Type_Checker& operator=(Type_Checker const&) = delete;
    explicit Type_Checker(Frame& frame,
        Vectors& globals,
        util::AST_Node const& hoisted_symbols,
        Call_Arguments* call_arguments = nullptr)
        : frame_(frame)
        , globals_(globals)
        , hoisted_symbols_(hoisted_symbols)
        , call_arguments_(call_arguments)
    {
    }

  public:
    static constexpr std::size_t max_vector_size{ 999 };

  public:
    Value from_expression(Data_Kind::Kind const& expression);
    void from_return_expression(Data_Kind::Kind const& expression);
    void assign(LValue const& lvalue,
        Value const& rvalue,
        RValue const& symbol);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Value from_literal(literal::Literal const& literal);
    Value from_lvalue(LValue const& lvalue);
    Value from_vector_offset(LValue const& lvalue);
    Value from_unary_expression(Data_Kind::Unary const& unary);
    Value from_address_of_expression(Data_Kind::Kind const& operand);
    Value from_dereference_expression(Data_Kind::Kind const& operand);
    Value from_dereference(Value const& value, RValue const& symbol);
    Value from_increment_expression(Data_Kind::Kind const& operand);
    Value from_relation_expression(Data_Kind::Relation const& relation);
    Value from_function_expression(Data_Kind::Function const& function);
    Value from_assignment_expression(Data_Kind::Symbol const& assignment);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void assign_to_scalar(LValue const& lvalue,
        Value const& rvalue,
        RValue const& symbol);
    void assign_to_pointer(LValue const& lvalue,
        Value const& rvalue,
        RValue const& symbol);
    void assign_to_dereference(LValue const& lvalue,
        Value const& rvalue,
        RValue const& symbol);
    void assign_to_vector(LValue const& lvalue,
        Value const& rvalue,
        RValue const& symbol);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Vector* get_vector_at_offset(LValue const& lvalue);
    RValue get_constant_offset(LValue const& lvalue);
    Type get_pointer_type(Value const& value);
    Vector* get_vector(LValue const& label);
    bool is_parameter_or_pointer(LValue const& label);
    // clang-format on

  public:
    static bool is_compatible(Type const& lhs, Type const& rhs);
    static bool is_numeric(Type const& type);

  private:
    void type_error(std::string_view message,
        std::string_view symbol,
        std::source_location const& location = std::source_location::current());

  private:
    Frame& frame_;
    Vectors& globals_;
    util::AST_Node const& hoisted_symbols_;
    Call_Arguments* call_arguments_;
};

RValue get_symbol_from_expression(Data_Kind::Kind const& expression);

Value get_value_from_literal(literal::Literal const& literal);

} // namespace credence::language::hir::checking
