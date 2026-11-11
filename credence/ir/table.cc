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

#include <algorithm>         // for find_if
#include <credence/error.h>  // for credence_assert
#include <credence/ir/ita.h> // for Instruction, Quadruple, emit
#include <credence/object.h> // for Object, Function, LValue
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for get_rvalue_datatype_from_string
#include <credence/util.h>   // for contains, AST_Node, str_trim_ws
#include <deque>             // for deque
#include <easyjson.h>        // for JSON
#include <fmt/format.h>      // for format
#include <map>               // for operator!=
#include <matchit.h>         // for pattern, PatternHelper, Patt...
#include <string>            // for basic_string, char_traits
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple, operator==
#include <utility>           // for pair, get

/****************************************************************************
 * Table
 *
 * Binds the ITA instructions of a program onto the object table built by
 * the semantic analysis pass in hir/table.h. A visitor pattern on ITA
 * instructions that stores function frame addresses, label addresses,
 * temporaries, call parameters, and the storage of each local for backend
 * passes. Type checking is done on the HIR, before the IR is constructed.
 *
 *  Example table construction:
 *
 *  main() {
 *    auto x,
 *    auto arr[3];
 *    x = 10;
 *    arr[0] = x;
 *  }
 *
 *  Builds Object table:
 *    functions["main"] -> { locals: {x: (10:int:4)} }
 *    vectors["arr"]    -> { size: 3, data: {"0": (10:int:4)} }
 *
 ****************************************************************************/

namespace credence {

namespace ir {

namespace m = matchit;

namespace {

/**
 * @brief Check if an rvalue is one literal in data type form
 *
 * A binary expression of two literals is also in parentheses, so the
 * parenthesis that opens a literal must close at its end. A parenthesis or
 * space in a string literal is part of the string.
 *
 *  Examples:
 *
 *    is_single_rvalue_data_type("(\"m is %d\":string:7)")   // true
 *    is_single_rvalue_data_type("(2:int:4) + (3:int:4)")    // false
 */
bool is_single_rvalue_data_type(RValue const& rvalue)
{
    if (!type::is_rvalue_data_type(rvalue))
        return false;
    bool quoted{ false };
    int depth{ 0 };
    for (std::size_t i = 0; i < rvalue.size(); i++) {
        if (rvalue[i] == '"' and (i == 0 or rvalue[i - 1] != '\\'))
            quoted = not quoted;
        else if (quoted)
            continue;
        else if (rvalue[i] == '(')
            depth++;
        else if (rvalue[i] == ')' and --depth == 0 and i + 1 != rvalue.size())
            return false;
    }
    return true;
}

} // namespace

/**
 * @brief IR Emitter factory
 *
 * Emit the ita instructions of a program bound onto its object table
 */
void emit(std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    auto table = make_table(symbols, ast);
    detail::emit(os, *table->get_table_instructions());
}

/**
 * @brief Bind each IR instruction onto the object table
 */
void Table::build_from_ir_instructions()
{
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
                [&] { from_label_ita_instruction(instruction); });
    }
}

/**
 * @brief Construct table entry from locl (auto) instruction
 */
void Table::from_locl_ita_instruction(Quadruple const& instruction)
{
    auto label = std::get<1>(instruction);
    auto& frame = objects_->get_stack_frame();
    if (label.starts_with("*"))
        frame.locals.set_symbol_by_name(label.substr(1), "NULL");
    else
        frame.locals.set_symbol_by_name(label, type::NULL_RVALUE_LITERAL);
}

/**
 * @brief Set return rvalue on the current stack frame in the table
 *
 * The last return statement of a function is its return value for callers
 */
void Table::from_return_instruction(Quadruple const& instruction)
{
    auto return_rvalue = util::str_trim_ws(std::get<1>(instruction));
    auto& frame = objects_->get_stack_frame();
    // a returned literal, such as a string, is a literal of the data section
    if (is_single_rvalue_data_type(return_rvalue))
        objects_->insert_address_storage_rvalue(
            type::get_rvalue_datatype_from_string(return_rvalue));
    object::set_stack_frame_return_value(return_rvalue, frame, *objects_);
}

