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
#include <credence/util.h>  // for contains, str_trim_ws
#include <fmt/format.h>     // for format
#include <functional>       // for function
#include <initializer_list> // for initializer_list
#include <memory>           // for shared_ptr
#include <source_location>  // for source_location
#include <string>           // for basic_string, operator==, char_traits
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

struct Storage_Type_Checker
{
    Storage_Type_Checker() = delete;
    Storage_Type_Checker(Storage_Type_Checker const&) = delete;
    Storage_Type_Checker& operator=(Storage_Type_Checker const&) = delete;
    explicit Storage_Type_Checker(hir::binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
    {
    }

    // @TODO: All needs to go in object_table

    using Lambda_Checker = std::function<bool(type::semantic::LValue)>;

    inline bool is_null_symbol(LValue const& lvalue)
    {
        if (is_vector(lvalue))
            return false;
        if (is_pointer(lvalue))
            return stack_frame_.get_locals().get_pointer_by_name(lvalue) ==
                   "NULL";
        return type::get_type_from_rvalue_data_type(
                   stack_frame_.get_locals().get_symbol_by_name(
                       util::str_trim_ws(lvalue).data())) == "null";
    }

    inline bool is_vector_or_pointer(RValue const& rvalue)
    {
        return is_vector(rvalue) or is_pointer(rvalue) or
               type::is_dereference_expression(rvalue);
    }

    inline bool is_vector(RValue const& rvalue)
    {
        auto label = util::contains(rvalue, "[")
                         ? type::from_lvalue_offset(rvalue)
                         : rvalue;
        return objects_.get_vectors().contains(label);
    }
    inline bool is_pointer(RValue const& rvalue)
    {
        return stack_frame_.get_locals().is_pointer(rvalue) or
               rvalue.starts_with("&") or
               type::is_rvalue_data_type_string(rvalue);
    }

    bool vector_contains_(type::semantic::LValue const& lvalue)
    {
        return objects_.get_vectors().contains(lvalue);
    }
    bool local_contains_(type::semantic::LValue const& lvalue)
    {
        const auto& locals = stack_frame_.get_locals();
        return locals.is_defined(lvalue) and
               not binding::is_vector_lvalue(lvalue);
    }

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;
};

struct Pointer_Vector_Decay_Checker
{
    Pointer_Vector_Decay_Checker() = delete;
    Pointer_Vector_Decay_Checker(Pointer_Vector_Decay_Checker const&) = delete;
    Pointer_Vector_Decay_Checker& operator=(
        Pointer_Vector_Decay_Checker const&) = delete;
    explicit Pointer_Vector_Decay_Checker(hir::binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
        , storage_type_(objects)
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

    inline bool is_null_symbol(LValue const& lvalue)
    {
        if (storage_type_.is_vector(lvalue))
            return false;
        if (storage_type_.is_pointer(lvalue))
            return stack_frame_.get_locals().get_pointer_by_name(lvalue) ==
                   "NULL";
        return type::get_type_from_rvalue_data_type(
                   stack_frame_.get_locals().get_symbol_by_name(
                       util::str_trim_ws(lvalue).data())) == "null";
    }

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;

  private:
    Storage_Type_Checker storage_type_;
};

struct Value_Type_Assignment_Checker
{
    Value_Type_Assignment_Checker() = delete;
    Value_Type_Assignment_Checker(
        Value_Type_Assignment_Checker const&) = delete;
    Value_Type_Assignment_Checker& operator=(
        Value_Type_Assignment_Checker const&) = delete;
    explicit Value_Type_Assignment_Checker(hir::binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
    {
    }
    void type_invalid_assignment_check(LValue const& lvalue,
        RValue const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        type::Data_Type const& rvalue);
    void type_invalid_assignment_check(LValue const& lvalue,
        binding::Vector const& vector,
        RValue const& index);
    void type_invalid_assignment_check(binding::Vector const& vector_lhs,
        binding::Vector const& vector_rhs,
        RValue const& index);
    void type_invalid_assignment_check(binding::Vector const& vector_lhs,
        binding::Vector const& vector_rhs,
        RValue const& index_lhs,
        RValue const& index_rhs);

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;
};

struct Value_Storage_Assignment_Checker
{
    Value_Storage_Assignment_Checker() = delete;
    Value_Storage_Assignment_Checker(
        Value_Storage_Assignment_Checker const&) = delete;
    Value_Storage_Assignment_Checker& operator=(
        Value_Storage_Assignment_Checker const&) = delete;
    explicit Value_Storage_Assignment_Checker(
        hir::binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
    {
    }

    inline bool is_trivial_vector_assignment(LValue const& lhs,
        LValue const& rhs)
    {
        return ((objects_->get_vectors().contains(lhs) and
                    objects_->get_vectors()[lhs]->get_data().size() == 1) or
                (objects_->get_vectors().contains(rhs) and
                    objects_->get_vectors()[rhs]->get_data().size() == 1));
    }

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;
};

class Trivial_Type_Assignment_Checker
{
    Trivial_Type_Assignment_Checker() = delete;
    Trivial_Type_Assignment_Checker(
        Trivial_Type_Assignment_Checker const&) = delete;
    Trivial_Type_Assignment_Checker& operator=(
        Trivial_Type_Assignment_Checker const&) = delete;
    explicit Trivial_Type_Assignment_Checker(
        hir::binding::Object_Table& objects)
        : objects_(objects)
        , stack_frame_(objects.get_stack_frame())
    {
    }

  private:
    type::Locals& get_stack_frame_locals()
    {
        return stack_frame_->get_locals();
    }

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

    inline bool lhs_rhs_type_is_equal(LValue const& lhs, LValue const& rhs)
    {
        return get_type_from_rvalue_data_type(objects, lhs) ==
               get_type_from_rvalue_data_type(objects, rhs);
    }
    inline bool lhs_rhs_type_is_equal(LValue const& lhs,
        type::Data_Type const& rvalue)
    {
        return get_type_from_rvalue_data_type(objects, lhs) ==
               std::get<1>(rvalue);
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
            objects_.get_stack_frame().get_symbol(),
            objects_.get_hoisted_symbols());
    }

  private:
    binding::Object_Table& objects_;
    binding::Function& stack_frame_;
};

} // namespace ir