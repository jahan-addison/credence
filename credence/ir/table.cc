#include <credence/ir/table.h>

#include <algorithm>         // for __find, find
#include <credence/assert.h> // for credence_runtime_error, CREDENC...
#include <credence/ir/ita.h> // for ITA, make_ITA_instructions
#include <credence/queue.h>  // for rvalue_to_string
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for LITERAL_TYPE
#include <format>            // for format
#include <initializer_list>  // for initializer_list
#include <limits>            // for numeric_limits
#include <matchit.h>         // for pattern, PatternHelper, Pattern...
#include <optional>          // for nullopt, nullopt_t, optional
#include <string>            // for basic_string, char_traits, string
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple

namespace credence {

namespace ir {

namespace m = matchit;

/**
 * @brief Construct table and pre-selection pass on a set of ITA instructions
 */
ITA::Instructions Table::build_from_ita_instructions()
{
    bool skip = false;
    ITA::Instruction last_instruction = ITA::Instruction::NOOP;

    build_symbols_from_vector_definitions();

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
            m::pattern | ITA::Instruction::PUSH =
                [&] { from_push_instruction(instruction); },
            m::pattern | ITA::Instruction::CALL =
                [&] { from_call_ita_instruction(std::get<1>(instruction)); },
            m::pattern | ITA::Instruction::POP =
                [&] { from_pop_instruction(instruction); },
            m::pattern | ITA::Instruction::VARIABLE =
                [&] { from_variable_ita_instruction(instruction); },
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
void Table::build_symbols_from_vector_definitions()
{
    auto keys = hoisted_symbols_.dump_keys();
    for (LValue const& key : keys) {
        auto symbol_type = hoisted_symbols_[key]["type"].to_string();
        if (symbol_type == "vector_definition" or
            symbol_type == "vector_lvalue") {
            if (!symbols_.is_defined(key))
                symbols_.set_symbol_by_name(
                    key, { key, "word", sizeof(void*) });
            auto size = static_cast<std::size_t>(
                hoisted_symbols_[key]["size"].to_int());
            if (!vectors.contains(key))
                vectors[key] = std::make_shared<Vector>(Vector{ size });
        }
    }
}

/**
 * @brief Set vector globals from an ITA constructor
 */
void Table::set_globals(Symbol_Table<>& globals)
{
    for (auto i = globals.begin_t(); i != globals.end_t(); i++) {
        auto symbol = *i;
        vectors[symbol.first] =
            std::make_shared<Vector>(Vector{ symbol.second.size() });
        for (auto const& item : symbol.second) {
            vectors[symbol.first]->data.emplace_back(
                get_rvalue_symbol_type_size(rvalue_to_string(item)));
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
 * @brief Deconstruct assignment instructions into
 * each type and populate function frame stack table
 *
 *  * LValues that begin with `_t` or `_p` are temporaries or parameters
 *  * Ressignments reallocate the frame stack size and update table
 *
 *  * Assign symbols to the table and allocate them as a local on the
 *  * frame stack
 */
void Table::from_variable_ita_instruction(ITA::Quadruple const& instruction)
{
    LValue lhs = std::get<1>(instruction);
    RValue rhs = is_unary(std::get<2>(instruction)) and
                         not std::get<3>(instruction).empty()
                     ? std::get<3>(instruction)
                     : std::get<2>(instruction);

    auto frame = get_stack_frame();

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        frame->temporary[lhs] = std::get<2>(instruction);
    } else if (rhs.starts_with("_t") and is_stack_frame()) {
        from_temporary_reassignment(lhs, rhs);
    } else if (hoisted_symbols_.has_key(std::get<2>(instruction))) {
        from_symbol_reassignment(lhs, std::get<2>(instruction));
    } else {
        if (util::contains(lhs, "[") or
            util::contains(std::get<2>(instruction), "[")) {
            from_pointer_assignment(lhs, rhs);
        } else {
            RValue_Data_Type rvalue_symbol =
                is_unary(std::get<2>(instruction))
                    ? from_rvalue_unary_expression(
                          lhs, rhs, std::get<2>(instruction))
                    : get_rvalue_symbol_type_size(rhs);

            Size size = std::get<2>(rvalue_symbol);
            if (size > std::numeric_limits<unsigned int>::max())
                construct_error(
                    std::format("exceeds maximum byte size ({})", rhs), lhs);
            if (!frame->locals.contains(lhs)) {
                frame->allocation += size;
                frame->locals.emplace(lhs);
                symbols_.set_symbol_by_name(lhs, rvalue_symbol);
            } else {
                //  resize stack frame allocation on reassignment
                frame->allocation -=
                    std::get<2>(symbols_.get_symbol_by_name(lhs));
                symbols_.set_symbol_by_name(lhs, rvalue_symbol);
                frame->allocation += size;
            }
        }
    }
}

/**
 * @brief Out-of-range boundary check on left-
 * hand-side and right-hand-side of assignment
 */
void Table::from_pointer_assignment(LValue const& lvalue, RValue const& rvalue)
{
    if (util::contains(lvalue, "["))
        from_boundary_out_of_range(lvalue);
    if (util::contains(rvalue, "["))
        from_boundary_out_of_range(rvalue);
}

/**
 * @brief Check the boundary of a vector or pointer offset by its allocation
 * size
 */
void Table::from_boundary_out_of_range(RValue const& rvalue)
{
    CREDENCE_ASSERT(util::contains(rvalue, "["));
    CREDENCE_ASSERT(util::contains(rvalue, "]"));
    auto lvalue = from_lvalue_offset(rvalue);
    auto offset = from_pointer_offset(rvalue);
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
        if (ul_offset > Vector::max_size)
            construct_error(
                std::format(
                    "invalid rvalue, integer offset \"{}\" is buffer-overflow",
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
                    "invalid out-of-range vector assignment \"{}\" at index "
                    "\"{}\"",
                    lvalue,
                    ul_offset),
                rvalue);

    } else {
        auto stack_frame = get_stack_frame();
        if (!symbols_.is_defined(offset) and
            not stack_frame->is_parameter(offset))
            construct_error(
                std::format("invalid vector offset \"{}\"", offset), rvalue);
    }
}

/**
 * @brief Set function definition label as current frame stack,
 *  Set instruction address location on the frame
 */
void Table::from_func_start_ita_instruction(Label const& label)
{
    Label human_label = Function::get_label_as_human_readable(label);
    address_table.set_symbol_by_name(label, instruction_index - 1);
    if (labels.contains(human_label))
        construct_error(
            std::format("function symbol is already defined"), human_label);

    functions[human_label] =
        std::make_shared<Function>(Function{ human_label });
    functions[human_label]->address_location[0] = instruction_index + 1;
    functions[human_label]->set_parameters_from_symbolic_label(label);
    labels.emplace(human_label);
    stack_frame = functions[human_label];
    for (auto const& parameter : stack_frame.value()->parameters) {
        symbols_.set_symbol_by_name(
            parameter, { "__WORD__", "word", sizeof(void*) });
        functions[human_label]->locals.emplace(parameter);
    }
}

/**
 * @brief End of function, reset stack frame and clear local symbols
 */
void Table::from_func_end_ita_instruction()
{
    if (is_stack_frame()) {
        get_stack_frame()->address_location[1] = instruction_index - 1;
        for (auto const& parameter : stack_frame.value()->parameters)
            symbols_.remove_symbol_by_name(parameter);
    }
    stack_frame = std::nullopt;
}

/**
 * @brief Parse ITA function parameters into locals on the frame stack
 *
 * e.g. `__convert(s,v) = (s,v)`
 */
void Table::Function::set_parameters_from_symbolic_label(Label const& label)
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
 * @brief Parse RValue::Value_Type into a 3-tuple of value, type, and size
 *
 * e.g. (10:int:4) -> 10, "int", 4UL
 */
Table::RValue_Data_Type Table::get_rvalue_symbol_type_size(RValue const& rvalue)
{
    CREDENCE_ASSERT(util::substring_count_of(rvalue, ":") == 2);

    size_t search = rvalue.find_last_of(":");
    auto bytes = std::string{ rvalue.begin() + search + 1, rvalue.end() - 1 };
    auto type_search = rvalue.substr(0, search - 1).find_last_of(":") + 1;
    auto type =
        std::string{ rvalue.begin() + type_search, rvalue.begin() + search };

    auto value = std::ranges::find(rvalue, '"') == rvalue.end()
                     ? rvalue.substr(1, type_search - 2)
                     : rvalue.substr(2, type_search - 4);

    return { value, type, std::stoul(bytes) };
}

/**
 * @brief Parse the unary rvalue types into their operator and lvalue
 */
Table::RValue_Data_Type Table::from_rvalue_unary_expression(
    LValue const& lvalue,
    RValue& rvalue,
    std::string_view unary_operator)
{

    if (unary_operator.find("*") != std::string_view::npos) {
        rvalue = unary_operator.substr(1);
        unary_operator = "*";
    }

    return m::match(unary_operator)(
        // cppcheck-suppress syntaxError
        m::pattern | "*" =
            [&] {
                if (!symbols_.is_pointer(rvalue))
                    construct_error(
                        std::format(
                            "indirection on invalid lvalue, "
                            "right-hand-side is "
                            "not "
                            "a pointer \"{}\"",
                            rvalue),
                        lvalue);
                LValue indirect_lvalue = symbols_.get_pointer_by_name(rvalue);
                if (!symbols_.is_defined(indirect_lvalue))
                    construct_error("invalid indirection assignment", lvalue);
                return symbols_.get_symbol_by_name(indirect_lvalue);
            },
        m::pattern | "&" =
            [&] {
                if (!symbols_.is_defined(rvalue))
                    construct_error(
                        std::format(
                            "invalid pointer assignment, right-hand-side "
                            "is "
                            "not "
                            "initialized ({})",
                            rvalue),
                        lvalue);
                symbols_.set_symbol_by_name(lvalue, rvalue);
                return RValue_Data_Type{ rvalue, "word", sizeof(void*) };
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

        m::pattern |
            m::_ = [&] { return symbols_.get_symbol_by_name(lvalue); });
}

/**
 * @brief Parse ITA binary expression into
 * its operator and operands in the table
 */
Table::Binary_Expression Table::from_rvalue_binary_expression(
    RValue const& rvalue)
{
    auto lhs = rvalue.find_first_of(" ");
    auto rhs = rvalue.find_last_of(" ");
    auto lhs_lvalue = std::string{ rvalue.begin(), rvalue.begin() + lhs };
    auto rhs_lvalue = std::string{ rvalue.begin() + 1 + rhs, rvalue.end() };
    auto binary_operator =
        std::string{ rvalue.begin() + lhs + 1, rvalue.begin() + rhs };

    return { lhs_lvalue, rhs_lvalue, binary_operator };
}

/**
 * @brief Recursively resolve and return the
 * rvalue of a temporary lvalue in the table
 */
Table::RValue Table::from_temporary_lvalue(LValue const& lvalue)
{
    auto rvalue = util::contains(lvalue, "_t")
                      ? get_stack_frame()->temporary[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        auto expression = from_rvalue_binary_expression(rvalue);
        auto lhs = from_temporary_lvalue(std::get<0>(expression));
        auto rhs = from_temporary_lvalue(std::get<1>(expression));
        return std::string{ lhs + " " + std::get<2>(expression) + " " + rhs };
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
    if (is_unary(rhs)) {
        auto unary_type = get_unary(rhs);
        auto unary_lvalue = get_unary_rvalue_reference(rhs);
        auto unary_rvalue =
            from_rvalue_unary_expression(rhs, unary_lvalue, unary_type);
        symbols_.set_symbol_by_name(
            lhs, { rhs, std::get<1>(unary_rvalue), std::get<2>(unary_rvalue) });
    } else {
        symbols_.set_symbol_by_name(lhs, { rhs, "word", sizeof(void*) });
    }
}

/**
 * @brief Reallocate and store updated local
 * for a symbol reassignment in the stack frame
 */
void Table::from_symbol_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = get_stack_frame();
    if (!symbols_.is_defined(rhs))
        construct_error(
            "invalid lvalue assignment, right-hand-side is not initialized",
            rhs);
    if (symbols_.is_pointer(rhs)) {
        symbols_.set_symbol_by_name(lhs, symbols_.get_pointer_by_name(rhs));
        frame->allocation += type::LITERAL_TYPE.at("word").second;
    } else {
        auto symbol = symbols_.get_symbol_by_name(rhs);
        if (symbols_.is_defined(lhs)) {
            if (symbols_.is_pointer(lhs))
                frame->allocation -= std::get<2>(symbols_.get_symbol_by_name(
                    symbols_.get_pointer_by_name(lhs)));
            else

                frame->allocation -=
                    std::get<2>(symbols_.get_symbol_by_name(lhs));
        }
        symbols_.set_symbol_by_name(lhs, symbol);
        frame->allocation += std::get<2>(symbol);
    }
}

/**
 * @brief Parse numeric ITA unary expressions
 */
Table::RValue_Data_Type Table::from_integral_unary_expression(
    RValue const& lvalue)
{
    const std::initializer_list<std::string_view> integral_unary = {
        "int", "double", "float", "long"
    };
    auto rvalue = get_unary_rvalue_reference(lvalue);
    if (!symbols_.is_defined(rvalue) and
        not get_stack_frame()->temporary.contains(rvalue))
        construct_error(
            std::format(
                "invalid numeric unary expression, lvalue symbol \"{}\" is not "
                "initialized",
                rvalue),
            lvalue);

    auto symbol = get_stack_frame()->temporary.contains(rvalue)
                      ? lvalue
                      : std::get<1>(symbols_.get_symbol_by_name(rvalue));
    if (std::ranges::find(integral_unary, symbol) == integral_unary.end() and
        not get_stack_frame()->temporary.contains(rvalue))
        construct_error(
            std::format(
                "invalid numeric unary expression on lvalue, lvalue \"{}\" is "
                "not a "
                "numeric type",
                symbol),
            lvalue);

    return symbols_.get_symbol_by_name(rvalue);
}

/**
 * @brief Parse ITA::Node ast and symbols to a table instructions pair
 */
Table::ITA_Table Table::build_from_ast(
    ITA::Node const& symbols,
    ITA::Node const& ast)
{
    auto instructions = make_ITA_instructions(symbols, ast);
    return std::make_unique<Table>(Table{ symbols, instructions });
}

/**
 * @brief Raise runtime construction error with stack frame symbol
 */
inline void Table::construct_error(
    std::string_view message,
    [[maybe_unused]] RValue_Reference symbol)
{
#ifdef CREDENCE_TEST
    throw std::runtime_error(message.data());
#else
    credence_runtime_error(
        std::format(
            "{} in function \"{}\"", message, get_stack_frame()->symbol),
        symbol,
        hoisted_symbols_);
#endif
}

} // namespace ir

} // namespace credence