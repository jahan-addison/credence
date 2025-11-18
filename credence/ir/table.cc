#include <credence/ir/table.h>

#include <algorithm>         // for __find_if, find_if
#include <credence/assert.h> // for CREDENCE_ASSERT, credence_compile_error
#include <credence/ir/ita.h> // for ITA, make_ITA_instructions
#include <credence/map.h>    // for Ordered_Map
#include <credence/queue.h>  // for value_type_pointer_to_string
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for get_type_from_rvalue_data_type, get_u...
#include <credence/util.h>   // for contains, AST_Node, is_numeric, subst...
#include <cstddef>           // for size_t
#include <deque>             // for __deque_iterator, operator==
#include <format>            // for format
#include <limits>            // for numeric_limits
#include <matchit.h>         // for pattern, PatternHelper, PatternPipable
#include <optional>          // for optional
#include <string>            // for basic_string, char_traits, operator==
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple, operator==
#include <variant>           // for visit
#include <vector>            // for vector

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Construct table and type-checking pass on a set of ITA instructions
 */
ITA::Instructions Table::build_from_ita_instructions()
{
    bool skip = false;
    ITA::Instruction last_instruction = ITA::Instruction::NOOP;

    build_symbols_from_vector_lvalues();

    for (instruction_index = 0; instruction_index < instructions.size();
         instruction_index++) {
        auto instruction = instructions.at(instruction_index);
        m::match(std::get<ITA::Instruction>(instruction))(
            m::pattern | ITA::Instruction::FUNC_START =
                [&] {
                    from_func_start_ita_instruction(
                        std::get<1>(instructions.at(instruction_index - 1)));
                },
            m::pattern | ITA::Instruction::FUNC_END =
                [&] { from_func_end_ita_instruction(); },
            m::pattern | ITA::Instruction::GLOBL =
                [&] { from_globl_ita_instruction(std::get<1>(instruction)); },
            m::pattern | ITA::Instruction::LOCL =
                [&] { from_locl_ita_instruction(instruction); },
            m::pattern | ITA::Instruction::PUSH =
                [&] { from_push_instruction(instruction); },
            m::pattern | ITA::Instruction::CALL =
                [&] { from_call_ita_instruction(std::get<1>(instruction)); },
            m::pattern | ITA::Instruction::POP =
                [&] { from_pop_instruction(instruction); },
            m::pattern | ITA::Instruction::MOV =
                [&] { from_mov_ita_instruction(instruction); },
            m::pattern | ITA::Instruction::LABEL =
                [&] { from_label_ita_instruction(instruction); },
            m::pattern | ITA::Instruction::GOTO =
                [&] {
                    if (last_instruction == ITA::Instruction::GOTO)
                        skip = true;
                });
        if (skip) {
            instructions.erase(instructions.begin() + instruction_index);
            skip = false;
        }
        last_instruction = std::get<ITA::Instruction>(instruction);
    }
    return instructions;
}

/**
 * @brief Construct table of vector symbols and size allocation
 */
void Table::build_symbols_from_vector_lvalues()
{
    auto keys = hoisted_symbols_.dump_keys();
    for (LValue const& key : keys) {
        auto symbol_type = hoisted_symbols_[key]["type"].to_string();
        if (symbol_type == "vector_lvalue") {
            auto size = static_cast<std::size_t>(
                hoisted_symbols_[key]["size"].to_int());
            if (!vectors.contains(key))
                vectors[key] =
                    std::make_shared<detail::Vector>(detail::Vector{ size });
        }
    }
}

/**
 * @brief Construct table entry from locl (auto) instruction
 */
void Table::from_locl_ita_instruction(ITA::Quadruple const& instruction)
{
    auto label = std::get<1>(instruction);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        if (std::get<1>(instruction).starts_with("*")) {
            label = std::get<1>(instruction);
            get_stack_frame()->locals.set_symbol_by_name(
                label.substr(1), "NULL");
        } else
            get_stack_frame()->locals.set_symbol_by_name(
                label, type::NULL_RVALUE_LITERAL);
    }
}

/**
 * @brief Construct table entry from globl (extrn) instruction
 */
