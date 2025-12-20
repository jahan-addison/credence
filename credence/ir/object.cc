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

#include <credence/ir/object.h>

#include <algorithm>         // for __find_if, find_if
#include <array>             // for array
#include <credence/error.h>  // for assert_nequal_impl, credence_assert
#include <credence/ir/ita.h> // for Instructions, Quadruple
#include <credence/map.h>    // for Ordered_Map
#include <credence/types.h>  // for get_value_from_rvalue_data_type, from_l...
#include <credence/util.h>   // for contains
#include <fmt/format.h>      // for format
#include <matchit.h>         // for App, app, pattern, PatternHelper, Patte...
#include <memory>            // for shared_ptr
#include <source_location>   // for source_location
#include <string>            // for basic_string, char_traits, operator==
#include <string_view>       // for basic_string_view
#include <tuple>             // for tuple, get

namespace credence::ir::object {

namespace m = matchit;

namespace detail {

/**
 * @brief Get the rvalue at the address of an offset in memory
 */
RValue Vector_Offset::get_rvalue_offset_of_vector(RValue const& offset)
{
    // clang-format off
    return stack_frame_->locals.is_defined(offset)
    ? type::get_value_from_rvalue_data_type(
        get_rvalue_at_lvalue_object_storage(
            offset, stack_frame_, vectors_))
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
    if (stack_frame_->is_parameter(offset)) {
        return true;
    }
    return vectors_.at(address)->data.contains(
        get_rvalue_offset_of_vector(offset));
}

} // namespace detail

/**
 * @brief Resolve the rvalue of a pointer in the table object and stack frame
 */
type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    object::Function_PTR& stack_frame,
    object::Vectors& vectors,
    std::source_location const& location)
{
    auto& locals = stack_frame->locals;
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);

    if (lvalue_reference == "RET")
        return type::NULL_RVALUE_LITERAL;
    if (locals.is_pointer(lvalue_reference)) {
        auto address_at = locals.get_pointer_by_name(lvalue, location);
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
        if (stack_frame->is_parameter(offset)) {
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
        return vectors.at(address)->data[offset_rvalue];
    }
    if (type::is_rvalue_data_type(lvalue))
        return type::get_rvalue_datatype_from_string(lvalue);

    return locals.get_symbol_by_name(lvalue_reference, location);
}

/**
 * @brief Resolve the rvalue at a temporary storage address in the object table
 */
RValue Object::lvalue_at_temporary_object_address(LValue const& lvalue,
    Function_PTR& stack_frame)
{
    auto rvalue = util::contains(lvalue, "_t") or util::contains(lvalue, "_p")
                      ? stack_frame->temporary[lvalue]
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
 * @brief Search the ir instructions in a stack frame for a instruction
 */
bool Object::stack_frame_contains_ir_instruction(Label name,
    Instruction inst,
    ir::Instructions const& instructions)
{
    credence_assert(functions.contains(name));
    auto frame = functions.at(name);
    auto search = std::ranges::find_if(
        instructions.begin() + frame->address_location[0],
        instructions.begin() + frame->address_location[1],
        [&](ir::Quadruple const& quad) { return std::get<0>(quad) == inst; });
    return search != instructions.begin() + frame->address_location[1];
}

/**
 * @brief Set the stack frame return value in the table
 */
void set_stack_frame_return_value(RValue const& rvalue,
    Function_PTR& frame,
    Object_PTR& objects)
{
    auto is_vector_lvalue = [&](LValue const& lvalue) {
        return util::contains(lvalue, "[") and util::contains(lvalue, "]");
    };
    auto local_contains = [&](LValue const& lvalue) {
        const auto& locals = frame->locals;
        return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
    };
    auto is_pointer_ = [&](RValue const& value) {
        return frame->locals.is_pointer(value);
    };
    auto is_parameter = [&](RValue const& value) {
        return frame->is_parameter(value);
    };

    m::match(rvalue)(
        m::pattern | m::app(is_pointer_, true) =
            [&] {
                frame->ret = std::make_pair(
                    frame->locals.get_pointer_by_name(rvalue), rvalue);
            },
        m::pattern | m::app(is_parameter, true) =
            [&] {
                if (objects->stack.empty()) {
                    frame->ret = std::make_pair("NULL", rvalue);
                    return;
                }
                auto index_of = frame->get_index_of_parameter(rvalue);
                credence_assert_nequal(index_of, -1);
                frame->ret =
                    std::make_pair(objects->stack.at(index_of), rvalue);
            },
        m::pattern | m::app(local_contains, true) =
            [&] {
                auto value_at = type::get_value_from_rvalue_data_type(
                    frame->locals.get_symbol_by_name(rvalue));
                frame->ret = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                auto datatype = type::get_rvalue_datatype_from_string(rvalue);
                auto value_at = type::get_value_from_rvalue_data_type(datatype);
                frame->ret = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                auto value_at = get_rvalue_at_lvalue_object_storage(
                    rvalue, frame, objects->vectors, __source__);
                frame->ret = std::make_pair(
                    type::get_value_from_rvalue_data_type(value_at), rvalue);
            },
        m::pattern | m::app(type::is_temporary, true) =
            [&] {
                frame->ret =
                    std::make_pair(frame->temporary.at(rvalue), rvalue);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); }

    );
}
}
