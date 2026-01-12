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

#include <credence/ir/checker.h>

#include <credence/error.h>     // for throw_type_check_error, credence_as...
#include <credence/ir/object.h> // for RValue, Vector, LValue, Object, get_...
#include <credence/types.h>     // for get_type_from_rvalue_data_type, is_d...
#include <credence/util.h>      // for contains, is_numeric, overload
#include <easyjson.h>           // for JSON
#include <fmt/format.h>         // for format
#include <functional>           // for __bind, __ph, bind, _1
#include <matchit.h>            // for app, Ds, App, pattern, ds, Wildcard
#include <memory>               // for shared_ptr
#include <string>               // for basic_string, operator==, char_traits
#include <string_view>          // for basic_string_view
#include <tuple>                // for tuple, get
#include <variant>              // for visit

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
 *    - and more.
 *
 ****************************************************************************/

namespace credence::ir {

namespace m = matchit;

/**
 * @brief Type check pointer and address-of pointer assignments
 */
void Type_Checker::type_safe_assign_pointer(LValue const& lvalue,
    RValue const& rvalue,
    bool indirection)
{
    // pointer to pointer
    auto& locals = get_stack_frame_locals();
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue)) {
        locals.set_symbol_by_name(lvalue,
            type::get_rvalue_data_type_as_string(
                get_rvalue_at_lvalue_object_storage(rvalue,
                    stack_frame_,
                    objects_->get_vectors(),
                    __source__)));
        return; // Ok
    }
    // pointer to address-of lvalue
    if (locals.is_pointer(lvalue) and type::get_unary_operator(rvalue) == "&") {
        // do not allow "x = &string[k]", or pointer to string pointer
        auto data_of = get_rvalue_at_lvalue_object_storage(
            type::get_unary_rvalue_reference(rvalue),
            stack_frame_,
            objects_->get_vectors(),
            __source__);
        if (type::get_type_from_rvalue_data_type(data_of) == "string")
            throw_type_check_error(
                fmt::format(
                    "invalid pointer assignment, right-hand-side '{}' is a "
                    "pointer to string pointer, which is not allowed",
                    rvalue),
                lvalue);
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    // pointer to string literal
    if (locals.is_pointer(lvalue) and
        type::is_rvalue_data_type_string(rvalue)) {
        objects_->get_strings().insert(
            type::get_value_from_rvalue_data_type(rvalue));
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    // pointer to string storage in a vector
    if (locals.is_pointer(lvalue) and is_vector(rvalue)) {
        if (type::get_type_from_rvalue_data_type(
                get_rvalue_at_lvalue_object_storage(
                    rvalue, stack_frame_, objects_->get_vectors())) ==
            "string") {
            locals.set_symbol_by_name(lvalue, rvalue);
            return; // Ok
        }
    }

    auto human_symbol = type::is_rvalue_data_type(rvalue)
                            ? type::get_value_from_rvalue_data_type(
                                  type::get_rvalue_datatype_from_string(rvalue))
                            : rvalue;
    if (indirection) {
        if (!locals.is_pointer(lvalue) or
            type::is_rvalue_data_type_string(lvalue))
            throw_type_check_error(
                fmt::format("invalid pointer dereference assignment, "
                            "left-hand-side "
                            "'{}' is not a pointer",
                    lvalue),
                human_symbol);
        else
            throw_type_check_error(
                fmt::format("invalid pointer dereference assignment, "
                            "right-hand-side "
                            "'{}' is not a pointer",
                    human_symbol),
                lvalue);
    } else {
        if (!locals.is_pointer(lvalue) or
            type::is_rvalue_data_type_string(lvalue))
            throw_type_check_error(fmt::format("invalid pointer assignment, "
                                               "left-hand-side "
                                               "'{}' is not a pointer",
                                       lvalue),
                human_symbol);
        else
            throw_type_check_error(fmt::format("invalid pointer assignment, "
                                               "right-hand-side "
                                               "'{}' is not a pointer",
                                       human_symbol),
                lvalue);
    }
}

/**
 * @brief
 * Type check the left-hand-side lvalue type and right-hand-side of
 * trivial vectors, or vectors with one element. If both are the same type,
 * then the assignment is allowed
 *
 */
void Type_Checker::type_safe_assign_trivial_vector(LValue const& lvalue,
    RValue const& rvalue)
{
    Type_Check_Lambda vector_contains =
        std::bind(&Type_Checker::vector_contains_, this, std::placeholders::_1);
    Type_Check_Lambda local_contains =
        std::bind(&Type_Checker::local_contains_, this, std::placeholders::_1);
    auto& locals = get_stack_frame_locals();
    auto& vectors = objects_->get_vectors();
    m::match(lvalue, rvalue)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(
                    vectors[lvalue], vectors[rvalue], "0");
                vectors[lvalue]->get_data()["0"] =
                    vectors[rvalue]->get_data()["0"];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                type_invalid_assignment_check(rvalue, vectors[lvalue], "0");
                vectors[lvalue]->get_data()["0"] =
                    locals.get_symbol_by_name(rvalue);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                auto vector_rvalue = vectors[rvalue]->get_data().at("0");
                type_invalid_assignment_check(lvalue, vectors[rvalue], "0");
                locals.set_symbol_by_name(
                    lvalue, vectors[rvalue]->get_data()["0"]);
            });
}

