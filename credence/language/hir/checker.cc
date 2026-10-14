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

#include "checker.h"

#include "binding.h"        // for RValue, Vector, LValue, Object_Table
#include <credence/error.h> // for credence_assert, credence_error
#include <credence/types.h> // for get_type_from_rvalue_data_type, is_d...
#include <credence/util.h>  // for contains, is_numeric, overload
#include <easyjson.h>       // for JSON
#include <fmt/format.h>     // for format
#include <matchit.h>        // for app, Ds, App, pattern, ds, Wildcard
#include <string>           // for basic_string, operator==, char_traits
#include <tuple>            // for tuple, get
#include <variant>          // for visit

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

namespace m = matchit;

/**
 * @brief Type check pointer and address-of pointer assignments
 */
void Pointer_Vector_Decay_Checker::type_safe_assign_pointer(
    LValue const& lvalue,
    RValue const& rvalue,
    bool indirection)
{
    // pointer to pointer
    auto& locals = stack_frame_.locals;
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue)) {
        locals.set_symbol_by_name(lvalue,
            type::get_rvalue_data_type_as_string(
                get_rvalue_at_lvalue_object_storage(
                    rvalue, stack_frame_, objects_.vectors, __source__)));
        return; // Ok
    }
    // pointer to address-of lvalue
    if (locals.is_pointer(lvalue) and type::get_unary_operator(rvalue) == "&") {
        // do not allow "x = &string[k]", or pointer to string pointer
        auto data_of = get_rvalue_at_lvalue_object_storage(
            type::get_unary_rvalue_reference(rvalue),
            stack_frame_,
            objects_.vectors,
            __source__);
        if (type::get_type_from_rvalue_data_type(data_of) == "string")
            throw_type_check_error(objects_,
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
        objects_.strings.insert(type::get_value_from_rvalue_data_type(rvalue));
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    // pointer to string storage in a vector
    if (locals.is_pointer(lvalue) and objects_.is_vector(rvalue)) {
        if (type::get_type_from_rvalue_data_type(
                get_rvalue_at_lvalue_object_storage(
                    rvalue, stack_frame_, objects_.vectors)) == "string") {
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
            throw_type_check_error(objects_,
                fmt::format("invalid pointer dereference assignment, "
                            "left-hand-side "
                            "'{}' is not a pointer",
                    lvalue),
                human_symbol);
        else
            throw_type_check_error(objects_,
                fmt::format("invalid pointer dereference assignment, "
                            "right-hand-side "
                            "'{}' is not a pointer",
                    human_symbol),
                lvalue);
    } else {
        if (!locals.is_pointer(lvalue) or
            type::is_rvalue_data_type_string(lvalue))
            throw_type_check_error(objects_,
                fmt::format("invalid pointer assignment, "
                            "left-hand-side "
                            "'{}' is not a pointer",
                    lvalue),
                human_symbol);
        else
            throw_type_check_error(objects_,
                fmt::format("invalid pointer assignment, "
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
void Pointer_Vector_Decay_Checker::type_safe_assign_trivial_vector(
    LValue const& lvalue,
    RValue const& rvalue)
{
    auto vector_contains = [&](LValue const& label) {
        return objects_.vector_contains(label);
    };
    auto local_contains = [&](LValue const& label) {
        return objects_.local_contains(label);
    };
    auto& locals = stack_frame_.locals;
    auto& vectors = objects_.vectors;
    m::match(lvalue, rvalue)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                value_type_.type_invalid_assignment_check(
                    vectors.at(lvalue), vectors.at(rvalue), "0");
                vectors.at(lvalue).get().data["0"] =
                    vectors.at(rvalue).get().data["0"];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                value_type_.type_invalid_assignment_check(
                    rvalue, vectors.at(lvalue), "0");
                vectors.at(lvalue).get().data["0"] =
                    locals.get_symbol_by_name(rvalue);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                auto vector_rvalue = vectors.at(rvalue).get().data.at("0");
                value_type_.type_invalid_assignment_check(
                    lvalue, vectors.at(rvalue), "0");
                locals.set_symbol_by_name(
                    lvalue, vectors.at(rvalue).get().data["0"]);
            });
}

/**
 * @brief
 * Type check the left-hand-side lvalue type and right-hand-side with a
 * vector lvalue, If both are the same type, then assign from a vector to lvalue
 * or vice-versa is allowed
 */
void Pointer_Vector_Decay_Checker::type_safe_assign_vector(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = stack_frame_.locals;
    RValue lvalue_offset{ "0" };
    RValue rvalue_offset{ "0" };
    auto vector_contains = [&](LValue const& label) {
        return objects_.vector_contains(label);
    };
    auto local_contains = [&](LValue const& label) {
        return objects_.local_contains(label);
    };

    auto lvalue_direct = m::match(lvalue)(
        m::pattern | m::app(binding::is_vector_lvalue, true) =
            [&] {
                lvalue_offset = type::from_decay_offset(lvalue);
                return type::from_lvalue_offset(lvalue);
            },
        m::pattern | m::_ = [&] { return lvalue; });

    auto rvalue_direct = m::match(rvalue)(
        m::pattern | m::app(binding::is_vector_lvalue, true) =
            [&] {
                rvalue_offset = type::from_decay_offset(rvalue);
                return type::from_lvalue_offset(rvalue);
            },
        m::pattern | m::_ = [&] { return rvalue; });

    m::match(lvalue_direct, rvalue_direct)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                value_type_.type_invalid_assignment_check(
                    objects_.vectors.at(lvalue_direct),
                    objects_.vectors.at(rvalue_direct),
                    lvalue_offset,
                    rvalue_offset);
                objects_.vectors.at(lvalue_direct).get().data[lvalue_offset] =
                    objects_.vectors.at(rvalue_direct)
                        .get()
                        .data[rvalue_offset];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                value_type_.type_invalid_assignment_check(rvalue_direct,
                    objects_.vectors.at(lvalue_direct),
                    lvalue_offset);
                objects_.vectors.at(lvalue_direct).get().data[lvalue_offset] =
                    locals.get_symbol_by_name(rvalue_direct);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                if (locals.is_defined(rvalue_offset)) {
                    rvalue_offset = type::get_value_from_rvalue_data_type(
                        binding::get_rvalue_at_lvalue_object_storage(
                            rvalue_offset, stack_frame_, objects_.vectors));
                    is_boundary_out_of_range(
                        fmt::format("{}[{}]", rvalue_direct, rvalue_offset));
                }
                auto& vector = objects_.vectors.at(rvalue_direct).get();
                if (vector.data.empty())
                    throw_type_check_error(objects_,
                        fmt::format("invalid vector assignment, vector '{}' "
                                    "has no elements",
                            rvalue_direct),
                        lvalue_direct);
                // an offset not known at compile time, e.g. `x[errno]`,
                // resolves to the type of the first element of the vector
                if (!vector.data.contains(rvalue_offset))
                    rvalue_offset = vector.data.first().first;
                value_type_.type_invalid_assignment_check(
                    lvalue_direct, vector, rvalue_offset);
                locals.set_symbol_by_name(
                    lvalue_direct, vector.data.at(rvalue_offset));
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Type check the assignment of dereferenced lvalue pointers
 */
void Pointer_Vector_Decay_Checker::type_safe_assign_dereference(
    LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = stack_frame_.locals;
    auto& vectors = objects_.vectors;

    auto lhs_lvalue = type::get_unary_rvalue_reference(lvalue);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rvalue);

    if (locals.is_pointer(lvalue) and type::is_dereference_expression(rvalue))
        throw_type_check_error(objects_,
            "invalid pointer dereference, "
            "right-hand-side is not a pointer",
            lvalue);
    if (locals.is_pointer(rvalue) and type::is_dereference_expression(lvalue))
        throw_type_check_error(objects_,
            "invalid pointer dereference, "
            "right-hand-side is not a pointer",
            lvalue);
    if (type::is_dereference_expression(rvalue)) {
        auto symbol = get_rvalue_at_lvalue_object_storage(
            rhs_lvalue, stack_frame_, vectors, __source__);
        if (type::get_type_from_rvalue_data_type(symbol) == "null")
            throw_type_check_error(objects_,
                "invalid pointer dereference, "
                "right-hand-side is a null pointer!",
                lvalue);
    }
    if (!locals.is_pointer(lhs_lvalue) and
        not type::is_dereference_expression(rvalue))
        throw_type_check_error(objects_,
            "invalid pointer dereference, "
            "left-hand-side is not a pointer",
            lhs_lvalue);
    if (!locals.is_pointer(rhs_lvalue) and
        not type::is_dereference_expression(lvalue))
        throw_type_check_error(objects_,
            "invalid pointer dereference, "
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
        !trivial_type_.lhs_rhs_type_is_equal(lhs_address, rhs_address))
        throw_type_check_error(objects_,
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
void Pointer_Vector_Decay_Checker::type_safe_assign_pointer_or_vector_lvalue(
    LValue const& lvalue,
    type::RValue_Reference_Type const& rvalue,
    bool indirection)
{
    auto& locals = stack_frame_.locals;
    // clang-format off
    std::visit(
        util::overload{
        [&](RValue const& value) {
            if (value == "NULL")
                throw_type_check_error(objects_,
                    "invalid pointer dereference assignment, "
                    "right-hand-side is a NULL pointer!",
                    lvalue);
            if (value_storage_.is_trivial_vector_assignment(lvalue, value)) {
                type_safe_assign_trivial_vector(lvalue, value);
                return; // Done
            }
            if (objects_.is_pointer(lvalue) or objects_.is_pointer(value)) {
                if (!type::is_dereference_expression(value)) {
                    type_safe_assign_pointer(lvalue, value);
                    return; // Done
                }
            }
            if (objects_.is_vector(lvalue) or objects_.is_vector(value)) {
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
                throw_type_check_error(objects_,
                    "invalid lvalue assignment, left-hand-side is a "
                    "pointer to non-pointer rvalue",
                    lvalue);
            if (binding::is_vector_lvalue(lvalue))
                is_boundary_out_of_range(lvalue);
            // the lvalue and rvalue vector data entry type must match
            if (objects_.get_type_from_object_data_type(lvalue) != "null" and
                !trivial_type_.lhs_rhs_type_is_equal(lvalue, value))
                throw_type_check_error(objects_,
                    fmt::format("invalid lvalue assignment, left-hand-side "
                                "'{}' "
                                "with "
                                "type '{}' is not the same type ({})",
                        lvalue,
                        objects_.get_type_from_object_data_type(lvalue),
                        type::get_type_from_rvalue_data_type(value)),
                    lvalue);
            locals.set_symbol_by_name(lvalue, value);
        } },
    rvalue);
    // clang-format on
}

/**
 * @brief Check the boundary of a vector or pointer offset
 *.    The allocation size and type data is in the table object
 */
void Pointer_Vector_Decay_Checker::is_boundary_out_of_range(
    RValue const& rvalue)
{
    credence_assert(util::contains(rvalue, "["));
    credence_assert(util::contains(rvalue, "]"));
    auto& vectors = objects_.vectors;
    auto lvalue = type::from_lvalue_offset(rvalue);
    auto offset = type::from_decay_offset(rvalue);
    if (!vectors.contains(lvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid vector assignment, vector identifier "
                        "'{}' does not "
                        "exist",
                lvalue),
            rvalue);
    if (util::is_numeric(offset)) {
        auto global_symbol = objects_.hoisted_symbols[lvalue];
        auto ul_offset = std::stoul(offset);
        if (ul_offset > binding::Vector::max_size)
            throw_type_check_error(objects_,
                fmt::format("invalid rvalue, integer offset '{}' is a"
                            "buffer-overflow",
                    ul_offset),
                rvalue);
        if (!vectors.contains(lvalue))
            throw_type_check_error(objects_,
                fmt::format(
                    "invalid vector assignment, right-hand-side does not "
                    "exist '{}'",
                    lvalue),
                rvalue);
        if (ul_offset > vectors.at(lvalue).get().size - 1)
            throw_type_check_error(objects_,
                fmt::format("invalid out-of-range vector assignment '{}' at "
                            "index "
                            "'{}'",
                    lvalue,
                    ul_offset),
                rvalue);
    } else {
        auto& locals = stack_frame_.locals;
        if (!locals.is_defined(offset) and
            not stack_frame_.is_scaler_parameter(offset))
            throw_type_check_error(objects_,
                fmt::format("invalid vector offset '{}'", offset),
                rvalue);
    }
}

/**
 * @brief Type checking on vector-to-vector same index assignment
 */
void Value_Type_Assignment_Checker::type_invalid_assignment_check(
    binding::Vector& vector_lhs,
    binding::Vector& vector_rhs,
    RValue const& index)
{
    auto vector_lvalue = vector_lhs.data.at(index);
    auto vector_rvalue = vector_rhs.data.at(index);
    if (!trivial_type_.lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "with type '{}' "
                        "is not the same type ({})",
                vector_lhs.symbol,
                type::get_type_from_rvalue_data_type(vector_lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs.symbol);
}

/**
 * @brief Type checking on vector-to-vector index assignment
 */
void Value_Type_Assignment_Checker::type_invalid_assignment_check(
    binding::Vector& vector_lhs,
    binding::Vector& vector_rhs,
    RValue const& index_lhs,
    RValue const& index_rhs)
{
    auto vector_lvalue = vector_lhs.data.at(index_lhs);
    auto vector_rvalue = vector_rhs.data.at(index_rhs);

    if (!trivial_type_.lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "at index '{}' "
                        "with type '{}' is not the same type as "
                        "right-hand-side vector "
                        "'{}' at index '{}' ({})",
                vector_lhs.symbol,
                index_lhs,
                type::get_type_from_rvalue_data_type(vector_lvalue),
                vector_rhs.symbol,
                index_rhs,
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_lhs.symbol);
}

/**
 * @brief Type checking on scaler lvalue and rvalues
 */
void Value_Type_Assignment_Checker::type_invalid_assignment_check(
    LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = stack_frame_.locals;
    if (objects_.get_type_from_object_data_type(lvalue) == "null")
        return;
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue))
        return;
    if (!trivial_type_.lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                rvalue,
                objects_.get_type_from_object_data_type(rvalue),
                objects_.get_type_from_object_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Type checking on lvalue-to-vector assignment
 */
void Value_Type_Assignment_Checker::type_invalid_assignment_check(
    LValue const& lvalue,
    binding::Vector& vector_rhs,
    RValue const& index)
{
    // an unassigned element has no type to check against
    if (!vector_rhs.data.contains(index))
        return;
    auto vector_rvalue = vector_rhs.data.at(index);
    if (objects_.get_type_from_object_data_type(lvalue) != "null" and
        not trivial_type_.lhs_rhs_type_is_equal(lvalue, vector_rvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid lvalue assignment to a "
                        "vector, left-hand-side '{}' with "
                        "type '{}' is not the same type "
                        "({})",
                lvalue,
                objects_.get_type_from_object_data_type(lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs.symbol);
}

/**
 * @brief Type checking on lvalue and Data_type (e.g. (10:"int":4UL))
 */
void Value_Type_Assignment_Checker::type_invalid_assignment_check(
    LValue const& lvalue,
    type::Data_Type const& rvalue)
{
    if (objects_.get_type_from_object_data_type(lvalue) == "null")
        return;
    if (!trivial_type_.lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_type_check_error(objects_,
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                std::get<0>(rvalue),
                type::get_type_from_rvalue_data_type(rvalue),
                objects_.get_type_from_object_data_type(lvalue)),
            lvalue);
}

} // namespace credence::language::hir::checking