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

#include <credence/ir/table.h>

#include <algorithm>         // for __find_if, find_if
#include <credence/error.h>  // for credence_assert, credence_compile_error
#include <credence/ir/ita.h> // for Instruction, ITA, Quadruple, get_rvalue...
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for get_unary_rvalue_reference, get_type_fr...
#include <credence/util.h>   // for contains, AST_Node, is_numeric, substri...
#include <credence/values.h> // for expression_type_to_string
#include <cstddef>           // for size_t
#include <deque>             // for __deque_iterator, operator==
#include <fmt/format.h>      // for format
#include <limits>            // for numeric_limits
#include <matchit.h>         // for pattern, PatternHelper, PatternPipable
#include <optional>          // for optional
#include <string>            // for basic_string, char_traits, operator==
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple, operator==
#include <utility>           // for pair
#include <variant>           // for visit
#include <vector>            // for vector

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Construct table and run type-checking pass on IR instructions
 */
Instructions Table::build_from_ita_instructions()
{
    bool skip = false;
    Instruction last_instruction = Instruction::NOOP;

    build_symbols_from_vector_lvalues();
    build_vector_definitions_from_globals();

    for (instruction_index = 0; instruction_index < instructions.size();
        instruction_index++) {
        auto instruction = instructions.at(instruction_index);
        m::match(std::get<Instruction>(instruction))(
            m::pattern | Instruction::FUNC_START =
                [&] {
                    from_func_start_ita_instruction(
                        std::get<1>(instructions.at(instruction_index - 1)));
                },
            m::pattern | Instruction::FUNC_END =
                [&] { from_func_end_ita_instruction(); },
            m::pattern | Instruction::GLOBL =
                [&] { from_globl_ita_instruction(std::get<1>(instruction)); },
            m::pattern | Instruction::LOCL =
                [&] { from_locl_ita_instruction(instruction); },
            m::pattern | Instruction::RETURN =
                [&] { from_return_instruction(instruction); },
            m::pattern |
                Instruction::PUSH = [&] { from_push_instruction(instruction); },
            m::pattern | Instruction::CALL =
                [&] { from_call_ita_instruction(std::get<1>(instruction)); },
            m::pattern |
                Instruction::POP = [&] { from_pop_instruction(instruction); },
            m::pattern | Instruction::MOV =
                [&] { from_mov_ita_instruction(instruction); },
            m::pattern | Instruction::LABEL =
                [&] { from_label_ita_instruction(instruction); },
            m::pattern | Instruction::GOTO =
                [&] {
                    if (last_instruction == Instruction::GOTO)
                        skip = true;
                });
        if (skip) {
            instructions.erase(instructions.begin() + instruction_index);
            skip = false;
        }
        last_instruction = std::get<Instruction>(instruction);
    }
    return instructions;
}

/**
 * @brief Construct table of vector symbols and size allocation
 */
void Table::build_symbols_from_vector_lvalues()
{
    auto keys = hoisted_symbols.dump_keys();
    for (LValue const& key : keys) {
        auto symbol_type = hoisted_symbols[key]["type"].to_string();
        if (symbol_type == "vector_lvalue") {
            auto size =
                static_cast<std::size_t>(hoisted_symbols[key]["size"].to_int());
            if (size > detail::Vector::max_size)
                throw_compiletime_error("stack overflow", key);
            if (!vectors.contains(key))
                vectors[key] = std::make_shared<detail::Vector>(
                    detail::Vector{ key, size });
        }
    }
}

/**
 * @brief Construct table entry from locl (auto) instruction
 */
void Table::from_locl_ita_instruction(Quadruple const& instruction)
{
    auto label = std::get<1>(instruction);
    auto frame = get_stack_frame();
    if (std::get<1>(instruction).starts_with("*")) {
        label = std::get<1>(instruction);
        get_stack_frame()->locals.set_symbol_by_name(label.substr(1), "NULL");
    } else
        get_stack_frame()->locals.set_symbol_by_name(
            label, type::NULL_RVALUE_LITERAL);
}

/**
 * @brief Set return rvalue on the current stack frame in the table
 */
void Table::from_return_instruction(Quadruple const& instruction)
{
    auto return_rvalue = util::str_trim_ws(std::get<1>(instruction));
    auto frame = get_stack_frame();
    if (frame->ret.has_value())
        throw_compiletime_error("invalid return statement", return_rvalue);
    set_stack_frame_return_value(return_rvalue);
}

/**
 * @brief Construct table entry from globl (extrn) instruction
 */
void Table::from_globl_ita_instruction(Label const& label)
{
    if (!vectors.contains(label))
        throw_compiletime_error(
            "extrn statement failed, identifier does not exist", label);
    auto frame = get_stack_frame();
    get_stack_frame()->locals.set_symbol_by_name(
        label, type::NULL_RVALUE_LITERAL);
}

/**
 * @brief Set vector globals from an ITA constructor
 */
