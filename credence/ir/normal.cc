#include <credence/ir/normal.h>

#include <credence/assert.h> // for CREDENCE_ASSERT, credence_error
#include <credence/ir/ita.h> // for ITA
#include <format>            // for format
#include <limits>            // for numeric_limits
#include <matchit.h>         // for pattern, PatternHelper, PatternPipable
#include <optional>          // for nullopt, nullopt_t, optional
#include <string>            // for basic_string, stoul, string
#include <tuple>             // for get, tuple

namespace credence {

namespace ir {

namespace m = matchit;

ITA::Instructions ITA_Normalize::from_ita_instructions()
{
    using std::get;
    bool skip = false;
    ITA::Instructions normalized{};
    Stack_Frame stack_frame = std::nullopt;
    ITA::Instruction last_instruction = ITA::Instruction::NOOP;

    for (instruction_index = 0; instruction_index < instructions_.size();
         instruction_index++) {
        auto instruction = instructions_.at(instruction_index);
        m::match(get<ITA::Instruction>(instruction))(
            m::pattern | ITA::Instruction::FUNC_START =
                [&] {
                    CREDENCE_ASSERT(instructions_.size() > 2);
                    std::string label =
                        get<1>(instructions_.at(instruction_index - 1));
                    if (labels.contains(label))
                        credence_error(
                            std::format(
                                "the function `{}` is already defined", label));
                    functions[label] = std::make_unique<Function_Definition>(
                        Function_Definition{});
                    labels.emplace(label);
                    // set current stack frame
                    stack_frame = &functions.at(label);
                },
            m::pattern | ITA::Instruction::FUNC_END =
                [&] {
                    CREDENCE_ASSERT(instructions_.size() > 2);
                    if (stack_frame.has_value())
                        stack_frame.value()->get()->instructions.emplace_back(
                            instruction);
                    stack_frame = std::nullopt;
                },
            m::pattern | ITA::Instruction::VARIABLE =
                [&] {
                    CREDENCE_ASSERT(instructions_.size() > 2);
                    // TODO: check type and reassignment
                    auto lhs = get<1>(instruction);
                    auto rhs = get<2>(instruction);
                    if (lhs.starts_with("_t") or lhs.starts_with("_p"))
                        return;
                    if (!rhs.starts_with("(") and !rhs.ends_with(");"))
                        return;
                    auto search = rhs.find_last_of(":") + 1;
                    auto bytes =
                        std::string{ rhs.begin() + search, rhs.end() - 1 };
                    auto rhs_t_size = std::stoul(bytes);
                    if (rhs_t_size > std::numeric_limits<unsigned int>::max())
                        credence_error(
                            std::format("`{}` exceeds maximum size", rhs));
                    if (stack_frame.has_value()) {
                        auto frame = stack_frame.value()->get();
                        if (!frame->locals.contains(lhs)) {
                            frame->allocation += rhs_t_size;
                            frame->locals.emplace(lhs);
                        }
                    }
                },

            m::pattern | ITA::Instruction::LABEL =
                [&] {
                    auto label = get<1>(instruction);
                    if (stack_frame.has_value()) {
                        auto frame = stack_frame.value()->get();
                        if (frame->labels.contains(label) != false)
                            credence_error(
                                std::format(
                                    "symbolic label `{}` is already defined",
                                    label));
                        frame->labels.emplace(label);
                    }
                },
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

ITA::Instructions ITA_Normalize::to_normal_form(
    ITA::Node const& symbols,
    ITA::Instructions const& instructions)
{
    auto normal = ITA_Normalize{ symbols, instructions };
    return normal.from_ita_instructions();
}

void ITA_Normalize::from_ita_function() {}

void ITA_Normalize::from_ita_vector() {}

} // namespace ir

} // namespace credence