/**
 * @brief
 * Type check the left-hand-side lvalue type and right-hand-side with a
 * vector lvalue, If both are the same type, then assign from a vector to lvalue
 * or vice-versa is allowed
 */
void Type_Checker::type_safe_assign_vector(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_locals();
    RValue lvalue_offset{ "0" };
    RValue rvalue_offset{ "0" };
    Type_Check_Lambda vector_contains =
        std::bind(&Type_Checker::vector_contains_, this, std::placeholders::_1);
    Type_Check_Lambda local_contains =
        std::bind(&Type_Checker::local_contains_, this, std::placeholders::_1);
    auto lvalue_direct = m::match(lvalue)(
        m::pattern | m::app(object::is_vector_lvalue, true) =
            [&] {
                lvalue_offset = type::from_decay_offset(lvalue);
                return type::from_lvalue_offset(lvalue);
            },
        m::pattern | m::_ = [&] { return lvalue; });

    auto rvalue_direct = m::match(rvalue)(
        m::pattern | m::app(object::is_vector_lvalue, true) =
            [&] {
                rvalue_offset = type::from_decay_offset(rvalue);
                return type::from_lvalue_offset(rvalue);
            },
        m::pattern | m::_ = [&] { return rvalue; });

    m::match(lvalue_direct, rvalue_direct)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(
                    objects_->get_vectors()[lvalue_direct],
                    objects_->get_vectors()[rvalue_direct],
                    lvalue_offset,
                    rvalue_offset);
                objects_->get_vectors()[lvalue_direct]
                    ->get_data()[lvalue_offset] =
                    objects_->get_vectors()[rvalue_direct]
                        ->get_data()[rvalue_offset];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                type_invalid_assignment_check(rvalue_direct,
                    objects_->get_vectors()[lvalue_direct],
                    lvalue_offset);
                objects_->get_vectors()[lvalue_direct]
                    ->get_data()[lvalue_offset] =
                    locals.get_symbol_by_name(rvalue_direct);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(lvalue_direct,
                    objects_->get_vectors()[rvalue_direct],
                    rvalue_offset);
                locals.set_symbol_by_name(lvalue_direct,
                    objects_->get_vectors()[rvalue_direct]->get_data().at(
                        rvalue_offset));
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Type check the assignment of dereferenced lvalue pointers
 */
void Type_Checker::type_safe_assign_dereference(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_locals();
    auto& vectors = objects_->get_vectors();

    auto lhs_lvalue = type::get_unary_rvalue_reference(lvalue);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rvalue);

    if (locals.is_pointer(lvalue) and type::is_dereference_expression(rvalue))
        throw_type_check_error("invalid pointer dereference, "
                               "right-hand-side is not a pointer",
            lvalue);
    if (locals.is_pointer(rvalue) and type::is_dereference_expression(lvalue))
        throw_type_check_error("invalid pointer dereference, "
                               "right-hand-side is not a pointer",
            lvalue);
    if (type::is_dereference_expression(rvalue)) {
        auto symbol = get_rvalue_at_lvalue_object_storage(
            rhs_lvalue, stack_frame_, vectors, __source__);
        if (type::get_type_from_rvalue_data_type(symbol) == "null")
            throw_type_check_error("invalid pointer dereference, "
                                   "right-hand-side is a null pointer!",
                lvalue);
    }
    if (!locals.is_pointer(lhs_lvalue) and
        not type::is_dereference_expression(rvalue))
        throw_type_check_error("invalid pointer dereference, "
                               "left-hand-side is not a pointer",
            lhs_lvalue);
    if (!locals.is_pointer(rhs_lvalue) and
        not type::is_dereference_expression(lvalue))
        throw_type_check_error("invalid pointer dereference, "
                               "right-hand-side is not a pointer",
            lhs_lvalue);
    if (type::is_dereference_expression(lvalue) and
        type::get_type_from_rvalue_data_type(rvalue) != "null") {
        locals.set_symbol_by_name(lhs_lvalue, rvalue);
        return;
    }

    auto lhs_address = get_rvalue_at_lvalue_object_storage(
        lhs_lvalue, stack_frame_, vectors, __source__);
    auto rhs_address = get_rvalue_at_lvalue_object_storage(
        rhs_lvalue, stack_frame_, vectors, __source__);

    if (type::get_type_from_rvalue_data_type(lhs_address) != "null" and
        !lhs_rhs_type_is_equal(lhs_address, rhs_address))
        throw_type_check_error(
            fmt::format(
                "invalid dereference assignment, dereference rvalue of "
                "left-hand-side with type '{}' is not the same type ({})",
                type::get_type_from_rvalue_data_type(lhs_address),
                type::get_type_from_rvalue_data_type(rhs_address)),
            lvalue);

    locals.set_symbol_by_name(lhs_lvalue, rvalue);
}