void Table::from_globl_ita_instruction(Label const& label)
{
    if (!vectors.contains(label))
        construct_error("extrn statement failed, symbol does not exist", label);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        get_stack_frame()->locals.set_symbol_by_name(
            label, type::NULL_RVALUE_LITERAL);
    }
}

/**
 * @brief Set vector globals from an ITA constructor
 */
void Table::build_vector_definitions_from_globals(Symbol_Table<>& globals)
{
    for (auto i = globals.begin_t(); i != globals.end_t(); i++) {
        std::size_t index = 0;
        auto symbol = *i;
        vectors[symbol.first] = std::make_shared<detail::Vector>(
            detail::Vector{ symbol.second.size() });
        for (auto const& item : symbol.second) {
            auto key = std::to_string(index++);
            vectors[symbol.first]->data[key] =
                type::get_rvalue_datatype_from_string(
                    internal::value::expression_type_to_string(item));
        }
    }
}

/**
 * @brief Ensure CALL right-hand-side is a valid symbol
 */
void Table::from_call_ita_instruction(Label const& label)
{
    if (!labels.contains(label) and not hoisted_symbols_.has_key(label))
        construct_error(
            std::format(
                "function call failed, \"{}\" identifier is not a function",
                label));
}

/**
 * @brief add label and label instruction address entry from LABEL instruction
 */
