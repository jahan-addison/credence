#include <credence/ir/context.h>

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

inline void Context::context_frame_error(
    std::string_view message,
    [[maybe_unused]] std::string_view symbol)
{
    credence_runtime_error(
        std::format(
            "{} in function \"{}\"", message, get_stack_frame()->symbol),
        symbol,
        hoisted_symbols_);
}

ITA::Instructions Context::from_ita_instructions()
{
    using std::get;
    bool skip = false;
    ITA::Instructions context_instructions{};
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
            context_instructions.emplace_back(instruction);
            if (is_stack_frame())
                get_stack_frame()->instructions.emplace_back(instruction);
        } else {
            skip = false;
        }
        last_instruction = std::get<ITA::Instruction>(instruction);
    }

    return context_instructions;
}

void Context::from_label_ita_instruction(ITA::Quadruple const& instruction)
{
    auto label = get<1>(instruction);
    if (is_stack_frame()) {
        auto frame = get_stack_frame();
        if (frame->labels.contains(label) != false)
            context_frame_error(
                std::format(
                    "symbol of symbolic label is already "
                    "defined"),
                label);
        frame->labels.emplace(label);
    }
}

void Context::from_variable_ita_instruction(ITA::Quadruple const& instruction)
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    LValue lhs = get<1>(instruction);
    RValue rhs =
        is_unary(get<2>(instruction)) and not get<3>(instruction).empty()
            ? get<3>(instruction)
            : get<2>(instruction);

    auto frame = get_stack_frame();

    if (lhs.starts_with("_t") or lhs.starts_with("_p")) {
        frame->temporary[lhs] = get<2>(instruction);
    } else if (rhs.starts_with("_t") and is_stack_frame()) {
        from_temporary_assignment(lhs, rhs);
    } else if (hoisted_symbols_.has_key(get<2>(instruction))) {
        from_symbol_reassignment(lhs, get<2>(instruction));
    } else {
        RValue_Data_Type rvalue_symbol =
            is_unary(get<2>(instruction))
                ? from_rvalue_unary_expression(lhs, rhs, get<2>(instruction))
                : get_rvalue_symbol_type_size(rhs);

        Size size = get<2>(rvalue_symbol);

        if (size > std::numeric_limits<unsigned int>::max())
            context_frame_error(
                std::format("exceeds maximum byte size ({})", rhs), lhs);
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

void Context::from_func_start_ita_instruction()
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    std::string label = get<1>(instructions_.at(instruction_index - 1));

    if (labels.contains(label))
        context_frame_error(
            std::format("function symbol is already defined"), label.substr(2));

    functions[label] =
        std::make_shared<Function_Definition>(Function_Definition{});

    labels.emplace(label);
    stack_frame = functions[label];
    stack_frame.value()->symbol = label;
}

void Context::from_func_end_ita_instruction(ITA::Quadruple const& instruction)
{
    CREDENCE_ASSERT(instructions_.size() > 2);
    if (is_stack_frame())
        get_stack_frame()->instructions.emplace_back(instruction);
    stack_frame = std::nullopt;
}

Context::RValue_Data_Type Context::get_rvalue_symbol_type_size(
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

Context::RValue_Data_Type Context::from_rvalue_unary_expression(
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
                    context_frame_error(
                        std::format(
                            "indirection on invalid lvalue, right-hand-side is "
                            "not "
                            "a pointer (`{}`)",
                            rvalue),
                        lvalue);
                LValue indirect_lvalue = symbols_.get_pointer_by_name(rvalue);
                if (!symbols_.is_defined(indirect_lvalue))
                    context_frame_error(
                        "invalid indirection assignment", lvalue);
                return symbols_.get_symbol_by_name(indirect_lvalue);
            },
        m::pattern | "&" =
            [&] {
                if (!symbols_.is_defined(rvalue))
                    context_frame_error(
                        std::format(
                            "invalid pointer assignment, right-hand-side is "
                            "not "
                            "initialized (`{}`)",
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

void Context::from_temporary_assignment(LValue const& lhs, LValue const& rhs)
{
    auto rvalue = get_stack_frame()->temporary.at(rhs);
    if (is_unary(rvalue)) {
        auto unary_type = get_unary(rvalue);
        auto unary_lvalue = get_unary_lvalue(rvalue);
        from_rvalue_unary_expression(lhs, unary_lvalue, unary_type);
    }
}

void Context::from_symbol_reassignment(LValue const& lhs, LValue const& rhs)
{
    auto frame = get_stack_frame();
    if (!symbols_.is_defined(rhs))
        context_frame_error(
            "invalid lvalue assignment, right-hand-side is not initialized",
            rhs);
    if (symbols_.is_pointer(rhs)) {
        symbols_.set_symbol_by_name(lhs, symbols_.get_pointer_by_name(rhs));
        frame->allocation += type::LITERAL_TYPE.at("word").second;
    } else {
        auto symbol = symbols_.get_symbol_by_name(rhs);
        if (symbols_.is_defined(lhs))
            frame->allocation -= get<2>(symbols_.get_symbol_by_name(lhs));
        symbols_.set_symbol_by_name(lhs, symbol);
        frame->allocation += get<2>(symbol);
    }
}

Context::RValue_Data_Type Context::from_integral_unary_expression(
    RValue const& lvalue)
{
    std::initializer_list<std::string_view> integral_unary = {
        "int", "double", "float", "long"
    };
    if (!symbols_.is_defined(lvalue))
        context_frame_error(
            "invalid numeric unary expression, lvalue symbol is not "
            "initialized",
            lvalue);

    auto symbol = symbols_.get_symbol_by_name(lvalue);
    if (std::ranges::find(integral_unary, std::get<1>(symbol)) ==
        integral_unary.end())
        context_frame_error(
            "invalid numeric unary expression on lvalue, lvalue is not an "
            "numeric type",
            lvalue);

    return symbols_.get_symbol_by_name(lvalue);
}

ITA::Instructions Context::to_ita_from_ast(
    ITA::Node const& symbols,
    ITA::Node const& ast)
{
    auto instructions = make_ITA_instructions(symbols, ast);
    auto context = Context{ symbols, instructions };
    return context.from_ita_instructions();
}

} // namespace ir

} // namespace credence