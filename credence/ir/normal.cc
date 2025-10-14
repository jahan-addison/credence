#include <credence/ir/normal.h>

#include <algorithm>         // for __find, find
#include <credence/assert.h> // for credence_runtime_error, CREDENC...
#include <credence/ir/ita.h> // for ITA, make_ITA_instructions
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for AST_Node
#include <format>            // for format
#include <initializer_list>  // for initializer_list
#include <iostream>          // for cout
#include <limits>            // for numeric_limits
#include <matchit.h>         // for pattern, PatternHelper, Pattern...
#include <optional>          // for nullopt, nullopt_t, optional
#include <ostream>           // for basic_ostream, operator<<
#include <string>            // for basic_string, char_traits, string
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get, tuple
#include <variant>           // for variant

namespace credence {

namespace ir {

namespace m = matchit;

ITA::Instructions Normalization::from_ita_instructions()
{
    using std::get;
    bool skip = false;
    ITA::Instructions normalized{};
    ITA::Instruction last_instruction = ITA::Instruction::NOOP;

    for (instruction_index = 0; instruction_index < instructions_.size();
         instruction_index++) {
        auto instruction = instructions_.at(instruction_index);
        m::match(get<ITA::Instruction>(instruction))(
            m::pattern | ITA::Instruction::FUNC_START =
                [&] { from_func_start_ita_instruction(); },
            m::pattern | ITA::Instruction::FUNC_END =
                [&] { from_func_end_ita_instruction(instruction); },
            m::pattern | ITA::Instruction::VARIABLE =
                [&] { from_variable_ita_instruction(instruction); },

            m::pattern | ITA::Instruction::LABEL =
                [&] { from_label_ita_instruction(instruction); },
            m::pattern | ITA::Instruction::GOTO =
                [&] {
                    if (last_instruction == ITA::Instruction::GOTO) {
                        skip = true;
                    }
                });
        if (!skip or get<0>(instruction) != ITA::Instruction::GOTO) {
            normalized.emplace_back(instruction);
            if (stack_frame.has_value())
                stack_frame.value()->get()->instructions.emplace_back(
                    instruction);
        } else {
            skip = false;
        }
        last_instruction = std::get<ITA::Instruction>(instruction);
    }

    return normalized;
}

void Normalization::from_label_ita_instruction(
    ITA::Quadruple const& instruction)
{
    auto label = get<1>(instruction);
    if (stack_frame.has_value()) {
        auto frame = stack_frame.value()->get();
        if (frame->labels.contains(label) != false)
            credence_runtime_error(
                std::format(
                    "symbol of symbolic label is already "
                    "defined"),
                label,
                hoisted_symbols_);
        frame->labels.emplace(label);
    }
}

void Normalization::from_variable_ita_instruction(
    ITA::Quadruple const& instruction)
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    LValue lhs = get<1>(instruction);
    RValue rhs = is_unary(get<2>(instruction)) ? get<3>(instruction)
                                               : get<2>(instruction);
    if (!hoisted_symbols_.has_key(lhs))
        return;

    // @TODO: need value from _t temporaries

    if (hoisted_symbols_.has_key(get<2>(instruction)) and
        stack_frame.has_value()) {
        from_symbol_reassignment(lhs, get<2>(instruction));
        return;
    }

    RValue_Data_Type rvalue_symbol =
        is_unary(get<2>(instruction))
            ? from_rvalue_unary_expression(lhs, rhs, get<2>(instruction))
            : get_rvalue_symbol_type_size(rhs);

    Size size = get<2>(rvalue_symbol);

    if (size > std::numeric_limits<unsigned int>::max())
        credence_runtime_error(
            std::format("exceeds maximum byte size ({})", rhs),
            lhs,
            hoisted_symbols_);
    if (stack_frame.has_value()) {
        auto frame = stack_frame.value()->get();
        if (!frame->locals.contains(lhs)) {
            frame->allocation += size;
            frame->locals.emplace(lhs);
            symbols_.set_symbol_by_name(lhs, rvalue_symbol);
        } else {
            //  resize stack frame allocation on reassignment
            frame->allocation -= get<2>(symbols_.get_symbol_by_name(lhs));
            symbols_.set_symbol_by_name(lhs, rvalue_symbol);
            frame->allocation += size;
        }
    }
}

void Normalization::from_func_start_ita_instruction()
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    std::string label = get<1>(instructions_.at(instruction_index - 1));

