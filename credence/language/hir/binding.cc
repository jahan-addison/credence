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

#include "binding.h"

#include <array>             // for array
#include <credence/error.h>  // for credence_assert, assert_nequal_impl
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for get_size_from_rvalue_data_type, get_val...
#include <credence/util.h>   // for contains, range_contains, AST_Node, __s...
#include <easyjson.h>        // for JSON
#include <fmt/base.h>        // for copy
#include <fmt/compile.h>     // for format, operator""_cf
#include <fmt/format.h>      // for format
#include <matchit.h>         // for App, app, pattern, PatternHelper, Patte...
#include <source_location>   // for source_location
#include <string>            // for basic_string, char_traits, operator==
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple

/****************************************************************************
 * Name Resolution and Object Binding
 *
 * Provides an object storage interface from the IR for target platform passes.
 * Represents vectors, functions, stack frames, and symbol tables as queryable
 * lvalues and rvalues for backend code generation.
 *
 * Example:
 *
 *  main() {
 *    auto array[10];
 *    array[5] = 42;
 *  }
 *
 *  Creates an Object table with:
 *    - Vector "array" with size 10
 *    - Function frame "main" with locals
 *    - Storage for array[5] = (42:int:4)
 *
 ****************************************************************************/

namespace credence::language::hir::binding {

/**
 * @brief Parse ITA function parameters into locals on the frame stack
 *
 * e.g. `__convert(s,v,*k) = (s,v,*k)`
 */
void Function::set_parameters_from_symbolic_label(Label const& label)
{
    auto search =
        std::string_view{ label.begin() + label.find_first_of("(") + 1,
            label.begin() + label.find_first_of(")") };

    if (!search.empty()) {
        std::string parameter;
        for (auto it = search.begin(); it <= search.end(); it++) {
            auto slice = *it;
            if (slice != ',' and it != search.end())
                parameter += slice;
            else {
                parameters.emplace_back(parameter);
                parameter = "";
            }
        }
    }
}

bool Function::is_pointer_in_stack_frame(RValue const& rvalue)
{
    return locals.is_pointer(rvalue) or is_pointer_parameter(rvalue);
}

bool Function::is_pointer_parameter(RValue const& parameter)
{
    using namespace fmt::literals;
    return util::range_contains(fmt::format("*{}"_cf, parameter), parameters);
}

bool Function::is_scaler_parameter(RValue const& parameter)
{
    return util::range_contains(
        type::from_lvalue_offset(parameter), parameters);
}

bool Function::is_parameter(RValue const& parameter)
{
    return is_scaler_parameter(parameter) or is_pointer_parameter(parameter);
}

int Function::get_index_of_parameter(RValue const& parameter)
{
    for (std::size_t i = 0; i < parameters.size(); i++) {
        if (type::get_unary_rvalue_reference(parameters[i]) ==
            type::from_lvalue_offset(parameter))
            return i;
    }
    return -1;
}

namespace detail {

/**
 * @brief Get the rvalue at the address of an offset in memory
 */
RValue Vector_Offset::get_rvalue_offset_of_vector(RValue const& offset)
{
    // clang-format off
    return stack_frame_.locals.is_defined(offset)
    ? type::get_value_from_rvalue_data_type(
        get_rvalue_at_lvalue_object_storage(offset, stack_frame_, vectors_))
    : offset;
    // clang-format on
}

/**
 * @brief Check that the offset rvalue is a valid address in the vector
 */
bool Vector_Offset::is_valid_vector_address_offset(LValue const& lvalue)
{
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);
    auto address = type::from_lvalue_offset(lvalue_reference);
    auto offset = type::from_decay_offset(lvalue_reference);
    if (stack_frame_.is_parameter(offset)) {
        return true;
    }
    return vectors_.at(address).get().data.contains(
        get_rvalue_offset_of_vector(offset));
}

} // namespace detail

/**
 * @brief Insert a function definition owned by the object table
 */
Function& Object_Table::set_function_definition(Label const& label)
{
    auto& function = function_storage_.emplace_back(label);
    functions.insert(label, std::ref(function));
    return function;
}

/**
 * @brief Insert a vector definition owned by the object table
 */