void Table::build_vector_definitions_from_globals()
{
    if (!globals.empty())
        for (auto i = globals.begin_t(); i != globals.end_t(); i++) {
            std::size_t index = 0;
            auto symbol = *i;
            vectors[symbol.first] = std::make_shared<detail::Vector>(
                detail::Vector{ symbol.first, symbol.second.size() });
            for (auto const& item : symbol.second) {
                auto key = std::to_string(index++);
                auto value = type::get_rvalue_datatype_from_string(
                    value::expression_type_to_string(item, false));
                if (type::get_type_from_rvalue_data_type(value) == "string")
                    strings.insert(
                        type::get_value_from_rvalue_data_type(value));
                vectors[symbol.first]->data[key] = value;
            }
        }
}

/**
 * @brief Ensure CALL right-hand-side is a valid symbol
 */
void Table::from_call_ita_instruction(Label const& label)
{
    if (!labels.contains(label) and not hoisted_symbols.has_key(label))
        throw_compiletime_error(
            "function call failed, function does not exist", label);
}

/**
 * @brief add label and label instruction address entry from LABEL
 * instruction
 */
void Table::from_label_ita_instruction(Quadruple const& instruction)
{
    Label label = std::get<1>(instruction);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        if (frame->labels.contains(label))
            throw_compiletime_error("label is already defined", label);
        frame->labels.emplace(label);
        frame->label_address.set_symbol_by_name(label, instruction_index);
    }
}

/**
 * @brief Deconstruct assignment instructions into each
 * type and populate function frame stack in the table
 *
 *  * LValues that begin with `_t` or `_p` are temporaries or parameters
 *
 *  * Checks the storage type of the lvalue and rvalues
 *
 *  * Unwind unary and binary operator expressions into ITA operands
 *
 *  Then assign symbols to the table and allocate them as a local on the
 *  frame stack
 */
void Table::from_mov_ita_instruction(Quadruple const& instruction)
{
    LValue lhs = std::get<1>(instruction);
    auto rvalue = get_rvalue_from_mov_qaudruple(instruction);
    RValue rhs = rvalue.first;
    auto frame = get_stack_frame();

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        from_temporary_assignment(lhs, rhs);
        return;
    }
    if (is_stack_frame() and (rhs.starts_with("_t") or rhs.starts_with("_p"))) {
        from_temporary_reassignment(lhs, rhs);
        return;
    }
    if (is_vector_or_pointer(lhs) or is_vector_or_pointer(rhs) or
        rvalue.second == "&") {
        from_pointer_or_vector_assignment(lhs, rhs);
        return;
    }
    if (hoisted_symbols.has_key(rhs)) {
        from_scaler_symbol_assignment(lhs, rhs);
        return;
    }

    type::Data_Type rvalue_symbol = type::NULL_RVALUE_LITERAL;

    if (type::is_unary_expression(rhs))
        rvalue_symbol = from_rvalue_unary_expression(
            lhs, rhs, type::get_unary_operator(rhs));

    if (type::is_binary_expression(rhs))
        rvalue_symbol = type::Data_Type{ rhs, "word", sizeof(void*) };

    if (rvalue_symbol == type::NULL_RVALUE_LITERAL and rvalue.second.empty())
        rvalue_symbol = type::get_rvalue_datatype_from_string(rhs);
    if (rvalue_symbol == type::NULL_RVALUE_LITERAL and
        type::is_unary_expression(rvalue.second))
        rvalue_symbol = from_rvalue_unary_expression(lhs, rhs, rvalue.second);
    if (rvalue_symbol == type::NULL_RVALUE_LITERAL)
        throw_compiletime_error(
            fmt::format("invalid lvalue assignment on '{}'", lhs), rhs);

    Size size = std::get<2>(rvalue_symbol);

    if (size > std::numeric_limits<unsigned int>::max())
        throw_compiletime_error(
            fmt::format("right-hand-side exceeds maximum byte size '{}'", rhs),
            lhs);

    frame->locals.set_symbol_by_name(lhs, rvalue_symbol);
    frame->allocation += size;

    Type type = type::get_type_from_rvalue_data_type(rvalue_symbol);
    if (type == "string")
        strings.insert(type::get_value_from_rvalue_data_type(rvalue_symbol));
}

/**
 * @brief Out-of-range boundary check on left-hand-side and right-hand-side
 * of assignment
 *
 * This function is quite complex, but it covers lhs and rhs pointer and
 * vector cases.
 *
 *  Examples:
 *
 *     auto *k, m, *z
 *     k = &m;             // allowed
 *     k = z;              // allowed
 *     array[2] = m;       // allowed
 *     array[1] = array[2] // allowed
 *     m = array[2];       // allowed
 *     k = &array[2];      // allowed
 *
 *  All other combintions are not allowed between pointers and vectors
 *
 */