    if (labels.contains(label))
        credence_runtime_error(
            std::format("function symbol is already defined"),
            label.substr(2),
            hoisted_symbols_);

    functions[label] =
        std::make_unique<Function_Definition>(Function_Definition{});

    labels.emplace(label);
    stack_frame = &functions.at(label);
}

void Normalization::from_func_end_ita_instruction(
    ITA::Quadruple const& instruction)
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    if (stack_frame.has_value())
        stack_frame.value()->get()->instructions.emplace_back(instruction);
    stack_frame = std::nullopt;
}

Normalization::RValue_Data_Type Normalization::get_rvalue_symbol_type_size(
    RValue const& rvalue)
{
    if (rvalue.starts_with("_t"))
        return { rvalue, "word", sizeof(void*) };

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

Normalization::RValue_Data_Type Normalization::from_rvalue_unary_expression(
    LValue const& lvalue,
    RValue const& rvalue,
    std::string_view unary_operator)
{

    return m::match(unary_operator)(
        // cppcheck-suppress syntaxError
        m::pattern | "*" =
            [&] {
                if (!symbols_.is_defined(lvalue))
                    credence_runtime_error(
                        "indirection on invalid lvalue, right-hand-side does "
                        "not exist",
                        lvalue,
                        hoisted_symbols_);
                LValue indirect_lvalue = symbols_.get_pointer_by_name(lvalue);
                if (!symbols_.is_defined(indirect_lvalue))
                    credence_runtime_error(
                        "invalid indirection assignment",
                        lvalue,
                        hoisted_symbols_);
                return symbols_.get_symbol_by_name(indirect_lvalue);
            },
        m::pattern | "&" =
            [&] {
                if (!symbols_.is_defined(lvalue))
                    credence_runtime_error(
                        "address-of invalid lvalue", lvalue, hoisted_symbols_);
                if (!symbols_.is_defined(rvalue))
                    credence_runtime_error(
                        "invalid pointer assignment, right-hand-side does not "
                        "exist",
                        rvalue,
                        hoisted_symbols_);

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

void Normalization::from_symbol_reassignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto frame = stack_frame.value()->get();
    if (!symbols_.is_defined(rhs))
        credence_runtime_error(
            "invalid lvalue assignment, right-hand-side is not initialized",
            rhs,
            hoisted_symbols_);
    auto symbol = symbols_.get_symbol_by_name(rhs);
    if (symbols_.is_defined(lhs))
        frame->allocation -= get<2>(symbols_.get_symbol_by_name(lhs));
    symbols_.set_symbol_by_name(lhs, symbol);
    frame->allocation += get<2>(symbol);
}

Normalization::RValue_Data_Type Normalization::from_integral_unary_expression(
    RValue const& lvalue)
{
    std::initializer_list<std::string_view> integral_unary = {
        "int", "double", "float", "long"
    };
    if (!symbols_.is_defined(lvalue))
        credence_runtime_error(
            "invalid integer unary expression, lvalue symbol is not "
            "initialized",
            lvalue,
            hoisted_symbols_);

    auto symbol = symbols_.get_symbol_by_name(lvalue);
    if (std::ranges::find(integral_unary, std::get<1>(symbol)) ==
        integral_unary.end())
        credence_runtime_error(
            "invalid integer unary expression on lvalue, lvalue is not an "
            "integer type",
            lvalue,
            hoisted_symbols_);

    return symbols_.get_symbol_by_name(lvalue);
}

ITA::Instructions Normalization::to_normal_form_ita(
    ITA::Node const& symbols,
    util::AST_Node ast)
{
    auto instructions = make_ITA_instructions(symbols, ast);
    auto normal = Normalization{ symbols, instructions };
    return normal.from_ita_instructions();
}

} // namespace ir

} // namespace credence