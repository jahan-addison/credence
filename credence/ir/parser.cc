#include <credence/ir/parser.h>

namespace credence {

namespace ir {

ITA::Instructions ITA_Parser::instructions_pruning_in_place(
    ITA::Instructions const& instructions)
{
    return instructions;
}

void ITA_Parser::from_ita_instructions() {}

void ITA_Parser::from_ita_function() {}

void ITA_Parser::from_ita_vector() {}

} // namespace ir

} // namespace credence