/**
 * @brief Type-safe assignment of pointers and vectors, or scalers and
 * dereferenced pointers
 *
 * Note: To get a better idea of how this function works,
 * check the test cases in test/fixtures/types
 */
void Type_Checker::type_safe_assign_pointer_or_vector_lvalue(
    LValue const& lvalue,
    type::RValue_Reference_Type const& rvalue,
    bool indirection)
{
    auto& locals = get_stack_frame_locals();
    // clang-format off
    std::visit(
        util::overload{
        [&](RValue const& value) {
            if (value == "NULL")
                throw_type_check_error(
                    "invalid pointer dereference assignment, "
                    "right-hand-side is a NULL pointer!",
                    lvalue);
            if (is_trivial_vector_assignment(lvalue, value)) {
                type_safe_assign_trivial_vector(lvalue, value);
                return; // Done
            }
            if (is_pointer(lvalue) or is_pointer(value)) {
                if (!type::is_dereference_expression(value)) {
                    type_safe_assign_pointer(lvalue, value);
                    return; // Done
                }
            }
            if (is_vector(lvalue) or is_vector(value)) {
                type_safe_assign_vector(lvalue, value);
                return; // Done
            }
            // A dereference assignment, check for invalid or
            // null pointers
            if (type::is_dereference_expression(lvalue) or
                type::is_dereference_expression(value)) {
                type_safe_assign_dereference(lvalue, value);
                return;
            }
            locals.set_symbol_by_name(lvalue, value);
        },
        [&](type::Data_Type const& value) {
            if (!indirection and locals.is_pointer(lvalue))
                throw_type_check_error(
                    "invalid lvalue assignment, left-hand-side is a "
                    "pointer to non-pointer rvalue",
                    lvalue);
            // the lvalue and rvalue vector data entry type must match
            if (get_type_from_rvalue_data_type(lvalue) != "null" and
                !lhs_rhs_type_is_equal(lvalue, value))
                throw_type_check_error(
                    fmt::format("invalid lvalue assignment, left-hand-side "
                                "'{}' "
                                "with "
                                "type '{}' is not the same type ({})",
                        lvalue,
                        get_type_from_rvalue_data_type(lvalue),
                        type::get_type_from_rvalue_data_type(value)),
                    lvalue);
            locals.set_symbol_by_name(lvalue, value);
        } },
    rvalue);
    // clang-format on
}

/**
 * @brief Get the type from a symbol in the local stack frame
 */
Type Type_Checker::get_type_from_rvalue_data_type(LValue const& lvalue)
{
    auto& locals = get_stack_frame_locals();
    auto& vectors = objects_->get_vectors();
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        return std::get<1>(vectors[lhs_lvalue]->get_data()[offset]);
    }
    return std::get<1>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Get the size from a symbol in the local stack frame
 */
Size Type_Checker::get_size_from_local_lvalue(LValue const& lvalue)
{
    auto& locals = get_stack_frame_locals();
    auto& vectors = objects_->get_vectors();
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        return std::get<2>(vectors[lhs_lvalue]->get_data()[offset]);
    }
    if (get_type_from_rvalue_data_type(lvalue) == "word" and
        locals.is_defined(type::get_unary_rvalue_reference(lvalue))) {
        return std::get<2>(locals.get_symbol_by_name(
            type::get_unary_rvalue_reference(lvalue)));
    }
    return std::get<2>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Check the boundary of a vector or pointer offset
 *.    The allocation size and type data is in the table object
 */
