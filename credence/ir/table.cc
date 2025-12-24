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

#include <credence/ir/table.h>

#include <array>                // for array
#include <credence/error.h>     // for throw_object_type_error, credence_as...
#include <credence/ir/ita.h>    // for Instruction, Quadruple, emit, get_rv...
#include <credence/ir/object.h> // for Object, Function, Vector, LValue
#include <credence/ir/types.h>  // for Type_Checker
#include <credence/map.h>       // for Ordered_Map
#include <credence/symbol.h>    // for Symbol_Table
#include <credence/types.h>     // for get_rvalue_datatype_from_string, get...
#include <credence/util.h>      // for contains, AST_Node, str_trim_ws
#include <credence/values.h>    // for expression_type_to_string
#include <cstddef>              // for size_t
#include <easyjson.h>           // for JSON
#include <fmt/format.h>         // for format
#include <limits>               // for numeric_limits
#include <map>                  // for operator!=
#include <matchit.h>            // for pattern, PatternHelper, PatternPipable
#include <optional>             // for optional
#include <string>               // for basic_string, char_traits, operator==
#include <string_view>          // for basic_string_view, string_view
#include <tuple>                // for get, tuple, operator==
#include <utility>              // for pair, get
#include <vector>               // for vector

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief IR Emitter factory
 *
 * Emit ita instructions type and vector boundary checking an
 * and to an std::ostream Passes the global symbols from the ITA object
 */
void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    auto [globals, instructions] = ir::make_ita_instructions(symbols, ast);
    auto table = ir::Table{ symbols, instructions, globals };
    table.build_from_ir_instructions();
    detail::emit(os, *table.get_table_instructions());
}

/**
 * @brief Construct table and run type-checking pass on IR instructions
 */