void Table::from_pointer_or_vector_assignment(LValue const& lvalue,
    RValue const& rvalue,
    bool indirection)
{
    auto& locals = get_stack_frame_symbols();
    auto frame = get_stack_frame();
    std::string rhs_lvalue{};
    std::optional<type::Data_Type> rvalue_symbol;

    // This is an lvalue indirection assignment, `m = *k;` or `*k = m`;
    // if the right-hand-side is a vector, save the lvalue and the data
    // type
    if (util::contains(rvalue, "[")) {
        rhs_lvalue = type::from_lvalue_offset(rvalue);
        auto safe_rvalue = type::get_unary_rvalue_reference(rhs_lvalue);
        auto offset = type::from_decay_offset(rvalue);
        is_boundary_out_of_range(type::get_unary_rvalue_reference(rvalue));

        if (!frame->is_scaler_parameter(offset) and
            not locals.is_defined(offset) and
            (!vectors.contains(safe_rvalue) or
                not vectors[safe_rvalue]->data.contains(offset)))
            throw_compiletime_error(
                fmt::format("invalid vector assignment, element at '{}' "
                            "does not exist",
                    offset),
                rvalue);
        type_safe_assign_pointer_or_vector_lvalue(lvalue, rvalue, indirection);
        return;
    }

    // if the left-hand-side is a normal vector:
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        // the rhs is a vector too, check accessed types
        if (rvalue_symbol.has_value()) {
            if (!lhs_rhs_type_is_equal(lhs_lvalue, rvalue_symbol.value()))
                throw_compiletime_error(
                    fmt::format("invalid lvalue assignment, "
                                "right-hand-side \"{}\" "
                                "with "
                                "type {} is not the same type ({})",
                        rvalue,
                        get_type_from_rvalue_data_type(lvalue),
                        type::get_type_from_rvalue_data_type(
                            rvalue_symbol.value())),
                    lvalue);
            vectors[lhs_lvalue]->data[offset] = rvalue_symbol.value();
        } else {
            // is the rhs a scaler rvalue? e.g. (10:"int":4UL)
            if (type::is_rvalue_data_type(rvalue)) {
                // update the lhs vector, if applicable
                auto value = type::get_rvalue_datatype_from_string(rvalue);
                if (type::get_type_from_rvalue_data_type(value) == "string")
                    strings.insert(
                        type::get_value_from_rvalue_data_type(value));
                if (vectors.contains(lhs_lvalue))
                    vectors[lhs_lvalue]->data[offset] =
                        type::get_rvalue_datatype_from_string(rvalue);
            }
        }
        return;
    }
    // is the rhs an address-of rvalue?
    if (rvalue.starts_with("&")) {
        // the left-hand-side must be a pointer
        if (!indirection and not locals.is_pointer(lvalue))
            throw_compiletime_error(
                fmt::format(
                    "invalid pointer assignment, left-hand-side '{}' is "
                    "not a pointer",
                    lvalue),
                rvalue);
        locals.set_symbol_by_name(
            lvalue, type::get_unary_rvalue_reference(rvalue));
        return;
    }

    if (!rvalue_symbol.has_value())
        type_safe_assign_pointer_or_vector_lvalue(lvalue, rvalue, indirection);
    else
        type_safe_assign_pointer_or_vector_lvalue(
            lvalue, rvalue_symbol.value(), indirection);
}

/**
 * @brief Trivial lvalue vector assignment
 *   Vector:
 *    " unit 10; "
 *   Assignment:
 *    "unit = 5;"
 */
void Table::from_trivial_vector_assignment(LValue const& lhs,
    type::Data_Type const& rvalue)
{
    if (vectors.contains(lhs)) {
        vectors[lhs]->data["0"] = rvalue;
    }
}

/**
 * @brief Type-safe pointer and address-of assignment with type validation
 */