void Table::from_label_ita_instruction(ITA::Quadruple const& instruction)
{
    Label label = std::get<1>(instruction);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        if (frame->labels.contains(label))
            construct_error(
                std::format(
                    "symbol of symbolic label is already "
                    "defined"),
                label);
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
void Table::from_mov_ita_instruction(ITA::Quadruple const& instruction)
{
    LValue lhs = std::get<1>(instruction);
    auto rvalue = get_rvalue_from_mov_qaudruple(instruction);
    RValue rhs = rvalue.first;
    auto frame = get_stack_frame();

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        frame->temporary[lhs] = std::get<2>(instruction);
        return;
    }
    if (rhs.starts_with("_t") and is_stack_frame()) {
        from_temporary_reassignment(lhs, rhs);
        return;
    }
    if (is_vector_or_pointer(lhs) or is_vector_or_pointer(rhs) or
        rvalue.second == "&") {
        from_pointer_or_vector_assignment(lhs, rhs);
        return;
    }
    if (hoisted_symbols_.has_key(rhs)) {
        from_scaler_symbol_assignment(lhs, rhs);
        return;
    }

    type::Data_Type rvalue_symbol = type::NULL_RVALUE_LITERAL;

    if (type::is_unary_operator(rhs))
        rvalue_symbol = from_rvalue_unary_expression(
            lhs, rhs, type::get_unary_operator(rhs));

    if (type::is_binary_expression(rhs))
        rvalue_symbol = type::Data_Type{ rhs, "word", sizeof(void*) };

    if (rvalue_symbol == type::NULL_RVALUE_LITERAL and rvalue.second.empty())
        rvalue_symbol = type::get_rvalue_datatype_from_string(rhs);

    if (rvalue_symbol == type::NULL_RVALUE_LITERAL and
        type::is_unary_operator(rvalue.second))
        rvalue_symbol = from_rvalue_unary_expression(lhs, rhs, rvalue.second);

    if (rvalue_symbol == type::NULL_RVALUE_LITERAL)
        construct_error(
            std::format("invalid lvalue assignment on '{}'", lhs), rhs);

    type::semantic::Size size = std::get<2>(rvalue_symbol);

    if (size > std::numeric_limits<unsigned int>::max())
        construct_error(
            std::format("right-hand-side exceeds maximum byte size '{}'", rhs),
            lhs);
    if (!frame->locals.is_defined(lhs)) {
        frame->allocation += size;
        frame->locals.set_symbol_by_name(lhs, rvalue_symbol);
    } else {
        frame->locals.set_symbol_by_name(lhs, rvalue_symbol);
        frame->allocation += size;
    }
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
void Table::from_pointer_or_vector_assignment(
    LValue const& lvalue,
    RValue& rvalue,
    bool indirection)
{
    auto& locals = get_stack_frame_symbols();
    auto frame = get_stack_frame();
    std::string rhs_lvalue{};
    std::optional<type::Data_Type> rvalue_symbol;
    // This is an lvalue indirection assignment, `m = *k;` or `*k = m`;
    if (indirection) {
        rvalue = type::get_unary_rvalue_reference(rvalue, "&");
    }
    // if the right-hand-side is a vector, save the lvalue and the data type
    if (util::contains(rvalue, "[")) {
        rhs_lvalue = type::from_lvalue_offset(rvalue);
        auto safe_rvalue = type::get_unary_rvalue_reference(rhs_lvalue);
        auto offset = type::from_pointer_offset(rvalue);
        is_boundary_out_of_range(type::get_unary_rvalue_reference(rvalue));
        if (!frame->is_parameter(offset) and
            (!vectors.contains(safe_rvalue) or
             not vectors[safe_rvalue]->data.contains(offset)))
            construct_error(
                std::format(
                    "invalid vector assignment, rvalue vector value at "
                    "\"{}\" "
                    "does not exist",
                    offset),
                rvalue);
        rvalue_symbol = vectors[safe_rvalue]->data[offset];
    }

    // if the left-hand-side is a normal vector:
    if (util::contains(lvalue, "[")) {
        is_boundary_out_of_range(lvalue);
        auto lhs_lvalue = type::from_lvalue_offset(lvalue);
        auto offset = type::from_pointer_offset(lvalue);
        // the rhs is a vector too, check accessed types
        if (rvalue_symbol.has_value()) {
            if (!lhs_rhs_type_is_equal(lhs_lvalue, rvalue_symbol.value()))
                construct_error(
                    std::format(
                        "invalid lvalue assignment, right-hand-side \"{}\" "
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
            if (util::substring_count_of(rvalue, ":") > 1) {
                // update the lhs vector, if applicable
                if (vectors.contains(lhs_lvalue))
                    vectors[lhs_lvalue]->data[offset] =
                        type::get_rvalue_datatype_from_string(rvalue);
            }
        }
    } else {
        // is the rhs an address-of rvalue?
        if (rvalue.starts_with("&")) {
            // the left-hand-side must be a pointer
            if (!indirection and not locals.is_pointer(lvalue))
                construct_error(
                    std::format(
                        "invalid pointer assignment, lvalue \"{}\" is not "
                        "a "
                        "pointer",
                        lvalue),
                    rvalue);
            locals.set_symbol_by_name(
                lvalue, type::get_unary_rvalue_reference(rvalue));
        } else {
            if (!rvalue_symbol.has_value()) {
                safely_reassign_pointers_or_vectors(
                    lvalue, rvalue, indirection);
            } else {
                safely_reassign_pointers_or_vectors(
                    lvalue, rvalue_symbol.value(), indirection);
            }
        }
    }
}

/**
 * @brief Trivial lvalue vector assignment
 *   Vector:
 *    " unit 10; "
 *   Assignment:
 *    "unit = 5;"
 */
void Table::from_trivial_vector_assignment(
    LValue const& lhs,
    type::Data_Type const& rvalue)
{
    if (vectors.contains(lhs)) {
        vectors[lhs]->data["0"] = rvalue;
    }
}

/**
 * @brief Reassigns pointers and vectors, or scalers and dereferenced
 * pointers
 *
 */
void Table::safely_reassign_pointers_or_vectors(
    LValue const& lvalue,
    type::RValue_Reference_Type const& rvalue,
    bool indirection)
{
    auto& locals = get_stack_frame_symbols();
    std::visit(
        util::overload{
            [&](RValue const& value) {
                // the right-hand-side must be a pointer, vector, or
                // indirect lvalue
                if (is_trivial_vector_assignment(lvalue, value)) {
                    // special case for trivial global vectors
                    if (vectors.contains(lvalue) and
                        not vectors.contains(value)) {
                        auto vector_rvalue = vectors[lvalue]->data.at("0");
                        if (get_type_from_rvalue_data_type(lvalue) != "null" and
                            !lhs_rhs_type_is_equal(value, vector_rvalue))
                            construct_error(
                                std::format(
                                    "invalid lvalue assignment, "
                                    "left-hand-side "
                                    "\"{}\" "
                                    "with "
                                    "type \"{}\" is not the same type ({})",
                                    value,
                                    type::get_type_from_rvalue_data_type(
                                        vector_rvalue),
                                    get_type_from_rvalue_data_type(value)),
                                lvalue);
                        from_trivial_vector_assignment(lvalue, vector_rvalue);
                    } else if (
                        vectors.contains(lvalue) and vectors.contains(value)) {
                        vectors[lvalue]->data["0"] = vectors[value]->data["0"];
                    }
                    return;
                }
                if (!locals.is_pointer(lvalue) or
                    not locals.is_pointer(value)) {
                    // do not allow dereferencing of null pointers
                    if (indirection) {
                        construct_error(
                            "invalid pointer assignment, right-hand-side "
                            "is a "
                            "dereferenced null pointer",
                            lvalue);
                    } else {
                        // the lvalue and rvalue must both be pointers
                        construct_error(
                            std::format(
                                "invalid pointer assignment, "
                                "left-hand-side "
                                "\"{}\" "
                                "and right-hand-side must both be pointers",
                                lvalue),
                            value);
                        locals.set_symbol_by_name(lvalue, value);
                    }
                }
            },
            [&](type::Data_Type const& value) {
                if (!indirection and locals.is_pointer(lvalue))
                    construct_error(
                        "invalid lvalue assignment, left-hand-side is a "
                        "pointer to non-pointer rvalue",
                        lvalue);
                // the lvalue and rvalue vector data entry type must match
                if (get_type_from_rvalue_data_type(lvalue) != "null" and
                    !lhs_rhs_type_is_equal(lvalue, value))
                    construct_error(
                        std::format(
                            "invalid lvalue assignment, left-hand-side "
                            "\"{}\" "
                            "with "
                            "type \"{}\" is not the same type ({})",
                            lvalue,
                            get_type_from_rvalue_data_type(lvalue),
                            type::get_type_from_rvalue_data_type(value)),
                        lvalue);
                locals.set_symbol_by_name(lvalue, value);
            } },
        rvalue);
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
        auto offset = type::from_pointer_offset(lvalue);
        return std::get<1>(vectors[lhs_lvalue]->data[offset]);
    }
    return std::get<1>(locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Check the boundary of a vector or pointer offset by its allocation
 * size
 */
void Table::is_boundary_out_of_range(RValue const& rvalue)
{
    CREDENCE_ASSERT(util::contains(rvalue, "["));
    CREDENCE_ASSERT(util::contains(rvalue, "]"));
    auto lvalue = type::from_lvalue_offset(rvalue);
    auto offset = type::from_pointer_offset(rvalue);
    if (!vectors.contains(lvalue))
        construct_error(
            std::format(
                "invalid vector assignment, vector lvalue \"{}\" does not "
                "exist",
                lvalue),
            rvalue);
    if (util::is_numeric(offset)) {
        auto global_symbol = hoisted_symbols_[lvalue];
        auto ul_offset = std::stoul(offset);
        if (ul_offset > detail::Vector::max_size)
            construct_error(
                std::format(
                    "invalid rvalue, integer offset \"{}\" is "
                    "buffer-overflow",
                    ul_offset),
                rvalue);
        if (!vectors.contains(lvalue))
            construct_error(
                std::format(
                    "invalid vector assignment, right-hand-side does not "
                    "exist \"{}\"",
                    lvalue),
                rvalue);
        if (ul_offset > vectors[lvalue]->size - 1)
            construct_error(
                std::format(
                    "invalid out-of-range vector assignment \"{}\" at "
                    "index "
                    "\"{}\"",
                    lvalue,
                    ul_offset),
                rvalue);

    } else {
        auto stack_frame = get_stack_frame();
        auto& locals = get_stack_frame_symbols();
        if (!locals.is_defined(offset) and
            not stack_frame->is_parameter(offset))
            construct_error(
                std::format("invalid vector offset \"{}\"", offset), rvalue);
    }
}

/**
 * @brief Set the stack frame
 */
void Table::set_stack_frame(Label const& label)
{
    CREDENCE_ASSERT(functions.contains(label));
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
        construct_error(
            std::format("function symbol is already defined"), human_label);

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
 * e.g. `__convert(s,v) = (s,v)`
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
void Table::from_push_instruction(ITA::Quadruple const& instruction)
{
    RValue operand = std::get<1>(instruction);
    if (is_stack_frame())
        stack.emplace_back(operand);
}

/**
 * @brief Pop off the top of the symbolic frame stack based on operand size
 */
void Table::from_pop_instruction(ITA::Quadruple const& instruction)
{
    auto operand = std::stoul(std::get<1>(instruction));
    auto pop_size = operand / sizeof(void*);
    CREDENCE_ASSERT(pop_size <= stack.size());
    for (std::size_t i = pop_size; i > 0; i--) {
        if (is_stack_frame())
            stack.pop_back();
    }
}

/**
 * @brief Parse the unary rvalue types into their operator and lvalue
 */
type::Data_Type Table::from_rvalue_unary_expression(
    LValue const& lvalue,
    RValue const& rvalue,
    std::string_view unary_operator)
{
    auto& locals = get_stack_frame_symbols();

    return m::match(unary_operator)(
        // cppcheck-suppress syntaxError
        m::pattern | "*" =
            [&] {
                auto rhs_lvalue = rvalue.substr(1);
                if (locals.is_pointer(lvalue))
                    construct_error(
                        std::format(
                            "indirection on invalid lvalue, "
                            "left-hand-side is a pointer",
                            lvalue),
                        rvalue);
                LValue indirection = locals.get_pointer_by_name(rhs_lvalue);
                from_pointer_or_vector_assignment(lvalue, indirection, true);
                return locals.get_symbol_by_name(lvalue);
            },
        m::pattern | "&" =
            [&] {
                if (!locals.is_defined(rvalue))
                    construct_error(
                        std::format(
                            "invalid pointer assignment, right-hand-side "
                            "is "
                            "not "
                            "initialized ({})",
                            rvalue),
                        lvalue);
                locals.set_symbol_by_name(lvalue, rvalue);
                return type::Data_Type{ rvalue, "word", sizeof(void*) };
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
 * @brief Recursively resolve and return the rvalue of a temporary lvalue in
 * the table
 */
Table::RValue Table::from_temporary_lvalue(LValue const& lvalue)
{
    auto rvalue = util::contains(lvalue, "_t")
                      ? get_stack_frame()->temporary[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        return rvalue;
    } else {
        if (util::contains(rvalue, "_t"))
            return from_temporary_lvalue(rvalue);
        else
            return rvalue;
    }
}

/**
 * @brief Construct table entry for a temporary lvalue assignment
 */
void Table::from_temporary_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto& locals = get_stack_frame_symbols();
    auto rvalue = from_temporary_lvalue(rhs);
    if (is_vector_or_pointer(rvalue))
        from_pointer_or_vector_assignment(lhs, rvalue);
    else if (!rvalue.empty())
        locals.set_symbol_by_name(lhs, { rvalue, "word", sizeof(void*) });
    else
        locals.set_symbol_by_name(lhs, { rhs, "word", sizeof(void*) });
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
        construct_error(
            std::format(
                "invalid lvalue assignment \"{}\", left-hand-side is not "
                "initialized",
                lhs),
            rhs);
    if (!locals.is_defined(rhs))
        construct_error(
            std::format(
                "invalid lvalue assignment \"{}\", right-hand-side is not "
                "initialized",
                rhs),
            lhs);

    from_type_invalid_assignment(lhs, rhs);
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
        construct_error(
            std::format(
                "invalid numeric unary expression, lvalue symbol \"{}\" is "
                "not "
                "initialized",
                rvalue),
            lvalue);

    if (frame->temporary.contains(lvalue)) {
        auto temp_rvalue =
            type::get_unary_rvalue_reference(frame->temporary[lvalue]);
        CREDENCE_ASSERT(type::is_rvalue_data_type(temp_rvalue));
        auto rdt = type::get_rvalue_datatype_from_string(temp_rvalue);
        assert_integral_unary_expression(
            lvalue, frame->temporary[lvalue], std::get<1>(rdt));
        return rdt;
    } else {
        auto symbol = locals.get_symbol_by_name(rvalue);
        auto type = std::get<1>(symbol);
        if (type == "word") {
            type::Data_Type local_rvalue{};
            auto rhs = type::get_unary_rvalue_reference(std::get<0>(symbol));
            local_rvalue = locals.is_defined(rhs)
                               ? locals.get_symbol_by_name(rhs)
                               : type::get_rvalue_datatype_from_string(rhs);

            auto local_rvalue_reference =
                type::get_value_from_rvalue_data_type(local_rvalue);
            if (type::is_unary_operator(local_rvalue_reference)) {
                auto lvalue_symbol = locals.get_symbol_by_name(lvalue);
                auto lvalue_type =
                    type::get_type_from_rvalue_data_type(lvalue_symbol);
                if (lvalue_type != "word")
                    assert_integral_unary_expression(
                        lvalue,
                        local_rvalue_reference,
                        std::get<1>(lvalue_symbol));
                return local_rvalue;
            }
            assert_integral_unary_expression(
                lvalue,
                std::get<0>(locals.get_symbol_by_name(rvalue)),
                std::get<1>(local_rvalue));
            return local_rvalue;
        } else {
            assert_integral_unary_expression(lvalue, rvalue, type);
        }
        return locals.get_symbol_by_name(rvalue);
    }
}

/**
 * @brief Search the ita instructions set in
 * a stack frame for a specific instruction
 */
bool Table::stack_frame_contains_ita_instruction(
    Label name,
    ITA::Instruction inst)
{
    CREDENCE_ASSERT(functions.contains(name));
    auto frame = functions.at(name);
    auto search = std::ranges::find_if(
        instructions.begin() + frame->address_location[0],
        instructions.begin() + frame->address_location[1],
        [&](ir::ITA::Quadruple const& quad) {
            return std::get<0>(quad) == inst;
        });
    return search != instructions.begin() + frame->address_location[1];
}

/**
 * @brief Parse ITA::Node ast and symbols to a table instructions pair
 */
Table::Table_PTR Table::build_from_ast(
    ITA::Node const& symbols,
    ITA::Node const& ast)
{
    auto instructions = make_ITA_instructions(symbols, ast);
    return std::make_unique<Table>(Table{ symbols, instructions });
}

/**
 * @brief Type checking on lvalue and rvalue
 */
inline void Table::from_type_invalid_assignment(
    LValue const& lvalue,
    RValue const& rvalue)
{
    auto& locals = get_stack_frame_symbols();
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (locals.is_pointer(lvalue) and locals.is_pointer(rvalue))
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        construct_error(
            std::format(
                "invalid lvalue assignment, right-hand-side \"{}\" "
                "with type {} is not the same type ({})",
                rvalue,
                get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Type checking on lvalue and value data type
 */
inline void Table::from_type_invalid_assignment(
    LValue const& lvalue,
    type::Data_Type const& rvalue)
{
    if (get_type_from_rvalue_data_type(lvalue) == "null")
        return;
    if (!lhs_rhs_type_is_equal(lvalue, rvalue))
        construct_error(
            std::format(
                "invalid lvalue assignment, right-hand-side \"{}\" "
                "with type {} is not the same type ({})",
                std::get<0>(rvalue),
                type::get_type_from_rvalue_data_type(rvalue),
                get_type_from_rvalue_data_type(lvalue)),
            lvalue);
}

/**
 * @brief Raise error with stack frame symbol
 */
inline void Table::construct_error(
    std::string_view message,
    type::RValue_Reference symbol)
{
#ifdef CREDENCE_TEST
    throw std::runtime_error(
        std::format("{} from \"{}\"", message.data(), symbol));
#else
    credence_compile_error(
        std::format(
            "{} in function \"{}\"", message, get_stack_frame()->symbol),
        symbol,
        hoisted_symbols_);
#endif
}

/**
 * @brief Emit ita instructions type and vector boundary checking an
 * and to an std::ostream Passes the global symbols from the ITA object
 */
void emit(
    std::ostream& os,
    util::AST_Node const& symbols,
    util::AST_Node const& ast)
{
    using namespace credence::ir;
    auto ita = ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table = Table{ symbols, instructions };
    table.build_vector_definitions_from_globals(ita.globals_);
    table.build_from_ita_instructions();
    ITA::emit(os, table.instructions);
}

} // namespace ir

} // namespace credence