/**
 * @brief Construct table entry from globl (extrn) instruction
 */
void Table::from_globl_ita_instruction(Label const& label)
{
    auto& frame = objects_->get_stack_frame();
    frame.locals.set_symbol_by_name(label, type::NULL_RVALUE_LITERAL);
}

void Table::from_call_ita_instruction(Label const& label)
{
    objects_->set_ir_parameters(label, temporary_parameter_stack);
    temporary_parameter_stack.clear();
}

/**
 * @brief add label and label instruction address entry from LABEL
 * instruction
 */
void Table::from_label_ita_instruction(Quadruple const& instruction)
{
    Label label = std::get<1>(instruction);
    if (objects_->is_stack_frame()) {
        auto& frame = objects_->get_stack_frame();
        frame.labels.emplace(label);
        frame.label_address.set_symbol_by_name(label, instruction_index);
    }
}

/**
 * @brief Deconstruct assignment instructions into each
 * type and populate function frame stack in the table
 *
 *  * LValues that begin with `_t` or `_p` are temporaries or parameters
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
    auto& frame = objects_->get_stack_frame();

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        from_temporary_assignment(lhs, rhs);
        return;
    }
    if (objects_->is_stack_frame() and
        (rhs.starts_with("_t") or rhs.starts_with("_p"))) {
        from_temporary_reassignment(lhs, rhs);
        return;
    }
    if (objects_->is_vector_or_pointer(lhs) or
        objects_->is_vector_or_pointer(rhs) or rvalue.second == "&") {
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

    credence_assert(rvalue_symbol != type::NULL_RVALUE_LITERAL);

    frame.locals.set_symbol_by_name(lhs, rvalue_symbol);
    frame.allocation += std::get<2>(rvalue_symbol);

    objects_->insert_address_storage_rvalue(rvalue_symbol);
}

/**
 * @brief Pointer or vector assignment of LValues
 *
 *  Examples:
 *
 *     auto *k, m, *z
 *     k = &m;
 *     k = z;
 *     array[2] = m;
 *     array[1] = array[2]
 *     m = array[2];
 *     k = &array[2];
 */
void Table::from_pointer_or_vector_assignment(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = objects_->get_stack_frame_symbols();
    auto& vectors = objects_->vectors;

    // the right-hand-side is a vector offset, `m = v[2];` or `k = &v[2];`
    if (util::contains(rvalue, "[")) {
        bind_pointer_or_vector_lvalue(lvalue, rvalue);
        return;
    }
    // the left-hand-side is a vector offset, `v[2] = m;`
    if (util::contains(lvalue, "[")) {
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_decay_offset(lvalue);
        if (type::is_rvalue_data_type(rvalue)) {
            auto value = type::get_rvalue_datatype_from_string(rvalue);
            objects_->insert_address_storage_rvalue(value);
            if (vectors.contains(lhs_lvalue))
                vectors.at(lhs_lvalue).get().data[offset] = value;
        }
        return;
    }
    // the right-hand-side is an address-of rvalue, `k = &m;`
    if (rvalue.starts_with("&")) {
        locals.set_symbol_by_name(
            lvalue, type::get_unary_rvalue_reference(rvalue));
        return;
    }
    bind_pointer_or_vector_lvalue(lvalue, rvalue);
}

/**
 * @brief Bind pointers and vectors, or scalers and dereferenced pointers
 */
void Table::bind_pointer_or_vector_lvalue(LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = objects_->get_stack_frame_symbols();
    if (is_trivial_vector_assignment(lvalue, rvalue)) {
        bind_trivial_vector(lvalue, rvalue);
        return;
    }
    if ((objects_->is_pointer(lvalue) or objects_->is_pointer(rvalue)) and
        not type::is_dereference_expression(rvalue)) {
        bind_pointer(lvalue, rvalue);
        return;
    }
    if (type::is_dereference_expression(lvalue) or
        type::is_dereference_expression(rvalue)) {
        bind_dereference(lvalue, rvalue);
        return;
    }
    if (objects_->is_vector(lvalue) or objects_->is_vector(rvalue)) {
        bind_vector(lvalue, rvalue);
        return;
    }
    locals.set_symbol_by_name(lvalue, rvalue);
}