Vector& Object_Table::set_vector_definition(Label const& label, Address size_of)
{
    auto& vector = vector_storage_.emplace_back(label, size_of);
    vectors.insert(label, std::ref(vector));
    return vector;
}

/**
 * @brief Pattern matching helpers
 */
bool Object_Table::vector_contains(LValue const& lvalue)
{
    return vectors.contains(lvalue);
}

bool Object_Table::function_contains(Label const& label)
{
    return functions.contains(label);
}

bool Object_Table::local_contains(LValue const& lvalue)
{
    const auto& locals = get_stack_frame_symbols();
    return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
}

/**
 * @brief Argument stack of ir parameters by function label
 */
void Object_Table::set_ir_parameters(Label const& label, Parameters& parameters)
{
    ir_parameters[label].emplace_back(parameters);
}

std::deque<Parameters>& Object_Table::get_ir_parameters(Label const& label)
{
    credence_assert(ir_parameters.contains(label));
    return ir_parameters.at(label);
}

/**
 * @brief Stack frame helpers
 */
void Object_Table::reset_frame_calls()
{
    for (auto& function : functions) {
        function.second.get().reset_calls();
    }
}

Function& Object_Table::get_stack_frame()
{
    credence_assert(functions.contains(stack_frame_symbol));
    return functions.at(stack_frame_symbol);
}

Function& Object_Table::get_stack_frame(Label const& label)
{
    credence_assert(functions.contains(label));
    return functions.at(label);
}

Locals& Object_Table::get_stack_frame_symbols()
{
    return get_stack_frame().locals;
}

/**
 * @brief Storage type helpers in the current stack frame
 */
bool Object_Table::is_vector(RValue const& rvalue)
{
    auto label =
        util::contains(rvalue, "[") ? type::from_lvalue_offset(rvalue) : rvalue;
    return vectors.contains(label);
}

bool Object_Table::is_pointer(RValue const& rvalue)
{
    return get_stack_frame_symbols().is_pointer(rvalue) or
           rvalue.starts_with("&") or type::is_rvalue_data_type_string(rvalue);
}

bool Object_Table::is_vector_or_pointer(RValue const& rvalue)
{
    return is_vector(rvalue) or is_pointer(rvalue) or
           type::is_dereference_expression(rvalue);
}

bool Object_Table::is_null_symbol(LValue const& lvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (is_vector(lvalue))
        return false;
    if (is_pointer(lvalue))
        return locals.get_pointer_by_name(lvalue) == "NULL";
    return type::get_type_from_rvalue_data_type(locals.get_symbol_by_name(
               util::str_trim_ws(lvalue).data())) == "null";
}

/**
 * @brief Get the type from a symbol in the stack frame or vector storage
 *
 * Note: vector boundaries are not checked here, see
 * Pointer_Vector_Decay_Checker::is_boundary_out_of_range
 */
