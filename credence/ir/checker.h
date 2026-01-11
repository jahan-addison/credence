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

#include <algorithm>            // for __find, find
#include <credence/error.h>     // for throw_compiletime_error
#include <credence/ir/object.h> // for LValue, RValue, Object, Function
#include <credence/types.h>     // for get_type_from_rvalue_data_type, Data...
#include <credence/util.h>      // for contains, str_trim_ws
#include <fmt/format.h>         // for format
#include <functional>           // for function
#include <initializer_list>     // for initializer_list
#include <memory>               // for shared_ptr
#include <source_location>      // for source_location
#include <string>               // for basic_string, operator==, char_traits
#include <string_view>          // for basic_string_view
#include <tuple>                // for get, tuple

/****************************************************************************
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
 *
 ****************************************************************************/

namespace credence::ir {

/**
 * @brief The discrete type, vector, and pointer assignment type checker
 */
class Type_Checker
{

  public:
    Type_Checker() = delete;
    Type_Checker(Type_Checker const&) = delete;
    Type_Checker& operator=(Type_Checker const&) = delete;

  public:
    explicit Type_Checker(object::Object_PTR& objects,
        object::Function_PTR& stack_frame)
        : objects_(objects)
        , stack_frame_(stack_frame)
    {
    }

    friend class Table;

  private:
    using Type_Check_Lambda = std::function<bool(type::semantic::LValue)>;
    bool vector_contains_(type::semantic::LValue const& lvalue)
    {
        return objects_->get_vectors().contains(lvalue);
    }
    bool local_contains_(type::semantic::LValue const& lvalue)
    {
        const auto& locals = stack_frame_->get_locals();
        return locals.is_defined(lvalue) and
               not object::is_vector_lvalue(lvalue);
    }

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void type_invalid_assignment_check(LValue const& lvalue,
        RValue const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        type::Data_Type const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        object::Vector_PTR const& vector,
        RValue const& index);
    void type_invalid_assignment_check(object::Vector_PTR const& vector_lhs,
        object::Vector_PTR const& vector_rhs,
        RValue const& index);
    void type_invalid_assignment_check(object::Vector_PTR const& vector_lhs,
        object::Vector_PTR const& vector_rhs,
        RValue const& index_lhs,
        RValue const& index_rhs);

  private:
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

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void is_boundary_out_of_range(RValue const& rvalue);

    // clang-format on
  public:
    Type get_type_from_rvalue_data_type(LValue const& lvalue);
    Size get_size_from_local_lvalue(LValue const& lvalue);

  public:
    /**
     * @brief Either lhs or rhs are trivial vector assignments
     */
    inline bool is_trivial_vector_assignment(LValue const& lhs,
        LValue const& rhs)
    {
        return ((objects_->get_vectors().contains(lhs) and
                    objects_->get_vectors()[lhs]->get_data().size() == 1) or
                (objects_->get_vectors().contains(rhs) and
                    objects_->get_vectors()[rhs]->get_data().size() == 1));
    }

    inline bool is_vector(RValue const& rvalue)
    {
        auto label = util::contains(rvalue, "[")
                         ? type::from_lvalue_offset(rvalue)
                         : rvalue;
        return objects_->get_vectors().contains(label);
    }
    inline bool is_pointer(RValue const& rvalue)
    {
        return stack_frame_->get_locals().is_pointer(rvalue) or
               rvalue.starts_with("&") or
               type::is_rvalue_data_type_string(rvalue);
    }
    inline bool is_null_symbol(LValue const& lvalue)
    {
        if (is_vector(lvalue))
            return false;
        if (is_pointer(lvalue))
            return stack_frame_->get_locals().get_pointer_by_name(lvalue) ==
                   "NULL";
        return type::get_type_from_rvalue_data_type(
                   stack_frame_->get_locals().get_symbol_by_name(
                       util::str_trim_ws(lvalue).data())) == "null";
    }

    inline bool is_vector_or_pointer(RValue const& rvalue)
    {
        return is_vector(rvalue) or is_pointer(rvalue) or
               type::is_dereference_expression(rvalue);
    }

  private:
    type::Locals& get_stack_frame_locals()
    {
        return stack_frame_->get_locals();
    }
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
        return get_type_from_rvalue_data_type(lhs) ==
               get_type_from_rvalue_data_type(rhs);
    }
    inline bool lhs_rhs_type_is_equal(LValue const& lhs,
        type::Data_Type const& rvalue)
    {
        return get_type_from_rvalue_data_type(lhs) == std::get<1>(rvalue);
    }

    inline bool lhs_rhs_type_is_equal(type::Data_Type const& lhs,
        type::Data_Type const& rhs)
    {
        return type::get_type_from_rvalue_data_type(lhs) ==
               type::get_type_from_rvalue_data_type(rhs);
    }

  private:
    void throw_type_check_error(std::string_view message,
        std::string_view symbol,
        std::string_view type_ = "symbol")
    {
        throw_compiletime_error(message,
            symbol,
            __source__,
            type_,
            objects_->get_stack_frame()->get_symbol(),
            objects_->get_hoisted_symbols());
    }

  private:
    object::Object_PTR& objects_;
    object::Function_PTR& stack_frame_;
};

} // namespace ir