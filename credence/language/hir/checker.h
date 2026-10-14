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

#include "binding.h"        // for Object_Table, Function, Vector
#include "types.h"          // for RValue, LValue, ...
#include <algorithm>        // for __find, find
#include <credence/error.h> // for throw_compiletime_error
#include <credence/types.h> // for get_type_from_rvalue_data_type, Data...
#include <credence/util.h>  // for __source__
#include <fmt/format.h>     // for format
#include <initializer_list> // for initializer_list
#include <source_location>  // for source_location
#include <string_view>      // for basic_string_view
#include <tuple>            // for get, tuple

/****************************************************************************
 *  Type Checker
 *
 *  Type checker for the intermediate representation. Validates assignments
 *  between scalars, vectors, pointers, and handles type conversions with
 *  proper boundary and null checks.
 *
 *  Example type checking:
 *
 *  auto x, *p;
 *  auto arr[10];
 *  x = 5;           // scalar assignment
 *  arr[0] = x;      // vector assignment with bounds check
 *  p = &x;          // pointer assignment
 *
 *  Type_Checker validates:
 *    - Type compatibility between lhs and rhs
 *    - Vector boundary access (arr[0..9] valid, arr[10] error)
 *    - Pointer targets (no &string[k] allowed)
 *    - Null assignments
 *    - Implicit conversions where safe
 *    - Raises compile-time errors on violations
 *    - Supports lvalue and rvalue type resolution
 *    - and more
 *
 ****************************************************************************/

namespace credence::language::hir::checking {

/**
 * @brief Raise a type check error in the current stack frame
 */
inline void throw_type_check_error(binding::Object_Table& objects,
    std::string_view message,
    std::string_view symbol,
    std::string_view type_ = "symbol")
{
    throw_compiletime_error(message,
        symbol,
        __source__,
        type_,
        objects.get_stack_frame().symbol,
        objects.hoisted_symbols);
}

/**
 * @brief Type equality and integral checks on lvalues and rvalues
 */
class Trivial_Type_Assignment_Checker
{
  public:
    Trivial_Type_Assignment_Checker() = delete;
    Trivial_Type_Assignment_Checker(
        Trivial_Type_Assignment_Checker const&) = delete;
    Trivial_Type_Assignment_Checker& operator=(
        Trivial_Type_Assignment_Checker const&) = delete;
    explicit Trivial_Type_Assignment_Checker(binding::Object_Table& objects)
        : objects_(objects)
    {
    }

  public:
    /**
     * @brief Check that the type of an lvalue is an integral type
     */
    inline void assert_integral_unary_expression(RValue const& rvalue,
        Type const& type,
        std::source_location const& location = std::source_location::current())
    {
        if (std::ranges::find(type::integral_unary_types, type) ==
            type::integral_unary_types.end())
            throw_compiletime_error(
                fmt::format(
                    "invalid numeric unary expression on lvalue, lvalue "
                    "type "
                    "\"{}\" is not a numeric type",
                    type),
                rvalue,
                location);
    }

    /** Left-hand-side and right-hand-side type equality check */
    inline bool lhs_rhs_type_is_equal(LValue const& lhs, LValue const& rhs)
    {
        return objects_.get_type_from_object_data_type(lhs) ==
               objects_.get_type_from_object_data_type(rhs);
    }
    inline bool lhs_rhs_type_is_equal(LValue const& lhs,
        type::Data_Type const& rvalue)
    {
        return objects_.get_type_from_object_data_type(lhs) ==
               std::get<1>(rvalue);
    }

    inline bool lhs_rhs_type_is_equal(type::Data_Type const& lhs,
        type::Data_Type const& rhs)
    {
        return type::get_type_from_rvalue_data_type(lhs) ==
               type::get_type_from_rvalue_data_type(rhs);
    }

  private:
    binding::Object_Table& objects_;
};

/**
 * @brief Type checking on lvalue, rvalue, and vector assignments
 */
struct Value_Type_Assignment_Checker
{
    Value_Type_Assignment_Checker() = delete;
    Value_Type_Assignment_Checker(
        Value_Type_Assignment_Checker const&) = delete;
    Value_Type_Assignment_Checker& operator=(
        Value_Type_Assignment_Checker const&) = delete;
    explicit Value_Type_Assignment_Checker(binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
        , trivial_type_(objects)
    {
    }
    void type_invalid_assignment_check(LValue const& lvalue,
        RValue const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        type::Data_Type const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        binding::Vector& vector,
        RValue const& index);
    void type_invalid_assignment_check(binding::Vector& vector_lhs,
        binding::Vector& vector_rhs,
        RValue const& index);
    void type_invalid_assignment_check(binding::Vector& vector_lhs,
        binding::Vector& vector_rhs,
        RValue const& index_lhs,
        RValue const& index_rhs);

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;

  private:
    Trivial_Type_Assignment_Checker trivial_type_;
};

/**
 * @brief Storage checks on vector assignments
 */
struct Value_Storage_Assignment_Checker
{
    Value_Storage_Assignment_Checker() = delete;
    Value_Storage_Assignment_Checker(
        Value_Storage_Assignment_Checker const&) = delete;
    Value_Storage_Assignment_Checker& operator=(
        Value_Storage_Assignment_Checker const&) = delete;
    explicit Value_Storage_Assignment_Checker(binding::Object_Table& objects)
        : objects_(objects)
    {
    }

    /**
     * @brief Either lhs or rhs are trivial vector assignments
     */
    inline bool is_trivial_vector_assignment(LValue const& lhs,
        LValue const& rhs)
    {
        return ((objects_.vectors.contains(lhs) and
                    objects_.vectors.at(lhs).get().data.size() == 1) or
                (objects_.vectors.contains(rhs) and
                    objects_.vectors.at(rhs).get().data.size() == 1));
    }

  private:
    binding::Object_Table& objects_;
};

/**
 * @brief Type-safe assignment and decay of pointers and vectors
 */
struct Pointer_Vector_Decay_Checker
{
    Pointer_Vector_Decay_Checker() = delete;
    Pointer_Vector_Decay_Checker(Pointer_Vector_Decay_Checker const&) = delete;
    Pointer_Vector_Decay_Checker& operator=(
        Pointer_Vector_Decay_Checker const&) = delete;
    explicit Pointer_Vector_Decay_Checker(binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
        , trivial_type_(objects)
        , value_type_(objects)
        , value_storage_(objects)
    {
    }
    void type_safe_assign_pointer(LValue const& lvalue,
        RValue const& rvalue,
        bool indirection = false);
    void type_safe_assign_trivial_vector(LValue const& lvalue,
        RValue const& rvalue);
    void type_safe_assign_dereference(LValue const& lvalue,
        RValue const& rvalue);
    void type_safe_assign_vector(LValue const& lvalue, RValue const& rvalue);
    void type_safe_assign_pointer_or_vector_lvalue(LValue const& lvalue,
        type::RValue_Reference_Type const& rvalue,
        bool indirection = false);

    void is_boundary_out_of_range(RValue const& rvalue);

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;

  private:
    Trivial_Type_Assignment_Checker trivial_type_;
    Value_Type_Assignment_Checker value_type_;
    Value_Storage_Assignment_Checker value_storage_;
};

} // namespace credence::language::hir::checking