Type Object_Table::get_type_from_object_data_type(LValue const& lvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (is_vector_lvalue(lvalue)) {
        auto address = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        credence_assert(vectors.contains(address));
        return std::get<1>(vectors.at(address).get().data[offset]);
    }
    return std::get<1>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Get the size from a symbol in the stack frame or vector storage
 *
 * Note: vector boundaries are not checked here, see
 * Pointer_Vector_Decay_Checker::is_boundary_out_of_range
 */
Size Object_Table::get_size_from_object_name(LValue const& lvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (is_vector_lvalue(lvalue)) {
        auto address = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        credence_assert(vectors.contains(address));
        return std::get<2>(vectors.at(address).get().data[offset]);
    }
    if (get_type_from_object_data_type(lvalue) == "word" and
        locals.is_defined(type::get_unary_rvalue_reference(lvalue))) {
        return std::get<2>(locals.get_symbol_by_name(
            type::get_unary_rvalue_reference(lvalue)));
    }
    return std::get<2>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Resolve the rvalue of a pointer in the table object and stack frame
 */
type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    Function& stack_frame,
    Vectors& vectors,
    std::source_location const& location)
{
    auto& locals = stack_frame.locals;
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);

    if (lvalue_reference == "RET")
        return type::NULL_RVALUE_LITERAL;
    if (locals.is_pointer(lvalue_reference)) {
        auto address_at =
            locals.get_pointer_by_name(lvalue_reference, location);
        if (address_at == "NULL")
            return type::NULL_RVALUE_LITERAL;
        return get_rvalue_at_lvalue_object_storage(
            address_at, stack_frame, vectors, location);
    }
    if (vectors.contains(type::from_lvalue_offset(lvalue_reference))) {
        auto vector_offset = detail::Vector_Offset{ stack_frame, vectors };
        auto address = type::from_lvalue_offset(lvalue_reference);
        auto offset = type::from_decay_offset(lvalue_reference);
        auto offset_rvalue = vector_offset.get_rvalue_offset_of_vector(offset);
        if (stack_frame.is_parameter(offset)) {
            return type::Data_Type{ lvalue, "word", 8UL };
        }
        if (!vector_offset.is_valid_vector_address_offset(lvalue))
            throw_compiletime_error(
                fmt::format("lvalue '{}' is not a vector with offset '{}' with "
                            "and storage of '{}'",
                    address,
                    offset,
                    offset_rvalue),
                lvalue,
                location);
        return vectors.at(address).get().data[offset_rvalue];
    }
    if (type::is_rvalue_data_type(lvalue))
        return type::get_rvalue_datatype_from_string(lvalue);

    return locals.get_symbol_by_name(lvalue_reference, location);
}

/**
 * @brief Resolve the rvalue at a temporary storage address in the object table
 */
RValue Object_Table::lvalue_at_temporary_object_address(LValue const& lvalue,
    Function& stack_frame)
{
    auto rvalue = type::is_temporary(lvalue) or util::contains(lvalue, "_p")
                      ? stack_frame.temporary[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        return rvalue;
    } else {
        if (util::contains(rvalue, "_t") or util::contains(lvalue, "_p"))
            return lvalue_at_temporary_object_address(rvalue, stack_frame);
        else
            return rvalue;
    }
}

/**
 * @brief Resolve the size of an rvalue data type that is NOT a word literal
 */