void Table::build_from_ir_instructions()
{
    bool skip = false;
    Instruction last_instruction = Instruction::NOOP;

    build_vector_definitions_from_symbols();
    build_vector_definitions_from_globals();

    for (instruction_index = 0; instruction_index < instructions_->size();
        instruction_index++) {
        auto instruction = instructions_->at(instruction_index);
        m::match(std::get<Instruction>(instruction))(
            m::pattern | Instruction::FUNC_START =
                [&] {
                    from_func_start_ita_instruction(
                        std::get<1>(instructions_->at(instruction_index - 1)));
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
            instructions_->erase(instructions_->begin() + instruction_index);
            skip = false;
        }
        last_instruction = std::get<Instruction>(instruction);
    }
}

/**
 * @brief Construct table of vector symbols and size allocation
 */
void Table::build_vector_definitions_from_symbols()
{
    auto& hoisted_symbols = objects_->hoisted_symbols;
    auto keys = hoisted_symbols.dump_keys();
    for (LValue const& key : keys) {
        auto symbol_type = hoisted_symbols[key]["type"].to_string();
        if (symbol_type == "vector_lvalue") {
            auto size =
                static_cast<std::size_t>(hoisted_symbols[key]["size"].to_int());
            if (size > object::Vector::max_size)
                throw_object_type_error("stack overflow", key);
            if (!objects_->vectors.contains(key))
                objects_->vectors[key] = std::make_shared<object::Vector>(
                    object::Vector{ key, size });
        }
    }
}

/**
 * @brief Construct table entry from locl (auto) instruction
 */
void Table::from_locl_ita_instruction(Quadruple const& instruction)
{
    auto label = std::get<1>(instruction);
    auto frame = objects_->get_stack_frame();
    if (std::get<1>(instruction).starts_with("*")) {
        label = std::get<1>(instruction);
        frame->locals.set_symbol_by_name(label.substr(1), "NULL");
    } else
        frame->locals.set_symbol_by_name(label, type::NULL_RVALUE_LITERAL);
}

/**
 * @brief Set return rvalue on the current stack frame in the table
 */
void Table::from_return_instruction(Quadruple const& instruction)
{
    auto return_rvalue = util::str_trim_ws(std::get<1>(instruction));
    auto frame = objects_->get_stack_frame();
    if (frame->ret.has_value())
        throw_object_type_error("invalid return statement", return_rvalue);
    object::set_stack_frame_return_value(return_rvalue, frame, objects_);
}

/**
 * @brief Construct table entry from globl (extrn) instruction
 */
void Table::from_globl_ita_instruction(Label const& label)
{
    if (!objects_->vectors.contains(label))
        throw_object_type_error(
            "extrn statement failed, identifier does not exist", label);
    auto frame = objects_->get_stack_frame();
    frame->locals.set_symbol_by_name(label, type::NULL_RVALUE_LITERAL);
}

/**
 * @brief Set vector globals from an ITA constructor
 */
void Table::build_vector_definitions_from_globals()
{
    auto& globals = objects_->globals;
    if (!globals.empty())
        for (auto i = globals.begin_t(); i != globals.end_t(); i++) {
            std::size_t index = 0;
            auto symbol = *i;
            objects_->vectors[symbol.first] = std::make_shared<object::Vector>(
                object::Vector{ symbol.first, symbol.second.size() });
            for (auto const& item : symbol.second) {
                auto key = std::to_string(index++);
                auto value = type::get_rvalue_datatype_from_string(
                    value::expression_type_to_string(item, false));
                insert_address_storage_rvalue(value);
                objects_->vectors[symbol.first]->data[key] = value;
            }
        }
}

/**
 * @brief Ensure CALL right-hand-side is a valid symbol
 */
void Table::from_call_ita_instruction(Label const& label)
{
    if (!objects_->labels.contains(label) and
        not objects_->hoisted_symbols.has_key(label))
        throw_object_type_error(
            "function call failed, function does not exist", label);
}

/**
 * @brief add label and label instruction address entry from LABEL
 * instruction
 */
void Table::from_label_ita_instruction(Quadruple const& instruction)
{
    Label label = std::get<1>(instruction);
    if (objects_->is_stack_frame()) {
        auto frame = objects_->get_stack_frame();
        if (frame->labels.contains(label))
            throw_object_type_error("label is already defined", label);
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
    auto frame = objects_->get_stack_frame();

    auto type_checker = Type_Checker{ objects_, frame };

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        from_temporary_assignment(lhs, rhs);
        return;
    }
    if (objects_->is_stack_frame() and
        (rhs.starts_with("_t") or rhs.starts_with("_p"))) {
        from_temporary_reassignment(lhs, rhs);
        return;
    }
    if (type_checker.is_vector_or_pointer(lhs) or
        type_checker.is_vector_or_pointer(rhs) or rvalue.second == "&") {
        from_pointer_or_vector_assignment(lhs, rhs);
        return;
    }
    if (objects_->hoisted_symbols.has_key(rhs)) {
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
        throw_object_type_error(
            fmt::format("invalid lvalue assignment on '{}'", lhs), rhs);

    Size size = std::get<2>(rvalue_symbol);

    if (size > std::numeric_limits<unsigned int>::max())
        throw_object_type_error(
            fmt::format("right-hand-side exceeds maximum byte size '{}'", rhs),
            lhs);

    frame->locals.set_symbol_by_name(lhs, rvalue_symbol);
    frame->allocation += size;

    insert_address_storage_rvalue(rvalue_symbol);
}

/**
 * @brief Save literals that need %rip address in the data section
 */
void Table::insert_address_storage_rvalue(RValue const& rvalue)
{
    auto type = type::get_type_from_rvalue_data_type(rvalue);
    if (type == "float")
        objects_->floats.insert(type::get_value_from_rvalue_data_type(rvalue));
    if (type == "double")
        objects_->doubles.insert(type::get_value_from_rvalue_data_type(rvalue));
    if (type == "string")
        objects_->strings.insert(type::get_value_from_rvalue_data_type(rvalue));
}

void Table::insert_address_storage_rvalue(type::Data_Type const& rvalue)
{
    auto type = type::get_type_from_rvalue_data_type(rvalue);
    if (type == "float")
        objects_->floats.insert(type::get_value_from_rvalue_data_type(rvalue));
    if (type == "double")
        objects_->doubles.insert(type::get_value_from_rvalue_data_type(rvalue));
    if (type == "string")
        objects_->strings.insert(type::get_value_from_rvalue_data_type(rvalue));
}

/**
 * @brief Pointer or vector assignment of LValues
 *
 * Type check out-of-range boundary on left-hand-side and right-hand-side
 * of assignment
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
    auto& locals = objects_->get_stack_frame_symbols();
    auto& vectors = objects_->vectors;
    auto frame = objects_->get_stack_frame();
    std::string rhs_lvalue{};

    auto type_checker = Type_Checker{ objects_, frame };

    std::optional<type::Data_Type> rvalue_symbol;

    // This is an lvalue indirection assignment, `m = *k;` or `*k = m`;
    // if the right-hand-side is a vector, save the lvalue and the data
    // type
    if (util::contains(rvalue, "[")) {
        rhs_lvalue = type::from_lvalue_offset(rvalue);
        auto safe_rvalue = type::get_unary_rvalue_reference(rhs_lvalue);
        auto offset = type::from_decay_offset(rvalue);

        type_checker.is_boundary_out_of_range(
            type::get_unary_rvalue_reference(rvalue));

        if (!frame->is_scaler_parameter(offset) and
            not locals.is_defined(offset) and
            (!vectors.contains(safe_rvalue) or
                not vectors[safe_rvalue]->data.contains(offset)))
            throw_object_type_error(
                fmt::format("invalid vector assignment, element at '{}' "
                            "does not exist",
                    offset),
                rvalue);
        type_checker.type_safe_assign_pointer_or_vector_lvalue(
            lvalue, rvalue, indirection);
        return;
    }

    // if the left-hand-side is a normal vector:
    if (util::contains(lvalue, "[")) {
        type_checker.is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        // the rhs is a vector too, check accessed types
        if (rvalue_symbol.has_value()) {
            if (!type_checker.lhs_rhs_type_is_equal(
                    lhs_lvalue, rvalue_symbol.value()))
                throw_object_type_error(
                    fmt::format("invalid lvalue assignment, "
                                "right-hand-side \"{}\" "
                                "with "
                                "type {} is not the same type ({})",
                        rvalue,
                        type_checker.get_type_from_rvalue_data_type(lvalue),
                        type::get_type_from_rvalue_data_type(
                            rvalue_symbol.value())),
                    lvalue);
            vectors[lhs_lvalue]->data[offset] = rvalue_symbol.value();
        } else {
            // is the rhs a scaler rvalue? e.g. (10:"int":4UL)
            if (type::is_rvalue_data_type(rvalue)) {
                // update the lhs vector, if applicable
                auto value = type::get_rvalue_datatype_from_string(rvalue);
                insert_address_storage_rvalue(value);
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
            throw_object_type_error(
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
        type_checker.type_safe_assign_pointer_or_vector_lvalue(
            lvalue, rvalue, indirection);
    else
        type_checker.type_safe_assign_pointer_or_vector_lvalue(
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
    if (objects_->vectors.contains(lhs)) {
        objects_->vectors[lhs]->data["0"] = rvalue;
    }
}

/**
 * @brief Set function definition label as current frame stack,
 *  Set instruction address location on the frame
 */
void Table::from_func_start_ita_instruction(Label const& label)
{
    Label human_label = type::get_label_as_human_readable(label);
    objects_->address_table.set_symbol_by_name(label, instruction_index - 1);
    if (objects_->labels.contains(human_label))
        throw_object_type_error("function name already exists", human_label);

    objects_->functions[human_label] =
        std::make_shared<object::Function>(object::Function{ human_label });
    objects_->functions[human_label]->address_location[0] =
        instruction_index + 1;
    objects_->functions[human_label]->set_parameters_from_symbolic_label(label);
    objects_->labels.emplace(human_label);
    objects_->set_stack_frame(human_label);
}

/**
 * @brief End of function, reset stack frame
 */
void Table::from_func_end_ita_instruction()
{
    if (objects_->is_stack_frame())
        objects_->get_stack_frame()->address_location[1] =
            instruction_index - 1;

    objects_->reset_stack_frame();
}

/**
 * @brief Push an RValue operand onto the symbolic frame stack
 */
void Table::from_push_instruction(Quadruple const& instruction)
{
    RValue operand = std::get<1>(instruction);
    auto frame = objects_->get_stack_frame();
    if (objects_->is_stack_frame()) {
        credence_assert(frame->temporary.contains(operand));
        objects_->stack.emplace_back(frame->temporary.at(operand));
    }
}

/**
 * @brief Pop off the top of the symbolic frame stack based on operand size
 */
void Table::from_pop_instruction(Quadruple const& instruction)
{
    auto operand = std::stoul(std::get<1>(instruction));
    auto pop_size = operand / sizeof(void*);
    credence_assert(pop_size <= objects_->stack.size());
}

/**
 * @brief Parse the unary rvalue types into their operator and lvalue
 */
type::Data_Type Table::from_rvalue_unary_expression(LValue const& lvalue,
    RValue const& rvalue,
    std::string_view unary_operator)
{
    auto& locals = objects_->get_stack_frame_symbols();

    return m::match(unary_operator)(
        m::pattern | "*" =
            [&] {
                if (locals.is_pointer(lvalue))
                    throw_object_type_error(
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
                    throw_object_type_error(
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

/**
 * @brief Reassign table entry for a temporary lvalue assignment
 */
void Table::from_temporary_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = objects_->get_stack_frame();
    auto& locals = objects_->get_stack_frame_symbols();
    auto rvalue = objects_->lvalue_at_temporary_object_address(rhs, frame);
    auto type_checker = Type_Checker{ objects_, frame };
    if (locals.is_pointer(lhs) and rvalue == "RET") {
        locals.set_symbol_by_name(lhs, "RET");
    }
    if (type_checker.is_vector_or_pointer(rvalue)) {
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
    auto frame = objects_->get_stack_frame();
    frame->temporary[lhs] = rhs;
    if (lhs.starts_with("_p"))
        frame->locals.set_symbol_by_name(lhs, rhs);
    if (type::is_rvalue_data_type(rhs)) {
        auto data_type = type::get_rvalue_datatype_from_string(rhs);
        insert_address_storage_rvalue(data_type);
    }
}

/**
 * @brief Assign left-hand-side lvalue to rvalue or rvalue reference
 *
 *  * Do not allow assignment or lvalues with different sizes
 */
void Table::from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = objects_->get_stack_frame();
    auto& locals = objects_->get_stack_frame_symbols();

    auto type_checker = Type_Checker{ objects_, frame };

    if (!locals.is_defined(lhs))
        throw_object_type_error(
            fmt::format("invalid lvalue assignment '{}', left-hand-side is not "
                        "initialized",
                lhs),
            rhs);
    if (!locals.is_defined(rhs))
        throw_object_type_error(
            fmt::format(
                "invalid lvalue assignment '{}', right-hand-side is not "
                "initialized",
                rhs),
            lhs);

    type_checker.type_invalid_assignment_check(lhs, rhs);
    locals.set_symbol_by_name(lhs, locals.get_symbol_by_name(rhs));
}

/**
 * @brief
 * Parse numeric ITA unary expressions, where the rvalue may
 * be from the temporary stack or the local frame stack
 */
type::Data_Type Table::from_integral_unary_expression(RValue const& lvalue)
{
    auto frame = objects_->get_stack_frame();
    auto& locals = objects_->get_stack_frame_symbols();

    auto type_checker = Type_Checker{ objects_, frame };

    auto rvalue = type::get_unary_rvalue_reference(lvalue);
    if (!locals.is_defined(rvalue) and not frame->temporary.contains(rvalue))
        throw_object_type_error(
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
        type_checker.assert_integral_unary_expression(
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
                    type_checker.assert_integral_unary_expression(
                        local_rvalue_reference, std::get<1>(lvalue_symbol));
                return local_rvalue;
            }
            if (type::get_type_from_rvalue_data_type(symbol) != "word")
                type_checker.assert_integral_unary_expression(
                    std::get<0>(locals.get_symbol_by_name(rvalue)),
                    std::get<1>(local_rvalue));
            return local_rvalue;
        } else {
            type_checker.assert_integral_unary_expression(rvalue, type);
        }
        return locals.get_symbol_by_name(rvalue);
    }
}

} // namespace ir

} // namespace credence