void Table::type_safe_assign_pointer(LValue const& lvalue,
    RValue const& rvalue,
    bool indirection)
{
    // pointer to pointer
    auto& locals = get_stack_frame_symbols();
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue)) {
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    // pointer to address-of lvalue
    if (locals.is_pointer(lvalue) and type::get_unary_operator(rvalue) == "&") {
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    // pointer to string literal
    if (locals.is_pointer(lvalue) and
        type::is_rvalue_data_type_string(rvalue)) {
        strings.insert(type::get_value_from_rvalue_data_type(rvalue));
        locals.set_symbol_by_name(lvalue, rvalue);
        return; // Ok
    }
    auto human_symbol = type::is_rvalue_data_type(rvalue)
                            ? type::get_value_from_rvalue_data_type(
                                  type::get_rvalue_datatype_from_string(rvalue))
                            : rvalue;
    if (indirection) {
        if (!locals.is_pointer(lvalue) or
            type::is_rvalue_data_type_string(lvalue))
            throw_compiletime_error(
                fmt::format("invalid pointer dereference assignment, "
                            "left-hand-side "
                            "'{}' is not a pointer",
                    lvalue),
                human_symbol);
        else
            throw_compiletime_error(
                fmt::format("invalid pointer dereference assignment, "
                            "right-hand-side "
                            "'{}' is not a pointer",
                    human_symbol),
                lvalue);
    } else {
        if (!locals.is_pointer(lvalue) or
            type::is_rvalue_data_type_string(lvalue))
            throw_compiletime_error(fmt::format("invalid pointer assignment, "
                                                "left-hand-side "
                                                "'{}' is not a pointer",
                                        lvalue),
                human_symbol);
        else
            throw_compiletime_error(fmt::format("invalid pointer assignment, "
                                                "right-hand-side "
                                                "'{}' is not a pointer",
                                        human_symbol),
                lvalue);
    }
}

/**
 * @brief Type check the left-hand-side lvalue type and right-hand-side
 *
 * If both are the same type, then assign
 *    * Variant method for the trivial vector with one element
 */
void Table::type_safe_assign_trivial_vector(LValue const& lvalue,
    RValue const& rvalue)
{
    Type_Check_Lambda vector_contains =
        std::bind(&Table::vector_contains_, this, std::placeholders::_1);
    Type_Check_Lambda local_contains =
        std::bind(&Table::local_contains_, this, std::placeholders::_1);
    auto& locals = get_stack_frame_symbols();
    m::match(lvalue, rvalue)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(
                    vectors[lvalue], vectors[rvalue], "0");
                vectors[lvalue]->data["0"] = vectors[rvalue]->data["0"];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                type_invalid_assignment_check(rvalue, vectors[lvalue], "0");
                vectors[lvalue]->data["0"] = locals.get_symbol_by_name(rvalue);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                auto vector_rvalue = vectors[rvalue]->data.at("0");
                type_invalid_assignment_check(lvalue, vectors[rvalue], "0");
                locals.set_symbol_by_name(lvalue, vectors[rvalue]->data["0"]);
            });
}

/**
 * @brief Type check the left-hand-side lvalue type and right-hand-side
 *
 * If both are the same type, then assign from vector
 * to lvalue or vice-versa
 */
void Table::type_safe_assign_vector(LValue const& lvalue, RValue const& rvalue)
{
    auto& locals = get_stack_frame_symbols();
    RValue lvalue_offset{ "0" };
    RValue rvalue_offset{ "0" };
    Type_Check_Lambda vector_contains =
        std::bind(&Table::vector_contains_, this, std::placeholders::_1);
    Type_Check_Lambda local_contains =
        std::bind(&Table::local_contains_, this, std::placeholders::_1);
    auto lvalue_direct = m::match(lvalue)(
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                lvalue_offset = type::from_decay_offset(lvalue);
                return type::from_lvalue_offset(lvalue);
            },
        m::pattern | m::_ = [&] { return lvalue; });

    auto rvalue_direct = m::match(rvalue)(
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                rvalue_offset = type::from_decay_offset(rvalue);
                return type::from_lvalue_offset(rvalue);
            },
        m::pattern | m::_ = [&] { return rvalue; });

    m::match(lvalue_direct, rvalue_direct)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(vectors[lvalue_direct],
                    vectors[rvalue_direct],
                    lvalue_offset,
                    rvalue_offset);
                vectors[lvalue_direct]->data[lvalue_offset] =
                    vectors[rvalue_direct]->data[rvalue_offset];
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                type_invalid_assignment_check(
                    rvalue_direct, vectors[lvalue_direct], lvalue_offset);
                vectors[lvalue_direct]->data[lvalue_offset] =
                    locals.get_symbol_by_name(rvalue_direct);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                type_invalid_assignment_check(
                    lvalue_direct, vectors[rvalue_direct], rvalue_offset);
                locals.set_symbol_by_name(lvalue_direct,
                    vectors[rvalue_direct]->data.at(rvalue_offset));
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Recursively resolve the rvalue of a pointer in the symbol table
 *
 * Including vector (array) decay and pointers to vectors
 */
type::Data_Type Table::get_rvalue_data_type_at_pointer(LValue const& lvalue,
    std::source_location const& location)
{
    auto& locals = get_stack_frame_symbols();
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);
    if (lvalue_reference == "RET")
        return type::NULL_RVALUE_LITERAL;
    if (locals.is_pointer(lvalue_reference))
        return get_rvalue_data_type_at_pointer(
            locals.get_pointer_by_name(lvalue, location));
    // array decay
    else if (vectors.contains(type::from_lvalue_offset(lvalue_reference))) {
        auto frame = get_stack_frame();
        auto address = type::from_lvalue_offset(lvalue_reference);
        auto offset = type::from_decay_offset(lvalue_reference);
        if (frame->is_parameter(offset)) {
            return type::Data_Type{ lvalue, "word", 8UL };
        }
        credence_assert_message_trace(
            vectors.at(address)->data.contains(offset),
            fmt::format("lvalue '{}' is not a vector with offset '{}'",
                address,
                offset),
            location);
        return vectors.at(address)->data[offset];

    } else {
        return locals.get_symbol_by_name(lvalue_reference, location);
    }
}

/**
 * @brief Type check the assignment of 2 dereferenced lvalue pointers
 */