Size Object_Table::get_symbol_size_from_rvalue_data_type(LValue const& lvalue,
    Function& stack_frame)
{
    auto& locals = stack_frame.locals;
    credence_assert(locals.is_defined(lvalue));
    auto datatype = locals.get_symbol_by_name(lvalue);
    if (type::is_rvalue_data_type_word(datatype))
        return lvalue_size_at_temporary_object_address(
            type::get_value_from_rvalue_data_type(datatype), stack_frame);
    else
        return type::get_size_from_rvalue_data_type(
            locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Resolve the size at a temporary storage address in the object table
 */
Size Object_Table::lvalue_size_at_temporary_object_address(LValue const& lvalue,
    Function& stack_frame)
{
    auto rvalue = lvalue_at_temporary_object_address(lvalue, stack_frame);
    auto& locals = stack_frame.locals;

    if (type::is_rvalue_data_type(rvalue) and
        not type::is_rvalue_data_type_word(rvalue))
        return type::get_size_from_rvalue_data_type(rvalue);
    if (type::is_unary_expression(rvalue))
        return lvalue_size_at_temporary_object_address(
            type::get_unary_rvalue_reference(rvalue), stack_frame);
    if (type::is_binary_expression(rvalue)) {
        auto [left, right, op] = type::from_rvalue_binary_expression(rvalue);
        if (type::is_rvalue_data_type(left) and
            not type::is_rvalue_data_type_word(left))
            return type::get_size_from_rvalue_data_type(left);
        if (type::is_rvalue_data_type(right) and
            not type::is_rvalue_data_type_word(right))
            return type::get_size_from_rvalue_data_type(right);
        if (locals.is_defined(left) and not locals.is_pointer(left)) {
            auto datatype = locals.get_symbol_by_name(left);
            if (type::is_rvalue_data_type_word(datatype))
                return lvalue_size_at_temporary_object_address(
                    type::get_value_from_rvalue_data_type(datatype),
                    stack_frame);
            else
                return type::get_size_from_rvalue_data_type(
                    locals.get_symbol_by_name(left));
        }
        if (locals.is_defined(right) and not locals.is_pointer(right)) {
            auto datatype = locals.get_symbol_by_name(right);
            if (type::is_rvalue_data_type_word(datatype))
                return lvalue_size_at_temporary_object_address(
                    type::get_value_from_rvalue_data_type(datatype),
                    stack_frame);
            else
                return type::get_size_from_rvalue_data_type(
                    locals.get_symbol_by_name(right));
        }
    }
    if (type::is_temporary_datatype_binary_expression(rvalue)) {
        auto [left, right, op] = type::from_rvalue_binary_expression(rvalue);
        return lvalue_size_at_temporary_object_address(left, stack_frame);
    }
    if (locals.is_defined(rvalue)) {
        auto datatype = locals.get_symbol_by_name(rvalue);
        if (type::is_rvalue_data_type_word(datatype))
            return lvalue_size_at_temporary_object_address(
                type::get_value_from_rvalue_data_type(datatype), stack_frame);
        else
            return type::get_size_from_rvalue_data_type(
                locals.get_symbol_by_name(rvalue));
    }
    if (rvalue == "RET")
        return 8UL;
    credence_error(fmt::format("undefined object: {}", rvalue));
    return 0UL;
}

Size Object_Table::get_size_of_temporary_binary_rvalue(RValue const& rvalue,
    Function& stack_frame)
{
    Size size = 0UL;
    auto temp_side = type::is_temporary_operand_binary_expression(rvalue);
    auto [left, right, _] = type::from_rvalue_binary_expression(rvalue);
    if (type::is_temporary(left) and type::is_temporary(right))
        return lvalue_size_at_temporary_object_address(left, stack_frame);
    if (temp_side == "left")
        if (!type::is_rvalue_data_type(right))
            size = lvalue_size_at_temporary_object_address(right, stack_frame);
        else
            size = type::get_size_from_rvalue_data_type(right);
    else {
        if (!type::is_rvalue_data_type(left))
            size = lvalue_size_at_temporary_object_address(left, stack_frame);
        else
            size = type::get_size_from_rvalue_data_type(left);
    }
    credence_assert_nequal(size, 0UL);
    return size;
}

/**
 * @brief Set the stack frame return value in the table
 */
void set_stack_frame_return_value(RValue const& rvalue,
    Function& frame,
    Object_Table& objects)
{
    auto local_contains = [&](LValue const& lvalue) {
        const auto& locals = frame.locals;
        return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
    };
    auto is_pointer_ = [&](RValue const& value) {
        return frame.locals.is_pointer(value);
    };
    auto is_parameter = [&](RValue const& value) {
        return frame.is_parameter(value);
    };
    namespace m = matchit;
    m::match(rvalue)(
        m::pattern | m::app(is_pointer_, true) =
            [&] {
                frame.ret = std::make_pair(
                    frame.locals.get_pointer_by_name(rvalue), rvalue);
            },
        m::pattern | m::app(is_parameter, true) =
            [&] {
                if (objects.stack.empty()) {
                    frame.ret = std::make_pair("NULL", rvalue);
                    return;
                }
                auto index_of = frame.get_index_of_parameter(rvalue);
                credence_assert_nequal(index_of, -1);
                frame.ret = std::make_pair(objects.stack.at(index_of), rvalue);
            },
        m::pattern | m::app(local_contains, true) =
            [&] {
                auto value_at = type::get_value_from_rvalue_data_type(
                    frame.locals.get_symbol_by_name(rvalue));
                frame.ret = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                auto datatype = type::get_rvalue_datatype_from_string(rvalue);
                auto value_at = type::get_value_from_rvalue_data_type(datatype);
                frame.ret = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                auto value_at = get_rvalue_at_lvalue_object_storage(
                    rvalue, frame, objects.vectors, __source__);
                frame.ret = std::make_pair(
                    type::get_value_from_rvalue_data_type(value_at), rvalue);
            },
        m::pattern | m::app(type::is_temporary, true) =
            [&] {
                frame.ret = std::make_pair(frame.temporary.at(rvalue), rvalue);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

} // namespace credence::language::hir::binding
