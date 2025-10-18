#include <credence/ir/table.h>

#include <algorithm>         // for __find, find
#include <credence/assert.h> // for credence_runtime_error, CREDENC...
#include <credence/ir/ita.h> // for ITA, make_ITA_instructions
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
 * @brief Construct table and apply a pre-
 * selection pass on a set of ITA instructions
 */
void Table::from_ita_instructions(ITA::Instructions& instructions)
{
    bool skip = false;
    ITA::Instruction last_instruction = ITA::Instruction::NOOP;

    for (instruction_index = 0; instruction_index < instructions.size();
         instruction_index++) {
        auto instruction = instructions.at(instruction_index);
        m::match(std::get<ITA::Instruction>(instruction))(
            m::pattern | ITA::Instruction::FUNC_START =
                [&] { from_func_start_ita_instruction(instructions); },
            m::pattern | ITA::Instruction::FUNC_END =
                [&] { from_func_end_ita_instruction(); },
            m::pattern | ITA::Instruction::PUSH =
                [&] { from_push_instruction(instruction); },
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
}

/**
 * @brief add label and label instruction address entry from LABEL instruction
 */
void Table::from_label_ita_instruction(ITA::Quadruple const& instruction)
{
    Label label = std::get<1>(instruction);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        if (frame->labels.contains(label) != false)
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
        from_temporary_assignment(lhs, rhs);
    } else if (hoisted_symbols_.has_key(std::get<2>(instruction))) {
        from_symbol_reassignment(lhs, std::get<2>(instruction));
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
            frame->allocation -= std::get<2>(symbols_.get_symbol_by_name(lhs));
            symbols_.set_symbol_by_name(lhs, rvalue_symbol);
            frame->allocation += size;
        }
    }
}

/**
 * @brief Set function definition label as current frame stack,
 *  Set instruction address location on the frame
 */
void Table::from_func_start_ita_instruction(
    ITA::Instructions const& instructions)
{
    auto symbolic_label = std::get<1>(instructions.at(instruction_index - 1));
    auto label = Function::get_label_as_human_readable(symbolic_label);
    address_table.set_symbol_by_name(symbolic_label, instruction_index - 1);
    if (labels.contains(label))
        construct_error(
            std::format("function symbol is already defined"), label);

    functions[label] = std::make_shared<Function>(Function{ symbolic_label });
    functions[label]->address_location[0] = instruction_index + 1;

    labels.emplace(label);
    stack_frame = functions[label];
    for (auto const& parameter : stack_frame.value()->parameters)
        symbols_.set_symbol_by_name(
            parameter, { "__WORD__", "word", sizeof(void*) });

    stack_frame.value()->symbol = label;
}

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
 * e.g. `__convert(s,v) = s,v`
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
    CREDENCE_ASSERT(!stack.empty());
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
                            "a pointer ({})",
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
Table::RValue Table::from_temporary(LValue const& lvalue)
{
    auto rvalue = util::contains(lvalue, "_t")
                      ? get_stack_frame()->temporary[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        auto expression = from_rvalue_binary_expression(rvalue);
        auto lhs = from_temporary(std::get<0>(expression));
        auto rhs = from_temporary(std::get<1>(expression));
        return std::string{ lhs + " " + std::get<2>(expression) + " " + rhs };
    } else {
        return rvalue;
    }
}

/**
 * @brief Construct table entry for a temporary lvalue assignment
 */
void Table::from_temporary_assignment(LValue const& lhs, LValue const& rhs)
{
    auto rvalue = get_stack_frame()->temporary.at(rhs);
    if (is_unary(rvalue)) {
        auto unary_type = get_unary(rvalue);
        auto unary_lvalue = get_unary_lvalue(rvalue);
        from_rvalue_unary_expression(lhs, unary_lvalue, unary_type);
    } else
        symbols_.set_symbol_by_name(lhs, { rvalue, "word", sizeof(void*) });
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
        if (symbols_.is_defined(lhs))
            frame->allocation -= std::get<2>(symbols_.get_symbol_by_name(lhs));
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
    if (!symbols_.is_defined(lvalue))
        construct_error(
            "invalid numeric unary expression, lvalue symbol is not "
            "initialized",
            lvalue);

    auto symbol = symbols_.get_symbol_by_name(lvalue);
    if (std::ranges::find(integral_unary, std::get<1>(symbol)) ==
        integral_unary.end())
        construct_error(
            "invalid numeric unary expression on lvalue, lvalue is not a "
            "numeric type",
            lvalue);

    return symbols_.get_symbol_by_name(lvalue);
}

/**
 * @brief Parse ITA::Node ast and symbols to a table instructions pair
 */
Table::ITA_Table Table::from_ast(ITA::Node const& symbols, ITA::Node const& ast)
{
    auto instructions = make_ITA_instructions(symbols, ast);
    auto table = std::make_unique<Table>(Table{ symbols });

    table->from_ita_instructions(instructions);

    return std::make_pair(std::move(table), instructions);
}

/**
 * @brief is ITA unary
 */
constexpr inline bool Table::is_unary(std::string_view rvalue)
{
    return std::ranges::any_of(UNARY_TYPES, [&](std::string_view x) {
        return rvalue.starts_with(x) or rvalue.ends_with(x);
    });
}

/**
 * @brief Get unary operator from ITA rvalue string
 */
constexpr inline std::string_view Table::get_unary(std::string_view rvalue)
{
    auto it = std::ranges::find_if(UNARY_TYPES, [&](std::string_view op) {
        return rvalue.find(op) != std::string_view::npos;
    });
    if (it == UNARY_TYPES.end())
        return "";
    return *it;
}

/**
 * @brief Get unary lvalue from ITA rvalue string
 */
constexpr inline Table::LValue Table::get_unary_lvalue(std::string& lvalue)
{
    std::string_view unary_chracters = "+-*&+~!";
    lvalue.erase(
        std::remove_if(
            lvalue.begin(),
            lvalue.end(),
            [&](char ch) {
                return ::isspace(ch) or
                       unary_chracters.find(ch) != std::string_view::npos;
            }),
        lvalue.end());
    return lvalue;
}

/**
 * @brief Raise runtime construction error with stack frame symbol
 */
inline void Table::construct_error(
    std::string_view message,
    [[maybe_unused]] std::string_view symbol)
{
    credence_runtime_error(
        std::format(
            "{} in function \"{}\"", message, get_stack_frame()->symbol),
        symbol,
        hoisted_symbols_);
}

} // namespace ir

} // namespace credence