/**
 * @brief Bind pointer and address-of pointer assignments
 *
 * A pointer increment or decrement, `p++`, keeps the address it points to
 */
void Table::bind_pointer(LValue const& lvalue, RValue const& rvalue)
{
    auto& frame = objects_->get_stack_frame();
    auto& locals = frame.locals;
    if (!locals.is_pointer(lvalue))
        return;
    // pointer to pointer
    if (locals.is_pointer(rvalue)) {
        locals.set_symbol_by_name(lvalue,
            type::get_rvalue_data_type_as_string(
                object::get_rvalue_at_lvalue_object_storage(
                    rvalue, frame, objects_->vectors, __source__)));
        return;
    }
    // pointer to address-of lvalue
    if (type::get_unary_operator(rvalue) == "&") {
        locals.set_symbol_by_name(lvalue, rvalue);
        return;
    }
    // pointer to string literal
    if (type::is_rvalue_data_type_string(rvalue)) {
        objects_->strings.insert(type::get_value_from_rvalue_data_type(rvalue));
        locals.set_symbol_by_name(lvalue, rvalue);
        return;
    }
    // pointer to string storage in a vector
    if (objects_->is_vector(rvalue) and
        type::get_type_from_rvalue_data_type(
            object::get_rvalue_at_lvalue_object_storage(
                rvalue, frame, objects_->vectors)) == "string")
        locals.set_symbol_by_name(lvalue, rvalue);
}

/**
 * @brief Bind trivial vectors, or vectors with one element
 */
void Table::bind_trivial_vector(LValue const& lvalue, RValue const& rvalue)
{
    auto vector_contains = [&](LValue const& label) {
        return objects_->vector_contains(label);
    };
    auto local_contains = [&](LValue const& label) {
        return objects_->local_contains(label);
    };
    auto& locals = objects_->get_stack_frame_symbols();
    auto& vectors = objects_->vectors;
    m::match(lvalue, rvalue)(
        m::pattern | m::ds(m::app(vector_contains, true),
                         m::app(vector_contains, true)) =
            [&] {
                auto value = vectors.at(rvalue).get().data["0"];
                vectors.at(lvalue).get().data["0"] = value;
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                vectors.at(lvalue).get().data["0"] =
                    locals.get_symbol_by_name(rvalue);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                locals.set_symbol_by_name(
                    lvalue, vectors.at(rvalue).get().data["0"]);
            });
}

/**
 * @brief Bind an assignment from a vector to an lvalue or vice-versa
 */