void Type_Checker::is_boundary_out_of_range(RValue const& rvalue)
{
    credence_assert(util::contains(rvalue, "["));
    credence_assert(util::contains(rvalue, "]"));
    auto& vectors = objects_->get_vectors();
    auto lvalue = type::from_lvalue_offset(rvalue);
    auto offset = type::from_decay_offset(rvalue);
    if (!vectors.contains(lvalue))
        throw_type_check_error(
            fmt::format("invalid vector assignment, vector identifier "
                        "'{}' does not "
                        "exist",
                lvalue),
            rvalue);
    if (util::is_numeric(offset)) {
        auto global_symbol = objects_->get_hoisted_symbols()[lvalue];
        auto ul_offset = std::stoul(offset);
        if (ul_offset > object::Vector::max_size)
            throw_type_check_error(
                fmt::format("invalid rvalue, integer offset '{}' is a"
                            "buffer-overflow",
                    ul_offset),
                rvalue);
        if (!vectors.contains(lvalue))
            throw_type_check_error(
                fmt::format(
                    "invalid vector assignment, right-hand-side does not "
                    "exist '{}'",
                    lvalue),
                rvalue);
        if (ul_offset > vectors[lvalue]->get_size() - 1)
            throw_type_check_error(
                fmt::format("invalid out-of-range vector assignment '{}' at "
                            "index "
                            "'{}'",
                    lvalue,
                    ul_offset),
                rvalue);
    } else {
        auto& locals = get_stack_frame_locals();
        if (!locals.is_defined(offset) and
            not stack_frame_->is_scaler_parameter(offset))
            throw_type_check_error(
                fmt::format("invalid vector offset '{}'", offset), rvalue);
    }
}

/**
 * @brief Type checking on vector-to-vector same index assignment
 */
void Type_Checker::type_invalid_assignment_check(
    object::Vector_PTR const& vector_lhs,
    object::Vector_PTR const& vector_rhs,
    RValue const& index)
{
    auto vector_lvalue = vector_lhs->get_data().at(index);
    auto vector_rvalue = vector_rhs->get_data().at(index);
    if (!lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_type_check_error(
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "with type '{}' "
                        "is not the same type ({})",
                vector_lhs->get_symbol(),
                type::get_type_from_rvalue_data_type(vector_lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs->get_symbol());
}

/**
 * @brief Type checking on vector-to-vector index assignment
 */
void Type_Checker::type_invalid_assignment_check(
    object::Vector_PTR const& vector_lhs,
    object::Vector_PTR const& vector_rhs,
    RValue const& index_lhs,
    RValue const& index_rhs)
{
    auto vector_lvalue = vector_lhs->get_data().at(index_lhs);
    auto vector_rvalue = vector_rhs->get_data().at(index_rhs);

    if (!lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_type_check_error(
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "at index '{}' "
                        "with type '{}' is not the same type as "
                        "right-hand-side vector "
                        "'{}' at index '{}' ({})",
                vector_lhs->get_symbol(),
                index_lhs,
                type::get_type_from_rvalue_data_type(vector_lvalue),
                vector_rhs->get_symbol(),
                index_rhs,
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_lhs->get_symbol());
}

/**
 * @brief Type checking on scaler lvalue and rvalues
 */
void Type_Checker::type_invalid_assignment_check(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_locals();
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue))
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_type_check_error(
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                rvalue,
                get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Type checking on lvalue-to-vector assignment
 */
void Type_Checker::type_invalid_assignment_check(LValue const& lvalue,
    object::Vector_PTR const& vector_rhs,
    RValue const& index)
{
    auto vector_rvalue = vector_rhs->get_data().at(index);
    if (get_type_from_rvalue_data_type(lvalue) != "null" and
        not lhs_rhs_type_is_equal(lvalue, vector_rvalue))
        throw_type_check_error(
            fmt::format("invalid lvalue assignment to a "
                        "vector, left-hand-side '{}' with "
                        "type '{}' is not the same type "
                        "({})",
                lvalue,
                get_type_from_rvalue_data_type(lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs->get_symbol());
}

/**
 * @brief Type checking on lvalue and Data_type (e.g. (10:"int":4UL))
 */
void Type_Checker::type_invalid_assignment_check(LValue const& lvalue,
    type::Data_Type const& rvalue)
{
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_type_check_error(
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                std::get<0>(rvalue),
                type::get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

} // namespace ir