void Table::type_safe_assign_dereference(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_symbols();

    auto lhs_lvalue = type::get_unary_rvalue_reference(lvalue);
    auto rhs_lvalue = type::get_unary_rvalue_reference(rvalue);

    if (locals.is_pointer(lvalue) and type::is_dereference_expression(rvalue))
        throw_compiletime_error("invalid pointer dereference, "
                                "right-hand-side is not a pointer",
            lvalue);
    if (locals.is_pointer(rvalue) and type::is_dereference_expression(lvalue))
        throw_compiletime_error("invalid pointer dereference, "
                                "right-hand-side is not a pointer",
            lvalue);
    if (!locals.is_pointer(lhs_lvalue) and
        not type::is_dereference_expression(rvalue))
        throw_compiletime_error("invalid pointer dereference, "
                                "left-hand-side is not a pointer",
            lhs_lvalue);
    if (!locals.is_pointer(rhs_lvalue) and
        not type::is_dereference_expression(lvalue))
        throw_compiletime_error("invalid pointer dereference, "
                                "right-hand-side is not a pointer",
            lhs_lvalue);

    auto lhs_address = get_rvalue_data_type_at_pointer(lhs_lvalue);
    auto rhs_address = get_rvalue_data_type_at_pointer(rhs_lvalue);

    if (type::get_type_from_rvalue_data_type(lhs_address) != "null" and
        !lhs_rhs_type_is_equal(lhs_address, rhs_address))
        throw_compiletime_error(
            fmt::format(
                "invalid dereference assignment, dereference rvalue of "
                "left-hand-side with type '{}' is not the same type ({})",
                type::get_type_from_rvalue_data_type(lhs_address),
                type::get_type_from_rvalue_data_type(rhs_address)),
            lvalue);
}

/**
 * @brief Type-safe assignment of pointers and vectors, or scalers and
 * dereferenced pointers
 *
 * Note: To get a better idea of how this function works,
 * check the test cases in test/fixtures/types
 */