void Table::bind_vector(LValue const& lvalue, RValue const& rvalue)
{
    auto& frame = objects_->get_stack_frame();
    auto& locals = frame.locals;
    RValue lvalue_offset{ "0" };
    RValue rvalue_offset{ "0" };
    auto vector_contains = [&](LValue const& label) {
        return objects_->vector_contains(label);
    };
    auto local_contains = [&](LValue const& label) {
        return objects_->local_contains(label);
    };

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
                // copy first, the left-hand-side may insert into the same
                // vector, e.g. `v[1] = v[0]`
                auto value = objects_->vectors.at(rvalue_direct)
                                 .get()
                                 .data[rvalue_offset];
                objects_->vectors.at(lvalue_direct).get().data[lvalue_offset] =
                    value;
            },
        m::pattern |
            m::ds(m::app(vector_contains, true), m::app(local_contains, true)) =
            [&] {
                objects_->vectors.at(lvalue_direct).get().data[lvalue_offset] =
                    locals.get_symbol_by_name(rvalue_direct);
            },
        m::pattern |
            m::ds(m::app(local_contains, true), m::app(vector_contains, true)) =
            [&] {
                if (locals.is_defined(rvalue_offset))
                    rvalue_offset = type::get_value_from_rvalue_data_type(
                        object::get_rvalue_at_lvalue_object_storage(
                            rvalue_offset, frame, objects_->vectors));
                auto& vector = objects_->vectors.at(rvalue_direct).get();
                if (vector.data.empty())
                    return;
                // an offset not known at compile time, e.g. `x[errno]`,
                // resolves to the type of the first element of the vector
                if (!vector.data.contains(rvalue_offset))
                    rvalue_offset = vector.data.first().first;
                locals.set_symbol_by_name(
                    lvalue_direct, vector.data.at(rvalue_offset));
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Bind the assignment of dereferenced lvalue pointers
 *
 * An assignment through a pointer, `*k = m`, binds the pointer to the
 * right-hand-side
 */
void Table::bind_dereference(LValue const& lvalue, RValue const& rvalue)
{
    auto& frame = objects_->get_stack_frame();
    auto lhs_lvalue = type::get_unary_rvalue_reference(lvalue);
    // the object a pointer parameter points to has no storage in the table
    if (!type::is_dereference_expression(lvalue) and
        frame.is_parameter(type::get_unary_rvalue_reference(rvalue))) {
        frame.locals.set_symbol_by_name(lhs_lvalue, { rvalue, "word", 4UL });
        return;
    }
    frame.locals.set_symbol_by_name(lhs_lvalue, rvalue);
}

/**
 * @brief Either lhs or rhs are trivial vector assignments
 */
bool Table::is_trivial_vector_assignment(LValue const& lhs, LValue const& rhs)
{
    auto& vectors = objects_->vectors;
    return (vectors.contains(lhs) and vectors.at(lhs).get().data.size() == 1) or
           (vectors.contains(rhs) and vectors.at(rhs).get().data.size() == 1);
}

/**
 * @brief Set function definition label as current frame stack,
 *  Set instruction address location on the frame
 */
void Table::from_func_start_ita_instruction(Label const& label)
{
    Label human_label = type::get_label_as_human_readable(label);
    objects_->address_table.set_symbol_by_name(label, instruction_index - 1);
    auto& frame = objects_->get_stack_frame(human_label);
    frame.address_location[0] = instruction_index + 1;
    objects_->set_stack_frame(human_label);
}

/**
 * @brief End of function, reset stack frame
 */
void Table::from_func_end_ita_instruction()
{
    if (objects_->is_stack_frame())
        objects_->get_stack_frame().address_location[1] = instruction_index - 1;

    objects_->reset_stack_frame();
}

/**
 * @brief Push an RValue operand onto the symbolic frame stack
 */
void Table::from_push_instruction(Quadruple const& instruction)
{
    RValue operand = std::get<1>(instruction);
    auto& frame = objects_->get_stack_frame();
    if (objects_->is_stack_frame()) {
        temporary_parameter_stack.emplace_back(operand);
        objects_->stack.emplace_back(frame.temporary.at(operand));
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
                from_pointer_or_vector_assignment(lvalue, rvalue);
                return type::Data_Type{ rvalue, "word", sizeof(void*) };
            },
        m::pattern | "&" =
            [&] {
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
 * @brief Reassign table entry for a temporary lvalue assignment
 */
void Table::from_temporary_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto& frame = objects_->get_stack_frame();
    auto& locals = objects_->get_stack_frame_symbols();
    auto rvalue = objects_->lvalue_at_temporary_object_address(rhs, frame);
    if (locals.is_pointer(lhs) and rvalue == "RET") {
        locals.set_symbol_by_name(lhs, "RET");
    }
    if (objects_->is_vector_or_pointer(rvalue)) {
        from_pointer_or_vector_assignment(lhs, rvalue);
        return;
    }
    if (!rvalue.empty()) {
        // resolve unary expression rvalue sizes
        if (type::is_unary_expression(rvalue)) {
            auto rvalue_reference = type::get_unary_rvalue_reference(rvalue);
            if (locals.is_defined(rvalue_reference)) {
                auto [_, type, size] =
                    locals.get_symbol_by_name(rvalue_reference);
                locals.set_symbol_by_name(lhs, { rvalue, type, size });
                return;
            }
            if (type::is_rvalue_data_type(rvalue_reference)) {
                auto [_, type, size] =
                    type::get_rvalue_datatype_from_string(rvalue_reference);
                locals.set_symbol_by_name(lhs, { rvalue, type, size });
                return;
            }
        }
        locals.set_symbol_by_name(lhs, { rvalue, "word", sizeof(void*) });
    } else
        locals.set_symbol_by_name(lhs, { rhs, "word", sizeof(void*) });
}

/**
 * @brief Construct table entry for a temporary lvalue assignment
 */
void Table::from_temporary_assignment(LValue const& lhs, LValue const& rhs)
{
    auto& frame = objects_->get_stack_frame();
    frame.temporary[lhs] = rhs;
    if (lhs.starts_with("_p"))
        frame.locals.set_symbol_by_name(lhs, rhs);
    // each literal operand of a binary expression is a literal of the data
    // section
    if (is_single_rvalue_data_type(rhs)) {
        objects_->insert_address_storage_rvalue(
            type::get_rvalue_datatype_from_string(rhs));
    } else if (type::is_binary_expression(rhs)) {
        auto [left, right, _] = type::from_rvalue_binary_expression(rhs);
        for (auto const& operand : { left, right })
            if (is_single_rvalue_data_type(operand))
                objects_->insert_address_storage_rvalue(
                    type::get_rvalue_datatype_from_string(operand));
    }
}

/**
 * @brief Assign left-hand-side lvalue to rvalue or rvalue reference
 *
 * A parameter has no storage in the table, so its value is a word
 */
void Table::from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs)
{
    auto& frame = objects_->get_stack_frame();
    auto& locals = frame.locals;
    if (frame.is_parameter(rhs) and not locals.is_defined(rhs)) {
        locals.set_symbol_by_name(lhs, { rhs, "word", sizeof(void*) });
        return;
    }
    locals.set_symbol_by_name(lhs, locals.get_symbol_by_name(rhs));
}

/**
 * @brief
 * Parse numeric ITA unary expressions, where the rvalue may
 * be from the temporary stack or the local frame stack
 */
type::Data_Type Table::from_integral_unary_expression(RValue const& lvalue)
{
    auto& frame = objects_->get_stack_frame();
    auto& locals = frame.locals;
    auto rvalue = type::get_unary_rvalue_reference(lvalue);

    if (frame.temporary.contains(lvalue))
        return type::get_rvalue_datatype_from_string(
            type::get_unary_rvalue_reference(frame.temporary[lvalue]));
    if (frame.is_parameter(rvalue) and not locals.is_defined(rvalue))
        return type::Data_Type{ rvalue, "word", sizeof(void*) };

    auto symbol = locals.get_symbol_by_name(rvalue);
    if (std::get<1>(symbol) != "word")
        return symbol;

    auto symbol_value = std::get<0>(symbol);
    if (type::is_unary_expression(symbol_value)) {
        auto rhs = type::get_unary_rvalue_reference(symbol_value);
        return locals.is_defined(rhs)
                   ? locals.get_symbol_by_name(rhs)
                   : type::get_rvalue_datatype_from_string(rhs);
    }
    return symbol;
}

/**
 * @brief Search the ir instructions in a stack frame for a call instruction
 */
bool stack_frame_contains_call_instruction(object::Function const& frame,
    Instructions const& instructions)
{
    auto end = instructions.begin() + frame.address_location[1];
    auto search =
        std::ranges::find_if(instructions.begin() + frame.address_location[0],
            end,
            [&](Quadruple const& quad) {
                return std::get<0>(quad) == Instruction::CALL;
            });
    return search != end;
}

} // namespace ir

} // namespace credence