void Table::type_safe_assign_pointer_or_vector_lvalue(LValue const& lvalue,
    type::RValue_Reference_Type const& rvalue,
    bool indirection)
{
    auto& locals = get_stack_frame_symbols();
    // clang-format off
    std::visit(
        util::overload{
        [&](RValue const& value) {
            if (value == "NULL")
                throw_compiletime_error(
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
                throw_compiletime_error(
                    "invalid lvalue assignment, left-hand-side is a "
                    "pointer to non-pointer rvalue",
                    lvalue);
            // the lvalue and rvalue vector data entry type must match
            if (get_type_from_rvalue_data_type(lvalue) != "null" and
                !lhs_rhs_type_is_equal(lvalue, value))
                throw_compiletime_error(
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
Table::Type Table::get_type_from_rvalue_data_type(LValue const& lvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        return std::get<1>(vectors[lhs_lvalue]->data[offset]);
    }
    return std::get<1>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Get the size from a symbol in the local stack frame
 */
Table::Size Table::get_size_from_local_lvalue(LValue const& lvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        return std::get<2>(vectors[lhs_lvalue]->data[offset]);
    }
    if (get_type_from_rvalue_data_type(lvalue) == "word" and
        locals.is_defined(type::get_unary_rvalue_reference(lvalue))) {
        return std::get<2>(locals.get_symbol_by_name(
            type::get_unary_rvalue_reference(lvalue)));
    }
    return std::get<2>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Check the boundary of a vector or pointer offset by its
 * allocation size
 */
void Table::is_boundary_out_of_range(RValue const& rvalue)
{
    credence_assert(util::contains(rvalue, "["));
    credence_assert(util::contains(rvalue, "]"));
    auto lvalue = type::from_lvalue_offset(rvalue);
    auto offset = type::from_decay_offset(rvalue);
    if (!vectors.contains(lvalue))
        throw_compiletime_error(
            fmt::format("invalid vector assignment, vector identifier "
                        "'{}' does not "
                        "exist",
                lvalue),
            rvalue);
    if (util::is_numeric(offset)) {
        auto global_symbol = hoisted_symbols[lvalue];
        auto ul_offset = std::stoul(offset);
        if (ul_offset > detail::Vector::max_size)
            throw_compiletime_error(
                fmt::format("invalid rvalue, integer offset '{}' is a"
                            "buffer-overflow",
                    ul_offset),
                rvalue);
        if (!vectors.contains(lvalue))
            throw_compiletime_error(
                fmt::format(
                    "invalid vector assignment, right-hand-side does not "
                    "exist '{}'",
                    lvalue),
                rvalue);
        if (ul_offset > vectors[lvalue]->size - 1)
            throw_compiletime_error(
                fmt::format("invalid out-of-range vector assignment '{}' at "
                            "index "
                            "'{}'",
                    lvalue,
                    ul_offset),
                rvalue);

    } else {
        auto stack_frame = get_stack_frame();
        auto& locals = get_stack_frame_symbols();
        if (!locals.is_defined(offset) and
            not stack_frame->is_scaler_parameter(offset))
            throw_compiletime_error(
                fmt::format("invalid vector offset '{}'", offset), rvalue);
    }
}

/**
 * @brief Set the stack frame
 */
void Table::set_stack_frame(Label const& label)
{
    credence_assert(functions.contains(label));
    stack_frame = functions[label];
}

/**
 * @brief Set function definition label as current frame stack,
 *  Set instruction address location on the frame
 */
void Table::from_func_start_ita_instruction(Label const& label)
{
    Label human_label = type::get_label_as_human_readable(label);
    address_table.set_symbol_by_name(label, instruction_index - 1);
    if (labels.contains(human_label))
        throw_compiletime_error("function name already exists", human_label);

    functions[human_label] =
        std::make_shared<detail::Function>(detail::Function{ human_label });
    functions[human_label]->address_location[0] = instruction_index + 1;
    functions[human_label]->set_parameters_from_symbolic_label(label);
    labels.emplace(human_label);
    stack_frame = functions[human_label];
}

/**
 * @brief End of function, reset stack frame
 */
void Table::from_func_end_ita_instruction()
{
    if (is_stack_frame())
        get_stack_frame()->address_location[1] = instruction_index - 1;

    stack_frame.reset();
}

/**
 * @brief Parse ITA function parameters into locals on the frame stack
 *
 * e.g. `__convert(s,v,*k) = (s,v,*k)`
 */
void detail::Function::set_parameters_from_symbolic_label(
    type::semantic::Label const& label)
{
    std::string parameter;
    auto search =
        std::string_view{ label.begin() + label.find_first_of("(") + 1,
            label.begin() + label.find_first_of(")") };

    if (!search.empty())
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

/**
 * @brief Push an RValue operand onto the symbolic frame stack
 */
void Table::from_push_instruction(Quadruple const& instruction)
{
    RValue operand = std::get<1>(instruction);
    auto frame = get_stack_frame();
    if (is_stack_frame()) {
        credence_assert(frame->temporary.contains(operand));
        stack.emplace_back(frame->temporary.at(operand));
    }
}

/**
 * @brief Pop off the top of the symbolic frame stack based on operand size
 */
void Table::from_pop_instruction(Quadruple const& instruction)
{
    auto operand = std::stoul(std::get<1>(instruction));
    auto pop_size = operand / sizeof(void*);
    credence_assert(pop_size <= stack.size());
}

/**
 * @brief Parse the unary rvalue types into their operator and lvalue
 */
type::Data_Type Table::from_rvalue_unary_expression(LValue const& lvalue,
    RValue const& rvalue,
    std::string_view unary_operator)
{
    auto& locals = get_stack_frame_symbols();

    return m::match(unary_operator)(
        // cppcheck-suppress syntaxError
        m::pattern | "*" =
            [&] {
                if (locals.is_pointer(lvalue))
                    throw_compiletime_error(
                        fmt::format("dereference on invalid lvalue, "
                                    "left-hand-side is a pointer",
                            lvalue),
                        rvalue);
                from_pointer_or_vector_assignment(lvalue, rvalue, true);
                return type::Data_Type{ rvalue, "word", sizeof(void*) };
            },
        m::pattern | "&" =
            [&] {
                if (!locals.is_defined(rvalue))
                    throw_compiletime_error(
                        fmt::format(
                            "invalid pointer assignment, right-hand-side "
                            "is "
                            "not "
                            "initialized ({})",
                            rvalue),
                        lvalue);
                locals.set_symbol_by_name(lvalue, rvalue);
                return type::Data_Type{ lvalue, "word", sizeof(void*) };
            },
        m::pattern |
            "+" = [&] { return from_integral_unary_expression(lvalue); },

        m::pattern |
            "-" = [&] { return from_integral_unary_expression(lvalue); },

        m::pattern |
            "++" = [&] { return from_integral_unary_expression(lvalue); },

        m::pattern |
            "--" = [&] { return from_integral_unary_expression(lvalue); },

        m::pattern |
            "~" = [&] { return from_integral_unary_expression(lvalue); },

        m::pattern | m::_ = [&] { return locals.get_symbol_by_name(lvalue); });
}

/**
 * @brief Recursively resolve and return the rvalue of a temporary lvalue
 * in the table
 */
Table::RValue Table::from_temporary_lvalue(LValue const& lvalue)
{
    auto rvalue = util::contains(lvalue, "_t") or util::contains(lvalue, "_p")
                      ? get_stack_frame()->temporary[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        return rvalue;
    } else {
        if (util::contains(rvalue, "_t") or util::contains(lvalue, "_p"))
            return from_temporary_lvalue(rvalue);
        else
            return rvalue;
    }
}

/**
 * @brief Reassign table entry for a temporary lvalue assignment
 */
void Table::from_temporary_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto& locals = get_stack_frame_symbols();
    auto rvalue = from_temporary_lvalue(rhs);
    if (locals.is_pointer(lhs) and rvalue == "RET") {
        locals.set_symbol_by_name(lhs, "RET");
    }
    if (is_vector_or_pointer(rvalue)) {
        from_pointer_or_vector_assignment(lhs, rvalue);
        return;
    }
    if (!rvalue.empty())
        locals.set_symbol_by_name(lhs, { rvalue, "word", sizeof(void*) });
    else
        locals.set_symbol_by_name(lhs, { rhs, "word", sizeof(void*) });
}

/**
 * @brief Construct table entry for a temporary lvalue assignment
 */
void Table::from_temporary_assignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = get_stack_frame();
    frame->temporary[lhs] = rhs;
    if (lhs.starts_with("_p"))
        frame->locals.set_symbol_by_name(lhs, rhs);
    if (type::is_rvalue_data_type(rhs)) {
        auto data_type = type::get_rvalue_datatype_from_string(rhs);
        if (type::get_type_from_rvalue_data_type(data_type) == "string")
            strings.emplace(type::get_value_from_rvalue_data_type(data_type));
        return;
    }
    if (lhs.starts_with("_p") and not rhs.starts_with("_t")) {
#ifndef CREDENCE_TEST
        if (!frame->is_parameter(rhs) and is_null_symbol(rhs))
            throw_compiletime_error("unitialized lvalue as argument", rhs);
#endif
    }
}

/**
 * @brief Assign left-hand-side lvalue to rvalue or rvalue reference
 *
 *  * Do not allow assignment or lvalues with different sizes
 */
void Table::from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = get_stack_frame();
    auto& locals = get_stack_frame_symbols();

    if (!locals.is_defined(lhs))
        throw_compiletime_error(
            fmt::format("invalid lvalue assignment '{}', left-hand-side is not "
                        "initialized",
                lhs),
            rhs);
    if (!locals.is_defined(rhs))
        throw_compiletime_error(
            fmt::format(
                "invalid lvalue assignment '{}', right-hand-side is not "
                "initialized",
                rhs),
            lhs);

    type_invalid_assignment_check(lhs, rhs);
    locals.set_symbol_by_name(lhs, locals.get_symbol_by_name(rhs));
}

/**
 * @brief
 * Parse numeric ITA unary expressions, where the rvalue may
 * be from the temporary stack or the local frame stack
 */
type::Data_Type Table::from_integral_unary_expression(RValue const& lvalue)
{
    auto frame = get_stack_frame();
    auto& locals = get_stack_frame_symbols();

    auto rvalue = type::get_unary_rvalue_reference(lvalue);
    if (!locals.is_defined(rvalue) and not frame->temporary.contains(rvalue))
        throw_compiletime_error(
            fmt::format(
                "invalid numeric unary expression, lvalue symbol '{}' is "
                "not "
                "initialized",
                rvalue),
            lvalue);

    if (frame->temporary.contains(lvalue)) {
        auto temp_rvalue =
            type::get_unary_rvalue_reference(frame->temporary[lvalue]);
        credence_assert(type::is_rvalue_data_type(temp_rvalue));
        auto rdt = type::get_rvalue_datatype_from_string(temp_rvalue);
        assert_integral_unary_expression(
            frame->temporary[lvalue], std::get<1>(rdt));
        return rdt;
    } else {
        auto symbol = locals.get_symbol_by_name(rvalue);
        auto type = std::get<1>(symbol);
        if (type == "word") {
            type::Data_Type local_rvalue{};
            auto symbol_value = std::get<0>(symbol);
            if (type::is_unary_expression(symbol_value)) {
                auto rhs = type::get_unary_rvalue_reference(symbol_value);
                local_rvalue = locals.is_defined(rhs)
                                   ? locals.get_symbol_by_name(rhs)
                                   : type::get_rvalue_datatype_from_string(rhs);
            } else {
                local_rvalue = symbol;
            }

            auto local_rvalue_reference =
                type::get_value_from_rvalue_data_type(local_rvalue);
            if (type::is_unary_expression(local_rvalue_reference)) {
                auto lvalue_symbol = locals.get_symbol_by_name(lvalue);
                auto lvalue_type =
                    type::get_type_from_rvalue_data_type(lvalue_symbol);
                if (lvalue_type != "word")
                    assert_integral_unary_expression(
                        local_rvalue_reference, std::get<1>(lvalue_symbol));
                return local_rvalue;
            }
            if (type::get_type_from_rvalue_data_type(symbol) != "word")
                assert_integral_unary_expression(
                    std::get<0>(locals.get_symbol_by_name(rvalue)),
                    std::get<1>(local_rvalue));
            return local_rvalue;
        } else {
            assert_integral_unary_expression(rvalue, type);
        }
        return locals.get_symbol_by_name(rvalue);
    }
}

/**
 * @brief Search the ita instructions set in
 * a stack frame for a specific instruction
 */
bool Table::stack_frame_contains_ita_instruction(Label name, Instruction inst)
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
 * @brief Parse ITA::Node ast and symbols to a table instructions pair
 */
Table::Table_PTR Table::build_from_ast(ITA::Node const& symbols,
    ITA::Node const& ast)
{
    auto [globals, instructions] = make_ITA_instructions(symbols, ast);
    return std::make_unique<Table>(Table{ symbols, instructions, globals });
}

/**
 * @brief Type checking on lvalue and rvalue
 */
inline void Table::type_invalid_assignment_check(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue))
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_compiletime_error(
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                rvalue,
                get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Type checking on vector-to-vector same index assignment
 */
inline void Table::type_invalid_assignment_check(Vector_PTR const& vector_lhs,
    Vector_PTR const& vector_rhs,
    RValue const& index)
{
    auto vector_lvalue = vector_lhs->data.at(index);
    auto vector_rvalue = vector_rhs->data.at(index);
    if (!lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_compiletime_error(
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "with type '{}' "
                        "is not the same type ({})",
                vector_lhs->symbol,
                type::get_type_from_rvalue_data_type(vector_lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs->symbol);
}

/**
 * @brief Type checking on vector-to-vector index assignment
 */
inline void Table::type_invalid_assignment_check(Vector_PTR const& vector_lhs,
    Vector_PTR const& vector_rhs,
    RValue const& index_lhs,
    RValue const& index_rhs)
{
    auto vector_lvalue = vector_lhs->data.at(index_lhs);
    auto vector_rvalue = vector_rhs->data.at(index_rhs);

    if (!lhs_rhs_type_is_equal(vector_lvalue, vector_rvalue))
        throw_compiletime_error(
            fmt::format("invalid vector assignment, left-hand-side '{}' "
                        "at index '{}' "
                        "with type '{}' is not the same type as "
                        "right-hand-side vector "
                        "'{}' at index '{}' ({})",
                vector_lhs->symbol,
                index_lhs,
                type::get_type_from_rvalue_data_type(vector_lvalue),
                vector_rhs->symbol,
                index_rhs,
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_lhs->symbol);
}

/**
 * @brief Type checking on lvalue-to-vector assignment
 */
inline void Table::type_invalid_assignment_check(LValue const& lvalue,
    Vector_PTR const& vector_rhs,
    RValue const& index)
{
    auto vector_rvalue = vector_rhs->data.at(index);
    if (get_type_from_rvalue_data_type(lvalue) != "null" and
        not lhs_rhs_type_is_equal(lvalue, vector_rvalue))
        throw_compiletime_error(
            fmt::format("invalid lvalue assignment to a "
                        "vector, left-hand-side '{}' with "
                        "type '{}' is not the same type "
                        "({})",
                lvalue,
                get_type_from_rvalue_data_type(lvalue),
                type::get_type_from_rvalue_data_type(vector_rvalue)),
            vector_rhs->symbol);
}

/**
 * @brief Type checking on lvalue and value data type
 */
inline void Table::type_invalid_assignment_check(LValue const& lvalue,
    type::Data_Type const& rvalue)
{
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        throw_compiletime_error(
            fmt::format("invalid assignment, right-hand-side '{}' "
                        "with type '{}' is not the same type ({})",
                std::get<0>(rvalue),
                type::get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Raise error with stack frame symbol
 */
inline void Table::throw_compiletime_error(std::string_view message,
    type::RValue_Reference symbol,
    std::source_location const& location)
{
    credence_compile_error(location,
        fmt::format("{} in function '{}'", message, get_stack_frame()->symbol),
        symbol,
        hoisted_symbols);
}

/**
 * @brief Set the stack frame return value in the table
 */
void Table::set_stack_frame_return_value(RValue const& rvalue)
{
    auto frame = get_stack_frame();
    auto is_pointer_ = [&](RValue const& value) {
        return frame->locals.is_pointer(value);
    };
    auto is_parameter = [&](RValue const& value) {
        return frame->is_parameter(value);
    };
    Type_Check_Lambda local_contains =
        std::bind(&Table::local_contains_, this, std::placeholders::_1);
    m::match(rvalue)(
        m::pattern | m::app(is_pointer_, true) =
            [&] {
                frame->ret = std::make_pair(
                    frame->locals.get_pointer_by_name(rvalue), rvalue);
            },
        m::pattern | m::app(is_parameter, true) =
            [&] {
                auto index_of = frame->get_index_of_parameter(rvalue);
                credence_assert_nequal(index_of, -1);
                frame->ret = std::make_pair(stack.at(index_of), rvalue);
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
                if (type::is_rvalue_data_type_string(rvalue)) {
                    frame->ret = std::make_pair(value_at, rvalue);
                    return;
                }
                if (type::is_rvalue_data_type_word(rvalue)) {
                    frame->ret = std::make_pair(value_at, rvalue);
                    return;
                }
                frame->ret = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                auto value_at = get_rvalue_data_type_at_pointer(rvalue);
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

// clang-format off
/**
 * @brief Emit ita instructions type and vector boundary checking an
 * and to an std::ostream Passes the global symbols from the ITA object
 */
void emit(std::ostream& os, util::AST_Node const& symbols, util::AST_Node const& ast)
{
    // clang-format on
    auto [globals, instructions] = ir::make_ITA_instructions(symbols, ast);
    auto table = ir::Table{ symbols, instructions, globals };
    table.build_from_ita_instructions();
    detail::emit(os, table.instructions);
}

} // namespace ir

} // namespace credence